#include "mainwindow.h"

#include "codemapgenerator.h"
#include "documentcontroller.h"
#include "documenttabwidget.h"
#include "editorwindow.h"
#include "markdownview.h"
#include "viewersettingsdialog.h"
#include "workspacemanager.h"

#include <algorithm>
#include <utility>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QStatusBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace
{
constexpr int kDocumentPathRole = Qt::UserRole + 1; ///< Роль дерева для хранения абсолютного пути Markdown-файла.
constexpr int kMaxRecentDocuments = 10; ///< Максимальное количество записей в истории последних Markdown-документов.
constexpr const char* kRecentDocumentsGroup = "RecentDocuments"; ///< Имя группы QSettings для хранения истории последних Markdown-документов.
}

/**
 * @brief Создаёт главное окно приложения.
 * @param parent Родительский виджет Qt для управления временем жизни окна.
 */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_workspaceManager(new WorkspaceManager())
    , m_viewerSettings(ViewerSettingsStore::load())
{
    setupUi();
    setupActions();
    loadRecentDocuments();
    updateRecentDocumentsMenu();

    m_documentController = new DocumentController(m_workspaceManager, m_documentTabs, this);
    connect(m_documentController, &DocumentController::documentOpened, this, &MainWindow::handleDocumentOpened);
    connect(m_documentController, &DocumentController::statusMessage, this, &MainWindow::showStatusMessage);
    connect(m_documentController, &DocumentController::errorOccurred, this, &MainWindow::showErrorMessage);
    connect(m_documentController, &DocumentController::currentDocumentChanged, this, &MainWindow::updateCurrentDocumentTitle);

    const QString defaultWorkspacePath = QDir::currentPath();
    openWorkspaceAtPath(defaultWorkspacePath);
    resize(1280, 800);
}

/**
 * @brief Закрывает дочерние окна редактора вместе с главным окном.
 * @param event Событие закрытия главного окна.
 */
void MainWindow::closeEvent(QCloseEvent* event)
{
    const QList<EditorWindow*> openEditors = m_openEditors.values(); ///< Копия списка редакторов для безопасного обхода при закрытии.
    for (EditorWindow* editorWindow : openEditors) {
        if (editorWindow != nullptr) {
            editorWindow->close();
            if (editorWindow->isVisible()) {
                event->ignore();
                return;
            }
        }
    }

    event->accept();
}

/**
 * @brief Показывает диалог выбора рабочей папки и открывает выбранный проект.
 */
void MainWindow::chooseWorkspaceFolder()
{
    const QString currentWorkspace = m_workspaceManager->workspaceRootPath().isEmpty()
        ? QDir::currentPath()
        : m_workspaceManager->workspaceRootPath();

    const QString selectedPath = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Открыть рабочую папку"),
        currentWorkspace);

    if (!selectedPath.isEmpty()) {
        openWorkspaceAtPath(selectedPath);
    }
}

/**
 * @brief Показывает диалог выбора Markdown-файла и открывает его.
 */
void MainWindow::chooseMarkdownFile()
{
    const QString baseDirectory = m_workspaceManager->workspaceRootPath().isEmpty()
        ? QDir::currentPath()
        : m_workspaceManager->workspaceRootPath();

    const QString selectedFilePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Открыть Markdown-файл"),
        baseDirectory,
        QStringLiteral("Markdown (*.md *.markdown)"));

    if (selectedFilePath.isEmpty()) {
        return;
    }

    if (!m_workspaceManager->containsFile(selectedFilePath)) {
        openWorkspaceAtPath(QFileInfo(selectedFilePath).absolutePath());
    }

    m_documentController->openDocument(selectedFilePath);
}

/**
 * @brief Перестраивает дерево Markdown-файлов после смены рабочей папки.
 */
