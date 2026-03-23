#pragma once

#include "viewersettings.h"

#include <QTextBrowser>

/**
 * @brief Виджет отображает Markdown-файл и хранит путь к его источнику.
 *
 * Виджет нужен для показа документа во вкладке и для перехода к якорям
 * внутри уже загруженного Markdown.
 */
class MarkdownView : public QTextBrowser
{
    Q_OBJECT

public:
    /**
     * @brief Создаёт виджет отображения Markdown.
     * @param parent Родительский виджет Qt для управления временем жизни объекта.
     */
    explicit MarkdownView(QWidget* parent = nullptr);

    /**
     * @brief Загружает Markdown-файл в виджет.
     * @param filePath Абсолютный путь к Markdown-файлу, который нужно отобразить.
     * @param errorText Строка для возврата текста ошибки, если загрузка не удалась.
     * @return true, если файл успешно прочитан и показан в виджете.
     */
    bool loadMarkdownFromFile(const QString& filePath, QString* errorText = nullptr);

    /**
     * @brief Загружает Markdown-текст напрямую в виджет без чтения файла с диска.
     * @param markdownText Текст Markdown, который нужно показать в окне просмотра.
     * @param documentPath Необязательный путь документа для вычисления baseUrl ссылок.
     */
    void setMarkdownContent(const QString& markdownText, const QString& documentPath = QString());

    /**
     * @brief Прокручивает документ к указанному якорю.
     * @param anchor Имя якоря внутри документа без символа '#'.
     */
    void jumpToAnchor(const QString& anchor);

    /**
     * @brief Возвращает путь к текущему открытому документу.
     * @return Абсолютный путь к Markdown-файлу, загруженному в виджет.
     */
    QString documentPath() const;

    /**
     * @brief Применяет настройки внешнего вида к текущему окну просмотра Markdown.
     * @param settings Набор настроек шрифта и оформления, который нужно применить.
     */
    void applyViewerSettings(const ViewerSettings& settings);

private:
    /**
     * @brief Перестраивает отображение Markdown по текущему тексту и настройкам просмотра.
     *
     * Метод нужен, чтобы можно было менять внешний вид открытого документа без
     * повторного чтения файла с диска.
     */
    void renderMarkdown();

    /**
     * @brief Возвращает CSS-строку для оформления элементов документа Markdown.
     * @return CSS с цветами и оформлением ссылок, кода и заголовков.
     */
    QString buildDocumentStyleSheet() const;

    /**
     * @brief Возвращает CSS-строку для оформления самого виджета просмотра.
     * @return CSS для фонового режима окна просмотра Markdown.
     */
    QString buildWidgetStyleSheet() const;

    QString m_documentPath;             ///< Абсолютный путь к открытому Markdown-документу.
    QString m_markdownText;             ///< Исходный Markdown-текст открытого документа.
    ViewerSettings m_viewerSettings;    ///< Текущие настройки внешнего вида окна просмотра.
};
