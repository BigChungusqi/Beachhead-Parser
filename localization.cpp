#include "localization.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>

Localization& Localization::instance()
{
    static Localization inst;
    return inst;
}

bool Localization::loadLanguage(const QString &langCode)
{
    m_currentLang = langCode;
    m_strings.clear();

    // Try to load from external JSON file first
    QStringList searchPaths = {
        QDir::currentPath() + "/lang/" + langCode + ".json",
        QDir::currentPath() + "/languages/" + langCode + ".json",
        QCoreApplication::applicationDirPath() + "/lang/" + langCode + ".json",
        QCoreApplication::applicationDirPath() + "/languages/" + langCode + ".json",
    };

    for (const QString &path : searchPaths) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                for (auto it = obj.begin(); it != obj.end(); ++it) {
                    m_strings[it.key()] = it.value().toString();
                }
                return true;
            }
        }
    }

    // Fallback to built-in defaults
    if (langCode == "zh" || langCode == "zh_CN" || langCode == "zh_TW") {
        loadDefaultChinese();
    } else {
        loadDefaultEnglish();
    }
    return true;
}

QString Localization::tr(const QString &key) const
{
    return m_strings.value(key, key);
}

QStringList Localization::availableLanguages() const
{
    return QStringList() << "en" << "zh";
}

void Localization::loadDefaultEnglish()
{
    m_strings["app_title"] = "Beachhead 2000 Parser Viewer";
    m_strings["file_list"] = "File List";
    m_strings["texture_info"] = "Texture Info";
    m_strings["open_file"] = "Open File";
    m_strings["open_directory"] = "Open Directory";
    m_strings["zoom_in"] = "Zoom In (+)";
    m_strings["zoom_out"] = "Zoom Out (-)";
    m_strings["fit_window"] = "Fit Window";
    m_strings["export_png"] = "Export PNG";
    m_strings["play"] = "Play";
    m_strings["pause"] = "Pause";
    m_strings["stop"] = "Stop";
    m_strings["frame"] = "Frame";
    m_strings["basic_info"] = "Basic Info";
    m_strings["animation_info"] = "Animation Info";
    m_strings["palette_info"] = "Palette";
    m_strings["statistics"] = "Statistics";
    m_strings["file_name"] = "File Name";
    m_strings["format"] = "Format";
    m_strings["dimensions"] = "Dimensions";
    m_strings["color_depth"] = "Color Depth";
    m_strings["file_size"] = "File Size";
    m_strings["header_size"] = "Header Size";
    m_strings["alpha_channel"] = "Alpha Channel";
    m_strings["frame_count"] = "Frame Count";
    m_strings["frame_height"] = "Frame Height";
    m_strings["palette"] = "Palette";
    m_strings["palette_file"] = "Palette File";
    m_strings["total_pixels"] = "Total Pixels";
    m_strings["actual_bpp"] = "Actual BPP";
    m_strings["drag_hint"] = "Drag files here or use File menu to open";
    m_strings["cannot_parse"] = "Cannot parse file";
    m_strings["no_image_data"] = "No image data";
    m_strings["export_failed"] = "Export Failed";
    m_strings["no_image_to_export"] = "No image to export";
    m_strings["about"] = "About";
    m_strings["version"] = "Version";
    m_strings["supported_formats"] = "Supported Formats";
    m_strings["drag_files_to_view"] = "Drag files to window to view";
    m_strings["theme_dark"] = "Dark Theme";
    m_strings["theme_light"] = "Light Theme";
    m_strings["language"] = "Language";
    m_strings["view"] = "View";
    m_strings["help"] = "Help";
    m_strings["exit"] = "Exit";
    m_strings["theme"] = "Theme";
    m_strings["yes"] = "Yes";
    m_strings["no"] = "No";
    m_strings["not_loaded"] = "Not loaded";
    m_strings["loaded"] = "Loaded";
    m_strings["pal_count"] = "PAL Count";
    m_strings["pal_status"] = "PAL Status";
    m_strings["palettes"] = "Palettes";
    m_strings["spr_label"] = "SPR Info";
    }