void MainWindow::rebuildWorkspaceTree()
{
    m_workspaceTree->clear();

    const QString workspaceRootPath = m_workspaceManager->workspaceRootPath();
    if (workspaceRootPath.isEmpty()) {
        return;
    }

    QTreeWidgetItem* rootItem = new QTreeWidgetItem(m_workspaceTree);
    rootItem->setText(0, QFileInfo(workspaceRootPath).fileName());
    rootItem->setExpanded(true);

    const QStringList markdownFiles = m_workspaceManager->markdownFiles();
    for (const QString& markdownFilePath : markdownFiles) {
        addFileToWorkspaceTree(m_workspaceManager->workspaceRelativePath(markdownFilePath), markdownFilePath);
    }

    m_workspaceTree->sortItems(0, Qt::AscendingOrder);
}

/**
 * @brief Открывает документ из выбранного элемента дерева рабочей папки.
 * @param item Элемент дерева, по которому кликнул пользователь.
 * @param column Номер колонки дерева, где произошёл клик.
 */
void MainWindow::openDocumentFromTree(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    const QString documentPath = item->data(0, kDocumentPathRole).toString();
    if (!documentPath.isEmpty()) {
        m_documentController->openDocument(documentPath);
    }
}

/**
 * @brief Открывает текущий документ в отдельном окне редактора.
 */
void MainWindow::openCurrentDocumentInEditor()
{
    const QString currentDocumentPath = m_documentController->currentDocumentPath();
    if (currentDocumentPath.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Нет документа"), QStringLiteral("Сначала откройте Markdown-документ."));
        return;
    }

    if (EditorWindow* existingEditor = findEditorWindow(currentDocumentPath); existingEditor != nullptr) {
        existingEditor->show();
        existingEditor->raise();
        existingEditor->activateWindow();
        return;
    }

    EditorWindow* editorWindow = new EditorWindow(currentDocumentPath, this);
    connect(editorWindow, &EditorWindow::documentSaved, m_documentController, &DocumentController::reloadDocument);
    connect(editorWindow, &EditorWindow::statusMessage, this, &MainWindow::showStatusMessage);
    connect(editorWindow, &QObject::destroyed, this,
        [this, currentDocumentPath]() {
            m_openEditors.remove(currentDocumentPath);
        });

    m_openEditors.insert(currentDocumentPath, editorWindow);
    editorWindow->show();
}

/**
 * @brief Показывает сообщение в строке состояния приложения.
 * @param message Текст статуса, который нужно показать пользователю.
 */
void MainWindow::showStatusMessage(const QString& message)
{
    statusBar()->showMessage(message, 5000);
}

/**
 * @brief Показывает пользователю ошибку через диалоговое окно.
 * @param errorText Текст ошибки для показа.
 */
void MainWindow::showErrorMessage(const QString& errorText)
{
    QMessageBox::warning(this, QStringLiteral("Ошибка"), errorText);
    statusBar()->showMessage(errorText, 5000);
}

/**
 * @brief Обновляет заголовок главного окна после смены текущего документа.
 * @param documentPath Абсолютный путь к текущему документу.
 */
void MainWindow::updateCurrentDocumentTitle(const QString& documentPath)
{
    const QString workspaceName = QFileInfo(m_workspaceManager->workspaceRootPath()).fileName();
    if (documentPath.isEmpty()) {
        setWindowTitle(QStringLiteral("MarkDVE - %1").arg(workspaceName));
        return;
    }

    setWindowTitle(QStringLiteral("MarkDVE - %1 - %2").arg(QFileInfo(documentPath).fileName(), workspaceName));
}

/**
 * @brief Добавляет успешно открытый документ в историю последних файлов.
 * @param documentPath Абсолютный путь к документу, который был открыт или активирован.
 */
void MainWindow::handleDocumentOpened(const QString& documentPath)
{
    if (!documentPath.isEmpty()) {
        addRecentDocument(m_workspaceManager->workspaceRootPath(), documentPath);
        applyViewerSettingsToOpenViews();
    }
}

