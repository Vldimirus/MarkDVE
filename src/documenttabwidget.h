#pragma once

#include <QTabWidget>

class MarkdownView;

/**
 * @brief Виджет управляет вкладками открытых Markdown-документов.
 *
 * Виджет нужен, чтобы переиспользовать вкладки, быстро искать уже открытый
 * документ и уведомлять главное окно о смене текущего документа.
 */
class DocumentTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    /**
     * @brief Создаёт виджет вкладок документов.
     * @param parent Родительский виджет Qt для управления временем жизни объекта.
     */
    explicit DocumentTabWidget(QWidget* parent = nullptr);

    /**
     * @brief Добавляет новую вкладку с документом или делает активной уже открытую.
     * @param view Виджет отображения Markdown, который нужно показать во вкладке.
     * @param title Заголовок вкладки для пользователя.
     * @return Индекс активной вкладки после операции.
     */
    int addOrActivateTab(MarkdownView* view, const QString& title);

    /**
     * @brief Ищет индекс вкладки по абсолютному пути документа.
     * @param documentPath Абсолютный путь к Markdown-документу.
     * @return Индекс вкладки или `-1`, если документ не открыт.
     */
    int findTabByPath(const QString& documentPath) const;

    /**
     * @brief Возвращает путь к документу в указанной вкладке.
     * @param tabIndex Индекс вкладки, для которой нужно вернуть путь.
     * @return Абсолютный путь к документу или пустая строка, если вкладка не найдена.
     */
    QString documentPathAt(int tabIndex) const;

    /**
     * @brief Возвращает путь к документу в текущей активной вкладке.
     * @return Абсолютный путь к текущему документу или пустая строка, если вкладок нет.
     */
    QString currentDocumentPath() const;

    /**
     * @brief Возвращает виджет Markdown текущей активной вкладки.
     * @return Указатель на MarkdownView текущей вкладки или `nullptr`.
     */
    MarkdownView* currentView() const;

signals:
    /**
     * @brief Сигнал сообщает о смене текущего активного документа.
     * @param documentPath Абсолютный путь к новому текущему документу.
     */
    void currentDocumentChanged(const QString& documentPath);

    /**
     * @brief Сигнал сообщает о закрытии вкладки документа.
     * @param documentPath Абсолютный путь к закрытому документу.
     */
    void documentClosed(const QString& documentPath);

private slots:
    /**
     * @brief Закрывает вкладку по индексу из интерфейса пользователя.
     * @param tabIndex Индекс вкладки, которую нужно закрыть.
     */
    void closeTabByIndex(int tabIndex);

    /**
     * @brief Переизлучает смену текущего документа после переключения вкладок.
     * @param tabIndex Индекс новой активной вкладки.
     */
    void emitCurrentDocumentPath(int tabIndex);
};