void Localization::loadDefaultChinese()
{
    m_strings["app_title"] = "Beachhead 2000 Parser Viewer";
    m_strings["file_list"] = "\u6587\u4ef6\u5217\u8868";
    m_strings["texture_info"] = "\u7eb9\u7406\u4fe1\u606f";
    m_strings["open_file"] = "\u6253\u5f00\u6587\u4ef6";
    m_strings["open_directory"] = "\u6253\u5f00\u76ee\u5f55";
    m_strings["zoom_in"] = "\u653e\u5927 (+)";
    m_strings["zoom_out"] = "\u7f29\u5c0f (-)";
    m_strings["fit_window"] = "\u9002\u5e94\u7a97\u53e3";
    m_strings["export_png"] = "\u5bfc\u51fa PNG";
    m_strings["play"] = "\u64ad\u653e";
    m_strings["pause"] = "\u6682\u505c";
    m_strings["stop"] = "\u505c\u6b62";
    m_strings["frame"] = "\u5e27";
    m_strings["basic_info"] = "\u57fa\u672c\u4fe1\u606f";
    m_strings["animation_info"] = "\u52a8\u753b\u4fe1\u606f";
    m_strings["palette_info"] = "\u8c03\u8272\u677f";
    m_strings["statistics"] = "\u7edf\u8ba1\u4fe1\u606f";
    m_strings["file_name"] = "\u6587\u4ef6\u540d";
    m_strings["format"] = "\u683c\u5f0f";
    m_strings["dimensions"] = "\u5c3a\u5bf8";
    m_strings["color_depth"] = "\u989c\u8272\u6df1\u5ea6";
    m_strings["file_size"] = "\u6587\u4ef6\u5927\u5c0f";
    m_strings["header_size"] = "\u5934\u90e8\u5927\u5c0f";
    m_strings["alpha_channel"] = "Alpha \u901a\u9053";
    m_strings["frame_count"] = "\u5e27\u6570";
    m_strings["frame_height"] = "\u5355\u5e27\u9ad8\u5ea6";
    m_strings["palette"] = "\u8c03\u8272\u677f";
    m_strings["palette_file"] = "\u8c03\u8272\u677f\u6587\u4ef6";
    m_strings["total_pixels"] = "\u603b\u50cf\u7d20\u6570";
    m_strings["actual_bpp"] = "\u5b9e\u9645\u6bd4\u7279\u7387";
    m_strings["drag_hint"] = "\u62d6\u653e\u6587\u4ef6\u5230\u6b64\u5904\u6216\u4f7f\u7528\u6587\u4ef6\u83dc\u5355\u6253\u5f00";
    m_strings["cannot_parse"] = "\u65e0\u6cd5\u89e3\u6790\u6587\u4ef6";
    m_strings["no_image_data"] = "\u65e0\u56fe\u50cf\u6570\u636e";
    m_strings["export_failed"] = "\u5bfc\u51fa\u5931\u8d25";
    m_strings["no_image_to_export"] = "\u6ca1\u6709\u53ef\u5bfc\u51fa\u7684\u56fe\u50cf";
    m_strings["about"] = "\u5173\u4e8e";
    m_strings["version"] = "\u7248\u672c";
    m_strings["supported_formats"] = "\u652f\u6301\u683c\u5f0f";
    m_strings["drag_files_to_view"] = "\u62d6\u653e\u6587\u4ef6\u5230\u7a97\u53e3\u5373\u53ef\u67e5\u770b";
    m_strings["theme_dark"] = "\u6df1\u8272\u4e3b\u9898";
    m_strings["theme_light"] = "\u6d45\u8272\u4e3b\u9898";
    m_strings["language"] = "\u8bed\u8a00";
    m_strings["view"] = "\u89c6\u56fe";
    m_strings["help"] = "\u5e2e\u52a9";
    m_strings["exit"] = "\u9000\u51fa";
    m_strings["theme"] = "\u4e3b\u9898";
    m_strings["yes"] = "是";
    m_strings["no"] = "否";
    m_strings["not_loaded"] = "未加载";
    m_strings["loaded"] = "已加载";
    m_strings["pal_count"] = "PAL 数量";
    m_strings["pal_status"] = "PAL 状态";
    m_strings["palettes"] = "调色板数";
    m_strings["spr_label"] = "SPR 信息";
    }
