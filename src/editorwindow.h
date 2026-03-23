#pragma once

#include "editorsettings.h"

#include <QMainWindow>

class QAction;
class QCloseEvent;
class MarkdownHighlighter;
class QPlainTextEdit;

/**
 * @brief Окно редактирования одного Markdown-файла.
 *
 * Окно открывается отдельно от просмотрщика и позволяет безопасно редактировать
 * исходный `.md` файл с сохранением в UTF-8.
 */
class EditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Создаёт окно редактора Markdown-файла.
     * @param documentPath Абсолютный путь к Markdown-файлу, который нужно редактировать.
     * @param parent Родительский виджет Qt для управления временем жизни окна.
     */
    explicit EditorWindow(const QString& documentPath, QWidget* parent = nullptr);

    /**
     * @brief Возвращает путь к открытому в редакторе документу.
     * @return Абсолютный путь к Markdown-файлу.
     */
    QString documentPath() const;

signals:
    /**
     * @brief Сигнал сообщает о сохранении файла на диск.
     * @param documentPath Абсолютный путь к сохранённому Markdown-файлу.
     */
    void documentSaved(const QString& documentPath);

    /**
     * @brief Сигнал сообщает о статусном сообщении редактора.
     * @param message Текст статуса для показа пользователю.
     */
    void statusMessage(const QString& message);

private slots:
    /**
     * @brief Сохраняет текущее содержимое редактора в Markdown-файл.
     */
    void saveDocument();

    /**
     * @brief Перечитывает файл с диска и заменяет текст редактора.
     */
    void reloadFromDisk();

    /**
     * @brief Открывает диалог настройки редактора и применяет выбранные параметры.
     */
    void openEditorSettings();

    /**
     * @brief Обновляет заголовок окна в зависимости от состояния изменений.
     * @param modified Признак наличия несохранённых изменений в редакторе.
     */
    void updateWindowTitle(bool modified);

    /**
     * @brief Обновляет подсветку текущей строки в текстовом редакторе.
     */
    void updateCurrentLineHighlight();

protected:
    /**
     * @brief Перехватывает закрытие окна, чтобы предупредить о несохранённых изменениях.
     * @param event Событие закрытия окна редактора.
     */
    void closeEvent(QCloseEvent* event) override;

private:
    /**
     * @brief Создаёт меню и действия окна редактора.
     *
     * Метод нужен, чтобы пользователь мог сохранить или перечитать файл через интерфейс.
     */
    void setupActions();

    /**
     * @brief Загружает текст Markdown-файла в редактор.
     * @param errorText Строка для возврата текста ошибки, если чтение не удалось.
     * @return true, если файл успешно прочитан в редактор.
     */
    bool loadFile(QString* errorText = nullptr);

    /**
     * @brief Применяет настройки внешнего вида и подсветки к редактору Markdown.
     * @param settings Набор настроек, который нужно применить к текущему окну редактора.
     */
    void applyEditorSettings(const EditorSettings& settings);

    /**
     * @brief Запрашивает подтверждение при наличии несохранённых изменений.
     * @return true, если окно можно закрыть или перезагрузить без потери данных.
     */
    bool confirmDiscardChanges();

    QString m_documentPath;             ///< Абсолютный путь к редактируемому Markdown-файлу.
    QPlainTextEdit* m_editor = nullptr; ///< Текстовый редактор содержимого Markdown-файла.
    QAction* m_saveAction = nullptr;    ///< Действие меню для сохранения файла.
    QAction* m_reloadAction = nullptr;  ///< Действие меню для перечитывания файла с диска.
    QAction* m_settingsAction = nullptr; ///< Действие меню для открытия настроек внешнего вида и подсветки редактора.
    MarkdownHighlighter* m_highlighter = nullptr; ///< Подсветчик Markdown для текстового редактора.
    EditorSettings m_editorSettings;    ///< Текущие сохранённые настройки внешнего вида редактора.
};
