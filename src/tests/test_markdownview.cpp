#include "markdownview.h"

#include <QTest>

/**
 * @brief Класс содержит тесты отображения Mermaid-блоков в Markdown viewer.
 *
 * Тесты проверяют, что fenced-блок `mermaid` не показывается как сырой код,
 * а преобразуется во внутреннее читаемое представление viewer.
 */
class MarkdownViewTests : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief Проверяет преобразование Mermaid-блока в читаемый текст документа.
     */
    void renderMermaidBlockAsViewerDiagram();

    /**
     * @brief Проверяет, что размер шрифта viewer переносится в HTML-документ после обработки Mermaid.
     */
    void applyViewerFontSizeToHtmlDocument();
};

/**
 * @brief Проверяет преобразование Mermaid-блока в читаемый текст документа.
 */
void MarkdownViewTests::renderMermaidBlockAsViewerDiagram()
{
    MarkdownView markdownView;
    markdownView.setMarkdownContent(
        QStringLiteral(
            "# Карта\n\n"
            "## Mermaid-диаграмма модулей\n\n"
            "```mermaid\n"
            "flowchart TB\n"
            "    subgraph class_modules[\"Классовые модули\"]\n"
            "        module_a[\"ModuleA<br/>функций: 3<br/>связей: 1\"]\n"
            "        module_b[\"ModuleB<br/>функций: 1<br/>связей: 0\"]\n"
            "    end\n"
            "    module_a -->|include| module_b\n"
            "```\n"));

    const QString plainText = markdownView.toPlainText(); ///< Текстовое представление документа после внутренней HTML-обработки Mermaid-блока.
    QVERIFY(plainText.contains(QStringLiteral("Mermaid-диаграмма модулей")));
    QVERIFY(plainText.contains(QStringLiteral("Классовые модули")));
    QVERIFY(plainText.contains(QStringLiteral("ModuleA")));
    QVERIFY(plainText.contains(QStringLiteral("ModuleB")));
    QVERIFY(plainText.contains(QStringLiteral("include")));
    QVERIFY(!plainText.contains(QStringLiteral("```mermaid")));
    QVERIFY(!plainText.contains(QStringLiteral("flowchart TB")));
}

/**
 * @brief Проверяет, что размер шрифта viewer переносится в HTML-документ после обработки Mermaid.
 */
void MarkdownViewTests::applyViewerFontSizeToHtmlDocument()
{
    MarkdownView markdownView;
    ViewerSettings viewerSettings = ViewerSettingsStore::defaultSettings(); ///< Набор настроек просмотра, который будет применён к тестовому viewer.
    viewerSettings.fontPointSize = 22;
    viewerSettings.fontFamily = QStringLiteral("DejaVu Sans");

    markdownView.applyViewerSettings(viewerSettings);
    markdownView.setMarkdownContent(
        QStringLiteral(
            "# Заголовок\n\n"
            "Текст документа.\n\n"
            "```mermaid\n"
            "flowchart TB\n"
            "    module_a[\"ModuleA\"]\n"
            "```\n"));

    const QString htmlText = markdownView.toHtml(); ///< HTML-представление документа после применения настроек viewer.
    QVERIFY(htmlText.contains(QStringLiteral("font-size:22pt"), Qt::CaseInsensitive));
    QVERIFY(htmlText.contains(QStringLiteral("DejaVu Sans"), Qt::CaseInsensitive));
}

QTEST_MAIN(MarkdownViewTests)

#include "test_markdownview.moc"