/**
 * @brief Открывает диалог настройки внешнего вида окна просмотра Markdown.
 */
void MainWindow::openViewerAppearanceSettings()
{
    ViewerSettingsDialog settingsDialog(m_viewerSettings, this);
    if (settingsDialog.exec() != QDialog::Accepted) {
        return;
    }

    m_viewerSettings = settingsDialog.viewerSettings();
    ViewerSettingsStore::save(m_viewerSettings);
    applyViewerSettingsToOpenViews();
    showStatusMessage(QStringLiteral("Настройки просмотра обновлены."));
}

/**
 * @brief Запускает мастер генерации Markdown-карты исходного кода.
 *
 * Метод последовательно запрашивает каталог исходников и каталог вывода,
 * а затем создаёт полную иерархию Markdown-файлов с перекрёстными ссылками.
 */
void MainWindow::generateCodeMap()
{
    const QString sourceDirectoryPath = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Выберите папку с исходным кодом"),
        m_workspaceManager->workspaceRootPath().isEmpty() ? QDir::currentPath() : m_workspaceManager->workspaceRootPath());

    if (sourceDirectoryPath.isEmpty()) {
        return;
    }

    const QString outputDirectoryPath = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Выберите папку для готовой Markdown-карты"),
        QFileInfo(sourceDirectoryPath).absolutePath());

    if (outputDirectoryPath.isEmpty()) {
        return;
    }

    if (!confirmReplacingOutputDirectory(outputDirectoryPath)) {
        return;
    }

    QProgressDialog progressDialog(QStringLiteral("Генерируется Markdown-карта кода..."), QString(), 0, 0, this);
    progressDialog.setWindowTitle(QStringLiteral("Генерация карты кода"));
    progressDialog.setCancelButton(nullptr);
    progressDialog.setMinimumDuration(0);
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.show();
    QApplication::processEvents();

    CodeMapGenerationResult generationResult;
    QString errorText;
    CodeMapGenerator codeMapGenerator; ///< Локальный генератор Markdown-карты, используемый только в рамках текущего запуска команды.
    if (!codeMapGenerator.generate(sourceDirectoryPath, outputDirectoryPath, &generationResult, &errorText)) {
        progressDialog.close();
        showErrorMessage(errorText);
        return;
    }

    progressDialog.close();

    QString successMessage = QStringLiteral(
        "Карта кода создана: %1\nКлассовых модулей: %2\nФайловых модулей: %3")
        .arg(generationResult.indexFilePath)
        .arg(generationResult.classModuleCount)
        .arg(generationResult.fileModuleCount);

    if (!generationResult.warnings.isEmpty()) {
        successMessage += QStringLiteral("\nПредупреждений: %1").arg(generationResult.warnings.size());
    }

    showStatusMessage(QStringLiteral("Карта кода создана: %1").arg(generationResult.indexFilePath));

    const QMessageBox::StandardButton openResultButton = QMessageBox::question(
        this,
        QStringLiteral("Генерация завершена"),
        successMessage + QStringLiteral("\n\nОткрыть созданный index.md в приложении?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (openResultButton == QMessageBox::Yes) {
        if (openWorkspaceAtPath(outputDirectoryPath)) {
            m_documentController->openDocument(generationResult.indexFilePath);
        }
    }
}

/**
 * @brief Создаёт виджеты и размещение главного окна.
 *
 * Метод нужен, чтобы собрать дерево рабочей папки, область вкладок и строку состояния.
 */
void MainWindow::setupUi()
{
    m_documentTabs = new DocumentTabWidget(this);
    setCentralWidget(m_documentTabs);

    m_workspaceDock = new QDockWidget(QStringLiteral("Документы проекта"), this);
    m_workspaceTree = new QTreeWidget(m_workspaceDock);
    m_workspaceTree->setHeaderHidden(true);
    m_workspaceDock->setWidget(m_workspaceTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_workspaceDock);

    connect(m_workspaceTree, &QTreeWidget::itemActivated, this, &MainWindow::openDocumentFromTree);

    statusBar()->showMessage(QStringLiteral("Готово"));
}

