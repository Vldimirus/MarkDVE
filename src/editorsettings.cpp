#include "editorsettings.h"

#include <QFontDatabase>
#include <QSettings>

namespace
{
constexpr const char* kEditorGroupName = "Editor";                 ///< Имя группы настроек редактора в QSettings.
constexpr const char* kFontFamilyKey = "FontFamily";               ///< Ключ семейства шрифта редактора.
constexpr const char* kFontPointSizeKey = "FontPointSize";         ///< Ключ размера шрифта редактора.
constexpr const char* kTabWidthSpacesKey = "TabWidthSpaces";       ///< Ключ ширины табуляции в пробелах.
constexpr const char* kWordWrapKey = "WordWrap";                   ///< Ключ признака переноса строк.
constexpr const char* kCurrentLineKey = "HighlightCurrentLine";    ///< Ключ признака подсветки текущей строки.
constexpr const char* kHighlightProfileKey = "HighlightProfile";   ///< Ключ выбранного профиля подсветки.
constexpr const char* kHeadingsKey = "HighlightHeadings";          ///< Ключ признака подсветки заголовков.
constexpr const char* kLinksKey = "HighlightLinks";                ///< Ключ признака подсветки ссылок.
constexpr const char* kQuotesKey = "HighlightQuotes";              ///< Ключ признака подсветки цитат.
constexpr const char* kListsKey = "HighlightLists";                ///< Ключ признака подсветки списков и чекбоксов.
constexpr const char* kCodeKey = "HighlightCode";                  ///< Ключ признака подсветки кода.
}

/**
 * @brief Возвращает набор настроек редактора по умолчанию.
 * @return Структура с базовыми значениями шрифта, подсветки и отображения.
 */
EditorSettings EditorSettingsStore::defaultSettings()
{
    EditorSettings settings;
    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    settings.fontFamily = fixedFont.family();
    settings.fontPointSize = fixedFont.pointSize() > 0 ? fixedFont.pointSize() : 12;
    settings.tabWidthSpaces = 4;
    settings.wordWrap = true;
    settings.highlightCurrentLine = true;
    settings.highlightProfile = HighlightProfile::Extended;
    settings.highlightHeadings = true;
    settings.highlightLinks = true;
    settings.highlightQuotes = true;
    settings.highlightLists = true;
    settings.highlightCode = true;
    return settings;
}

/**
 * @brief Загружает сохранённые настройки редактора из QSettings.
 * @return Структура с настройками редактора, готовая к применению.
 */
EditorSettings EditorSettingsStore::load()
{
    EditorSettings settings = defaultSettings();
    QSettings applicationSettings;
    applicationSettings.beginGroup(QString::fromUtf8(kEditorGroupName));

    settings.fontFamily = applicationSettings.value(QString::fromUtf8(kFontFamilyKey), settings.fontFamily).toString();
    settings.fontPointSize = applicationSettings.value(QString::fromUtf8(kFontPointSizeKey), settings.fontPointSize).toInt();
    settings.tabWidthSpaces = applicationSettings.value(QString::fromUtf8(kTabWidthSpacesKey), settings.tabWidthSpaces).toInt();
    settings.wordWrap = applicationSettings.value(QString::fromUtf8(kWordWrapKey), settings.wordWrap).toBool();
    settings.highlightCurrentLine = applicationSettings.value(QString::fromUtf8(kCurrentLineKey), settings.highlightCurrentLine).toBool();
    settings.highlightProfile = profileFromKey(applicationSettings.value(
        QString::fromUtf8(kHighlightProfileKey),
        profileKey(settings.highlightProfile)).toString());
    settings.highlightHeadings = applicationSettings.value(QString::fromUtf8(kHeadingsKey), settings.highlightHeadings).toBool();
    settings.highlightLinks = applicationSettings.value(QString::fromUtf8(kLinksKey), settings.highlightLinks).toBool();
    settings.highlightQuotes = applicationSettings.value(QString::fromUtf8(kQuotesKey), settings.highlightQuotes).toBool();
    settings.highlightLists = applicationSettings.value(QString::fromUtf8(kListsKey), settings.highlightLists).toBool();
    settings.highlightCode = applicationSettings.value(QString::fromUtf8(kCodeKey), settings.highlightCode).toBool();

    applicationSettings.endGroup();

    if (settings.fontPointSize <= 0) {
        settings.fontPointSize = defaultSettings().fontPointSize;
    }

    if (settings.tabWidthSpaces <= 0) {
        settings.tabWidthSpaces = defaultSettings().tabWidthSpaces;
    }

    settings.highlightProfile = detectProfile(settings);
    return settings;
}

/**
 * @brief Сохраняет настройки редактора в QSettings.
 * @param settings Набор настроек редактора, который нужно сохранить.
 */
