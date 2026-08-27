#include "texparser.h"
#include <QFile>
#include <QDataStream>
#include <QRegularExpression>
#include <QDebug>
#include <cmath>

TexParser::TexParser()
{
}

TextureFormat TexParser::detectFormat(const QString &fileName)
{
    QString ext = QFileInfo(fileName).suffix().toLower();
    if (ext == "clut")  return TextureFormat::CLUT;
    if (ext == "rgb")   return TextureFormat::RGB;
    if (ext == "rmap8") return TextureFormat::RMAP8;
    if (ext == "tmap8") return TextureFormat::TMAP8;
    if (ext == "tmap32") return TextureFormat::TMAP32;
    if (ext == "hmap")  return TextureFormat::HMAP;
    if (ext == "pal")   return TextureFormat::PAL;
    if (ext == "spr")   return TextureFormat::SPR;
    return TextureFormat::Unknown;
}

QString TexParser::formatToString(TextureFormat fmt)
{
    switch (fmt) {
    case TextureFormat::CLUT:   return "CLUT (\u8c03\u8272\u677f)";
    case TextureFormat::RGB:    return "RGB565";
    case TextureFormat::RMAP8:  return "RMAP8 (8\u4f4d\u7d22\u5f15\u8272)";
    case TextureFormat::TMAP8:  return "TMAP8 (8\u4f4d\u7d22\u5f15\u7eb9\u7406)";
    case TextureFormat::TMAP32: return "TMAP32 (32\u4f4dBGRA)";
    case TextureFormat::HMAP:   return "HMAP (\u9ad8\u5ea6\u56fe)";
    case TextureFormat::PAL:    return "PAL (\u8c03\u8272\u677f\u7ec4)";
    case TextureFormat::SPR:    return "SPR (\u7cbe\u7075/\u7eb9\u7406)";
    default:                   return "\u672a\u77e5\u683c\u5f0f";
    }
}

bool TexParser::parseDimensionsFromFilename(const QString &fileName, int &width, int &height)
{
    // Match patterns like "Barge256x256.RGB" or "texture_512x128.tmap8"
    QRegularExpression re(R"((\d{1,5})[xX](\d{1,5}))");
    QRegularExpressionMatch match = re.match(fileName);
    if (match.hasMatch()) {
        width = match.captured(1).toInt();
        height = match.captured(2).toInt();
        return true;
    }
    return false;
}

bool TexParser::tryLoadClut(const QString &filePath, QVector<QRgb> &palette)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray data = file.readAll();
    file.close();

    // CLUT is exactly 768 bytes: 256 entries * 3 bytes (RGB)
    if (data.size() != 768) {
        return false;
    }

    palette.resize(256);
    for (int i = 0; i < 256; ++i) {
        int r = (unsigned char)data[i * 3 + 0];
        int g = (unsigned char)data[i * 3 + 1];
        int b = (unsigned char)data[i * 3 + 2];
        palette[i] = qRgb(r, g, b);
    }
    return true;
}

bool TexParser::parseFile(const QString &filePath)
{
    return parseFile(filePath, QString());
}

