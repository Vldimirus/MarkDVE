#pragma once

#include "editorsettings.h"

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

/**
 * @brief Класс подсвечивает базовые элементы Markdown внутри текстового редактора.
 *
 * Подсветка нужна для визуального разделения заголовков, ссылок, списков,
 * чекбоксов, цитат и блоков кода при редактировании Markdown-файлов.
 */
class MarkdownHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    /**
     * @brief Создаёт подсветчик Markdown для документа редактора.
     * @param document Документ Qt, к которому нужно привязать подсветку.
     */
    explicit MarkdownHighlighter(QTextDocument* document);

    /**
     * @brief Применяет пользовательские настройки подсветки к редактору.
     * @param settings Набор настроек редактора, определяющий активные правила подсветки.
     */
    void applySettings(const EditorSettings& settings);

protected:
    /**
     * @brief Подсвечивает отдельный текстовый блок документа Markdown.
     * @param text Текст текущего блока, который анализирует подсветчик.
     */
    void highlightBlock(const QString& text) override;

private:
    /**
     * @brief Пересобирает QTextCharFormat после изменения настроек редактора.
     *
     * Метод нужен, чтобы обновить цвета и стили для всех типов Markdown-элементов.
     */
    void rebuildFormats();

    /**
     * @brief Применяет формат ко всем совпадениям регулярного выражения в строке.
     * @param text Текст строки, в которой нужно искать совпадения.
     * @param pattern Регулярное выражение для поиска Markdown-элемента.
     * @param format Формат текста, который нужно применить к найденным участкам.
     */
    void highlightMatches(const QString& text, const QRegularExpression& pattern, const QTextCharFormat& format);

    EditorSettings m_settings;               ///< Текущие пользовательские настройки подсветки Markdown.
    QTextCharFormat m_headingFormat;         ///< Формат заголовков Markdown.
    QTextCharFormat m_linkFormat;            ///< Формат Markdown-ссылок.
    QTextCharFormat m_quoteFormat;           ///< Формат Markdown-цитат.
    QTextCharFormat m_listFormat;            ///< Формат маркеров списков Markdown.
    QTextCharFormat m_checkBoxFormat;        ///< Формат маркеров чекбоксов `[ ]` и `[x]`.
    QTextCharFormat m_inlineCodeFormat;      ///< Формат inline-кода Markdown.
    QTextCharFormat m_codeBlockFormat;       ///< Формат fenced code blocks Markdown.
};