/**
 * @brief Создаёт меню и действия главного окна.
 *
 * Метод нужен, чтобы пользователь мог открывать рабочую папку, документы и редактор.
 */
void MainWindow::setupActions()
{
    m_openWorkspaceAction = new QAction(QStringLiteral("Открыть папку проекта"), this);
    m_openWorkspaceAction->setShortcut(QKeySequence::Open);
    connect(m_openWorkspaceAction, &QAction::triggered, this, &MainWindow::chooseWorkspaceFolder);

    m_openDocumentAction = new QAction(QStringLiteral("Открыть документ"), this);
    m_openDocumentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(m_openDocumentAction, &QAction::triggered, this, &MainWindow::chooseMarkdownFile);

    m_editDocumentAction = new QAction(QStringLiteral("Редактировать текущий документ"), this);
    m_editDocumentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_editDocumentAction, &QAction::triggered, this, &MainWindow::openCurrentDocumentInEditor);

    m_reloadDocumentAction = new QAction(QStringLiteral("Обновить текущий документ"), this);
    m_reloadDocumentAction->setShortcut(QKeySequence::Refresh);
    connect(m_reloadDocumentAction, &QAction::triggered, this,
        [this]() {
            const QString currentDocumentPath = m_documentController != nullptr
                ? m_documentController->currentDocumentPath()
                : QString();
            if (!currentDocumentPath.isEmpty()) {
                m_documentController->reloadDocument(currentDocumentPath);
            }
        });

    m_viewerAppearanceAction = new QAction(QStringLiteral("Внешний вид просмотра..."), this);
    m_viewerAppearanceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_V));
    connect(m_viewerAppearanceAction, &QAction::triggered, this, &MainWindow::openViewerAppearanceSettings);

    m_generateCodeMapAction = new QAction(QStringLiteral("Сгенерировать карту кода..."), this);
    m_generateCodeMapAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_G));
    connect(m_generateCodeMapAction, &QAction::triggered, this, &MainWindow::generateCodeMap);

    QAction* exitAction = new QAction(QStringLiteral("Выход"), this); ///< Действие меню для штатного завершения приложения.
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("Файл"));
    fileMenu->addAction(m_openWorkspaceAction);
    fileMenu->addAction(m_openDocumentAction);
    fileMenu->addAction(m_reloadDocumentAction);
    m_recentDocumentsMenu = fileMenu->addMenu(QStringLiteral("Последние документы"));
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    QMenu* editMenu = menuBar()->addMenu(QStringLiteral("Правка"));
    editMenu->addAction(m_editDocumentAction);

    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("Просмотр"));
    viewMenu->addAction(m_viewerAppearanceAction);

    QMenu* toolsMenu = menuBar()->addMenu(QStringLiteral("Инструменты"));
    toolsMenu->addAction(m_generateCodeMapAction);
}

/**
 * @brief Открывает рабочую папку без пользовательского диалога.
 * @param workspacePath Путь к корневой папке проекта, который нужно открыть.
 * @return true, если рабочая папка успешно открыта.
 */
bool MainWindow::openWorkspaceAtPath(const QString& workspacePath)
{
    WorkspaceManager nextWorkspaceState; ///< Временное состояние новой рабочей папки для безопасной проверки до переключения.
    const QList<EditorWindow*> openEditors = m_openEditors.values(); ///< Копия списка редакторов для безопасного закрытия при смене проекта.
    QString errorText;
    if (!nextWorkspaceState.openWorkspace(workspacePath, &errorText)) {
        showErrorMessage(errorText);
        return false;
    }

    for (EditorWindow* editorWindow : openEditors) {
        if (editorWindow != nullptr) {
            editorWindow->close();
            if (editorWindow->isVisible()) {
                return false;
            }
        }
    }

    *m_workspaceManager = nextWorkspaceState;

    while (m_documentTabs->count() > 0) {
        QWidget* tabWidget = m_documentTabs->widget(0);
        m_documentTabs->removeTab(0);
        delete tabWidget;
    }

    rebuildWorkspaceTree();
    openPreferredStartupDocument();
    showStatusMessage(QStringLiteral("Открыта рабочая папка: %1").arg(workspacePath));
    return true;
}

