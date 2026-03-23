#pragma once

#include "viewersettings.h"

#include <QMainWindow>

#include <QHash>
#include <QList>
#include <QString>

class DocumentController;
class DocumentTabWidget;
class EditorWindow;
class QAction;
class QCloseEvent;
class QDockWidget;
class QMenu;
class QTreeWidget;
class QTreeWidgetItem;
class WorkspaceManager;

/**
 * @brief Структура описывает запись в истории последних открытых документов.
 *
 * Запись хранит рабочую папку и путь к документу, чтобы приложение могло
 * корректно восстановить контекст при повторном открытии из меню.
 */
struct RecentDocumentEntry
{
    QString workspacePath;    ///< Абсолютный путь к рабочей папке, в которой был открыт документ.
    QString documentPath;     ///< Абсолютный путь к Markdown-документу.
};

/**
 * @brief Главное окно приложения для просмотра и навигации по Markdown-карте проекта.
 *
 * Окно отвечает за меню, дерево документов рабочей папки, вкладки просмотра
 * и открытие отдельных окон редактирования Markdown-файлов.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Создаёт главное окно приложения.
     * @param parent Родительский виджет Qt для управления временем жизни окна.
     */
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    /**
     * @brief Закрывает дочерние окна редактора вместе с главным окном.
     * @param event Событие закрытия главного окна.
     */
    void closeEvent(QCloseEvent* event) override;

private slots:
    /**
     * @brief Показывает диалог выбора рабочей папки и открывает выбранный проект.
     */
    void chooseWorkspaceFolder();

    /**
     * @brief Показывает диалог выбора Markdown-файла и открывает его.
     */
    void chooseMarkdownFile();

    /**
     * @brief Перестраивает дерево Markdown-файлов после смены рабочей папки.
     */
    void rebuildWorkspaceTree();

    /**
     * @brief Открывает документ из выбранного элемента дерева рабочей папки.
     * @param item Элемент дерева, по которому кликнул пользователь.
     * @param column Номер колонки дерева, где произошёл клик.
     */
    void openDocumentFromTree(QTreeWidgetItem* item, int column);

    /**
     * @brief Открывает текущий документ в отдельном окне редактора.
     */
    void openCurrentDocumentInEditor();

    /**
     * @brief Показывает сообщение в строке состояния приложения.
     * @param message Текст статуса, который нужно показать пользователю.
     */
    void showStatusMessage(const QString& message);

    /**
     * @brief Показывает пользователю ошибку через диалоговое окно.
     * @param errorText Текст ошибки для показа.
     */
    void showErrorMessage(const QString& errorText);

    /**
     * @brief Обновляет заголовок главного окна после смены текущего документа.
     * @param documentPath Абсолютный путь к текущему документу.
     */
    void updateCurrentDocumentTitle(const QString& documentPath);

    /**
     * @brief Добавляет успешно открытый документ в историю последних файлов.
     * @param documentPath Абсолютный путь к документу, который был открыт или активирован.
     */
    void handleDocumentOpened(const QString& documentPath);

    /**
     * @brief Открывает диалог настройки внешнего вида окна просмотра Markdown.
     */
    void openViewerAppearanceSettings();

    /**
     * @brief Запускает мастер генерации Markdown-карты кода из C/C++ исходников.
     *
     * Метод последовательно запрашивает каталог исходников и каталог вывода,
     * а затем создаёт полную иерархию Markdown-файлов с перекрёстными ссылками.
     */
    void generateCppCodeMap();

