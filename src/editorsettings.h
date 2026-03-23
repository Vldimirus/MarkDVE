#pragma once

#include <QString>

/**
 * @brief Перечисление задаёт профиль подсветки Markdown в редакторе.
 *
 * Профиль нужен для быстрого выбора типового набора правил подсветки без
 * ручного переключения каждого отдельного элемента.
 */
enum class HighlightProfile
{
    Disabled,   ///< Подсветка выключена полностью.
    Basic,      ///< Подсвечиваются только основные элементы Markdown.
    Extended,   ///< Подсвечиваются основные и дополнительные элементы Markdown.
    Custom      ///< Пользователь вручную выбрал набор подсвечиваемых элементов.
};

/**
 * @brief Структура хранит все пользовательские настройки редактора Markdown.
 *
 * Настройки применяются к отдельному окну редактора и сохраняются между
 * запусками приложения через QSettings.
 */
struct EditorSettings
{
    QString fontFamily;                        ///< Семейство шрифта редактора.
    int fontPointSize = 12;                   ///< Размер шрифта редактора в пунктах.
    int tabWidthSpaces = 4;                   ///< Ширина табуляции в количестве пробелов.
    bool wordWrap = true;                     ///< Признак переноса длинных строк по ширине виджета.
    bool highlightCurrentLine = true;         ///< Признак подсветки текущей строки курсора.
    HighlightProfile highlightProfile = HighlightProfile::Extended; ///< Выбранный профиль подсветки Markdown.
    bool highlightHeadings = true;            ///< Признак подсветки Markdown-заголовков.
    bool highlightLinks = true;               ///< Признак подсветки Markdown-ссылок.
    bool highlightQuotes = true;              ///< Признак подсветки цитат Markdown.
    bool highlightLists = true;               ///< Признак подсветки списков и чекбоксов Markdown.
    bool highlightCode = true;                ///< Признак подсветки inline-кода и fenced code blocks.
};

/**
 * @brief Класс загружает, сохраняет и нормализует настройки редактора.
 *
 * Класс отделяет логику хранения настроек от интерфейса редактора и диалога
 * настройки, чтобы все окна использовали единый формат сохранения.
 */
class EditorSettingsStore
{
public:
    /**
     * @brief Возвращает набор настроек редактора по умолчанию.
     * @return Структура с базовыми значениями шрифта, подсветки и отображения.
     */
    static EditorSettings defaultSettings();

    /**
     * @brief Загружает сохранённые настройки редактора из QSettings.
     * @return Структура с настройками редактора, готовая к применению.
     */
    static EditorSettings load();

    /**
     * @brief Сохраняет настройки редактора в QSettings.
     * @param settings Набор настроек редактора, который нужно сохранить.
     */
    static void save(const EditorSettings& settings);

    /**
     * @brief Применяет профиль подсветки к структуре настроек.
     * @param profile Выбранный профиль подсветки Markdown.
     * @param baseSettings Базовые настройки, в которые нужно подставить профиль.
     * @return Обновлённая структура настроек с включёнными правилами профиля.
     */
    static EditorSettings settingsForProfile(HighlightProfile profile, const EditorSettings& baseSettings = EditorSettings());

    /**
     * @brief Вычисляет профиль подсветки по состоянию флагов структуры настроек.
     * @param settings Набор настроек редактора, который нужно классифицировать.
     * @return Профиль подсветки, наиболее точно соответствующий текущим флагам.
     */
    static HighlightProfile detectProfile(const EditorSettings& settings);

private:
    /**
     * @brief Преобразует профиль подсветки в строковый ключ для QSettings.
     * @param profile Профиль подсветки, который нужно сериализовать.
     * @return Строковый ключ профиля для безопасного сохранения.
     */
    static QString profileKey(HighlightProfile profile);

    /**
     * @brief Преобразует строковый ключ из QSettings обратно в профиль подсветки.
     * @param profileKey Строковый ключ профиля, сохранённый в QSettings.
     * @return Профиль подсветки, соответствующий сохранённому ключу.
     */
    static HighlightProfile profileFromKey(const QString& profileKey);
};

