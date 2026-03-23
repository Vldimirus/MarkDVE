#pragma once

#include <QObject>

class DocumentTabWidget;
class QFileSystemWatcher;
class MarkdownView;
class QUrl;
class WorkspaceManager;

/**
 * @brief Контроллер управляет открытием документов, переходами по ссылкам и обновлением вкладок.
 *
 * Класс связывает рабочую папку, Markdown-вкладки и внешние изменения файлов,
 * чтобы пользователь мог навигировать по документам как по карте проекта.
 */
class DocumentController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Создаёт контроллер документов.
     * @param workspaceManager Менеджер рабочей папки для поиска файлов и разрешения ссылок.
     * @param documentTabs Виджет вкладок, куда нужно открывать документы.
     * @param parent Родительский объект Qt для управления временем жизни контроллера.
     */
    explicit DocumentController(WorkspaceManager* workspaceManager, DocumentTabWidget* documentTabs, QObject* parent = nullptr);

    /**
     * @brief Открывает документ во вкладке и, при необходимости, переходит к якорю.
     * @param documentPath Абсолютный или относительный путь к Markdown-файлу.
     * @param anchor Якорь внутри документа без символа '#', к которому нужно перейти.
     * @return true, если документ удалось открыть или активировать.
     */
    bool openDocument(const QString& documentPath, const QString& anchor = QString());

    /**
     * @brief Перечитывает уже открытый документ после изменения файла на диске.
     * @param documentPath Абсолютный путь к Markdown-файлу, который нужно обновить.
     * @return true, если документ был открыт и успешно перечитан.
     */
    bool reloadDocument(const QString& documentPath);

    /**
     * @brief Возвращает путь к текущему активному документу.
     * @return Абсолютный путь к документу активной вкладки или пустая строка.
     */
    QString currentDocumentPath() const;

signals:
    /**
     * @brief Сигнал сообщает об успешном открытии или активации документа.
     * @param documentPath Абсолютный путь к документу, который стал активным.
     */
    void documentOpened(const QString& documentPath);

    /**
     * @brief Сигнал сообщает о смене текущего документа.
     * @param documentPath Абсолютный путь к текущему документу.
     */
    void currentDocumentChanged(const QString& documentPath);

    /**
     * @brief Сигнал сообщает о текстовом статусе для строки состояния.
     * @param message Текст статуса, который нужно показать пользователю.
     */
    void statusMessage(const QString& message);

    /**
     * @brief Сигнал сообщает об ошибке открытия или обновления документа.
     * @param errorText Текст ошибки для показа пользователю.
     */
    void errorOccurred(const QString& errorText);

private slots:
    /**
     * @brief Обрабатывает изменение Markdown-файла на диске.
     * @param changedFilePath Абсолютный путь к файлу, который изменился.
     */
    void handleFileChanged(const QString& changedFilePath);

    /**
     * @brief Снимает наблюдение за файлом после закрытия вкладки.
     * @param documentPath Абсолютный путь к документу, закрытому пользователем.
     */
    void handleDocumentClosed(const QString& documentPath);

private:
    /**
     * @brief Открывает ссылку, на которую пользователь нажал внутри Markdown.
     * @param baseFilePath Абсолютный путь к исходному документу, содержащему ссылку.
     * @param targetUrl Ссылка из Markdown в виде URL.
     */
    void openLinkFromDocument(const QString& baseFilePath, const QUrl& targetUrl);

    /**
     * @brief Подключает обработчики событий для новой вкладки документа.
     * @param view Виджет Markdown, для которого нужно настроить сигналы и слежение за файлом.
     */
    void attachView(MarkdownView* view);

    WorkspaceManager* m_workspaceManager = nullptr;          ///< Менеджер рабочей папки для поиска файлов и ссылок.
    DocumentTabWidget* m_documentTabs = nullptr;            ///< Виджет вкладок, в который открываются документы.
    QFileSystemWatcher* m_fileWatcher = nullptr;            ///< Наблюдатель за открытыми файлами для автообновления.
};
