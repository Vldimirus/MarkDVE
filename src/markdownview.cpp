#include "markdownview.h"

#include <QFile>
#include <QFont>
#include <QPalette>
#include <QScrollBar>
#include <QStringConverter>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QTextStream>
#include <QUrl>

/**
 * @brief Создаёт виджет отображения Markdown.
 * @param parent Родительский виджет Qt для управления временем жизни объекта.
 */
MarkdownView::MarkdownView(QWidget* parent)
    : QTextBrowser(parent)
    , m_viewerSettings(ViewerSettingsStore::defaultSettings())
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setReadOnly(true);
    applyViewerSettings(m_viewerSettings);
}

/**
 * @brief Загружает Markdown-файл в виджет.
 * @param filePath Абсолютный путь к Markdown-файлу, который нужно отобразить.
 * @param errorText Строка для возврата текста ошибки, если загрузка не удалась.
 * @return true, если файл успешно прочитан и показан в виджете.
 */
bool MarkdownView::loadMarkdownFromFile(const QString& filePath, QString* errorText)
{
    QFile markdownFile(filePath);
    if (!markdownFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Не удалось открыть Markdown-файл для чтения.");
        }
        return false;
    }

    QTextStream stream(&markdownFile);
    stream.setEncoding(QStringConverter::Utf8);
    setMarkdownContent(stream.readAll(), filePath);
    return true;
}

/**
 * @brief Загружает Markdown-текст напрямую в виджет без чтения файла с диска.
 * @param markdownText Текст Markdown, который нужно показать в окне просмотра.
 * @param documentPath Необязательный путь документа для вычисления baseUrl ссылок.
 */
void MarkdownView::setMarkdownContent(const QString& markdownText, const QString& documentPath)
{
    m_markdownText = markdownText;
    m_documentPath = documentPath;
    renderMarkdown();
}

/**
 * @brief Прокручивает документ к указанному якорю.
 * @param anchor Имя якоря внутри документа без символа '#'.
 */
void MarkdownView::jumpToAnchor(const QString& anchor)
{
    if (!anchor.isEmpty()) {
        scrollToAnchor(anchor);
    }
}

/**
 * @brief Возвращает путь к текущему открытому документу.
 * @return Абсолютный путь к Markdown-файлу, загруженному в виджет.
 */
QString MarkdownView::documentPath() const
{
    return m_documentPath;
}

/**
 * @brief Применяет настройки внешнего вида к текущему окну просмотра Markdown.
 * @param settings Набор настроек шрифта и оформления, который нужно применить.
 */
void MarkdownView::applyViewerSettings(const ViewerSettings& settings)
{
    m_viewerSettings = settings;
    renderMarkdown();
}

/**
 * @brief Перестраивает отображение Markdown по текущему тексту и настройкам просмотра.
 *
 * Метод нужен, чтобы можно было менять внешний вид открытого документа без
 * повторного чтения файла с диска.
 */
void MarkdownView::renderMarkdown()
{
    const int verticalScrollValue = verticalScrollBar()->value();   ///< Текущая вертикальная позиция прокрутки перед обновлением оформления.
    const int horizontalScrollValue = horizontalScrollBar()->value(); ///< Текущая горизонтальная позиция прокрутки перед обновлением оформления.
    const bool useInvertedColors = m_viewerSettings.invertedColors; ///< Признак применения тёмной инвертированной схемы просмотра.
    const QColor pageBackgroundColor = useInvertedColors
        ? QColor(QStringLiteral("#181A1B"))
        : QColor(QStringLiteral("#FFFFFF")); ///< Цвет фона страницы Markdown в зависимости от цветовой схемы просмотра.
    const QColor textColor = useInvertedColors
        ? QColor(QStringLiteral("#E8EAED"))
        : QColor(QStringLiteral("#202124")); ///< Цвет основного текста страницы Markdown.

    QFont documentFont(m_viewerSettings.fontFamily, m_viewerSettings.fontPointSize); ///< Шрифт, который будет применён к окну просмотра.
    setFont(documentFont);

    QPalette viewerPalette = palette(); ///< Палитра виджета просмотра, управляющая реальным фоном viewport области документа.
    viewerPalette.setColor(QPalette::Base, pageBackgroundColor);
    viewerPalette.setColor(QPalette::Window, pageBackgroundColor);
    viewerPalette.setColor(QPalette::Text, textColor);
    viewerPalette.setColor(QPalette::WindowText, textColor);
    setPalette(viewerPalette);

    QPalette viewportPalette = viewport()->palette(); ///< Палитра внутренней viewport-области, где фактически рисуется документ Markdown.
    viewportPalette.setColor(QPalette::Base, pageBackgroundColor);
    viewportPalette.setColor(QPalette::Window, pageBackgroundColor);
    viewportPalette.setColor(QPalette::Text, textColor);
    viewportPalette.setColor(QPalette::WindowText, textColor);
    viewport()->setPalette(viewportPalette);
    viewport()->setAutoFillBackground(true);
    viewport()->setStyleSheet(
        QStringLiteral("background-color: %1; color: %2;")
            .arg(pageBackgroundColor.name(), textColor.name()));

    setStyleSheet(buildWidgetStyleSheet());
    setTextColor(textColor);

    document()->setBaseUrl(QUrl::fromLocalFile(m_documentPath));
    document()->setDefaultFont(documentFont);
    document()->setDocumentMargin(m_viewerSettings.documentMargin);

    if (!m_markdownText.isEmpty()) {
        setMarkdown(m_markdownText);
        document()->setDefaultStyleSheet(buildDocumentStyleSheet());
        document()->setDocumentMargin(m_viewerSettings.documentMargin);

        QTextFrameFormat rootFrameFormat = document()->rootFrame()->frameFormat(); ///< Формат корневого фрейма QTextDocument для явного задания фона страницы.
        rootFrameFormat.setBackground(pageBackgroundColor);
        document()->rootFrame()->setFrameFormat(rootFrameFormat);

        moveCursor(QTextCursor::Start);
        verticalScrollBar()->setValue(verticalScrollValue);
        horizontalScrollBar()->setValue(horizontalScrollValue);
    }
}

