#include "viewersettingsdialog.h"

#include "editorsettings.h"
#include "markdownhighlighter.h"
#include "markdownview.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontComboBox>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace
{
const QString kPreviewMarkdownText = QStringLiteral(
    "# Глобальная карта\n\n"
    "- [x] Навигация по Markdown\n"
    "- [ ] Настроить внешний вид\n\n"
    "> Этот блок показывает цитату в просмотре.\n\n"
    "Ссылка: [Viewer](docs/modules/viewer.md)\n\n"
    "`inline code`\n\n"
    "```sql\n"
    "SELECT status FROM modules;\n"
    "```\n");
}

/**
 * @brief Создаёт диалог настройки просмотра Markdown.
 * @param settings Текущие настройки внешнего вида, которые нужно показать пользователю.
 * @param parent Родительский виджет Qt для управления временем жизни диалога.
 */
ViewerSettingsDialog::ViewerSettingsDialog(const ViewerSettings& settings, QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    loadSettings(settings);

    setWindowTitle(QStringLiteral("Внешний вид просмотра"));
    resize(1040, 600);
}

/**
 * @brief Возвращает настройки просмотра, выбранные пользователем в диалоге.
 * @return Структура с параметрами внешнего вида окна просмотра Markdown.
 */
ViewerSettings ViewerSettingsDialog::viewerSettings() const
{
    ViewerSettings settings = ViewerSettingsStore::defaultSettings();
    settings.fontFamily = m_fontComboBox->currentFont().family();
    settings.fontPointSize = m_fontSizeSpinBox->value();
    settings.documentMargin = m_documentMarginSpinBox->value();
    settings.underlineLinks = m_underlineLinksCheckBox->isChecked();
    settings.emphasizeCodeBlocks = m_codeBlocksCheckBox->isChecked();
    settings.invertedColors = m_invertedColorsCheckBox->isChecked();
    return settings;
}

/**
 * @brief Восстанавливает стандартные настройки просмотра Markdown.
 */
void ViewerSettingsDialog::restoreDefaults()
{
    loadSettings(ViewerSettingsStore::defaultSettings());
}

/**
 * @brief Создаёт интерфейс диалога настройки просмотра Markdown.
 *
 * Метод собирает поля выбора шрифта, визуальных флагов и живого примера.
 */
void ViewerSettingsDialog::setupUi()
{
    m_fontComboBox = new QFontComboBox(this);

    m_fontSizeSpinBox = new QSpinBox(this);
    m_fontSizeSpinBox->setRange(8, 32);

    m_documentMarginSpinBox = new QSpinBox(this);
    m_documentMarginSpinBox->setRange(0, 64);

    m_underlineLinksCheckBox = new QCheckBox(QStringLiteral("Подчёркивать ссылки"), this);
    m_codeBlocksCheckBox = new QCheckBox(QStringLiteral("Выделять код и code blocks"), this);
    m_invertedColorsCheckBox = new QCheckBox(QStringLiteral("Инвертировать цвета просмотра"), this);

    m_editorPreview = new QPlainTextEdit(this);
    m_editorPreview->setReadOnly(true);
    m_editorPreview->setPlainText(kPreviewMarkdownText);
    m_previewHighlighter = new MarkdownHighlighter(m_editorPreview->document());

    m_viewerPreview = new MarkdownView(this);
    m_viewerPreview->setMarkdownContent(kPreviewMarkdownText);
    m_viewerPreview->setMinimumHeight(220);

    QFormLayout* appearanceLayout = new QFormLayout();
    appearanceLayout->addRow(QStringLiteral("Шрифт"), m_fontComboBox);
    appearanceLayout->addRow(QStringLiteral("Размер шрифта"), m_fontSizeSpinBox);
    appearanceLayout->addRow(QStringLiteral("Отступы документа"), m_documentMarginSpinBox);

    QGroupBox* appearanceGroup = new QGroupBox(QStringLiteral("Оформление просмотра"), this);
    QVBoxLayout* appearanceGroupLayout = new QVBoxLayout(appearanceGroup);
    appearanceGroupLayout->addLayout(appearanceLayout);
    appearanceGroupLayout->addWidget(m_underlineLinksCheckBox);
    appearanceGroupLayout->addWidget(m_codeBlocksCheckBox);
    appearanceGroupLayout->addWidget(m_invertedColorsCheckBox);

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
    connect(buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &ViewerSettingsDialog::restoreDefaults);

    connect(m_fontComboBox, &QFontComboBox::currentFontChanged, this, &ViewerSettingsDialog::updatePreview);
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ViewerSettingsDialog::updatePreview);
    connect(m_documentMarginSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ViewerSettingsDialog::updatePreview);
    connect(m_underlineLinksCheckBox, &QCheckBox::toggled, this, &ViewerSettingsDialog::updatePreview);
    connect(m_codeBlocksCheckBox, &QCheckBox::toggled, this, &ViewerSettingsDialog::updatePreview);
    connect(m_invertedColorsCheckBox, &QCheckBox::toggled, this, &ViewerSettingsDialog::updatePreview);

    QWidget* settingsPanel = new QWidget(this);
    QVBoxLayout* settingsPanelLayout = new QVBoxLayout(settingsPanel);
    settingsPanelLayout->addWidget(appearanceGroup);
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
 * @brief Загружает текущие настройки в виджеты диалога.
 * @param settings Настройки просмотра, которые нужно отобразить пользователю.
 */
void ViewerSettingsDialog::loadSettings(const ViewerSettings& settings)
{
    m_fontComboBox->setCurrentFont(QFont(settings.fontFamily));
    m_fontSizeSpinBox->setValue(settings.fontPointSize);
    m_documentMarginSpinBox->setValue(settings.documentMargin);
    m_underlineLinksCheckBox->setChecked(settings.underlineLinks);
    m_codeBlocksCheckBox->setChecked(settings.emphasizeCodeBlocks);
    m_invertedColorsCheckBox->setChecked(settings.invertedColors);

    updatePreview();
}

/**
 * @brief Обновляет демонстрационный пример редактора и просмотра по текущим полям диалога.
 *
 * Метод нужен, чтобы пользователь видел итоговый вид страницы сразу,
 * без закрытия окна настройки просмотра.
 */
void ViewerSettingsDialog::updatePreview()
{
    const EditorSettings editorPreviewSettings = EditorSettingsStore::load();
    const QFont editorFont(editorPreviewSettings.fontFamily, editorPreviewSettings.fontPointSize); ///< Шрифт примера Markdown-редактора в диалоге просмотра.
    m_editorPreview->setFont(editorFont);
    m_editorPreview->setLineWrapMode(editorPreviewSettings.wordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);

    const QFontMetricsF fontMetrics(editorFont);
    m_editorPreview->setTabStopDistance(fontMetrics.horizontalAdvance(QLatin1Char(' ')) * editorPreviewSettings.tabWidthSpaces);
    m_previewHighlighter->applySettings(editorPreviewSettings);

    m_viewerPreview->applyViewerSettings(viewerSettings());
}