bool TexParser::parseFile(const QString &filePath, const QString &clutPath)
{
    m_frames.clear();
    m_clutPalette.clear();
    m_palPalettes.clear();
    m_info = TextureInfo();

    QFileInfo fi(filePath);
    m_baseDir = fi.absolutePath();
    m_info.fileName = fi.fileName();
    m_info.fileSize = fi.size();
    m_info.format = detectFormat(filePath);
    m_info.formatName = formatToString(m_info.format);

    if (m_info.format == TextureFormat::Unknown) {
        qWarning() << "Unknown texture format:" << filePath;
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    // Load CLUT if provided or try to find matching CLUT
    if (clutPath.isEmpty()) {
        loadClutForFile(filePath);
    } else {
        tryLoadClut(clutPath, m_clutPalette);
    }

    bool success = false;
    switch (m_info.format) {
    case TextureFormat::CLUT:   success = parseCLUT(data); break;
    case TextureFormat::RGB:    success = parseRGB(data); break;
    case TextureFormat::RMAP8:  success = parseRMAP8(data); break;
    case TextureFormat::TMAP8:  success = parseTMAP8(data); break;
    case TextureFormat::TMAP32: success = parseTMAP32(data); break;
    case TextureFormat::HMAP:   success = parseHMAP(data); break;
    case TextureFormat::PAL:    success = parsePAL(data); break;
    case TextureFormat::SPR:    success = parseSPR(data); break;
    default: break;
    }

    return success;
}

bool TexParser::loadClutForFile(const QString &filePath)
{
    QFileInfo fi(filePath);
    QString baseName = fi.completeBaseName();
    QString dir = fi.absolutePath();

    // List of CLUT type suffixes supported by Beachhead 2000 engine
    // Based on analysis of Beachhead16.exe.lst:
    // Clut, ClutGenAlpha, ClutGenAlpha2, ClutExpand, ClutExpand555, ClutShaded
    QStringList clutTypes = {
        "",              // Base CLUT (e.g., Ocean.clut)
        "GenAlpha",      // ClutGenAlpha
        "GenAlpha2",     // ClutGenAlpha2
        "Expand",        // ClutExpand
        "Expand555",     // ClutExpand555
        "Shaded",        // ClutShaded
    };

    // Try each CLUT type with the base name
    for (const QString &type : clutTypes) {
        QString clutName = baseName + type;
        QString clutPath = dir + QDir::separator() + clutName + ".clut";
        if (tryLoadClut(clutPath, m_clutPalette)) {
            m_info.usesClut = true;
            m_info.clutFile = clutPath;
            return true;
        }

        // Try lowercase
        clutPath = dir + QDir::separator() + clutName.toLower() + ".clut";
        if (tryLoadClut(clutPath, m_clutPalette)) {
            m_info.usesClut = true;
            m_info.clutFile = clutPath;
            return true;
        }
    }

    // Try removing numbers and dimensions from base name
    // e.g., "Ocean256x256" -> "Ocean"
    QRegularExpression numRe(R"((\d+[xX]\d+)$)");
    QRegularExpressionMatch match = numRe.match(baseName);
    if (match.hasMatch()) {
        QString strippedName = baseName.left(match.capturedStart());
        for (const QString &type : clutTypes) {
            QString clutPath = dir + QDir::separator() + strippedName + type + ".clut";
            if (tryLoadClut(clutPath, m_clutPalette)) {
                m_info.usesClut = true;
                m_info.clutFile = clutPath;
                return true;
            }
        }
    }

    // Try common CLUT names in the directory
    QStringList commonCluts = {
        "Ocean", "Ocean256x256", "Ocean128x128",
        "Grass", "Grass256x256",
        "Sand", "Sand256x256",
        "Dirt", "Dirt256x256",
        "Rock", "Rock256x256",
        "Snow", "Snow256x256",
    };
    for (const QString &clutName : commonCluts) {
        QString clutPath = dir + QDir::separator() + clutName + ".clut";
        if (tryLoadClut(clutPath, m_clutPalette)) {
            m_info.usesClut = true;
            m_info.clutFile = clutPath;
            return true;
        }
    }

    return false;
}

QImage TexParser::indexedToImage(const QByteArray &indexedData, int width, int height) const
{
    return indexedToImage(indexedData, width, height, m_clutPalette);
}

QImage TexParser::indexedToImage(const QByteArray &indexedData, int width, int height, const QVector<QRgb> &palette) const
{
    if (indexedData.size() < width * height)
        return QImage();

    QImage img(width, height, QImage::Format_Indexed8);

    if (palette.size() == 256) {
        img.setColorTable(palette);
    } else {
        // Fallback: grayscale palette
        QVector<QRgb> grayPalette(256);
        for (int i = 0; i < 256; ++i)
            grayPalette[i] = qRgb(i, i, i);
        img.setColorTable(grayPalette);
    }

    memcpy(img.bits(), indexedData.constData(), width * height);
    return img;
}

bool TexParser::parseCLUT(const QByteArray &data)
{
    if (data.size() != 768) {
        qWarning() << "CLUT file should be exactly 768 bytes, got" << data.size();
        return false;
    }

    m_clutPalette.resize(256);
    for (int i = 0; i < 256; ++i) {
        int r = (unsigned char)data[i * 3 + 0];
        int g = (unsigned char)data[i * 3 + 1];
        int b = (unsigned char)data[i * 3 + 2];
        m_clutPalette[i] = qRgb(r, g, b);
    }

    // Display as 256x1 palette strip, scaled up for visibility
    QImage paletteImg(256, 1, QImage::Format_Indexed8);
    paletteImg.setColorTable(m_clutPalette);
    for (int i = 0; i < 256; ++i) {
        paletteImg.setPixel(i, 0, i);
    }

    m_info.width = 256;
    m_info.height = 1;
    m_info.colorDepth = 24;
    m_info.headerSize = 0;
    m_info.frameCount = 1;
    m_info.frameHeight = 1;
    m_info.hasAlpha = false;

    m_frames.append(paletteImg);
    return true;
}

bool TexParser::parseRGB(const QByteArray &data)
{
    const int RGB_HEADER_SIZE = 12;

    // Check for "RAW RGB " header
    bool hasHeader = false;
    int width = 0, height = 0;

    if (data.size() >= RGB_HEADER_SIZE) {
        QByteArray headerTag = data.left(8);
        if (headerTag == QByteArray("RAW RGB ", 8)) {
            hasHeader = true;
            // Width and height are at offset 8 and 10 (little-endian uint16)
            width = (unsigned char)data[8] | ((unsigned char)data[9] << 8);
            height = (unsigned char)data[10] | ((unsigned char)data[11] << 8);
        }
    }

    // If no header found, try to parse dimensions from filename
    if (!hasHeader) {
        if (!parseDimensionsFromFilename(m_info.fileName, width, height)) {
            qWarning() << "Cannot parse dimensions from RGB filename:" << m_info.fileName;
            return false;
        }
    }

    int pixelDataOffset = hasHeader ? RGB_HEADER_SIZE : 0;
    int pixelDataSize = data.size() - pixelDataOffset;
    int expectedSize = width * height * 2; // 16-bit RGB565

    if (pixelDataSize < expectedSize) {
        qWarning() << "RGB file too small:" << data.size()
                   << "header:" << (hasHeader ? RGB_HEADER_SIZE : 0)
                   << "expected pixels:" << expectedSize;
        return false;
    }

    QImage img(width, height, QImage::Format_RGBX8888);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int offset = pixelDataOffset + (y * width + x) * 2;
            // Little-endian
            quint16 pixel = (unsigned char)data[offset]
                          | ((unsigned char)data[offset + 1] << 8);

            int r5 = (pixel >> 11) & 0x1F;
            int g6 = (pixel >> 5)  & 0x3F;
            int b5 = pixel & 0x1F;

            // Expand to 8-bit
            int r = (r5 << 3) | (r5 >> 2);
            int g = (g6 << 2) | (g6 >> 4);
            int b = (b5 << 3) | (b5 >> 2);

            img.setPixelColor(x, y, QColor(r, g, b));
        }
    }

    m_info.width = width;
    m_info.height = height;
    m_info.colorDepth = 16;
    m_info.headerSize = hasHeader ? RGB_HEADER_SIZE : 0;
    m_info.frameCount = 1;
    m_info.frameHeight = height;
    m_info.hasAlpha = false;

    m_frames.append(img);
    return true;
}

