#include "coverprovider.h"
#include "palette.h"

#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QTest>

using namespace legato;

class PaletteTest : public QObject
{
    Q_OBJECT

private slots:
    void findsTheMainHues()
    {
        // Mostly red with a blue band: red first, blue second.
        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(QColor(220, 30, 40));
        QPainter(&image).fillRect(0, 0, 100, 30, QColor(30, 60, 220));
        const QList<QColor> colors = extractPalette(image);
        QCOMPARE(colors.size(), 3);
        QVERIFY(colors[0].red() > 180 && colors[0].blue() < 80);
        QVERIFY(colors[1].blue() > 180 && colors[1].red() < 80);
    }

    void greyCoversGiveANeutralPalette()
    {
        QImage image(64, 64, QImage::Format_RGB32);
        image.fill(QColor(128, 128, 128));
        const QList<QColor> colors = extractPalette(image);
        QCOMPARE(colors.size(), 3);
        QCOMPARE(colors[0].hsvSaturation(), 0);
    }

    void accentKeepsContrast()
    {
        // A near-white cover still gives an accent visible on the light background, and the
        // near-black one on the dark background.
        QVERIFY(accentFrom(QColor(250, 245, 230), false).lightnessF() <= 0.52f);
        QVERIFY(accentFrom(QColor(20, 10, 40), true).lightnessF() >= 0.62f);
    }

    void decodeCropsTheCenter()
    {
        // A wide image: left third red, middle green, right third blue.
        QImage wide(300, 100, QImage::Format_RGB32);
        QPainter painter(&wide);
        painter.fillRect(0, 0, 100, 100, Qt::red);
        painter.fillRect(100, 0, 100, 100, Qt::green);
        painter.fillRect(200, 0, 100, 100, Qt::blue);
        painter.end();
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        wide.save(&buffer, "PNG");

        const QImage square = CoverProvider::decode(png, QSize(50, 50));
        QCOMPARE(square.size(), QSize(50, 50));
        QCOMPARE(QColor(square.pixel(25, 25)), QColor(Qt::green));
        QVERIFY(CoverProvider::decode("not an image", QSize(50, 50)).isNull());
    }
};

QTEST_MAIN(PaletteTest)
#include "palette_test.moc"