/**
 * @brief Открывает документ из истории последних файлов.
 * @param workspacePath Абсолютный путь к рабочей папке, где находится документ.
 * @param documentPath Абсолютный путь к Markdown-документу, который нужно открыть.
 */
void MainWindow::openRecentDocument(const QString& workspacePath, const QString& documentPath)
{
    const QFileInfo documentInfo(documentPath);
    const QFileInfo workspaceInfo(workspacePath);

    if (!documentInfo.exists()) {
        showErrorMessage(QStringLiteral("Недавний документ больше не существует: %1").arg(documentPath));
        m_recentDocuments.erase(
            std::remove_if(m_recentDocuments.begin(), m_recentDocuments.end(),
                [&documentPath](const RecentDocumentEntry& recentEntry) {
                    return recentEntry.documentPath == documentPath;
                }),
            m_recentDocuments.end());
        saveRecentDocuments();
        updateRecentDocumentsMenu();
        return;
    }

    const QString targetWorkspacePath = workspaceInfo.exists() && workspaceInfo.isDir()
        ? workspacePath
        : documentInfo.absolutePath();

    if (!openWorkspaceAtPath(targetWorkspacePath)) {
        return;
    }

    m_documentController->openDocument(documentPath);
}

/**
 * @brief Добавляет Markdown-файл в дерево навигации по относительному пути.
 * @param relativePath Относительный путь документа внутри рабочей папки.
 * @param absolutePath Абсолютный путь к документу на диске.
 */
void MainWindow::addFileToWorkspaceTree(const QString& relativePath, const QString& absolutePath)
{
    if (m_workspaceTree->topLevelItemCount() == 0) {
        return;
    }

    QTreeWidgetItem* parentItem = m_workspaceTree->topLevelItem(0); ///< Текущий родительский узел при построении иерархии путей.
    const QStringList pathSegments = relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (int segmentIndex = 0; segmentIndex < pathSegments.size(); ++segmentIndex) {
        const QString& segmentName = pathSegments.at(segmentIndex);
        const bool isLeafFile = segmentIndex == pathSegments.size() - 1;

        QTreeWidgetItem* nextItem = nullptr; ///< Узел дерева для текущего сегмента пути.
        for (int childIndex = 0; childIndex < parentItem->childCount(); ++childIndex) {
            QTreeWidgetItem* childItem = parentItem->child(childIndex);
            if (childItem->text(0) == segmentName) {
                nextItem = childItem;
                break;
            }
        }

        if (nextItem == nullptr) {
            nextItem = new QTreeWidgetItem(parentItem);
            nextItem->setText(0, segmentName);
            nextItem->setExpanded(true);
        }

        if (isLeafFile) {
            nextItem->setData(0, kDocumentPathRole, absolutePath);
            nextItem->setToolTip(0, relativePath);
        }
        parentItem = nextItem;
    }
}

/**
 * @brief Загружает историю последних Markdown-документов из QSettings.
 *
 * Метод нужен, чтобы восстановить меню последних файлов между запусками приложения.
 */
