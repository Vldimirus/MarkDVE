#include "documenttabwidget.h"

#include "markdownview.h"

/**
 * @brief Создаёт виджет вкладок документов.
 * @param parent Родительский виджет Qt для управления временем жизни объекта.
 */
DocumentTabWidget::DocumentTabWidget(QWidget* parent)
    : QTabWidget(parent)
{
    setTabsClosable(true);
    setMovable(true);

    connect(this, &QTabWidget::tabCloseRequested, this, &DocumentTabWidget::closeTabByIndex);
    connect(this, &QTabWidget::currentChanged, this, &DocumentTabWidget::emitCurrentDocumentPath);
}

/**
 * @brief Добавляет новую вкладку с документом или делает активной уже открытую.
 * @param view Виджет отображения Markdown, который нужно показать во вкладке.
 * @param title Заголовок вкладки для пользователя.
 * @return Индекс активной вкладки после операции.
 */
int DocumentTabWidget::addOrActivateTab(MarkdownView* view, const QString& title)
{
    const int existingTabIndex = findTabByPath(view->documentPath());
    if (existingTabIndex >= 0) {
        setCurrentIndex(existingTabIndex);
        return existingTabIndex;
    }

    const int tabIndex = addTab(view, title);
    setCurrentIndex(tabIndex);
    return tabIndex;
}

/**
 * @brief Ищет индекс вкладки по абсолютному пути документа.
 * @param documentPath Абсолютный путь к Markdown-документу.
 * @return Индекс вкладки или `-1`, если документ не открыт.
 */
int DocumentTabWidget::findTabByPath(const QString& documentPath) const
{
    for (int tabIndex = 0; tabIndex < count(); ++tabIndex) {
        const MarkdownView* view = qobject_cast<MarkdownView*>(widget(tabIndex));
        if (view != nullptr && view->documentPath() == documentPath) {
            return tabIndex;
        }
    }

    return -1;
}

/**
 * @brief Возвращает путь к документу в указанной вкладке.
 * @param tabIndex Индекс вкладки, для которой нужно вернуть путь.
 * @return Абсолютный путь к документу или пустая строка, если вкладка не найдена.
 */
QString DocumentTabWidget::documentPathAt(int tabIndex) const
{
    const MarkdownView* view = qobject_cast<MarkdownView*>(widget(tabIndex));
    return view != nullptr ? view->documentPath() : QString();
}

/**
 * @brief Возвращает путь к документу в текущей активной вкладке.
 * @return Абсолютный путь к текущему документу или пустая строка, если вкладок нет.
 */
QString DocumentTabWidget::currentDocumentPath() const
{
    return documentPathAt(currentIndex());
}

/**
 * @brief Возвращает виджет Markdown текущей активной вкладки.
 * @return Указатель на MarkdownView текущей вкладки или `nullptr`.
 */
MarkdownView* DocumentTabWidget::currentView() const
{
    return qobject_cast<MarkdownView*>(currentWidget());
}

/**
 * @brief Закрывает вкладку по индексу из интерфейса пользователя.
 * @param tabIndex Индекс вкладки, которую нужно закрыть.
 */
void DocumentTabWidget::closeTabByIndex(int tabIndex)
{
    const QString closedDocumentPath = documentPathAt(tabIndex);
    QWidget* closedWidget = widget(tabIndex);

    removeTab(tabIndex);
    delete closedWidget;

    emit documentClosed(closedDocumentPath);
    emit currentDocumentChanged(currentDocumentPath());
}

/**
 * @brief Переизлучает смену текущего документа после переключения вкладок.
 * @param tabIndex Индекс новой активной вкладки.
 */
void DocumentTabWidget::emitCurrentDocumentPath(int tabIndex)
{
    Q_UNUSED(tabIndex)
    emit currentDocumentChanged(currentDocumentPath());
}