bool TexParser::parseRMAP8(const QByteArray &data)
{
    const int headerSize = 128;
    if (data.size() <= headerSize) {
        qWarning() << "RMAP8 file too small:" << data.size();
        return false;
    }

    QByteArray pixelData = data.mid(headerSize);

    int width = 0, height = 0;
    if (!parseDimensionsFromFilename(m_info.fileName, width, height)) {
        // Fallback: try to guess dimensions from data size
        int pixelCount = pixelData.size();
        // Common sizes for RMAP8
        QVector<QPair<int,int>> commonSizes = {
            {256, 256}, {128, 128}, {64, 64}, {512, 512},
            {256, 128}, {128, 256}, {64, 128}, {1024, 1024}
        };
        for (auto &sz : commonSizes) {
            if (sz.first * sz.second == pixelCount) {
                width = sz.first;
                height = sz.second;
                break;
            }
        }
        if (width == 0 || height == 0) {
            qWarning() << "Cannot determine RMAP8 dimensions for:" << m_info.fileName;
            return false;
        }
    }

    QImage img = indexedToImage(pixelData, width, height);
    if (img.isNull()) return false;

    m_info.width = width;
    m_info.height = height;
    m_info.colorDepth = 8;
    m_info.headerSize = headerSize;
    m_info.frameCount = 1;
    m_info.frameHeight = height;
    m_info.hasAlpha = false;

    m_frames.append(img);
    return true;
}

bool TexParser::parseTMAP8(const QByteArray &data)
{
    const int headerSize = 128;
    if (data.size() <= headerSize) {
        qWarning() << "TMAP8 file too small:" << data.size();
        return false;
    }

    QByteArray pixelData = data.mid(headerSize);

    int width = 0, height = 0;
    if (!parseDimensionsFromFilename(m_info.fileName, width, height)) {
        qWarning() << "Cannot parse dimensions from TMAP8 filename:" << m_info.fileName;
        return false;
    }

    int totalPixels = pixelData.size();
    int singleFramePixels = width * height;

    // Detect animation: if total pixels > single frame, it's animated
    int frameCount = totalPixels / singleFramePixels;
    if (frameCount < 1) frameCount = 1;
    if (totalPixels % singleFramePixels != 0) {
        // Not exact multiple, treat as single frame
        frameCount = 1;
    }

    // Auto-detect animation for textures where height >> width
    if (frameCount == 1 && height > width * 2) {
        // Might be an animated texture stored as tall strip
        // Check if height is multiple of a reasonable frame height
        int possibleFrameHeight = width; // Assume square frames
        if (height % possibleFrameHeight == 0) {
            frameCount = height / possibleFrameHeight;
            height = possibleFrameHeight;
        }
    }

    m_info.isAnimated = (frameCount > 1);
    m_info.frameCount = frameCount;
    m_info.frameHeight = height;
    m_info.width = width;
    m_info.height = height * frameCount; // Total height
    m_info.colorDepth = 8;
    m_info.headerSize = headerSize;
    m_info.hasAlpha = false;

    // Parse frames
    for (int f = 0; f < frameCount; ++f) {
        QByteArray frameData = pixelData.mid(f * singleFramePixels, singleFramePixels);
        QImage frame = indexedToImage(frameData, width, height);
        if (frame.isNull()) {
            qWarning() << "Failed to parse TMAP8 frame" << f;
            continue;
        }
        m_frames.append(frame);
    }

    return !m_frames.isEmpty();
}

bool TexParser::parseTMAP32(const QByteArray &data)
{
    int width = 0, height = 0;
    if (!parseDimensionsFromFilename(m_info.fileName, width, height)) {
        qWarning() << "Cannot parse dimensions from TMAP32 filename:" << m_info.fileName;
        return false;
    }

    int expectedSize = width * height * 4; // 32-bit BGRA
    if (data.size() < expectedSize) {
        qWarning() << "TMAP32 file too small:" << data.size() << "expected" << expectedSize;
        return false;
    }

    QImage img(width, height, QImage::Format_ARGB32);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int offset = (y * width + x) * 4;
            quint8 b = (unsigned char)data[offset + 0];
            quint8 g = (unsigned char)data[offset + 1];
            quint8 r = (unsigned char)data[offset + 2];
            quint8 a = (unsigned char)data[offset + 3];
            img.setPixelColor(x, y, QColor(r, g, b, a));
        }
    }

    m_info.width = width;
    m_info.height = height;
    m_info.colorDepth = 32;
    m_info.headerSize = 0;
    m_info.frameCount = 1;
    m_info.frameHeight = height;
    m_info.hasAlpha = true;

    m_frames.append(img);
    return true;
}

