#include "automix.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace legato::automix {

namespace {

// Levels are judged on a second of sound (21 windows), so a drum hit or a short pause between notes
// does not count as the outro or the intro.
constexpr qsizetype SmoothRadius = 10;
// Below the song's loudest second by this much, or below SilenceFloor, a window is silence.
constexpr float SilenceDb = 45;
constexpr float SilenceFloor = -70;
// The old song plays at full level until it falls this far below its loudest second,
constexpr float BodyDb = 4;
// and the new one comes in once it has fallen this far.
constexpr float DropDb = 12;
// An intro is quiet until it comes within this much of the new song's loudest second.
constexpr float IntroDb = 6;
// A head can hold nothing but a quiet intro; it is quiet next to the old song too when it stays this
// far under the old song's loudest second.
constexpr float QuieterThanOldDb = 6;
// Most of a quiet intro that plays under the old song before that one fades.
constexpr double MaxUnderlay = 6;
// A quiet intro shorter than this is the attack of a loud start, which fades in over FadeIn.
constexpr double QuietIntroSeconds = 1;
constexpr double FadeIn = 1.5;

// Beats are found in onsets (rises in level) in six bands from the kick to the cymbals, in 10 ms
// steps. Each band's power is averaged over 40 ms and compared with 30 ms before; a rise counts above
// 1 dB, so the ripple of a held tone is none, and levels count from 50 dB under the band's loudest
// step, so noise in a quiet passage makes none either. Onsets count above their mean over half a
// second around them, so a busy texture does not look like a beat.
constexpr double HopSeconds = 0.01;
constexpr std::array<double, 6> BandHz{60, 150, 400, 1000, 2500, 6000};
constexpr qsizetype PowerHops = 4;
constexpr qsizetype RiseHops = 3;
constexpr double MinRise = 1;
constexpr float OnsetRange = 50;
constexpr double OnsetMeanSeconds = 0.25;
// Fewer onsets than this on average (dB a step, over the bands) are a few stray notes; music has at
// least three times as much.
constexpr double MinOnset = 0.02;
// How clearly the onsets repeat is judged over six seconds, at periods of 200 to 60 BPM, against
// periods down to twice as fast.
constexpr double BeatSpan = 6;
constexpr double FastestBeat = 0.3;
constexpr double SlowestBeat = 1.0;
// Judged every quarter of a second, which the six seconds hardly change, and held in between.
constexpr qsizetype BeatStride = 5;
// A part has a beat when its onsets repeat at least this clearly.
constexpr float BeatThreshold = 0.3f;
// How long two beats that are not matched may play together: the change from one to the other.
constexpr double BeatChange = 1;
// Overlaps this long swap the bass.
constexpr double BassSwapOverlap = 2;

// A band-pass filter, Q 1 (Robert Bristow-Johnson's cookbook).
struct BandPass {
    double b0 = 1, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    BandPass(double hz, int rate)
    {
        const double w = 2 * std::numbers::pi * hz / rate;
        const double alpha = std::sin(w) / 2;
        const double a0 = 1 + alpha;
        b0 = alpha / a0;
        b2 = -b0;
        a1 = -2 * std::cos(w) / a0;
        a2 = (1 - alpha) / a0;
    }

