#include "editorsettingsdialog.h"

#include "markdownhighlighter.h"
#include "markdownview.h"
#include "viewersettings.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontComboBox>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextFormat>
#include <QVBoxLayout>

namespace
{
const QString kPreviewMarkdownText = QStringLiteral(
    "# Карта модуля\n\n"
    "- [x] Просмотр реализован\n"
    "- [ ] Доработать AI-протокол\n\n"
    "> Важная заметка по архитектуре.\n\n"
    "Ссылка: [Project Map](docs/Project_Map/map_main.md)\n\n"
    "`inline code`\n\n"
    "```cpp\n"
    "int buildStatus = 1;\n"
    "return buildStatus;\n"
    "```\n");
}

/**
 * @brief Создаёт диалог настройки редактора Markdown.
 * @param settings Текущие настройки редактора, которые нужно показать пользователю.
 * @param parent Родительский виджет Qt для управления временем жизни диалога.
 */
EditorSettingsDialog::EditorSettingsDialog(const EditorSettings& settings, QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    loadSettings(settings);

    setWindowTitle(QStringLiteral("Настройки редактора"));
    resize(1040, 640);
}

/**
 * @brief Возвращает настройки редактора, выбранные пользователем в диалоге.
 * @return Структура со шрифтом, отображением и параметрами подсветки.
 */
EditorSettings EditorSettingsDialog::editorSettings() const
{
    EditorSettings settings = EditorSettingsStore::defaultSettings();
    settings.fontFamily = m_fontComboBox->currentFont().family();
    settings.fontPointSize = m_fontSizeSpinBox->value();
    settings.tabWidthSpaces = m_tabWidthSpinBox->value();
    settings.wordWrap = m_wordWrapCheckBox->isChecked();
    settings.highlightCurrentLine = m_currentLineCheckBox->isChecked();
    settings.highlightHeadings = m_headingCheckBox->isChecked();
    settings.highlightLinks = m_linkCheckBox->isChecked();
    settings.highlightQuotes = m_quoteCheckBox->isChecked();
    settings.highlightLists = m_listCheckBox->isChecked();
    settings.highlightCode = m_codeCheckBox->isChecked();
    settings.highlightProfile = EditorSettingsStore::detectProfile(settings);
    return settings;
}

/**
 * @brief Применяет выбранный профиль подсветки к группе чекбоксов элементов Markdown.
 * @param profileIndex Индекс выбранного профиля в комбобоксе.
 */
void EditorSettingsDialog::applySelectedHighlightProfile(int profileIndex)
{
    Q_UNUSED(profileIndex)

    const HighlightProfile profile = selectedProfile();
    if (profile == HighlightProfile::Custom) {
        updatePreview();
        return;
    }

    m_isApplyingProfile = true;
    const EditorSettings profileSettings = EditorSettingsStore::settingsForProfile(profile);
    m_headingCheckBox->setChecked(profileSettings.highlightHeadings);
    m_linkCheckBox->setChecked(profileSettings.highlightLinks);
    m_quoteCheckBox->setChecked(profileSettings.highlightQuotes);
    m_listCheckBox->setChecked(profileSettings.highlightLists);
    m_codeCheckBox->setChecked(profileSettings.highlightCode);
    m_isApplyingProfile = false;

    updatePreview();
}

/**
 * @brief Синхронизирует профиль подсветки после ручного изменения чекбоксов.
 *
 * Метод нужен, чтобы автоматически переключать профиль в "пользовательский"
 * режим, когда состав подсвечиваемых элементов перестаёт совпадать с шаблоном.
 */
void EditorSettingsDialog::syncHighlightProfileWithOptions()
{
    if (m_isApplyingProfile) {
        return;
    }

    EditorSettings settings = EditorSettingsStore::defaultSettings();
    settings.highlightHeadings = m_headingCheckBox->isChecked();
    settings.highlightLinks = m_linkCheckBox->isChecked();
    settings.highlightQuotes = m_quoteCheckBox->isChecked();
    settings.highlightLists = m_listCheckBox->isChecked();
    settings.highlightCode = m_codeCheckBox->isChecked();

    const HighlightProfile detectedProfile = EditorSettingsStore::detectProfile(settings);
    const int comboIndex = m_highlightProfileComboBox->findData(static_cast<int>(detectedProfile));
    if (comboIndex >= 0) {
        m_isApplyingProfile = true;
        m_highlightProfileComboBox->setCurrentIndex(comboIndex);
        m_isApplyingProfile = false;
    }

    updatePreview();
}

/**
 * @brief Восстанавливает настройки редактора к значениям по умолчанию.
 */
void EditorSettingsDialog::restoreDefaults()
{
    loadSettings(EditorSettingsStore::defaultSettings());
}

/**
 * @brief Создаёт интерфейс диалога настройки редактора.
 *
 * Метод нужен, чтобы собрать поля выбора шрифта, отображения, подсветки и живого примера.
 */
