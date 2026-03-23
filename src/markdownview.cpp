#include "markdownview.h"

#include <QFile>
#include <QFont>
#include <QHash>
#include <QPalette>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStringConverter>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QTextStream>
#include <QUrl>

namespace
{
/**
 * @brief Структура описывает один узел Mermaid-диаграммы.
 *
 * Структура нужна для преобразования Mermaid-блока в оформленный HTML с
 * карточками модулей и дальнейшим построением таблицы связей.
 */
struct MermaidNodeInfo
{
    QString nodeId;            ///< Внутренний идентификатор узла из Mermaid-диаграммы.
    QString groupTitle;        ///< Название группы Mermaid `subgraph`, в которую входит узел.
    QStringList labelLines;    ///< Строки подписи узла, разделённые по `<br/>`.
};

/**
 * @brief Структура описывает одно ребро Mermaid-диаграммы.
 *
 * Структура хранит направленную связь между двумя узлами и подпись на стрелке,
 * чтобы отрисовать её в виде читаемой HTML-таблицы.
 */
struct MermaidEdgeInfo
{
    QString sourceNodeId;      ///< Идентификатор узла-источника связи.
    QString targetNodeId;      ///< Идентификатор узла-получателя связи.
    QString relationLabel;     ///< Текстовая подпись связи, например `include` или `signal-slot`.
};

/**
 * @brief Структура хранит разобранную Mermaid-диаграмму.
 *
 * Структура нужна как промежуточное представление между исходным Mermaid-кодом
 * и итоговым HTML-блоком, который будет показан в Markdown viewer.
 */
struct MermaidDiagramInfo
{
    QStringList groupOrder;                        ///< Порядок групп Mermaid `subgraph`, найденных в диаграмме.
    QHash<QString, QStringList> groupNodes;        ///< Карта групп на список идентификаторов входящих в них узлов.
    QHash<QString, MermaidNodeInfo> nodesById;     ///< Карта идентификаторов узлов на их текстовое описание.
    QList<MermaidEdgeInfo> edges;                  ///< Список связей между узлами диаграммы.
};

/**
 * @brief Разбивает подпись узла Mermaid на отдельные строки по `<br/>`.
 * @param rawLabel Исходная подпись узла Mermaid внутри `["..."]`.
 * @return Список строк подписи узла без HTML-тега переноса строки.
 */
QStringList splitMermaidLabel(const QString& rawLabel)
{
    QString normalizedLabel = rawLabel;
    normalizedLabel.replace(QStringLiteral("<br/>"), QStringLiteral("\n"), Qt::CaseInsensitive);
    normalizedLabel.replace(QStringLiteral("<br />"), QStringLiteral("\n"), Qt::CaseInsensitive);
    normalizedLabel.replace(QStringLiteral("<br>"), QStringLiteral("\n"), Qt::CaseInsensitive);

    QStringList labelLines = normalizedLabel.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (QString& nextLine : labelLines) {
        nextLine = nextLine.trimmed();
    }

    return labelLines;
}

/**
 * @brief Возвращает основное имя узла Mermaid для таблицы связей.
 * @param nodeInfo Описание узла Mermaid.
 * @return Первая строка подписи узла или его внутренний идентификатор, если подпись пуста.
 */
QString mermaidNodeTitle(const MermaidNodeInfo& nodeInfo)
{
    return nodeInfo.labelLines.isEmpty() ? nodeInfo.nodeId : nodeInfo.labelLines.first();
}

/**
 * @brief Разбирает Mermaid `flowchart` блок в структурированное представление.
 * @param mermaidSource Текст Mermaid-блока без внешних тройных кавычек.
 * @param diagramInfo Структура для возврата разобранной диаграммы.
 * @return true, если Mermaid-блок распознан как поддерживаемая диаграмма.
 */
bool parseMermaidDiagram(const QString& mermaidSource, MermaidDiagramInfo* diagramInfo)
{
    if (diagramInfo == nullptr) {
        return false;
    }

    const QStringList lines = mermaidSource.split(QLatin1Char('\n'));
    QString currentGroupTitle; ///< Название текущей группы `subgraph`, в которую будут складываться новые узлы.

    for (const QString& rawLine : lines) {
        const QString trimmedLine = rawLine.trimmed();
        if (trimmedLine.isEmpty()
            || trimmedLine.startsWith(QStringLiteral("flowchart "))
            || trimmedLine.startsWith(QStringLiteral("classDef "))
            || trimmedLine.startsWith(QStringLiteral("class "))) {
            continue;
        }

        if (trimmedLine == QStringLiteral("end")) {
            currentGroupTitle.clear();
            continue;
        }

        if (trimmedLine.startsWith(QStringLiteral("subgraph "))) {
            const int titleStartIndex = trimmedLine.indexOf(QStringLiteral("[\""));
            const int titleEndIndex = trimmedLine.lastIndexOf(QStringLiteral("\"]"));
            if (titleStartIndex >= 0 && titleEndIndex > titleStartIndex) {
                currentGroupTitle = trimmedLine.mid(titleStartIndex + 2, titleEndIndex - titleStartIndex - 2).trimmed();
            } else {
                currentGroupTitle = trimmedLine.mid(QStringLiteral("subgraph ").size()).trimmed();
            }

            if (!currentGroupTitle.isEmpty() && !diagramInfo->groupOrder.contains(currentGroupTitle)) {
                diagramInfo->groupOrder.append(currentGroupTitle);
            }
            continue;
        }

        const QRegularExpressionMatch nodeMatch = QRegularExpression(QStringLiteral("^([A-Za-z_][A-Za-z0-9_]*)\\[\"(.+)\"\\]$")).match(trimmedLine);
        if (nodeMatch.hasMatch()) {
            MermaidNodeInfo nodeInfo;
            nodeInfo.nodeId = nodeMatch.captured(1);
            nodeInfo.groupTitle = currentGroupTitle;
            nodeInfo.labelLines = splitMermaidLabel(nodeMatch.captured(2));
            diagramInfo->nodesById.insert(nodeInfo.nodeId, nodeInfo);

            const QString targetGroupTitle = currentGroupTitle.isEmpty()
                ? QStringLiteral("Прочие узлы")
                : currentGroupTitle;
            if (!diagramInfo->groupOrder.contains(targetGroupTitle)) {
                diagramInfo->groupOrder.append(targetGroupTitle);
            }
            diagramInfo->groupNodes[targetGroupTitle].append(nodeInfo.nodeId);
            continue;
        }

        const QRegularExpressionMatch edgeMatch = QRegularExpression(QStringLiteral("^([A-Za-z_][A-Za-z0-9_]*)\\s*-->(?:\\|(.+?)\\|)?\\s*([A-Za-z_][A-Za-z0-9_]*)$")).match(trimmedLine);
        if (edgeMatch.hasMatch()) {
            MermaidEdgeInfo edgeInfo;
            edgeInfo.sourceNodeId = edgeMatch.captured(1);
            edgeInfo.relationLabel = edgeMatch.captured(2).trimmed();
            edgeInfo.targetNodeId = edgeMatch.captured(3);
            diagramInfo->edges.append(edgeInfo);
        }
    }

    return !diagramInfo->nodesById.isEmpty();
}

/**
 * @brief Создаёт HTML-блок для показа Mermaid-диаграммы внутри Markdown viewer.
 * @param diagramInfo Разобранная Mermaid-диаграмма с узлами и связями.
 * @param viewerSettings Текущие настройки оформления просмотрщика, влияющие на цвета блока.
 * @return Готовый HTML-фрагмент с карточками модулей и таблицей связей.
 */
QString buildMermaidDiagramHtml(const MermaidDiagramInfo& diagramInfo, const ViewerSettings& viewerSettings)
{
    const bool useInvertedColors = viewerSettings.invertedColors; ///< Признак тёмной схемы оформления viewer для выбора цветов Mermaid-блока.
    const QString panelBackground = useInvertedColors ? QStringLiteral("#202124") : QStringLiteral("#FAFBFC"); ///< Цвет фона всей Mermaid-панели.
    const QString panelBorderColor = useInvertedColors ? QStringLiteral("#4A4D52") : QStringLiteral("#DADCE0"); ///< Цвет внешней рамки Mermaid-панели.
    const QString panelTextColor = useInvertedColors ? QStringLiteral("#E8EAED") : QStringLiteral("#202124"); ///< Цвет обычного текста Mermaid-панели.
    const QString mutedTextColor = useInvertedColors ? QStringLiteral("#BDC1C6") : QStringLiteral("#5F6368"); ///< Цвет вспомогательного текста внутри карточек модулей.
    const QString cardBackground = useInvertedColors ? QStringLiteral("#2D2F31") : QStringLiteral("#F6F8FC"); ///< Цвет фона карточки отдельного модуля.
    const QString tableHeaderBackground = useInvertedColors ? QStringLiteral("#2B2F33") : QStringLiteral("#EEF3FB"); ///< Цвет фона заголовков таблиц Mermaid-панели.

    QString htmlText;
    QTextStream htmlStream(&htmlText);
    htmlStream << "<div style=\"margin: 16px 0; padding: 14px; border: 1px solid " << panelBorderColor
               << "; border-radius: 10px; background: " << panelBackground << "; color: " << panelTextColor << ";\">";
    htmlStream << "<div style=\"font-weight: 700; font-size: 115%; margin-bottom: 10px;\">Mermaid-диаграмма модулей</div>";

    htmlStream << "<table style=\"width: 100%; border-collapse: collapse; margin: 8px 0 14px 0;\">";
    htmlStream << "<tr>"
               << "<th style=\"text-align: left; padding: 8px; border: 1px solid " << panelBorderColor << "; background: " << tableHeaderBackground << ";\">Группа</th>"
               << "<th style=\"text-align: left; padding: 8px; border: 1px solid " << panelBorderColor << "; background: " << tableHeaderBackground << ";\">Модули</th>"
               << "</tr>";

    for (const QString& groupTitle : diagramInfo.groupOrder) {
        const QStringList groupNodeIds = diagramInfo.groupNodes.value(groupTitle);
        if (groupNodeIds.isEmpty()) {
            continue;
        }

        htmlStream << "<tr>";
        htmlStream << "<td style=\"width: 22%; vertical-align: top; padding: 8px; border: 1px solid " << panelBorderColor << "; font-weight: 600;\">"
                   << groupTitle.toHtmlEscaped()
                   << "</td>";
        htmlStream << "<td style=\"padding: 8px; border: 1px solid " << panelBorderColor << ";\">";

        for (const QString& nodeId : groupNodeIds) {
            const MermaidNodeInfo nodeInfo = diagramInfo.nodesById.value(nodeId);
            htmlStream << "<span style=\"display: inline-block; margin: 4px 8px 4px 0; padding: 9px 11px; border: 1px solid " << panelBorderColor
                       << "; border-radius: 8px; background: " << cardBackground << ";\">";
            htmlStream << "<span style=\"font-weight: 700; color: " << panelTextColor << ";\">"
                       << mermaidNodeTitle(nodeInfo).toHtmlEscaped()
                       << "</span>";
            for (int lineIndex = 1; lineIndex < nodeInfo.labelLines.size(); ++lineIndex) {
                htmlStream << "<br/><span style=\"color: " << mutedTextColor << "; font-size: 90%;\">"
                           << nodeInfo.labelLines.at(lineIndex).toHtmlEscaped()
                           << "</span>";
            }
            htmlStream << "</span>";
        }

        htmlStream << "</td>";
        htmlStream << "</tr>";
    }

    htmlStream << "</table>";

    if (!diagramInfo.edges.isEmpty()) {
        htmlStream << "<div style=\"font-weight: 700; margin: 8px 0 6px 0;\">Связи</div>";
        htmlStream << "<table style=\"width: 100%; border-collapse: collapse; margin: 0;\">";
        htmlStream << "<tr>"
                   << "<th style=\"text-align: left; padding: 8px; border: 1px solid " << panelBorderColor << "; background: " << tableHeaderBackground << ";\">Откуда</th>"
                   << "<th style=\"text-align: left; padding: 8px; border: 1px solid " << panelBorderColor << "; background: " << tableHeaderBackground << ";\">Связь</th>"
                   << "<th style=\"text-align: left; padding: 8px; border: 1px solid " << panelBorderColor << "; background: " << tableHeaderBackground << ";\">Куда</th>"
                   << "</tr>";

        for (const MermaidEdgeInfo& edgeInfo : diagramInfo.edges) {
            const MermaidNodeInfo sourceNode = diagramInfo.nodesById.value(edgeInfo.sourceNodeId);
            const MermaidNodeInfo targetNode = diagramInfo.nodesById.value(edgeInfo.targetNodeId);
            htmlStream << "<tr>"
                       << "<td style=\"padding: 8px; border: 1px solid " << panelBorderColor << ";\">"
                       << mermaidNodeTitle(sourceNode).toHtmlEscaped()
                       << "</td>"
                       << "<td style=\"padding: 8px; border: 1px solid " << panelBorderColor << ";\">"
                       << (edgeInfo.relationLabel.isEmpty() ? QStringLiteral("связь") : edgeInfo.relationLabel).toHtmlEscaped()
                       << "</td>"
                       << "<td style=\"padding: 8px; border: 1px solid " << panelBorderColor << ";\">"
                       << mermaidNodeTitle(targetNode).toHtmlEscaped()
                       << "</td>"
                       << "</tr>";
        }

        htmlStream << "</table>";
    }

    htmlStream << "</div>";
    return htmlText;
}

/**
 * @brief Создаёт fallback HTML для Mermaid-блока, если он не распознан обработчиком.
 * @param mermaidSource Исходный Mermaid-код, который не удалось разобрать.
 * @param viewerSettings Текущие настройки оформления viewer для выбора цветов fallback-блока.
 * @return HTML-фрагмент с сообщением об ошибке разбора и исходным кодом Mermaid.
 */
QString buildMermaidFallbackHtml(const QString& mermaidSource, const ViewerSettings& viewerSettings)
{
    const bool useInvertedColors = viewerSettings.invertedColors; ///< Признак тёмной схемы оформления для выбора цветов fallback-блока.
    const QString panelBackground = useInvertedColors ? QStringLiteral("#202124") : QStringLiteral("#FAFBFC"); ///< Цвет фона fallback-блока Mermaid.
    const QString panelBorderColor = useInvertedColors ? QStringLiteral("#4A4D52") : QStringLiteral("#DADCE0"); ///< Цвет рамки fallback-блока Mermaid.
    const QString codeBackground = useInvertedColors ? QStringLiteral("#2D2F31") : QStringLiteral("#F6F1E7"); ///< Цвет фона области исходного Mermaid-кода.

    return QStringLiteral(
        "<div style=\"margin: 16px 0; padding: 14px; border: 1px solid %1; border-radius: 10px; background: %2;\">"
        "<div style=\"font-weight: 700; margin-bottom: 8px;\">Mermaid-диаграмма</div>"
        "<div style=\"margin-bottom: 8px;\">Этот Mermaid-блок не удалось обработать встроенным viewer.</div>"
        "<pre style=\"margin: 0; padding: 10px; border-radius: 8px; border: 1px solid %1; background: %3; white-space: pre-wrap;\">%4</pre>"
        "</div>")
        .arg(panelBorderColor, panelBackground, codeBackground, mermaidSource.toHtmlEscaped());
}

/**
 * @brief Встраивает HTML-обработку Mermaid-блоков в общий HTML документа Markdown.
 * @param markdownText Исходный Markdown-текст, который может содержать fenced-блоки `mermaid`.
 * @param viewerSettings Текущие настройки viewer, влияющие на оформление Mermaid-диаграммы.
 * @return HTML-документ, в котором Mermaid-блоки заменены на оформленные HTML-фрагменты.
 */
QString renderMarkdownWithMermaid(const QString& markdownText, const ViewerSettings& viewerSettings)
{
    const QStringList lines = markdownText.split(QLatin1Char('\n'));
    QString processedMarkdown; ///< Промежуточный Markdown-текст с токенами вместо Mermaid-блоков.
    QHash<QString, QString> htmlFragmentsByToken; ///< Карта токенов на HTML-фрагменты, которые будут внедрены после Markdown->HTML преобразования.
    bool insideMermaidBlock = false; ///< Признак нахождения внутри fenced-блока ` ```mermaid `.
    QString currentMermaidSource; ///< Накопитель строк текущего Mermaid-блока до закрывающей тройной кавычки.
    int mermaidBlockIndex = 0; ///< Счётчик Mermaid-блоков для построения уникальных токенов замены.

    for (const QString& nextLine : lines) {
        const QString trimmedLine = nextLine.trimmed();
        if (!insideMermaidBlock && trimmedLine == QStringLiteral("```mermaid")) {
            insideMermaidBlock = true;
            currentMermaidSource.clear();
            continue;
        }

        if (insideMermaidBlock) {
            if (trimmedLine == QStringLiteral("```")) {
                const QString placeholderToken = QStringLiteral("MERMAID_BLOCK_TOKEN_%1").arg(mermaidBlockIndex++);
                MermaidDiagramInfo diagramInfo;
                htmlFragmentsByToken.insert(
                    placeholderToken,
                    parseMermaidDiagram(currentMermaidSource, &diagramInfo)
                        ? buildMermaidDiagramHtml(diagramInfo, viewerSettings)
                        : buildMermaidFallbackHtml(currentMermaidSource, viewerSettings));

                processedMarkdown += placeholderToken + QLatin1String("\n\n");
                insideMermaidBlock = false;
                currentMermaidSource.clear();
            } else {
                currentMermaidSource += nextLine + QLatin1Char('\n');
            }
            continue;
        }

        processedMarkdown += nextLine + QLatin1Char('\n');
    }

    if (insideMermaidBlock) {
        processedMarkdown += QStringLiteral("```mermaid\n") + currentMermaidSource + QStringLiteral("```\n");
    }

    QTextDocument conversionDocument;
    conversionDocument.setMarkdown(processedMarkdown);
    QString htmlDocument = conversionDocument.toHtml();

    for (auto tokenIterator = htmlFragmentsByToken.cbegin(); tokenIterator != htmlFragmentsByToken.cend(); ++tokenIterator) {
        const QString placeholderToken = tokenIterator.key();
        const QString htmlFragment = tokenIterator.value();
        const QRegularExpression paragraphExpression(
            QStringLiteral("<p[^>]*>\\s*%1\\s*</p>").arg(QRegularExpression::escape(placeholderToken)),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);

        htmlDocument.replace(paragraphExpression, htmlFragment);
        htmlDocument.replace(placeholderToken, htmlFragment);
    }

    return htmlDocument;
}
}

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
        setHtml(buildHtmlDocument(m_markdownText));
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
    QString fontFamilyForCss = m_viewerSettings.fontFamily; ///< Имя семейства шрифта, подготовленное для безопасной вставки в CSS-строку HTML-документа.
    fontFamilyForCss.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    fontFamilyForCss.replace(QLatin1Char('\''), QStringLiteral("\\'"));
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
    const QString tableBorderColor = useInvertedColors ? QStringLiteral("#4A4D52") : QStringLiteral("#DADCE0"); ///< Цвет границы Markdown-таблиц.
    const QString tableHeaderBackground = useInvertedColors ? QStringLiteral("#2B2F33") : QStringLiteral("#EEF3FB"); ///< Цвет фона заголовков Markdown-таблиц.

    return QStringLiteral(
        "html, body, p, li, ul, ol, dl, dt, dd, div, span, table, tr, td, th { color: %6; background: transparent; font-family: '%13'; font-size: %14pt; }"
        "html, body { background: %5; font-family: '%13'; font-size: %14pt; }"
        "a { color: %7; text-decoration: %1; }"
        "h1, h2, h3, h4, h5, h6 { color: %8; font-weight: 700; }"
        "blockquote { color: %9; border-left: 4px solid %10; margin-left: 0px; padding-left: 12px; }"
        "table { width: 100%; border-collapse: collapse; margin: 12px 0; }"
        "th, td { border: 1px solid %11; padding: 7px 9px; vertical-align: top; }"
        "th { background: %12; font-weight: 700; }"
        "pre { color: %6; background: %2; border: %3; border-radius: 6px; padding: 10px; white-space: pre-wrap; }"
        "code { color: %6; background: %4; border-radius: 4px; padding: 1px 3px; }")
        .arg(linkDecoration,
            preBackground,
            preBorder,
            codeBackground,
            pageBackground,
            pageTextColor,
            linkColor,
            headingColor,
            quoteColor,
            quoteBorderColor,
            tableBorderColor,
            tableHeaderBackground,
            fontFamilyForCss,
            QString::number(m_viewerSettings.fontPointSize));
}

/**
 * @brief Собирает HTML-документ для показа Markdown с обработанными Mermaid-блоками.
 * @param markdownText Исходный Markdown-текст, который нужно преобразовать для окна просмотра.
 * @return Готовый HTML-документ с внедрёнными стилями и обработанными диаграммами.
 */
QString MarkdownView::buildHtmlDocument(const QString& markdownText) const
{
    QString htmlDocument = renderMarkdownWithMermaid(markdownText, m_viewerSettings);
    htmlDocument.replace(
        QStringLiteral("</head>"),
        QStringLiteral("<style>%1</style></head>").arg(buildDocumentStyleSheet()));
    return htmlDocument;
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