bool TexParser::parseHMAP(const QByteArray &data)
{
    const int hmapWidth = 1501;
    const int hmapHeight = 751;
    const int expectedPixels = hmapWidth * hmapHeight;

    int headerSize = data.size() - expectedPixels;
    if (headerSize < 0) {
        qWarning() << "HMAP file too small:" << data.size()
                   << "expected at least" << expectedPixels;
        return false;
    }

    QByteArray pixelData = data.mid(headerSize);

    QImage img(hmapWidth, hmapHeight, QImage::Format_Indexed8);

    // Build terrain-like color palette
    QVector<QRgb> terrainPalette(256);
    for (int i = 0; i < 256; ++i) {
        float t = i / 255.0f;
        int r, g, b;
        if (t < 0.2f) {
            // Deep water
            r = 0; g = 0; b = 100 + (int)(t / 0.2f * 80);
        } else if (t < 0.35f) {
            // Shallow water
            float s = (t - 0.2f) / 0.15f;
            r = 0; g = (int)(s * 100); b = 180 + (int)(s * 40);
        } else if (t < 0.4f) {
            // Beach/sand
            float s = (t - 0.35f) / 0.05f;
            r = 180 + (int)(s * 60); g = 170 + (int)(s * 50); b = 100 + (int)(s * 40);
        } else if (t < 0.7f) {
            // Grass/land
            float s = (t - 0.4f) / 0.3f;
            r = 30 + (int)(s * 50); g = 120 + (int)(s * 40); b = 20 + (int)(s * 20);
        } else if (t < 0.85f) {
            // Hills/mountains
            float s = (t - 0.7f) / 0.15f;
            r = 100 + (int)(s * 60); g = 80 + (int)(s * 30); b = 40 + (int)(s * 30);
        } else {
            // Snow/peaks
            float s = (t - 0.85f) / 0.15f;
            r = 180 + (int)(s * 70); g = 180 + (int)(s * 70); b = 190 + (int)(s * 60);
        }
        terrainPalette[i] = qRgb(qMin(r, 255), qMin(g, 255), qMin(b, 255));
    }
    img.setColorTable(terrainPalette);

    memcpy(img.bits(), pixelData.constData(), expectedPixels);

    m_info.width = hmapWidth;
    m_info.height = hmapHeight;
    m_info.colorDepth = 8;
    m_info.headerSize = headerSize;
    m_info.frameCount = 1;
    m_info.frameHeight = hmapHeight;
    m_info.hasAlpha = false;

    m_frames.append(img);
    return true;
}

QImage TexParser::getImage(int frame) const
{
    if (m_frames.isEmpty()) return QImage();
    if (frame >= 0 && frame < m_frames.size())
        return m_frames[frame];
    return m_frames.first();
}

QImage TexParser::getFrame(int index) const
{
    if (index >= 0 && index < m_frames.size())
        return m_frames[index];
    return QImage();
}

bool TexParser::parsePAL(const QByteArray &data)
{
    // PAL format: offset table followed by RGB palette data
    // Each offset points to a palette; palette size = next_offset - current_offset
    // Colors are stored as RGB triplets

    QVector<quint32> offsets;
    int i = 0;
    while (i + 3 < data.size()) {
        quint32 off = (quint8)data[i] | ((quint8)data[i+1] << 8)
                    | ((quint8)data[i+2] << 16) | ((quint8)data[i+3] << 24);
        if (off >= (quint32)data.size())
            break;
        offsets.append(off);
        i += 4;
    }

    if (offsets.isEmpty()) {
        qWarning() << "PAL: no valid offsets found";
        return false;
    }

    m_palPalettes.clear();
    int numPalettes = offsets.size();

    for (int p = 0; p < numPalettes; ++p) {
        int off = offsets[p];
        int nextOff = (p + 1 < numPalettes) ? offsets[p + 1] : data.size();
        int palSize = nextOff - off;

        QVector<QRgb> palette(256);
        int colorCount = 0;
        for (int c = 0; c < palSize / 3 && c < 256; ++c) {
            int r = (quint8)data[off + c * 3 + 0];
            int g = (quint8)data[off + c * 3 + 1];
            int b = (quint8)data[off + c * 3 + 2];
            palette[c] = qRgb(r, g, b);
            colorCount++;
        }
        // Fill remaining with black
        for (int c = colorCount; c < 256; ++c)
            palette[c] = qRgb(0, 0, 0);

        m_palPalettes.append(palette);
    }

    // Display each palette as a horizontal strip
    int stripHeight = 20;
    QImage palImg(256, numPalettes * stripHeight, QImage::Format_RGB32);
    palImg.fill(Qt::black);

    for (int p = 0; p < numPalettes; ++p) {
        for (int x = 0; x < 256; ++x) {
            QColor c(m_palPalettes[p][x]);
            for (int y = 0; y < stripHeight; ++y)
                palImg.setPixelColor(x, p * stripHeight + y, c);
        }
    }

    m_info.width = 256;
    m_info.height = numPalettes * stripHeight;
    m_info.colorDepth = 24;
    m_info.headerSize = offsets.size() * 4;
    m_info.frameCount = numPalettes;
    m_info.frameHeight = stripHeight;
    m_info.hasAlpha = false;
    m_info.palCount = numPalettes;

    m_frames.append(palImg);
    return true;
}