void EditorSettingsDialog::setupUi()
{
    m_fontComboBox = new QFontComboBox(this);
    m_fontSizeSpinBox = new QSpinBox(this);
    m_fontSizeSpinBox->setRange(8, 36);

    m_tabWidthSpinBox = new QSpinBox(this);
    m_tabWidthSpinBox->setRange(2, 12);

    m_wordWrapCheckBox = new QCheckBox(QStringLiteral("Переносить длинные строки"), this);
    m_currentLineCheckBox = new QCheckBox(QStringLiteral("Подсвечивать текущую строку"), this);

    m_highlightProfileComboBox = new QComboBox(this);
    m_highlightProfileComboBox->addItem(QStringLiteral("Без подсветки"), static_cast<int>(HighlightProfile::Disabled));
    m_highlightProfileComboBox->addItem(QStringLiteral("Базовая"), static_cast<int>(HighlightProfile::Basic));
    m_highlightProfileComboBox->addItem(QStringLiteral("Расширенная"), static_cast<int>(HighlightProfile::Extended));
    m_highlightProfileComboBox->addItem(QStringLiteral("Пользовательская"), static_cast<int>(HighlightProfile::Custom));

    m_headingCheckBox = new QCheckBox(QStringLiteral("Подсвечивать заголовки"), this);
    m_linkCheckBox = new QCheckBox(QStringLiteral("Подсвечивать ссылки"), this);
    m_quoteCheckBox = new QCheckBox(QStringLiteral("Подсвечивать цитаты"), this);
    m_listCheckBox = new QCheckBox(QStringLiteral("Подсвечивать списки и чекбоксы"), this);
    m_codeCheckBox = new QCheckBox(QStringLiteral("Подсвечивать код"), this);

    m_editorPreview = new QPlainTextEdit(this);
    m_editorPreview->setReadOnly(true);
    m_editorPreview->setPlainText(kPreviewMarkdownText);
    m_previewHighlighter = new MarkdownHighlighter(m_editorPreview->document());

    m_viewerPreview = new MarkdownView(this);
    m_viewerPreview->setMarkdownContent(kPreviewMarkdownText);
    m_viewerPreview->setMinimumHeight(220);

    QFormLayout* fontLayout = new QFormLayout();
    fontLayout->addRow(QStringLiteral("Шрифт"), m_fontComboBox);
    fontLayout->addRow(QStringLiteral("Размер шрифта"), m_fontSizeSpinBox);
    fontLayout->addRow(QStringLiteral("Табуляция, пробелов"), m_tabWidthSpinBox);

    QGroupBox* appearanceGroup = new QGroupBox(QStringLiteral("Отображение"), this);
    QVBoxLayout* appearanceLayout = new QVBoxLayout(appearanceGroup);
    appearanceLayout->addLayout(fontLayout);
    appearanceLayout->addWidget(m_wordWrapCheckBox);
    appearanceLayout->addWidget(m_currentLineCheckBox);

    QGroupBox* highlightGroup = new QGroupBox(QStringLiteral("Подсветка Markdown"), this);
    QVBoxLayout* highlightLayout = new QVBoxLayout(highlightGroup);
    highlightLayout->addWidget(new QLabel(QStringLiteral("Профиль подсветки"), highlightGroup));
    highlightLayout->addWidget(m_highlightProfileComboBox);
    highlightLayout->addWidget(m_headingCheckBox);
    highlightLayout->addWidget(m_linkCheckBox);
    highlightLayout->addWidget(m_quoteCheckBox);
    highlightLayout->addWidget(m_listCheckBox);
    highlightLayout->addWidget(m_codeCheckBox);

    QGroupBox* editorPreviewGroup = new QGroupBox(QStringLiteral("Пример в редакторе"), this);
    QVBoxLayout* editorPreviewLayout = new QVBoxLayout(editorPreviewGroup);
    editorPreviewLayout->addWidget(m_editorPreview);

    QGroupBox* viewerPreviewGroup = new QGroupBox(QStringLiteral("Пример в просмотре"), this);
    QVBoxLayout* viewerPreviewLayout = new QVBoxLayout(viewerPreviewGroup);
    viewerPreviewLayout->addWidget(m_viewerPreview);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,
        Qt::Horizontal,
        this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &EditorSettingsDialog::restoreDefaults);

    connect(m_highlightProfileComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EditorSettingsDialog::applySelectedHighlightProfile);
    connect(m_headingCheckBox, &QCheckBox::toggled, this, &EditorSettingsDialog::syncHighlightProfileWithOptions);
    connect(m_linkCheckBox, &QCheckBox::toggled, this, &EditorSettingsDialog::syncHighlightProfileWithOptions);
    connect(m_quoteCheckBox, &QCheckBox::toggled, this, &EditorSettingsDialog::syncHighlightProfileWithOptions);
    connect(m_listCheckBox, &QCheckBox::toggled, this, &EditorSettingsDialog::syncHighlightProfileWithOptions);
    connect(m_codeCheckBox, &QCheckBox::toggled, this, &EditorSettingsDialog::syncHighlightProfileWithOptions);
    connect(m_fontComboBox, &QFontComboBox::currentFontChanged, this, &EditorSettingsDialog::updatePreview);
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &EditorSettingsDialog::updatePreview);
    connect(m_tabWidthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &EditorSettingsDialog::updatePreview);
    connect(m_wordWrapCheckBox, &QCheckBox::toggled, this, &EditorSettingsDialog::updatePreview);
    connect(m_currentLineCheckBox, &QCheckBox::toggled, this, &EditorSettingsDialog::updatePreview);

    QWidget* settingsPanel = new QWidget(this);
    QVBoxLayout* settingsPanelLayout = new QVBoxLayout(settingsPanel);
    settingsPanelLayout->addWidget(appearanceGroup);
    settingsPanelLayout->addWidget(highlightGroup);
    settingsPanelLayout->addStretch();

    QWidget* previewPanel = new QWidget(this);
    QVBoxLayout* previewPanelLayout = new QVBoxLayout(previewPanel);
    previewPanelLayout->addWidget(editorPreviewGroup, 1);
    previewPanelLayout->addWidget(viewerPreviewGroup, 1);

    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->addWidget(settingsPanel, 0);
    contentLayout->addWidget(previewPanel, 1);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(contentLayout, 1);
    mainLayout->addWidget(buttonBox);
}

