#pragma once

#include "editorsettings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class MarkdownHighlighter;
class MarkdownView;
class QPlainTextEdit;
class QSpinBox;

/**
 * @brief Диалог позволяет настроить внешний вид и подсветку Markdown-редактора.
 *
 * Через диалог пользователь выбирает шрифт, размер, параметры отображения и
 * состав Markdown-элементов, которые будут подсвечиваться в редакторе.
 */
class EditorSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Создаёт диалог настройки редактора Markdown.
     * @param settings Текущие настройки редактора, которые нужно показать пользователю.
     * @param parent Родительский виджет Qt для управления временем жизни диалога.
     */
    explicit EditorSettingsDialog(const EditorSettings& settings, QWidget* parent = nullptr);

    /**
     * @brief Возвращает настройки редактора, выбранные пользователем в диалоге.
     * @return Структура со шрифтом, отображением и параметрами подсветки.
     */
    EditorSettings editorSettings() const;

private slots:
    /**
     * @brief Применяет выбранный профиль подсветки к группе чекбоксов элементов Markdown.
     * @param profileIndex Индекс выбранного профиля в комбобоксе.
     */
    void applySelectedHighlightProfile(int profileIndex);

    /**
     * @brief Синхронизирует профиль подсветки после ручного изменения чекбоксов.
     *
     * Метод нужен, чтобы автоматически переключать профиль в "пользовательский"
     * режим, когда состав подсвечиваемых элементов перестаёт совпадать с шаблоном.
     */
    void syncHighlightProfileWithOptions();

    /**
     * @brief Восстанавливает настройки редактора к значениям по умолчанию.
     */
    void restoreDefaults();

private:
    /**
     * @brief Создаёт интерфейс диалога настройки редактора.
     *
     * Метод нужен, чтобы собрать поля выбора шрифта, отображения, подсветки и живого примера.
     */
    void setupUi();

    /**
     * @brief Загружает текущие настройки редактора в виджеты диалога.
     * @param settings Настройки редактора, которые нужно отобразить пользователю.
     */
    void loadSettings(const EditorSettings& settings);

    /**
     * @brief Обновляет демонстрационный пример редактора и просмотра по текущим полям диалога.
     *
     * Метод нужен, чтобы пользователь видел результат изменения настроек сразу,
     * не закрывая окно настройки редактора.
     */
    void updatePreview();

    /**
     * @brief Возвращает профиль подсветки, выбранный в комбобоксе.
     * @return Профиль подсветки Markdown, соответствующий текущему выбору.
     */
    HighlightProfile selectedProfile() const;

    QFontComboBox* m_fontComboBox = nullptr;          ///< Виджет выбора семейства шрифта редактора.
    QSpinBox* m_fontSizeSpinBox = nullptr;            ///< Виджет выбора размера шрифта редактора.
    QSpinBox* m_tabWidthSpinBox = nullptr;            ///< Виджет выбора ширины табуляции в пробелах.
    QCheckBox* m_wordWrapCheckBox = nullptr;          ///< Чекбокс переноса длинных строк редактора.
    QCheckBox* m_currentLineCheckBox = nullptr;       ///< Чекбокс подсветки текущей строки редактора.
    QComboBox* m_highlightProfileComboBox = nullptr;  ///< Список типовых профилей подсветки Markdown.
    QCheckBox* m_headingCheckBox = nullptr;           ///< Чекбокс подсветки Markdown-заголовков.
    QCheckBox* m_linkCheckBox = nullptr;              ///< Чекбокс подсветки Markdown-ссылок.
    QCheckBox* m_quoteCheckBox = nullptr;             ///< Чекбокс подсветки Markdown-цитат.
    QCheckBox* m_listCheckBox = nullptr;              ///< Чекбокс подсветки списков и чекбоксов Markdown.
    QCheckBox* m_codeCheckBox = nullptr;              ///< Чекбокс подсветки inline-кода и fenced code blocks.
    QPlainTextEdit* m_editorPreview = nullptr;        ///< Демонстрационный пример Markdown в режиме редактора.
    MarkdownHighlighter* m_previewHighlighter = nullptr; ///< Подсветчик Markdown для демонстрационного редактора.
    MarkdownView* m_viewerPreview = nullptr;          ///< Демонстрационный пример Markdown в режиме просмотра.
    bool m_isApplyingProfile = false;                 ///< Защитный флаг против рекурсивной смены профиля и чекбоксов.
};
