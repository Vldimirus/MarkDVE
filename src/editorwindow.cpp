#include "editorwindow.h"

#include "editorsettingsdialog.h"
#include "markdownhighlighter.h"

#include <QAction>
#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QMessageBox>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPalette>
#include <QStringConverter>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextOption>
#include <QTextStream>
#include <QToolBar>

/**
 * @brief Создаёт окно редактора Markdown-файла.
 * @param documentPath Абсолютный путь к Markdown-файлу, который нужно редактировать.
 * @param parent Родительский виджет Qt для управления временем жизни окна.
 */
EditorWindow::EditorWindow(const QString& documentPath, QWidget* parent)
    : QMainWindow(parent)
    , m_documentPath(QFileInfo(documentPath).absoluteFilePath())
    , m_editor(new QPlainTextEdit(this))
    , m_highlighter(new MarkdownHighlighter(m_editor->document()))
    , m_editorSettings(EditorSettingsStore::load())
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setCentralWidget(m_editor);

    setupActions();
    applyEditorSettings(m_editorSettings);

    connect(m_editor->document(), &QTextDocument::modificationChanged, this, &EditorWindow::updateWindowTitle);
    connect(m_editor, &QPlainTextEdit::cursorPositionChanged, this, &EditorWindow::updateCurrentLineHighlight);

    QString errorText;
    if (!loadFile(&errorText)) {
        QMessageBox::warning(this, QStringLiteral("Ошибка открытия"), errorText);
    }

    updateWindowTitle(false);
    resize(900, 700);
}

/**
 * @brief Возвращает путь к открытому в редакторе документу.
 * @return Абсолютный путь к Markdown-файлу.
 */
QString EditorWindow::documentPath() const
{
    return m_documentPath;
}

/**
 * @brief Сохраняет текущее содержимое редактора в Markdown-файл.
 */
void EditorWindow::saveDocument()
{
    QFile markdownFile(m_documentPath);
    if (!markdownFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("Ошибка сохранения"), QStringLiteral("Не удалось открыть файл для записи."));
        return;
    }

    QTextStream stream(&markdownFile);
    stream.setEncoding(QStringConverter::Utf8);
    stream << m_editor->toPlainText();

    m_editor->document()->setModified(false);
    emit documentSaved(m_documentPath);
    emit statusMessage(QStringLiteral("Файл сохранён: %1").arg(QFileInfo(m_documentPath).fileName()));
}

/**
 * @brief Перечитывает файл с диска и заменяет текст редактора.
 */
void EditorWindow::reloadFromDisk()
{
    if (!confirmDiscardChanges()) {
        return;
    }

    QString errorText;
    if (!loadFile(&errorText)) {
        QMessageBox::warning(this, QStringLiteral("Ошибка чтения"), errorText);
        return;
    }

    emit statusMessage(QStringLiteral("Файл перечитан: %1").arg(QFileInfo(m_documentPath).fileName()));
}

/**
 * @brief Открывает диалог настройки редактора и применяет выбранные параметры.
 */
void EditorWindow::openEditorSettings()
{
    EditorSettingsDialog settingsDialog(m_editorSettings, this);
    if (settingsDialog.exec() != QDialog::Accepted) {
        return;
    }

    applyEditorSettings(settingsDialog.editorSettings());
    EditorSettingsStore::save(m_editorSettings);
    emit statusMessage(QStringLiteral("Настройки редактора обновлены."));
}

/**
 * @brief Обновляет заголовок окна в зависимости от состояния изменений.
 * @param modified Признак наличия несохранённых изменений в редакторе.
 */
void EditorWindow::updateWindowTitle(bool modified)
{
    const QString modifiedSuffix = modified ? QStringLiteral(" *") : QString();
    setWindowTitle(QStringLiteral("Редактор Markdown: %1%2").arg(QFileInfo(m_documentPath).fileName(), modifiedSuffix));
}

/**
 * @brief Обновляет подсветку текущей строки в текстовом редакторе.
 */