    double step(double x)
    {
        const double y = b0 * x + z1;
        z1 = -a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// The mean of `values` from index `from` to `to`, both included; missing values count as 0.
float mean(const QList<float> &values, qsizetype from, qsizetype to)
{
    if (to < from)
        return 0;
    double sum = 0;
    for (qsizetype i = from; i <= to; ++i)
        sum += i >= 0 && i < values.size() ? values.at(i) : 0;
    return float(sum / double(to - from + 1));
}

QList<float> smooth(const QList<float> &levels)
{
    QList<double> power(levels.size());
    for (qsizetype i = 0; i < levels.size(); ++i)
        power[i] = std::pow(10.0, levels.at(i) / 10.0);
    QList<float> out(levels.size());
    for (qsizetype i = 0; i < levels.size(); ++i) {
        const qsizetype from = std::max<qsizetype>(0, i - SmoothRadius);
        const qsizetype to = std::min(levels.size() - 1, i + SmoothRadius);
        double sum = 0;
        for (qsizetype j = from; j <= to; ++j)
            sum += power.at(j);
        out[i] = float(10 * std::log10(std::max(sum / double(to - from + 1), 1e-10)));
    }
    return out;
}

float loudest(const QList<float> &levels)
{
    return levels.isEmpty() ? Silence : *std::max_element(levels.cbegin(), levels.cend());
}

bool sound(float level, float reference)
{
    return level > std::max(reference - SilenceDb, SilenceFloor);
}

} // namespace

QList<float> envelope(const qint16 *samples, qsizetype count, int rate)
{
    const qsizetype window = std::max<qsizetype>(1, qsizetype(rate * WindowSeconds));
    QList<float> out;
    out.reserve(count / window);
    for (qsizetype start = 0; start + window <= count; start += window) {
        double sum = 0;
        for (qsizetype i = start; i < start + window; ++i) {
            const double s = samples[i] / 32768.0;
            sum += s * s;
        }
        const double rms = std::sqrt(sum / double(window));
        out.append(rms > 0 ? std::max(Silence, float(20 * std::log10(rms))) : Silence);
    }
    return out;
}

QList<float> beats(const qint16 *samples, qsizetype count, int rate)
{
    const qsizetype window = std::max<qsizetype>(1, qsizetype(rate * WindowSeconds));
    const qsizetype hop = std::max<qsizetype>(1, qsizetype(rate * HopSeconds));
    const qsizetype hops = count / hop;
    QList<float> out(count / window, 0.0f);
    if (hops <= RiseHops)
        return out;

    // The onsets of each band, averaged over the bands.
    QList<double> onset(hops, 0.0);
    QList<double> power(hops);
    QList<double> db(hops);
    for (const double hz : BandHz) {
        BandPass filter(hz, rate);
        for (qsizetype h = 0; h < hops; ++h) {
            double sum = 0;
            for (qsizetype i = h * hop; i < (h + 1) * hop; ++i) {
                const double y = filter.step(samples[i] / 32768.0);
                sum += y * y;
            }
            power[h] = sum / double(hop);
        }
        double running = 0;
        for (qsizetype h = 0; h < hops; ++h) {
            running += power.at(h) - (h >= PowerHops ? power.at(h - PowerHops) : 0);
            db[h] = 10 * std::log10(std::max(running, 0.0) / double(std::min(h + 1, PowerHops)) + 1e-12);
        }
        const double floor = *std::max_element(db.cbegin(), db.cend()) - OnsetRange;
        for (double &level : db)
            level = std::max(level, floor);
        for (qsizetype h = RiseHops; h < hops; ++h)
            onset[h] += std::max(0.0, db.at(h) - db.at(h - RiseHops) - MinRise) / double(BandHz.size());
    }
    const qsizetype meanRadius = qsizetype(OnsetMeanSeconds / HopSeconds);
    QList<double> peaks(hops);
    {
        double sum = 0;
        qsizetype from = 0;
        qsizetype to = 0;
        for (qsizetype h = 0; h < hops; ++h) {
            for (; to < std::min(hops, h + meanRadius + 1); ++to)
                sum += onset.at(to);
            for (; from < h - meanRadius; ++from)
                sum -= onset.at(from);
            peaks[h] = std::max(0.0, onset.at(h) - sum / double(to - from));
        }
    }

    // How well the onsets match themselves one beat later: their autocorrelation at the best beat
    // period, above its median down to half that period, against the autocorrelation at no delay.
    const qsizetype span = qsizetype(BeatSpan / HopSeconds);
    const qsizetype fastest = qsizetype(FastestBeat / HopSeconds);
    const qsizetype slowest = qsizetype(SlowestBeat / HopSeconds);
    QList<double> x;
    QList<double> r;
    for (qsizetype w = 0; w < out.size(); ++w) {
        if (w % BeatStride != 0) {
            out[w] = out.at(w - 1);
            continue;
        }
        const qsizetype centre = ((w + BeatStride / 2) * window + window / 2) / hop;
        const qsizetype to = std::min(hops, std::max(centre + span / 2, span));
        const qsizetype from = std::max<qsizetype>(0, to - span);
        const qsizetype n = to - from;
        if (n < 2 * slowest)
            continue;
        double average = 0;
        for (qsizetype h = from; h < to; ++h)
            average += peaks.at(h);
        average /= double(n);
        if (average < MinOnset)
            continue;
        x.resize(n);
        double zero = 0;
        for (qsizetype i = 0; i < n; ++i) {
            x[i] = peaks.at(from + i) - average;
            zero += x[i] * x[i];
        }
        zero /= double(n);
        if (zero <= 1e-12)
            continue;
        r.clear();
        double best = 0;
        for (qsizetype lag = fastest / 2; lag <= slowest; ++lag) {
            double sum = 0;
            for (qsizetype i = 0; i + lag < n; ++i)
                sum += x[i] * x[i + lag];
            r.append(sum / double(n - lag));
            if (lag >= fastest)
                best = std::max(best, r.last());
        }
        std::nth_element(r.begin(), r.begin() + r.size() / 2, r.end());
        out[w] = float(std::clamp((best - r.at(r.size() / 2)) / zero, 0.0, 1.0));
    }
    return out;
}

Part measure(const qint16 *samples, qsizetype count, int rate)
{
    return {envelope(samples, count, rate), beats(samples, count, rate)};
}

std::optional<crossfade::Plan> plan(const Part &tailPart, const Part &headPart, double maxOverlap, Judged *judged)
{
    const QList<float> &tail = tailPart.level;
    const QList<float> &head = headPart.level;
    const QList<float> outLevel = smooth(tail);
    const QList<float> inLevel = smooth(head);
    const float outRef = loudest(outLevel);
    const float inRef = std::max(loudest(inLevel), outRef - QuieterThanOldDb);
    const qsizetype n = tail.size();

    // The old song's last sound, and where it starts to fade before it.
    qsizetype last = n - 1;
    while (last >= 0 && !sound(tail.at(last), outRef))
        --last;
    qsizetype first = 0;
    while (first < head.size() && !sound(head.at(first), inRef))
        ++first;
    if (last < 0 || first >= head.size())
        return std::nullopt;
    qsizetype body = last;
    while (body > 0 && outLevel.at(body) < outRef - BodyDb)
        --body;
    qsizetype drop = body;
    while (drop < last && outLevel.at(drop) > outRef - DropDb)
        ++drop;

    // The new song's intro: from its first sound until it is nearly as loud as it gets.
    qsizetype loud = first;
    while (loud < head.size() && inLevel.at(loud) < inRef - IntroDb)
        ++loud;
    const double intro = double(loud - first) * WindowSeconds;

    // Seconds before the end of the old song.
    const auto fromEnd = [n](qsizetype i) { return double(n - 1 - i) * WindowSeconds; };
    const double silence = fromEnd(last);
    const double dropAt = fromEnd(drop);
    const double limit = std::max(maxOverlap, 0.0);
    // The new song's body arrives as the old one drops; the old song's fade under it is cut short
    // when the change would overlap longer than the limit.
    double underlay = std::min({intro, MaxUnderlay, limit});
    double fadeOut = std::min(dropAt - silence, limit - underlay);

    // The beats of the parts that would play together: the old song from where the new one starts
    // until it stops, the new one from its first sound for as long.
    const auto tailIndex = [n](double beforeEnd) { return n - 1 - qsizetype(std::lround(beforeEnd / WindowSeconds)); };
    const double overlap = underlay + fadeOut;
    const float oldBeat = mean(tailPart.beat, tailIndex(dropAt + underlay), tailIndex(dropAt - fadeOut));
    const float newBeat = mean(headPart.beat, first, first + qsizetype(std::lround(overlap / WindowSeconds)));
    const bool clash = overlap > BeatChange && oldBeat >= BeatThreshold && newBeat >= BeatThreshold;
    if (clash) {
        underlay = 0;
        fadeOut = std::min(fadeOut, BeatChange);
    }
    if (judged)
        *judged = {oldBeat, newBeat, clash};

    crossfade::Plan out;
    out.start = dropAt + underlay;
    out.stop = dropAt - fadeOut;
    out.inFrom = double(first) * WindowSeconds;
    out.outFadeFrom = underlay;
    out.inRamp = intro >= QuietIntroSeconds ? 0 : std::min(FadeIn, out.overlap());
    // The new song brings its bass as its body arrives, where the old one begins to fade.
    if (out.overlap() >= BassSwapOverlap)
        out.bassSwap = underlay;
    return out;
}

} // namespace legato::automix
