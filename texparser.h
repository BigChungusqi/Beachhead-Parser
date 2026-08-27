#ifndef TEXPARSER_H
#define TEXPARSER_H

#include <QString>
#include <QImage>
#include <QVector>
#include <QFileInfo>
#include <QDir>

enum class TextureFormat {
    Unknown,
    CLUT,
    RGB,
    RMAP8,
    TMAP8,
    TMAP32,
    HMAP,
    PAL,
    SPR
};

struct TextureInfo {
    TextureFormat format = TextureFormat::Unknown;
    QString formatName;
    QString fileName;
    int width = 0;
    int height = 0;
    int colorDepth = 0;
    int headerSize = 0;
    int frameCount = 1;
    int frameHeight = 0;
    qint64 fileSize = 0;
    bool hasAlpha = false;
    bool isAnimated = false;
    bool usesClut = false;
    QString clutFile;
    int palIndex = 0;
    int palCount = 0;
};

class TexParser
{
public:
    explicit TexParser();

    static TextureFormat detectFormat(const QString &fileName);
    static QString formatToString(TextureFormat fmt);

    bool parseFile(const QString &filePath);
    bool parseFile(const QString &filePath, const QString &clutPath);

    QImage getImage(int frame = 0) const;
    QImage getFrame(int index) const;
    int getFrameCount() const { return m_info.frameCount; }
    const TextureInfo &getInfo() const { return m_info; }

    const QVector<QRgb> &getClutPalette() const { return m_clutPalette; }
    const QVector<QVector<QRgb>> &getPalPalettes() const { return m_palPalettes; }

    bool loadExternalPal(const QString &palPath);
    bool reparseSprWithPal(const QString &sprPath, const QString &palPath);

    static bool tryLoadClut(const QString &filePath, QVector<QRgb> &palette);
    static bool parseDimensionsFromFilename(const QString &fileName, int &width, int &height);

private:
    bool parseCLUT(const QByteArray &data);
    bool parseRGB(const QByteArray &data);
    bool parseRMAP8(const QByteArray &data);
    bool parseTMAP8(const QByteArray &data);
    bool parseTMAP32(const QByteArray &data);
    bool parseHMAP(const QByteArray &data);
    bool parsePAL(const QByteArray &data);
    bool parseSPR(const QByteArray &data);

    // Bh.exe (2000-05-08 build) tagged-container SPR/.PAL format
    bool parseSPR_Bh(const QByteArray &data);
    bool parsePal_Bh(const QByteArray &data);
    bool loadPalForSpr_Bh(const QString &sprPath);
    static bool isBhSpr(const QByteArray &data);
    // Sprite pixel stream of Bh.exe SPR files: one continuous RLE sequence of
    //   SKIP, RUN, RUN x 8-bit palette indices
    // triplets filling width*height pixels in raster order. Counts are 1 byte,
    // or 2 bytes ((b1<<8|b2)&0x7FFF) when the first byte has the high bit set.
    // A trailing lone SKIP terminates the stream (rest stays transparent),
    // exactly how Bh.exe's blitter (sub_41B160/sub_41B3D0) consumes it.
    // Skipped pixels are emitted fully transparent because the engine leaves
    // the destination untouched there.
    static bool decodeBhSpriteRle(const QByteArray &data, int srcOff, int srcLen,
                                  int pixelsNeeded, const QVector<QRgb> *palette,
                                  QVector<QRgb> &out);

    bool loadClutForFile(const QString &filePath);
    bool loadPalForSpr(const QString &sprPath);
    QImage indexedToImage(const QByteArray &indexedData, int width, int height) const;
    QImage indexedToImage(const QByteArray &indexedData, int width, int height, const QVector<QRgb> &palette) const;

    TextureInfo m_info;
    QVector<QImage> m_frames;
    QVector<QRgb> m_clutPalette;
    QVector<QVector<QRgb>> m_palPalettes;
    QString m_baseDir;
};

#endif // TEXPARSER_H