void MainWindow::loadRecentDocuments()
{
    m_recentDocuments.clear();

    QSettings applicationSettings;
    const int recentEntryCount = applicationSettings.beginReadArray(QString::fromUtf8(kRecentDocumentsGroup));
    for (int entryIndex = 0; entryIndex < recentEntryCount; ++entryIndex) {
        applicationSettings.setArrayIndex(entryIndex);

        RecentDocumentEntry recentEntry; ///< Запись истории последних Markdown-документов, загруженная из QSettings.
        recentEntry.workspacePath = applicationSettings.value(QStringLiteral("workspacePath")).toString();
        recentEntry.documentPath = applicationSettings.value(QStringLiteral("documentPath")).toString();

        if (!recentEntry.documentPath.isEmpty()) {
            m_recentDocuments.append(recentEntry);
        }
    }
    applicationSettings.endArray();
}

/**
 * @brief Сохраняет историю последних Markdown-документов в QSettings.
 *
 * Метод нужен, чтобы пользователь мог вернуть недавние файлы после перезапуска приложения.
 */
void MainWindow::saveRecentDocuments() const
{
    QSettings applicationSettings;
    applicationSettings.beginWriteArray(QString::fromUtf8(kRecentDocumentsGroup));

    for (int entryIndex = 0; entryIndex < m_recentDocuments.size(); ++entryIndex) {
        const RecentDocumentEntry& recentEntry = m_recentDocuments.at(entryIndex);
        applicationSettings.setArrayIndex(entryIndex);
        applicationSettings.setValue(QStringLiteral("workspacePath"), recentEntry.workspacePath);
        applicationSettings.setValue(QStringLiteral("documentPath"), recentEntry.documentPath);
    }

    applicationSettings.endArray();
}

/**
 * @brief Перестраивает меню истории последних Markdown-документов.
 *
 * Метод создаёт пункты меню по текущему содержимому истории.
 */
void MainWindow::updateRecentDocumentsMenu()
{
    if (m_recentDocumentsMenu == nullptr) {
        return;
    }

    m_recentDocumentsMenu->clear();

    if (m_recentDocuments.isEmpty()) {
        QAction* emptyAction = m_recentDocumentsMenu->addAction(QStringLiteral("История пуста"));
        emptyAction->setEnabled(false);
        return;
    }

    for (const RecentDocumentEntry& recentEntry : std::as_const(m_recentDocuments)) {
        const QString fileName = QFileInfo(recentEntry.documentPath).fileName(); ///< Короткое имя Markdown-файла для пользовательского меню.
        const QString workspaceName = recentEntry.workspacePath.isEmpty()
            ? QStringLiteral("без рабочей папки")
            : QFileInfo(recentEntry.workspacePath).fileName();
        QAction* recentAction = m_recentDocumentsMenu->addAction(QStringLiteral("%1 — %2").arg(fileName, workspaceName));
        recentAction->setToolTip(recentEntry.documentPath);
        connect(recentAction, &QAction::triggered, this,
            [this, recentEntry]() {
                openRecentDocument(recentEntry.workspacePath, recentEntry.documentPath);
            });
    }
}

/**
 * @brief Добавляет документ в начало истории последних файлов.
 * @param workspacePath Абсолютный путь к рабочей папке, где находится документ.
 * @param documentPath Абсолютный путь к Markdown-документу.
 */
void MainWindow::addRecentDocument(const QString& workspacePath, const QString& documentPath)
{
    if (documentPath.isEmpty()) {
        return;
    }

    m_recentDocuments.erase(
        std::remove_if(m_recentDocuments.begin(), m_recentDocuments.end(),
            [&documentPath](const RecentDocumentEntry& recentEntry) {
                return recentEntry.documentPath == documentPath;
            }),
        m_recentDocuments.end());

    RecentDocumentEntry recentEntry; ///< Новая или обновлённая запись истории последних Markdown-документов.
    recentEntry.workspacePath = workspacePath;
    recentEntry.documentPath = documentPath;
    m_recentDocuments.prepend(recentEntry);

    while (m_recentDocuments.size() > kMaxRecentDocuments) {
        m_recentDocuments.removeLast();
    }

    saveRecentDocuments();
    updateRecentDocumentsMenu();
}