/**
 * @brief Возвращает CSS-строку для оформления элементов документа Markdown.
 * @return CSS с цветами и оформлением ссылок, кода и заголовков.
 */
QString MarkdownView::buildDocumentStyleSheet() const
{
    const bool useInvertedColors = m_viewerSettings.invertedColors; ///< Признак применения тёмной инвертированной схемы документа Markdown.
    const QString linkDecoration = m_viewerSettings.underlineLinks ? QStringLiteral("underline") : QStringLiteral("none"); ///< Стиль подчёркивания ссылок.
    const QString pageBackground = useInvertedColors ? QStringLiteral("#181A1B") : QStringLiteral("#FFFFFF"); ///< Цвет основного фона страницы Markdown.
    const QString pageTextColor = useInvertedColors ? QStringLiteral("#E8EAED") : QStringLiteral("#202124"); ///< Цвет основного текста страницы Markdown.
    const QString linkColor = useInvertedColors ? QStringLiteral("#8AB4F8") : QStringLiteral("#0B57D0"); ///< Цвет Markdown-ссылок.
    const QString headingColor = useInvertedColors ? QStringLiteral("#AECBFA") : QStringLiteral("#174EA6"); ///< Цвет заголовков Markdown.
    const QString quoteColor = useInvertedColors ? QStringLiteral("#BDC1C6") : QStringLiteral("#5F6368"); ///< Цвет Markdown-цитат.
    const QString quoteBorderColor = useInvertedColors ? QStringLiteral("#5F6368") : QStringLiteral("#D9CBB7"); ///< Цвет боковой линии цитаты.
    const QString codeBackground = m_viewerSettings.emphasizeCodeBlocks
        ? (useInvertedColors ? QStringLiteral("#2D2F31") : QStringLiteral("#F3EEDF"))
        : QStringLiteral("transparent"); ///< Цвет фона для inline-кода.
    const QString preBackground = m_viewerSettings.emphasizeCodeBlocks
        ? (useInvertedColors ? QStringLiteral("#242628") : QStringLiteral("#F6F1E7"))
        : QStringLiteral("transparent"); ///< Цвет фона для блоков кода.
    const QString preBorder = m_viewerSettings.emphasizeCodeBlocks
        ? QStringLiteral("1px solid %1").arg(useInvertedColors ? QStringLiteral("#4A4D52") : QStringLiteral("#DDD2BE"))
        : QStringLiteral("none"); ///< Граница вокруг блока кода.

    return QStringLiteral(
        "html, body, p, li, ul, ol, dl, dt, dd, div, span, table, tr, td, th { color: %6; background: transparent; }"
        "html, body { background: %5; }"
        "a { color: %7; text-decoration: %1; }"
        "h1, h2, h3, h4, h5, h6 { color: %8; font-weight: 700; }"
        "blockquote { color: %9; border-left: 4px solid %10; margin-left: 0px; padding-left: 12px; }"
        "pre { color: %6; background: %2; border: %3; border-radius: 6px; padding: 10px; white-space: pre-wrap; }"
        "code { color: %6; background: %4; border-radius: 4px; padding: 1px 3px; }")
        .arg(linkDecoration, preBackground, preBorder, codeBackground, pageBackground, pageTextColor, linkColor, headingColor, quoteColor, quoteBorderColor);
}

/**
 * @brief Возвращает CSS-строку для оформления самого виджета просмотра.
 * @return CSS для фонового режима окна просмотра Markdown.
 */
QString MarkdownView::buildWidgetStyleSheet() const
{
    const bool useInvertedColors = m_viewerSettings.invertedColors; ///< Признак применения тёмной инвертированной схемы самого виджета просмотра.
    const QString pageBackground = useInvertedColors ? QStringLiteral("#181A1B") : QStringLiteral("#FFFFFF"); ///< Цвет фона области просмотра Markdown.
    const QString pageTextColor = useInvertedColors ? QStringLiteral("#E8EAED") : QStringLiteral("#202124"); ///< Цвет текста области просмотра Markdown.

    return QStringLiteral(
        "QTextBrowser { border: none; background-color: %1; color: %2; selection-background-color: #C7DAF8; }"
        "QTextBrowser > QWidget { background: transparent; color: %2; }")
        .arg(pageBackground, pageTextColor);
}