bool TexParser::loadPalForSpr(const QString &sprPath)
{
    QFileInfo fi(sprPath);
    QString baseName = fi.completeBaseName();
    // callers may pass a bare file name; fall back to the opened file's dir
    QString dir = (fi.isRelative() && !m_baseDir.isEmpty())
                      ? m_baseDir : fi.absolutePath();

    // Try matching PAL file with same base name
    QString palPath = dir + QDir::separator() + baseName + ".PAL";
    if (!QFile::exists(palPath)) {
        palPath = dir + QDir::separator() + baseName.toLower() + ".pal";
    }

    if (!QFile::exists(palPath))
        return false;

    QFile file(palPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray data = file.readAll();
    file.close();

    // Parse PAL file inline (same logic as parsePAL but store to m_palPalettes)
    QVector<quint32> offsets;
    int i = 0;
    while (i + 3 < data.size()) {
        quint32 off = (quint8)data[i] | ((quint8)data[i+1] << 8)
                    | ((quint8)data[i+2] << 16) | ((quint8)data[i+3] << 24);
        if (off >= (quint32)data.size())
            break;
        offsets.append(off);
        i += 4;
    }

    m_palPalettes.clear();
    for (int p = 0; p < offsets.size(); ++p) {
        int off = offsets[p];
        int nextOff = (p + 1 < offsets.size()) ? offsets[p + 1] : data.size();
        int palSize = nextOff - off;

        QVector<QRgb> palette(256);
        int colorCount = 0;
        for (int c = 0; c < palSize / 3 && c < 256; ++c) {
            int r = (quint8)data[off + c * 3 + 0];
            int g = (quint8)data[off + c * 3 + 1];
            int b = (quint8)data[off + c * 3 + 2];
            palette[c] = qRgb(r, g, b);
            colorCount++;
        }
        for (int c = colorCount; c < 256; ++c)
            palette[c] = qRgb(0, 0, 0);

        m_palPalettes.append(palette);
    }

    return !m_palPalettes.isEmpty();
}

bool TexParser::loadExternalPal(const QString &palPath)
{
    QFile file(palPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray data = file.readAll();
    file.close();

    // Parse PAL file
    QVector<quint32> offsets;
    int i = 0;
    while (i + 3 < data.size()) {
        quint32 off = (quint8)data[i] | ((quint8)data[i+1] << 8)
                    | ((quint8)data[i+2] << 16) | ((quint8)data[i+3] << 24);
        if (off >= (quint32)data.size())
            break;
        offsets.append(off);
        i += 4;
    }

    m_palPalettes.clear();
    for (int p = 0; p < offsets.size(); ++p) {
        int off = offsets[p];
        int nextOff = (p + 1 < offsets.size()) ? offsets[p + 1] : data.size();
        int palSize = nextOff - off;

        QVector<QRgb> palette(256);
        int colorCount = 0;
        for (int c = 0; c < palSize / 3 && c < 256; ++c) {
            int r = (quint8)data[off + c * 3 + 0];
            int g = (quint8)data[off + c * 3 + 1];
            int b = (quint8)data[off + c * 3 + 2];
            palette[c] = qRgb(r, g, b);
            colorCount++;
        }
        for (int c = colorCount; c < 256; ++c)
            palette[c] = qRgb(0, 0, 0);

        m_palPalettes.append(palette);
    }

    return !m_palPalettes.isEmpty();
}

bool TexParser::reparseSprWithPal(const QString &sprPath, const QString &palPath)
{
    // Load the external PAL first
    if (!palPath.isEmpty()) {
        loadExternalPal(palPath);
    }

    // Re-parse the SPR file
    QFile file(sprPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray data = file.readAll();
    file.close();

    m_frames.clear();
    m_info = TextureInfo();

    QFileInfo fi(sprPath);
    m_baseDir = fi.absolutePath();
    m_info.fileName = fi.fileName();
    m_info.fileSize = fi.size();
    m_info.format = TextureFormat::SPR;
    m_info.formatName = formatToString(m_info.format);

    return parseSPR(data);
}

bool TexParser::parseSPR(const QByteArray &data)
{
    // SPR format:
    // - 4 bytes: number of sprites
    // - 4 bytes each: offsets to sprite data
    // Each sprite:
    // - byte 0: type (0x05)
    // - bytes 1-2: width (little-endian)
    // - bytes 3-4: height (little-endian)
    // - bytes 5-7: unknown flags
    // - remaining: raw 8-bit indexed pixel data

    if (data.size() < 4) {
        qWarning() << "SPR file too small";
        return false;
    }

    // Bh.exe (2000-05-08 build) uses a tagged container; detect & dispatch.
    if (isBhSpr(data))
        return parseSPR_Bh(data);

    quint32 numSprites = (quint8)data[0] | ((quint8)data[1] << 8)
                       | ((quint8)data[2] << 16) | ((quint8)data[3] << 24);

    QVector<quint32> offsets;
    for (quint32 j = 0; j < numSprites && 4 + j * 4 + 3 < (quint32)data.size(); ++j) {
        quint32 off = (quint8)data[4 + j * 4 + 0] | ((quint8)data[4 + j * 4 + 1] << 8)
                    | ((quint8)data[4 + j * 4 + 2] << 16) | ((quint8)data[4 + j * 4 + 3] << 24);
        if (off > 0 && off < (quint32)data.size())
            offsets.append(off);
        else
            break;
    }

    if (offsets.isEmpty()) {
        qWarning() << "SPR: no valid sprite offsets found";
        return false;
    }

    // Load matching PAL file
    if (m_palPalettes.isEmpty()) {
        loadPalForSpr(m_info.fileName);
    }

    int validSprites = 0;
    int totalWidth = 0;
    int maxHeight = 0;

    // First pass: collect valid sprite dimensions and their palette indices
    struct SpriteInfo { int width; int height; int offset; int size; int palIndex; };
    QVector<SpriteInfo> sprites;

    for (int s = 0; s < offsets.size(); ++s) {
        int off = offsets[s];
        int nextOff = (s + 1 < offsets.size()) ? offsets[s + 1] : data.size();
        int spriteSize = nextOff - off;

        if (spriteSize < 8)
            continue;

        int width = (quint8)data[off + 1] | ((quint8)data[off + 2] << 8);
        int height = (quint8)data[off + 3] | ((quint8)data[off + 4] << 8);
        int palIdx = (quint8)data[off + 5]; // Palette index from sprite header

        if (width <= 0 || height <= 0 || width > 2048 || height > 2048)
            continue;

        sprites.append({width, height, off, spriteSize, palIdx});
        totalWidth += width;
        maxHeight = qMax(maxHeight, height);
    }

    if (sprites.isEmpty()) {
        qWarning() << "SPR: no valid sprites found";
        return false;
    }

    // Render all sprites into a single atlas image
    // Use ARGB32 format to support per-sprite palette switching
    QImage atlas(totalWidth, maxHeight, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);

    int xPos = 0;
    for (const auto &spr : sprites) {
        int width = spr.width;
        int height = spr.height;
        int off = spr.offset;
        int palIdx = spr.palIndex;

        // Select palette for this sprite
        QVector<QRgb> spritePalette;
        if (!m_palPalettes.isEmpty() && palIdx >= 0 && palIdx < m_palPalettes.size()) {
            spritePalette = m_palPalettes[palIdx];
        } else if (!m_palPalettes.isEmpty()) {
            spritePalette = m_palPalettes[0];
        }

        QByteArray pixelData = data.mid(off + 8, width * height);
        if (pixelData.size() < width * height) {
            // Try 6-byte header
            pixelData = data.mid(off + 6, width * height);
        }

        if (pixelData.size() >= width * height && spritePalette.size() == 256) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    quint8 idx = (quint8)pixelData[y * width + x];
                    QRgb rgb = spritePalette[idx];
                    // Treat palette index 0 as transparent
                    if (idx == 0) {
                        atlas.setPixelColor(xPos + x, y, Qt::transparent);
                    } else {
                        atlas.setPixelColor(xPos + x, y, QColor(qRed(rgb), qGreen(rgb), qBlue(rgb)));
                    }
                }
            }
        } else if (pixelData.size() >= width * height) {
            // Grayscale fallback
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    quint8 idx = (quint8)pixelData[y * width + x];
                    if (idx == 0) {
                        atlas.setPixelColor(xPos + x, y, Qt::transparent);
                    } else {
                        atlas.setPixelColor(xPos + x, y, QColor(idx, idx, idx));
                    }
                }
            }
        }
        xPos += width;
        validSprites++;
    }

    m_info.width = totalWidth;
    m_info.height = maxHeight;
    m_info.colorDepth = 8;
    m_info.headerSize = 4 + numSprites * 4;
    m_info.frameCount = validSprites;
    m_info.frameHeight = maxHeight;
    m_info.hasAlpha = false;

    m_frames.append(atlas);
    return true;
}