/**
 * @brief Применяет настройки внешнего вида ко всем открытым вкладкам просмотра.
 *
 * Метод нужен, чтобы новая тема просмотра сразу распространялась на уже открытые документы.
 */
void MainWindow::applyViewerSettingsToOpenViews()
{
    for (int tabIndex = 0; tabIndex < m_documentTabs->count(); ++tabIndex) {
        MarkdownView* markdownView = qobject_cast<MarkdownView*>(m_documentTabs->widget(tabIndex));
        if (markdownView != nullptr) {
            markdownView->applyViewerSettings(m_viewerSettings);
        }
    }
}

/**
 * @brief Возвращает уже открытое окно редактора для указанного документа.
 * @param documentPath Абсолютный путь к Markdown-файлу.
 * @return Указатель на окно редактора или `nullptr`, если окно ещё не открыто.
 */
EditorWindow* MainWindow::findEditorWindow(const QString& documentPath) const
{
    return m_openEditors.value(documentPath, nullptr);
}

/**
 * @brief Открывает стартовый документ рабочей папки после загрузки проекта.
 *
 * Предпочтение отдаётся `docs/Project_Map/map_main.md`, затем `docs/index.md`,
 * затем корневому `index.md`, а если их нет — первому найденному Markdown-файлу.
 */
void MainWindow::openPreferredStartupDocument()
{
    const QString workspaceRootPath = m_workspaceManager->workspaceRootPath();
    if (workspaceRootPath.isEmpty()) {
        return;
    }

    const QString mapMainDocumentPath = QDir(workspaceRootPath).absoluteFilePath(QStringLiteral("docs/Project_Map/map_main.md"));
    if (QFileInfo::exists(mapMainDocumentPath) && m_workspaceManager->containsFile(mapMainDocumentPath)) {
        m_documentController->openDocument(mapMainDocumentPath);
        return;
    }

    const QString indexDocumentPath = QDir(workspaceRootPath).absoluteFilePath(QStringLiteral("docs/index.md"));
    if (QFileInfo::exists(indexDocumentPath) && m_workspaceManager->containsFile(indexDocumentPath)) {
        m_documentController->openDocument(indexDocumentPath);
        return;
    }

    const QString rootIndexDocumentPath = QDir(workspaceRootPath).absoluteFilePath(QStringLiteral("index.md"));
    if (QFileInfo::exists(rootIndexDocumentPath) && m_workspaceManager->containsFile(rootIndexDocumentPath)) {
        m_documentController->openDocument(rootIndexDocumentPath);
        return;
    }

    const QStringList markdownFiles = m_workspaceManager->markdownFiles();
    if (!markdownFiles.isEmpty()) {
        m_documentController->openDocument(markdownFiles.first());
    }
}

/**
 * @brief Проверяет, нужно ли подтверждение на очистку существующего каталога вывода.
 * @param outputDirectoryPath Абсолютный путь к выбранному пользователем каталогу вывода.
 * @return true, если генерацию можно продолжать и каталог разрешено пересобрать.
 */
bool MainWindow::confirmReplacingOutputDirectory(const QString& outputDirectoryPath)
{
    const QDir outputDirectory(outputDirectoryPath);
    if (!outputDirectory.exists()) {
        return true;
    }

    const QFileInfoList directoryEntries = outputDirectory.entryInfoList(
        QDir::NoDotAndDotDot | QDir::AllEntries,
        QDir::Name | QDir::DirsFirst);

    if (directoryEntries.isEmpty()) {
        return true;
    }

    const QMessageBox::StandardButton confirmButton = QMessageBox::warning(
        this,
        QStringLiteral("Папка вывода не пуста"),
        QStringLiteral("Каталог вывода уже содержит файлы.\n"
                       "Старая автогенерированная структура будет удалена и создана заново.\n\n"
                       "Продолжить?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    return confirmButton == QMessageBox::Yes;
}