void EditorSettingsStore::save(const EditorSettings& settings)
{
    QSettings applicationSettings;
    applicationSettings.beginGroup(QString::fromUtf8(kEditorGroupName));

    applicationSettings.setValue(QString::fromUtf8(kFontFamilyKey), settings.fontFamily);
    applicationSettings.setValue(QString::fromUtf8(kFontPointSizeKey), settings.fontPointSize);
    applicationSettings.setValue(QString::fromUtf8(kTabWidthSpacesKey), settings.tabWidthSpaces);
    applicationSettings.setValue(QString::fromUtf8(kWordWrapKey), settings.wordWrap);
    applicationSettings.setValue(QString::fromUtf8(kCurrentLineKey), settings.highlightCurrentLine);
    applicationSettings.setValue(QString::fromUtf8(kHighlightProfileKey), profileKey(settings.highlightProfile));
    applicationSettings.setValue(QString::fromUtf8(kHeadingsKey), settings.highlightHeadings);
    applicationSettings.setValue(QString::fromUtf8(kLinksKey), settings.highlightLinks);
    applicationSettings.setValue(QString::fromUtf8(kQuotesKey), settings.highlightQuotes);
    applicationSettings.setValue(QString::fromUtf8(kListsKey), settings.highlightLists);
    applicationSettings.setValue(QString::fromUtf8(kCodeKey), settings.highlightCode);

    applicationSettings.endGroup();
}

/**
 * @brief Применяет профиль подсветки к структуре настроек.
 * @param profile Выбранный профиль подсветки Markdown.
 * @param baseSettings Базовые настройки, в которые нужно подставить профиль.
 * @return Обновлённая структура настроек с включёнными правилами профиля.
 */
EditorSettings EditorSettingsStore::settingsForProfile(HighlightProfile profile, const EditorSettings& baseSettings)
{
    EditorSettings settings = baseSettings;
    settings.highlightProfile = profile;

    switch (profile) {
    case HighlightProfile::Disabled:
        settings.highlightHeadings = false;
        settings.highlightLinks = false;
        settings.highlightQuotes = false;
        settings.highlightLists = false;
        settings.highlightCode = false;
        break;
    case HighlightProfile::Basic:
        settings.highlightHeadings = true;
        settings.highlightLinks = true;
        settings.highlightQuotes = false;
        settings.highlightLists = false;
        settings.highlightCode = true;
        break;
    case HighlightProfile::Extended:
        settings.highlightHeadings = true;
        settings.highlightLinks = true;
        settings.highlightQuotes = true;
        settings.highlightLists = true;
        settings.highlightCode = true;
        break;
    case HighlightProfile::Custom:
        break;
    }

    return settings;
}

/**
 * @brief Вычисляет профиль подсветки по состоянию флагов структуры настроек.
 * @param settings Набор настроек редактора, который нужно классифицировать.
 * @return Профиль подсветки, наиболее точно соответствующий текущим флагам.
 */
HighlightProfile EditorSettingsStore::detectProfile(const EditorSettings& settings)
{
    const EditorSettings disabledSettings = settingsForProfile(HighlightProfile::Disabled, settings);
    if (settings.highlightHeadings == disabledSettings.highlightHeadings
        && settings.highlightLinks == disabledSettings.highlightLinks
        && settings.highlightQuotes == disabledSettings.highlightQuotes
        && settings.highlightLists == disabledSettings.highlightLists
        && settings.highlightCode == disabledSettings.highlightCode) {
        return HighlightProfile::Disabled;
    }

    const EditorSettings basicSettings = settingsForProfile(HighlightProfile::Basic, settings);
    if (settings.highlightHeadings == basicSettings.highlightHeadings
        && settings.highlightLinks == basicSettings.highlightLinks
        && settings.highlightQuotes == basicSettings.highlightQuotes
        && settings.highlightLists == basicSettings.highlightLists
        && settings.highlightCode == basicSettings.highlightCode) {
        return HighlightProfile::Basic;
    }

    const EditorSettings extendedSettings = settingsForProfile(HighlightProfile::Extended, settings);
    if (settings.highlightHeadings == extendedSettings.highlightHeadings
        && settings.highlightLinks == extendedSettings.highlightLinks
        && settings.highlightQuotes == extendedSettings.highlightQuotes
        && settings.highlightLists == extendedSettings.highlightLists
        && settings.highlightCode == extendedSettings.highlightCode) {
        return HighlightProfile::Extended;
    }

    return HighlightProfile::Custom;
}

/**
 * @brief Преобразует профиль подсветки в строковый ключ для QSettings.
 * @param profile Профиль подсветки, который нужно сериализовать.
 * @return Строковый ключ профиля для безопасного сохранения.
 */
QString EditorSettingsStore::profileKey(HighlightProfile profile)
{
    switch (profile) {
    case HighlightProfile::Disabled:
        return QStringLiteral("disabled");
    case HighlightProfile::Basic:
        return QStringLiteral("basic");
    case HighlightProfile::Extended:
        return QStringLiteral("extended");
    case HighlightProfile::Custom:
        return QStringLiteral("custom");
    }

    return QStringLiteral("extended");
}

/**
 * @brief Преобразует строковый ключ из QSettings обратно в профиль подсветки.
 * @param storedProfileKey Строковый ключ профиля, сохранённый в QSettings.
 * @return Профиль подсветки, соответствующий сохранённому ключу.
 */
HighlightProfile EditorSettingsStore::profileFromKey(const QString& storedProfileKey)
{
    if (storedProfileKey == QStringLiteral("disabled")) {
        return HighlightProfile::Disabled;
    }

    if (storedProfileKey == QStringLiteral("basic")) {
        return HighlightProfile::Basic;
    }

    if (storedProfileKey == QStringLiteral("custom")) {
        return HighlightProfile::Custom;
    }

    return HighlightProfile::Extended;
}