/**
 * @brief Загружает текущие настройки редактора в виджеты диалога.
 * @param settings Настройки редактора, которые нужно отобразить пользователю.
 */
void EditorSettingsDialog::loadSettings(const EditorSettings& settings)
{
    m_isApplyingProfile = true;
    m_fontComboBox->setCurrentFont(QFont(settings.fontFamily));
    m_fontSizeSpinBox->setValue(settings.fontPointSize);
    m_tabWidthSpinBox->setValue(settings.tabWidthSpaces);
    m_wordWrapCheckBox->setChecked(settings.wordWrap);
    m_currentLineCheckBox->setChecked(settings.highlightCurrentLine);
    m_headingCheckBox->setChecked(settings.highlightHeadings);
    m_linkCheckBox->setChecked(settings.highlightLinks);
    m_quoteCheckBox->setChecked(settings.highlightQuotes);
    m_listCheckBox->setChecked(settings.highlightLists);
    m_codeCheckBox->setChecked(settings.highlightCode);

    const HighlightProfile detectedProfile = EditorSettingsStore::detectProfile(settings);
    const int comboIndex = m_highlightProfileComboBox->findData(static_cast<int>(detectedProfile));
    m_highlightProfileComboBox->setCurrentIndex(comboIndex >= 0 ? comboIndex : 0);
    m_isApplyingProfile = false;

    updatePreview();
}

/**
 * @brief Обновляет демонстрационный пример редактора и просмотра по текущим полям диалога.
 *
 * Метод нужен, чтобы пользователь видел результат изменения настроек сразу,
 * не закрывая окно настройки редактора.
 */
void EditorSettingsDialog::updatePreview()
{
    const EditorSettings settings = editorSettings();
    const QFont editorFont(settings.fontFamily, settings.fontPointSize); ///< Шрифт демонстрационного Markdown-редактора.
    m_editorPreview->setFont(editorFont);
    m_editorPreview->setLineWrapMode(settings.wordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);

    const QFontMetricsF fontMetrics(editorFont);
    m_editorPreview->setTabStopDistance(fontMetrics.horizontalAdvance(QLatin1Char(' ')) * settings.tabWidthSpaces);
    m_previewHighlighter->applySettings(settings);

    QTextCursor previewCursor = m_editorPreview->textCursor(); ///< Курсор демонстрационного редактора для выделения текущей строки.
    previewCursor.movePosition(QTextCursor::Start);
    previewCursor.movePosition(QTextCursor::Down);
    m_editorPreview->setTextCursor(previewCursor);

    QList<QTextEdit::ExtraSelection> extraSelections; ///< Временные выделения для показа подсветки текущей строки в примере редактора.
    if (settings.highlightCurrentLine) {
        QTextEdit::ExtraSelection currentLineSelection; ///< Выделение текущей строки в демонстрационном примере редактора.
        currentLineSelection.format.setBackground(QColor(QStringLiteral("#DCE8FA")));
        currentLineSelection.format.setProperty(QTextFormat::FullWidthSelection, true);
        currentLineSelection.cursor = m_editorPreview->textCursor();
        currentLineSelection.cursor.clearSelection();
        extraSelections.append(currentLineSelection);
    }
    m_editorPreview->setExtraSelections(extraSelections);

    m_viewerPreview->applyViewerSettings(ViewerSettingsStore::load());
}

/**
 * @brief Возвращает профиль подсветки, выбранный в комбобоксе.
 * @return Профиль подсветки Markdown, соответствующий текущему выбору.
 */
HighlightProfile EditorSettingsDialog::selectedProfile() const
{
    return static_cast<HighlightProfile>(m_highlightProfileComboBox->currentData().toInt());
}