// ----------------------------------------------------------------------------
// Bh.exe (2000-05-08 build) tagged-container SPR / .PAL loader.
//
// SPR file layout (little-endian), reverse-engineered from Bh.exe.lst:
//   u32  [0]        = directory byte size, which doubles as offset[0]: the
//                     first sprite sits right behind the table
//   u32  [i*4]      = offset[i]      (entry count = dword[0] >> 2)
//   sprite[offset]:
//     u8  +0  type     shipped files use 5 only; 5/21 are the drawable records
//                      in sub_411990; bit1 marks palette-mapped images, bit0
//                      the keyed variant; type 8 = named external PCX ref
//     u16 +1  width
//     u16 +3  height
//     u8  +5  palette index (selects a block in the matching .PAL)
//     ...+6  continuous RLE stream: [SKIP][RUN][RUN x indices] triplets,
//            see decodeBhSpriteRle()
// .PAL file layout:
//   u32  [0]        = F'              (M' = F'>>2 directory entries)
//   u32  [i*4]      = blockOffset[i]
//   block[blockOffset]:
//     u16 header word: bit15 = alternate-CLUT flag, bits 0..14 = color count
//     count * 3 bytes  => 24-bit RGB palette
// The engine converts blocks to RGB565 on demand using the current display
// mode's shift/mask globals; sprites map indices through palette[palIdx].
// Skipped RLE spans render transparent because the blitter leaves the
// destination untouched there.
// ----------------------------------------------------------------------------

