#include "documentcontroller.h"

#include "documenttabwidget.h"
#include "markdownview.h"
#include "workspacemanager.h"

#include <QDir>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QUrl>

/**
 * @brief Создаёт контроллер документов.
 * @param workspaceManager Менеджер рабочей папки для поиска файлов и разрешения ссылок.
 * @param documentTabs Виджет вкладок, куда нужно открывать документы.
 * @param parent Родительский объект Qt для управления временем жизни контроллера.
 */
DocumentController::DocumentController(WorkspaceManager* workspaceManager, DocumentTabWidget* documentTabs, QObject* parent)
    : QObject(parent)
    , m_workspaceManager(workspaceManager)
    , m_documentTabs(documentTabs)
    , m_fileWatcher(new QFileSystemWatcher(this))
{
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &DocumentController::handleFileChanged);
    connect(m_documentTabs, &DocumentTabWidget::currentDocumentChanged, this, &DocumentController::currentDocumentChanged);
    connect(m_documentTabs, &DocumentTabWidget::documentClosed, this, &DocumentController::handleDocumentClosed);
}

/**
 * @brief Открывает документ во вкладке и, при необходимости, переходит к якорю.
 * @param documentPath Абсолютный или относительный путь к Markdown-файлу.
 * @param anchor Якорь внутри документа без символа '#', к которому нужно перейти.
 * @return true, если документ удалось открыть или активировать.
 */
bool DocumentController::openDocument(const QString& documentPath, const QString& anchor)
{
    const QString normalizedDocumentPath = QFileInfo(documentPath).isAbsolute()
        ? QFileInfo(documentPath).absoluteFilePath()
        : QDir(m_workspaceManager->workspaceRootPath()).absoluteFilePath(documentPath);

    if (!m_workspaceManager->containsFile(normalizedDocumentPath)) {
        emit errorOccurred(QStringLiteral("Документ находится вне рабочей папки или не является Markdown-файлом."));
        return false;
    }

    const int existingTabIndex = m_documentTabs->findTabByPath(normalizedDocumentPath);
    if (existingTabIndex >= 0) {
        m_documentTabs->setCurrentIndex(existingTabIndex);

        MarkdownView* existingView = m_documentTabs->currentView();
        if (existingView != nullptr) {
            existingView->jumpToAnchor(anchor);
        }

        emit documentOpened(normalizedDocumentPath);
        emit statusMessage(QStringLiteral("Документ уже открыт: %1").arg(QFileInfo(normalizedDocumentPath).fileName()));
        return true;
    }

    MarkdownView* view = new MarkdownView(m_documentTabs);
    QString errorText;
    if (!view->loadMarkdownFromFile(normalizedDocumentPath, &errorText)) {
        delete view;
        emit errorOccurred(QStringLiteral("Не удалось открыть документ: %1").arg(errorText));
        return false;
    }

    attachView(view);

    const QString tabTitle = QFileInfo(normalizedDocumentPath).fileName();
    m_documentTabs->addOrActivateTab(view, tabTitle);
    view->jumpToAnchor(anchor);

    emit documentOpened(normalizedDocumentPath);
    emit statusMessage(QStringLiteral("Открыт документ: %1").arg(tabTitle));
    return true;
}

/**
 * @brief Перечитывает уже открытый документ после изменения файла на диске.
 * @param documentPath Абсолютный путь к Markdown-файлу, который нужно обновить.
 * @return true, если документ был открыт и успешно перечитан.
 */
bool DocumentController::reloadDocument(const QString& documentPath)
{
    const int tabIndex = m_documentTabs->findTabByPath(QFileInfo(documentPath).absoluteFilePath());
    if (tabIndex < 0) {
        return false;
    }

    MarkdownView* view = qobject_cast<MarkdownView*>(m_documentTabs->widget(tabIndex));
    if (view == nullptr) {
        return false;
    }

    QString errorText;
    if (!view->loadMarkdownFromFile(view->documentPath(), &errorText)) {
        emit errorOccurred(QStringLiteral("Не удалось обновить документ: %1").arg(errorText));
        return false;
    }

    emit statusMessage(QStringLiteral("Документ обновлён: %1").arg(QFileInfo(view->documentPath()).fileName()));
    return true;
}

/**
 * @brief Возвращает путь к текущему активному документу.
 * @return Абсолютный путь к документу активной вкладки или пустая строка.
 */
QString DocumentController::currentDocumentPath() const
{
    return m_documentTabs->currentDocumentPath();
}

/**
 * @brief Обрабатывает изменение Markdown-файла на диске.
 * @param changedFilePath Абсолютный путь к файлу, который изменился.
 */
void DocumentController::handleFileChanged(const QString& changedFilePath)
{
    if (QFileInfo::exists(changedFilePath) && !m_fileWatcher->files().contains(changedFilePath)) {
        m_fileWatcher->addPath(changedFilePath);
    }

    reloadDocument(changedFilePath);
}

/**
 * @brief Снимает наблюдение за файлом после закрытия вкладки.
 * @param documentPath Абсолютный путь к документу, закрытому пользователем.
 */
void DocumentController::handleDocumentClosed(const QString& documentPath)
{
    if (documentPath.isEmpty()) {
        return;
    }

    if (m_fileWatcher->files().contains(documentPath)) {
        m_fileWatcher->removePath(documentPath);
    }
}

/**
 * @brief Открывает ссылку, на которую пользователь нажал внутри Markdown.
 * @param baseFilePath Абсолютный путь к исходному документу, содержащему ссылку.
 * @param targetUrl Ссылка из Markdown в виде URL.
 */
void DocumentController::openLinkFromDocument(const QString& baseFilePath, const QUrl& targetUrl)
{
    const QString fullyDecodedTarget = targetUrl.toString(QUrl::FullyDecoded);
    const ResolvedLink resolvedLink = m_workspaceManager->resolveLink(baseFilePath, fullyDecodedTarget);

    if (!resolvedLink.isValid) {
        emit errorOccurred(QStringLiteral("Ссылка не указывает на Markdown-файл внутри рабочей папки: %1").arg(fullyDecodedTarget));
        return;
    }

    if (resolvedLink.isExternal) {
        QDesktopServices::openUrl(QUrl(resolvedLink.externalTarget));
        emit statusMessage(QStringLiteral("Открыта внешняя ссылка: %1").arg(resolvedLink.externalTarget));
        return;
    }

    openDocument(resolvedLink.filePath, resolvedLink.anchor);
}

/**
 * @brief Подключает обработчики событий для новой вкладки документа.
 * @param view Виджет Markdown, для которого нужно настроить сигналы и слежение за файлом.
 */
void DocumentController::attachView(MarkdownView* view)
{
    const QString watchedFilePath = view->documentPath(); ///< Путь документа сохраняется отдельно для безопасной очистки наблюдателя.

    connect(view, &QTextBrowser::anchorClicked, this,
        [this, view](const QUrl& targetUrl) {
            openLinkFromDocument(view->documentPath(), targetUrl);
        });

    connect(view, &QObject::destroyed, this,
        [this, watchedFilePath]() {
            if (!watchedFilePath.isEmpty() && m_fileWatcher->files().contains(watchedFilePath)) {
                m_fileWatcher->removePath(watchedFilePath);
            }
        });

    if (!watchedFilePath.isEmpty() && !m_fileWatcher->files().contains(watchedFilePath)) {
        m_fileWatcher->addPath(watchedFilePath);
    }
}
