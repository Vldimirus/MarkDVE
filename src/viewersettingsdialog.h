#pragma once

#include "viewersettings.h"

#include <QDialog>

class MarkdownHighlighter;
class MarkdownView;
class QCheckBox;
class QFontComboBox;
class QPlainTextEdit;
class QSpinBox;

/**
 * @brief Диалог позволяет настроить внешний вид окна просмотра Markdown.
 *
 * Через этот диалог пользователь выбирает шрифт и визуальные параметры
 * отображения Markdown-документов во вкладках просмотрщика.
 */
class ViewerSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Создаёт диалог настройки просмотра Markdown.
     * @param settings Текущие настройки внешнего вида, которые нужно показать пользователю.
     * @param parent Родительский виджет Qt для управления временем жизни диалога.
     */
    explicit ViewerSettingsDialog(const ViewerSettings& settings, QWidget* parent = nullptr);

    /**
     * @brief Возвращает настройки просмотра, выбранные пользователем в диалоге.
     * @return Структура с параметрами внешнего вида окна просмотра Markdown.
     */
    ViewerSettings viewerSettings() const;

private slots:
    /**
     * @brief Восстанавливает стандартные настройки просмотра Markdown.
     */
    void restoreDefaults();

private:
    /**
     * @brief Создаёт интерфейс диалога настройки просмотра Markdown.
     *
     * Метод собирает поля выбора шрифта, визуальных флагов и живого примера.
     */
    void setupUi();

    /**
     * @brief Загружает текущие настройки в виджеты диалога.
     * @param settings Настройки просмотра, которые нужно отобразить пользователю.
     */
    void loadSettings(const ViewerSettings& settings);

    /**
     * @brief Обновляет демонстрационный пример редактора и просмотра по текущим полям диалога.
     *
     * Метод нужен, чтобы пользователь видел итоговый вид страницы сразу,
     * без закрытия окна настройки просмотра.
     */
    void updatePreview();

    QFontComboBox* m_fontComboBox = nullptr;        ///< Виджет выбора семейства шрифта окна просмотра.
    QSpinBox* m_fontSizeSpinBox = nullptr;          ///< Виджет выбора размера шрифта окна просмотра.
    QSpinBox* m_documentMarginSpinBox = nullptr;    ///< Виджет выбора внутренних отступов документа.
    QCheckBox* m_underlineLinksCheckBox = nullptr;  ///< Чекбокс подчёркивания ссылок в просмотре.
    QCheckBox* m_codeBlocksCheckBox = nullptr;      ///< Чекбокс выделения inline-кода и блоков кода.
    QCheckBox* m_invertedColorsCheckBox = nullptr;  ///< Чекбокс инвертированной цветовой схемы просмотра.
    QPlainTextEdit* m_editorPreview = nullptr;      ///< Демонстрационный пример Markdown в режиме редактора.
    MarkdownHighlighter* m_previewHighlighter = nullptr; ///< Подсветчик Markdown для демонстрационного редактора.
    MarkdownView* m_viewerPreview = nullptr;        ///< Демонстрационный пример Markdown в режиме просмотра.
};