static inline quint32 rd32(const char *p)
{
    return (quint8)p[0] | ((quint8)p[1] << 8) | ((quint8)p[2] << 16) | ((quint8)p[3] << 24);
}
static inline quint16 rd16(const char *p)
{
    return (quint8)p[0] | ((quint8)p[1] << 8);
}

bool TexParser::isBhSpr(const QByteArray &data)
{
    if (data.size() < 8) return false;
    quint32 F = rd32(data.constData());
    if (F < 4 || (F & 3) != 0) return false;
    quint32 M = F >> 2;
    if ((quint32)(M * 4) > (quint32)data.size()) return false;
    // record 0 sits at byte F; its type must be a known Bh tag, dims sane.
    quint8 t = (quint8)data[F];
    if (t != 2 && t != 3 && t != 4 && t != 5 && t != 8 && t != 20 && t != 21) return false;
    int w = rd16(data.constData() + F + 1);
    int h = rd16(data.constData() + F + 3);
    if (w <= 0 || w > 2048 || h <= 0 || h > 2048) return false;
    return true;
}

// Bounds-safe variable-length count reader: mirrors the engine's inner loops
// (sub_41B160 seek / sub_412E00 converter): one byte, or two bytes forming
// ((b1<<8|b2) & 0x7FFF) when b1 has the high bit set. Returns false at EOF.
static inline bool rdBhCount(const uchar *src, int len, int &p, quint32 &val)
{
    if (p >= len) return false;
    quint32 v = src[p++];
    if (v & 0x80) {
        if (p >= len) return false;
        v = ((v << 8) | src[p++]) & 0x7FFF;
    }
    val = v;
    return true;
}

bool TexParser::decodeBhSpriteRle(const QByteArray &data, int srcOff, int srcLen,
                                  int pixelsNeeded, const QVector<QRgb> *palette,
                                  QVector<QRgb> &out)
{
    out.clear();
    if (pixelsNeeded <= 0 || srcLen <= 0 || srcOff > data.size() - srcLen)
        return false;
    out.reserve(pixelsNeeded);

    const uchar *src = reinterpret_cast<const uchar *>(data.constData()) + srcOff;

    auto colorAt = [&](quint8 idx) -> QRgb {
        if (!palette || palette->isEmpty())
            return qRgb(idx, idx, idx);                     // no palette: grayscale (debug)
        return (*palette)[qMin<int>(idx, palette->size() - 1)];
    };

    int p = 0;
    while (out.size() < pixelsNeeded) {
        quint32 skip;
        if (!rdBhCount(src, srcLen, p, skip))
            break;                                          // stream end; rest transparent

        for (quint32 k = 0; k < skip && out.size() < pixelsNeeded; ++k)
            out.append(0);                                  // skipped pixels stay transparent

        if (out.size() >= pixelsNeeded)
            break;                                          // terminal tail skip reached W*H

        quint32 run;
        if (!rdBhCount(src, srcLen, p, run)) {
            qWarning("decodeBhSpriteRle: truncated run count at stream offset %d", p);
            break;
        }
        if (p + (int)run > srcLen) {                        // malformed/clipped literals
            qWarning("decodeBhSpriteRle: literal run %u overflows %d bytes, clipping",
                     run, srcLen - p);
            run = (quint32)(srcLen - p);
        }
        while (run-- > 0 && out.size() < pixelsNeeded) {
            const quint8 idx = src[p++];
            out.append(colorAt(idx));
        }
    }

    while (out.size() < pixelsNeeded)                       // safety pad
        out.append(0);
    return true;
}

bool TexParser::parsePal_Bh(const QByteArray &data)
{
    m_palPalettes.clear();
    if (data.size() < 4) return false;
    quint32 F = rd32(data.constData());
    quint32 M = F >> 2;
    for (quint32 i = 0; i < M; ++i) {
        quint32 blockOff = rd32(data.constData() + i * 4);
        if (blockOff + 2 > (quint32)data.size()) break;
        // u16 header word: bit15 is the engine's alternate-CLUT flag
        // (sub_40E770 vs sub_40E630); the color count lives in bits 0..14.
        // Shipped .PAL blocks are flagged 0x8100 = 256 RGB triplets.
        const quint16 raw = rd16(data.constData() + blockOff);
        const quint32 numColors = raw & 0x7FFFu;
        QVector<QRgb> pal(256, qRgb(0, 0, 0));
        const quint32 count = qMin<quint32>(numColors, 256);
        for (quint32 c = 0; c < count; ++c) {
            const quint32 p = blockOff + 2 + c * 3;
            if (p + 2 >= (quint32)data.size()) break;
            pal[c] = qRgb((quint8)data[p], (quint8)data[p + 1], (quint8)data[p + 2]);
        }
        if (numColors > 256 || numColors == 0)
            qWarning("parsePal_Bh: block %u has odd color count %u (raw=0x%04X)",
                     i, numColors, raw);
        m_palPalettes.append(pal);
    }
    return !m_palPalettes.isEmpty();
}

