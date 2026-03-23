#include "viewersettings.h"

#include <QApplication>
#include <QFontDatabase>
#include <QSettings>

namespace
{
constexpr const char* kViewerGroupName = "Viewer";                 ///< Имя группы настроек просмотра в QSettings.
constexpr const char* kFontFamilyKey = "FontFamily";               ///< Ключ семейства шрифта просмотра.
constexpr const char* kFontPointSizeKey = "FontPointSize";         ///< Ключ размера шрифта просмотра.
constexpr const char* kDocumentMarginKey = "DocumentMargin";       ///< Ключ внутреннего отступа документа.
constexpr const char* kUnderlineLinksKey = "UnderlineLinks";       ///< Ключ признака подчёркивания ссылок.
constexpr const char* kCodeBlocksKey = "EmphasizeCodeBlocks";      ///< Ключ признака выделения кода.
constexpr const char* kInvertedColorsKey = "InvertedColors";       ///< Ключ признака инвертированной схемы просмотра.
}

/**
 * @brief Возвращает базовые настройки внешнего вида просмотрщика.
 * @return Структура с настройками просмотра Markdown по умолчанию.
 */
ViewerSettings ViewerSettingsStore::defaultSettings()
{
    ViewerSettings settings;
    const QFont defaultFont = QApplication::font();

    settings.fontFamily = defaultFont.family().isEmpty()
        ? QFontDatabase::systemFont(QFontDatabase::GeneralFont).family()
        : defaultFont.family();
    settings.fontPointSize = defaultFont.pointSize() > 0 ? defaultFont.pointSize() : 13;
    settings.documentMargin = 24;
    settings.underlineLinks = true;
    settings.emphasizeCodeBlocks = true;
    settings.invertedColors = false;
    return settings;
}

/**
 * @brief Загружает сохранённые настройки просмотрщика из QSettings.
 * @return Структура с настройками, готовая к применению в MarkdownView.
 */
ViewerSettings ViewerSettingsStore::load()
{
    ViewerSettings settings = defaultSettings();
    QSettings applicationSettings;
    applicationSettings.beginGroup(QString::fromUtf8(kViewerGroupName));

    settings.fontFamily = applicationSettings.value(QString::fromUtf8(kFontFamilyKey), settings.fontFamily).toString();
    settings.fontPointSize = applicationSettings.value(QString::fromUtf8(kFontPointSizeKey), settings.fontPointSize).toInt();
    settings.documentMargin = applicationSettings.value(QString::fromUtf8(kDocumentMarginKey), settings.documentMargin).toInt();
    settings.underlineLinks = applicationSettings.value(QString::fromUtf8(kUnderlineLinksKey), settings.underlineLinks).toBool();
    settings.emphasizeCodeBlocks = applicationSettings.value(QString::fromUtf8(kCodeBlocksKey), settings.emphasizeCodeBlocks).toBool();
    settings.invertedColors = applicationSettings.value(QString::fromUtf8(kInvertedColorsKey), settings.invertedColors).toBool();

    applicationSettings.endGroup();

    if (settings.fontPointSize <= 0) {
        settings.fontPointSize = defaultSettings().fontPointSize;
    }

    if (settings.documentMargin < 0) {
        settings.documentMargin = defaultSettings().documentMargin;
    }

    return settings;
}

/**
 * @brief Сохраняет настройки просмотрщика в QSettings.
 * @param settings Набор настроек, который нужно сохранить для следующих запусков.
 */
void ViewerSettingsStore::save(const ViewerSettings& settings)
{
    QSettings applicationSettings;
    applicationSettings.beginGroup(QString::fromUtf8(kViewerGroupName));

    applicationSettings.setValue(QString::fromUtf8(kFontFamilyKey), settings.fontFamily);
    applicationSettings.setValue(QString::fromUtf8(kFontPointSizeKey), settings.fontPointSize);
    applicationSettings.setValue(QString::fromUtf8(kDocumentMarginKey), settings.documentMargin);
    applicationSettings.setValue(QString::fromUtf8(kUnderlineLinksKey), settings.underlineLinks);
    applicationSettings.setValue(QString::fromUtf8(kCodeBlocksKey), settings.emphasizeCodeBlocks);
    applicationSettings.setValue(QString::fromUtf8(kInvertedColorsKey), settings.invertedColors);

    applicationSettings.endGroup();
}