private:
    /**
     * @brief Создаёт виджеты и размещение главного окна.
     *
     * Метод нужен, чтобы собрать дерево рабочей папки, область вкладок и строку состояния.
     */
    void setupUi();

    /**
     * @brief Создаёт меню и действия главного окна.
     *
     * Метод нужен, чтобы пользователь мог открывать рабочую папку, документы и редактор.
     */
    void setupActions();

    /**
     * @brief Открывает рабочую папку без пользовательского диалога.
     * @param workspacePath Путь к корневой папке проекта, который нужно открыть.
     * @return true, если рабочая папка успешно открыта.
     */
    bool openWorkspaceAtPath(const QString& workspacePath);

    /**
     * @brief Открывает документ из истории последних файлов.
     * @param workspacePath Абсолютный путь к рабочей папке, где находится документ.
     * @param documentPath Абсолютный путь к Markdown-документу, который нужно открыть.
     */
    void openRecentDocument(const QString& workspacePath, const QString& documentPath);

    /**
     * @brief Добавляет Markdown-файл в дерево навигации по относительному пути.
     * @param relativePath Относительный путь документа внутри рабочей папки.
     * @param absolutePath Абсолютный путь к документу на диске.
     */
    void addFileToWorkspaceTree(const QString& relativePath, const QString& absolutePath);

    /**
     * @brief Загружает историю последних Markdown-документов из QSettings.
     *
     * Метод нужен, чтобы восстановить меню последних файлов между запусками приложения.
     */
    void loadRecentDocuments();

    /**
     * @brief Сохраняет историю последних Markdown-документов в QSettings.
     *
     * Метод нужен, чтобы пользователь мог вернуть недавние файлы после перезапуска приложения.
     */
    void saveRecentDocuments() const;

    /**
     * @brief Перестраивает меню истории последних Markdown-документов.
     *
     * Метод создаёт пункты меню по текущему содержимому истории.
     */
    void updateRecentDocumentsMenu();

    /**
     * @brief Добавляет документ в начало истории последних файлов.
     * @param workspacePath Абсолютный путь к рабочей папке, где находится документ.
     * @param documentPath Абсолютный путь к Markdown-документу.
     */
    void addRecentDocument(const QString& workspacePath, const QString& documentPath);

    /**
     * @brief Применяет настройки внешнего вида ко всем открытым вкладкам просмотра.
     *
     * Метод нужен, чтобы новая тема просмотра сразу распространялась на уже открытые документы.
     */
    void applyViewerSettingsToOpenViews();

    /**
     * @brief Возвращает уже открытое окно редактора для указанного документа.
     * @param documentPath Абсолютный путь к Markdown-файлу.
     * @return Указатель на окно редактора или `nullptr`, если окно ещё не открыто.
     */
    EditorWindow* findEditorWindow(const QString& documentPath) const;

    /**
     * @brief Открывает стартовый документ рабочей папки после загрузки проекта.
     *
     * Предпочтение отдаётся `docs/index.md`, а если его нет — первому найденному Markdown-файлу.
     */
    void openPreferredStartupDocument();

    /**
     * @brief Проверяет, нужно ли подтверждение на очистку существующего каталога вывода.
     * @param outputDirectoryPath Абсолютный путь к выбранному пользователем каталогу вывода.
     * @return true, если генерацию можно продолжать и каталог разрешено пересобрать.
     */
    bool confirmReplacingOutputDirectory(const QString& outputDirectoryPath);

    /**
     * @brief Показывает сообщение о том, что генератор для выбранного языка ещё не реализован.
     * @param languageName Имя языка, для которого пользователь попытался запустить генерацию.
     */
    void showUnavailableCodeMapGenerator(const QString& languageName);

    WorkspaceManager* m_workspaceManager = nullptr;                 ///< Менеджер корня проекта и списка Markdown-файлов.
    DocumentTabWidget* m_documentTabs = nullptr;                   ///< Центральный виджет вкладок открытых документов.
    DocumentController* m_documentController = nullptr;            ///< Контроллер открытия документов и ссылок.
    QDockWidget* m_workspaceDock = nullptr;                        ///< Док-панель с деревом Markdown-файлов проекта.
    QTreeWidget* m_workspaceTree = nullptr;                        ///< Дерево навигации по документам рабочей папки.
    QAction* m_openWorkspaceAction = nullptr;                      ///< Действие меню для открытия рабочей папки.
    QAction* m_openDocumentAction = nullptr;                       ///< Действие меню для открытия Markdown-файла.
    QAction* m_editDocumentAction = nullptr;                       ///< Действие меню для открытия текущего документа в редакторе.
    QAction* m_reloadDocumentAction = nullptr;                     ///< Действие меню для обновления текущего документа.
    QAction* m_viewerAppearanceAction = nullptr;                   ///< Действие меню для настройки внешнего вида окна просмотра Markdown.
    QAction* m_generateCppCodeMapAction = nullptr;                 ///< Действие меню для генерации Markdown-карты из C/C++ исходников.
    QAction* m_generatePythonCodeMapAction = nullptr;              ///< Действие меню для будущей генерации Markdown-карты из Python исходников.
    QAction* m_generateJavaCodeMapAction = nullptr;                ///< Действие меню для будущей генерации Markdown-карты из Java исходников.
    QMenu* m_recentDocumentsMenu = nullptr;                        ///< Подменю истории последних Markdown-документов.
    QHash<QString, EditorWindow*> m_openEditors;                   ///< Карта уже открытых окон редактора по пути документа.
    QList<RecentDocumentEntry> m_recentDocuments;                  ///< История последних открытых Markdown-документов.
    ViewerSettings m_viewerSettings;                               ///< Текущие глобальные настройки внешнего вида вкладок просмотра.
};