bool TexParser::loadPalForSpr_Bh(const QString &sprPath)
{
    QFileInfo fi(sprPath);
    // callers may pass a bare file name; fall back to the opened file's dir
    QString dir = (fi.isRelative() && !m_baseDir.isEmpty())
                      ? m_baseDir : fi.absolutePath();
    QString baseName = fi.completeBaseName();

    auto tryPal = [&](const QString &path) -> bool {
        if (!QFile::exists(path)) return false;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return false;
        QByteArray d = f.readAll();
        f.close();
        return parsePal_Bh(d);
    };

    // 1) Same-basename palette, several common casings.
    if (tryPal(dir + QDir::separator() + baseName + ".PAL")) return true;
    if (tryPal(dir + QDir::separator() + baseName + ".pal")) return true;
    if (tryPal(dir + QDir::separator() + baseName + ".Pal")) return true;

    // 2) Fallback: first *.pal in the same directory (a folder often ships one
    //    shared palette used by many sprites, or the stem differs).
    QDir d(dir);
    QStringList pals = d.entryList(QStringList() << "*.pal" << "*.PAL",
                                   QDir::Files, QDir::Name);
    if (!pals.isEmpty())
        return tryPal(d.absoluteFilePath(pals.first()));

    return false;
}

bool TexParser::parseSPR_Bh(const QByteArray &data)
{
    m_frames.clear();
    if (m_palPalettes.isEmpty())
        loadPalForSpr_Bh(m_info.fileName);

    quint32 F = rd32(data.constData());
    quint32 M = F >> 2;
    QVector<quint32> offsets;
    for (quint32 i = 0; i < M; ++i) {
        quint32 o = rd32(data.constData() + i * 4);
        if (o < (quint32)data.size())
            offsets.append(o);
        else
            break;
    }
    if (offsets.isEmpty()) return false;

    int maxW = 0, maxH = 0;
    for (int s = 0; s < offsets.size(); ++s) {
        const quint32 off = offsets[s];
        if (off < 6) continue;                 // null directory slot

        // Directory offsets are stored ascending; the next strictly greater
        // one (or EOF) bounds this sprite's pixel stream. Null slots and
        // shared/duplicate offsets must not terminate the region early.
        int j = s + 1;
        while (j < offsets.size() && offsets[j] <= off) ++j;
        const quint32 nxt = (j < offsets.size()) ? offsets[j]
                                                 : (quint32)data.size();
        if (off + 6 > (quint32)data.size() || nxt <= off + 6) continue;

        const quint8 type = (quint8)data[off];
        // Drawable records drawn by sub_411990 are 5/21 (plus unkeyed 4/20).
        // Types 2/3 are the legacy palette-mapped family whose streams carry
        // 16-bit indices (sub_412E00); none ship in the released files.
        switch (type) {
        case 4: case 5: case 20: case 21:
            break;
        case 2: case 3:
            qWarning("parseSPR_Bh: entry %d uses legacy type %d, skipped", s, type);
            continue;
        case 8:                                 // named external PCX reference
            qDebug("parseSPR_Bh: entry %d is an external PCX reference, skipped", s);
            continue;
        default:
            continue;
        }
        const int w = rd16(data.constData() + off + 1);
        const int h = rd16(data.constData() + off + 3);
        const int palIdx = (quint8)data[off + 5];
        if (w <= 0 || h <= 0 || w > 2048 || h > 2048) continue;

        const QVector<QRgb> *pal =
            (palIdx >= 0 && palIdx < m_palPalettes.size()) ? &m_palPalettes[palIdx]
                                                           : nullptr;
        QVector<QRgb> px;
        if (!decodeBhSpriteRle(data, (int)(off + 6), (int)(nxt - off - 6),
                               w * h, pal, px))
            continue;

        QImage img(w, h, QImage::Format_ARGB32);
        QRgb *dst = reinterpret_cast<QRgb *>(img.bits());
        for (int i = 0; i < w * h; ++i) {
            const QRgb c = px[i];
            dst[i] = c ? (c | 0xFF000000u) : 0u;   // alpha only where painted
        }

        m_frames.append(img);
        maxW = qMax(maxW, w);
        maxH = qMax(maxH, h);
    }

    if (m_frames.isEmpty()) return false;
    m_info.format = TextureFormat::SPR;
    m_info.formatName = "SPR (Bh.exe tagged)";
    m_info.width = maxW;
    m_info.height = maxH;
    m_info.colorDepth = 8;
    m_info.headerSize = (int)(4 + M * 4);
    m_info.frameCount = m_frames.size();
    m_info.frameHeight = maxH;
    m_info.isAnimated = m_frames.size() > 1;
    m_info.hasAlpha = true;
    m_info.palCount = m_palPalettes.size();
    return true;
}
