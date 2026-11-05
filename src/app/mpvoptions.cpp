#include "mpvoptions.h"

#include <QCoreApplication>
#include <QString>

#include <mpv/client.h>

using namespace Qt::Literals::StringLiterals;

namespace legato::mpvoptions {

void setCommon(mpv_handle *mpv)
{
    // An option this libmpv does not know fails here and is skipped: the list covers older and newer
    // versions than the one a system has.
    const auto option = [mpv](const char *name, const char *value) { mpv_set_option_string(mpv, name, value); };
    option("vid", "no");
    option("audio-display", "no");
    option("terminal", "no");
    option("config", "no");
    option("input-default-bindings", "no");
    option("idle", "yes");
    option("user-agent", qPrintable(u"Legato/%1"_s.arg(QCoreApplication::applicationVersion())));
    // load-scripts skips the user's scripts only. mpv's built-in ones load in every instance, each a Lua
    // thread that receives every event; each is turned off by its own option.
    option("load-scripts", "no");
    option("ytdl", "no");
    option("osc", "no");
    option("load-stats-overlay", "no");
    option("load-osd-console", "no");
    option("load-console", "no");
    option("load-auto-profiles", "no");
    option("load-select", "no");
    option("load-positioning", "no");
    option("load-commands", "no");
    option("load-context-menu", "no");
}

} // namespace legato::mpvoptions
