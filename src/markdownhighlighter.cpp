#include "markdownhighlighter.h"

#include <QBrush>
#include <QColor>
#include <QRegularExpression>
#include <QTextDocument>

namespace
{
constexpr int kCodeBlockState = 1; ///< Специальное состояние блока Qt для fenced code blocks.
}

/**
 * @brief Создаёт подсветчик Markdown для документа редактора.
 * @param document Документ Qt, к которому нужно привязать подсветку.
 */
MarkdownHighlighter::MarkdownHighlighter(QTextDocument* document)
    : QSyntaxHighlighter(document)
    , m_settings(EditorSettingsStore::defaultSettings())
{
    rebuildFormats();
}

/**
 * @brief Применяет пользовательские настройки подсветки к редактору.
 * @param settings Набор настроек редактора, определяющий активные правила подсветки.
 */
void MarkdownHighlighter::applySettings(const EditorSettings& settings)
{
    m_settings = settings;
    rebuildFormats();
    rehighlight();
}

/**
 * @brief Подсвечивает отдельный текстовый блок документа Markdown.
 * @param text Текст текущего блока, который анализирует подсветчик.
 */
void MarkdownHighlighter::highlightBlock(const QString& text)
{
    static const QRegularExpression codeFencePattern(QStringLiteral(R"(^\s*(```|~~~))"));
    static const QRegularExpression headingPattern(QStringLiteral(R"(^\s{0,3}#{1,6}\s+.+$)"));
    static const QRegularExpression quotePattern(QStringLiteral(R"(^\s*>\s?.+$)"));
    static const QRegularExpression listPattern(QStringLiteral(R"(^\s*(?:[-*+]|\d+\.)\s+)"));
    static const QRegularExpression checkBoxPattern(QStringLiteral(R"(\[(?: |x|X)\])"));
    static const QRegularExpression linkPattern(QStringLiteral(R"(\[[^\]]+\]\([^)]+\))"));
    static const QRegularExpression inlineCodePattern(QStringLiteral(R"(`[^`]+`)"));

    setCurrentBlockState(0);

    const bool isFenceLine = codeFencePattern.match(text).hasMatch();     ///< Признак строки-ограждения fenced code block.
    const bool wasInsideCodeBlock = previousBlockState() == kCodeBlockState; ///< Признак нахождения внутри fenced code block.

    if (m_settings.highlightCode && (wasInsideCodeBlock || isFenceLine)) {
        setFormat(0, text.length(), m_codeBlockFormat);

        if (isFenceLine) {
            setCurrentBlockState(wasInsideCodeBlock ? 0 : kCodeBlockState);
        } else {
            setCurrentBlockState(kCodeBlockState);
        }
        return;
    }

    if (m_settings.highlightHeadings && headingPattern.match(text).hasMatch()) {
        setFormat(0, text.length(), m_headingFormat);
    }

    if (m_settings.highlightQuotes && quotePattern.match(text).hasMatch()) {
        setFormat(0, text.length(), m_quoteFormat);
    }

    if (m_settings.highlightLists) {
        highlightMatches(text, listPattern, m_listFormat);
        highlightMatches(text, checkBoxPattern, m_checkBoxFormat);
    }

    if (m_settings.highlightLinks) {
        highlightMatches(text, linkPattern, m_linkFormat);
    }

    if (m_settings.highlightCode) {
        highlightMatches(text, inlineCodePattern, m_inlineCodeFormat);
    }
}

/**
 * @brief Пересобирает QTextCharFormat после изменения настроек редактора.
 *
 * Метод нужен, чтобы обновить цвета и стили для всех типов Markdown-элементов.
 */
void MarkdownHighlighter::rebuildFormats()
{
    m_headingFormat = QTextCharFormat();
    m_headingFormat.setForeground(QColor(QStringLiteral("#174EA6")));
    m_headingFormat.setFontWeight(QFont::Bold);

    m_linkFormat = QTextCharFormat();
    m_linkFormat.setForeground(QColor(QStringLiteral("#0B8043")));
    m_linkFormat.setFontUnderline(true);

    m_quoteFormat = QTextCharFormat();
    m_quoteFormat.setForeground(QColor(QStringLiteral("#6A1B9A")));
    m_quoteFormat.setFontItalic(true);

    m_listFormat = QTextCharFormat();
    m_listFormat.setForeground(QColor(QStringLiteral("#B45309")));
    m_listFormat.setFontWeight(QFont::Bold);

    m_checkBoxFormat = QTextCharFormat();
    m_checkBoxFormat.setForeground(QColor(QStringLiteral("#D97706")));
    m_checkBoxFormat.setFontWeight(QFont::Bold);

    m_inlineCodeFormat = QTextCharFormat();
    m_inlineCodeFormat.setForeground(QColor(QStringLiteral("#A61E4D")));
    m_inlineCodeFormat.setBackground(QBrush(QColor(QStringLiteral("#F5E8EF"))));

    m_codeBlockFormat = QTextCharFormat();
    m_codeBlockFormat.setForeground(QColor(QStringLiteral("#7C2D12")));
    m_codeBlockFormat.setBackground(QBrush(QColor(QStringLiteral("#F8F1E7"))));
}

/**
 * @brief Применяет формат ко всем совпадениям регулярного выражения в строке.
 * @param text Текст строки, в которой нужно искать совпадения.
 * @param pattern Регулярное выражение для поиска Markdown-элемента.
 * @param format Формат текста, который нужно применить к найденным участкам.
 */
void MarkdownHighlighter::highlightMatches(const QString& text, const QRegularExpression& pattern, const QTextCharFormat& format)
{
    QRegularExpressionMatchIterator matchIterator = pattern.globalMatch(text);
    while (matchIterator.hasNext()) {
        const QRegularExpressionMatch match = matchIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), format);
    }
}

