#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include <QString>
#include <QMap>
#include <QJsonObject>

class Localization
{
public:
    static Localization& instance();

    bool loadLanguage(const QString &langCode);
    QString tr(const QString &key) const;
    QString currentLanguage() const { return m_currentLang; }
    QStringList availableLanguages() const;

private:
    Localization() = default;
    QMap<QString, QString> m_strings;
    QString m_currentLang;
    void loadDefaultEnglish();
    void loadDefaultChinese();
};

#define L(key) Localization::instance().tr(key)

#endif // LOCALIZATION_H