void EditorWindow::updateCurrentLineHighlight()
{
    QList<QTextEdit::ExtraSelection> extraSelections; ///< Набор дополнительных визуальных выделений поверх текста редактора.
    if (m_editorSettings.highlightCurrentLine) {
        QTextEdit::ExtraSelection currentLineSelection; ///< Выделение всей текущей строки курсора.
        QColor lineHighlightColor = palette().color(QPalette::Highlight).lighter(185); ///< Базовый цвет подсветки активной строки.
        lineHighlightColor.setAlpha(70);

        currentLineSelection.format.setBackground(lineHighlightColor);
        currentLineSelection.format.setProperty(QTextFormat::FullWidthSelection, true);
        currentLineSelection.cursor = m_editor->textCursor();
        currentLineSelection.cursor.clearSelection();
        extraSelections.append(currentLineSelection);
    }

    m_editor->setExtraSelections(extraSelections);
}

/**
 * @brief Перехватывает закрытие окна, чтобы предупредить о несохранённых изменениях.
 * @param event Событие закрытия окна редактора.
 */
void EditorWindow::closeEvent(QCloseEvent* event)
{
    if (confirmDiscardChanges()) {
        event->accept();
        return;
    }

    event->ignore();
}

/**
 * @brief Создаёт меню и действия окна редактора.
 *
 * Метод нужен, чтобы пользователь мог сохранить или перечитать файл через интерфейс.
 */
void EditorWindow::setupActions()
{
    m_saveAction = new QAction(QStringLiteral("Сохранить"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &EditorWindow::saveDocument);

    m_reloadAction = new QAction(QStringLiteral("Перечитать"), this);
    m_reloadAction->setShortcut(QKeySequence::Refresh);
    connect(m_reloadAction, &QAction::triggered, this, &EditorWindow::reloadFromDisk);

    m_settingsAction = new QAction(QStringLiteral("Внешний вид и подсветка..."), this);
    m_settingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    m_settingsAction->setStatusTip(QStringLiteral("Настроить шрифт, отображение и Markdown-подсветку редактора"));
    connect(m_settingsAction, &QAction::triggered, this, &EditorWindow::openEditorSettings);

    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("Файл"));
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_reloadAction);

    QMenu* editorMenu = menuBar()->addMenu(QStringLiteral("Редактор"));
    editorMenu->addAction(m_settingsAction);

    QToolBar* fileToolBar = addToolBar(QStringLiteral("Файл"));
    fileToolBar->addAction(m_saveAction);
    fileToolBar->addAction(m_reloadAction);
    fileToolBar->addAction(m_settingsAction);
}

/**
 * @brief Загружает текст Markdown-файла в редактор.
 * @param errorText Строка для возврата текста ошибки, если чтение не удалось.
 * @return true, если файл успешно прочитан в редактор.
 */
bool EditorWindow::loadFile(QString* errorText)
{
    QFile markdownFile(m_documentPath);
    if (!markdownFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Не удалось открыть Markdown-файл для чтения.");
        }
        return false;
    }

    QTextStream stream(&markdownFile);
    stream.setEncoding(QStringConverter::Utf8);
    const QString markdownText = stream.readAll();

    m_editor->setPlainText(markdownText);
    m_editor->document()->setModified(false);
    return true;
}

/**
 * @brief Применяет настройки внешнего вида и подсветки к редактору Markdown.
 * @param settings Набор настроек, который нужно применить к текущему окну редактора.
 */
void EditorWindow::applyEditorSettings(const EditorSettings& settings)
{
    m_editorSettings = settings;
    m_editorSettings.highlightProfile = EditorSettingsStore::detectProfile(m_editorSettings);

    QFont editorFont(settings.fontFamily, settings.fontPointSize); ///< Шрифт, который будет применён к тексту Markdown-редактора.
    m_editor->setFont(editorFont);
    m_editor->setLineWrapMode(settings.wordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);

    const QFontMetricsF fontMetrics(editorFont);
    m_editor->setTabStopDistance(fontMetrics.horizontalAdvance(QLatin1Char(' ')) * settings.tabWidthSpaces);

    m_highlighter->applySettings(m_editorSettings);
    updateCurrentLineHighlight();
}

/**
 * @brief Запрашивает подтверждение при наличии несохранённых изменений.
 * @return true, если окно можно закрыть или перезагрузить без потери данных.
 */
bool EditorWindow::confirmDiscardChanges()
{
    if (!m_editor->document()->isModified()) {
        return true;
    }

    const QMessageBox::StandardButton userChoice = QMessageBox::warning(
        this,
        QStringLiteral("Несохранённые изменения"),
        QStringLiteral("В документе есть несохранённые изменения. Закрыть их без сохранения?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    return userChoice == QMessageBox::Yes;
}
