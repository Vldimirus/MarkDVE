#include "codemapgenerator.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

/**
 * @brief Класс содержит модульные тесты генератора Markdown-карты кода.
 *
 * Тесты проверяют, что генератор создаёт иерархию Markdown-файлов, переносит
 * комментарии из кода и строит перекрёстные ссылки между модулями.
 */
class CodeMapGeneratorTests : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief Проверяет создание `index.md`, страниц модулей и ссылок между ними.
     */
    void generateCreatesMarkdownHierarchy();
};

/**
 * @brief Записывает UTF-8 текст в тестовый файл.
 * @param filePath Абсолютный путь к тестовому файлу, который нужно создать.
 * @param text Текст файла, который должен быть записан на диск.
 */
static void writeTestFile(const QString& filePath, const QString& text)
{
    QFile outputFile(filePath);
    QVERIFY(outputFile.open(QIODevice::WriteOnly | QIODevice::Text));

    QTextStream outputStream(&outputFile);
    outputStream.setEncoding(QStringConverter::Utf8);
    outputStream << text;
}

/**
 * @brief Проверяет создание `index.md`, страниц модулей и ссылок между ними.
 */
void CodeMapGeneratorTests::generateCreatesMarkdownHierarchy()
{
    QTemporaryDir sourceDirectory;
    QVERIFY(sourceDirectory.isValid());

    QTemporaryDir outputDirectory;
    QVERIFY(outputDirectory.isValid());

    QVERIFY(QDir().mkpath(sourceDirectory.path() + QStringLiteral("/src")));

    writeTestFile(
        sourceDirectory.path() + QStringLiteral("/src/helperservice.h"),
        QStringLiteral(
            "#pragma once\n\n"
            "class QObject;\n\n"
            "/**\n"
            " * @brief Сервис помогает главному виджету обработать сигнал сохранения.\n"
            " */\n"
            "class HelperService\n"
            "{\n"
            "public:\n"
            "    /**\n"
            "     * @brief Принимает сигнал от виджета и обновляет состояние.\n"
            "     * @param filePath Путь к изменённому файлу.\n"
            "     */\n"
            "    void reload(const QString& filePath);\n"
            "};\n"));

    writeTestFile(
        sourceDirectory.path() + QStringLiteral("/src/samplewidget.h"),
        QStringLiteral(
            "#pragma once\n\n"
            "#include \"helperservice.h\"\n\n"
            "class QString;\n\n"
            "/**\n"
            " * @brief Виджет управляет сохранением Markdown-документа.\n"
            " */\n"
            "class SampleWidget\n"
            "{\n"
            "public:\n"
            "    /**\n"
            "     * @brief Создаёт виджет и принимает зависимость сервиса.\n"
            "     * @param helperService Сервис для обработки сигнала сохранения.\n"
            "     */\n"
            "    explicit SampleWidget(HelperService* helperService);\n\n"
            "signals:\n"
            "    /**\n"
            "     * @brief Сообщает о сохранении файла.\n"
            "     * @param filePath Путь к сохранённому файлу.\n"
            "     */\n"
            "    void fileSaved(const QString& filePath);\n\n"
            "private:\n"
            "    HelperService* m_helperService = nullptr;\n"
            "};\n"));

    writeTestFile(
        sourceDirectory.path() + QStringLiteral("/src/samplewidget.cpp"),
        QStringLiteral(
            "#include \"samplewidget.h\"\n\n"
            "SampleWidget::SampleWidget(HelperService* helperService)\n"
            "    : m_helperService(helperService)\n"
            "{\n"
            "    connect(this, &SampleWidget::fileSaved, m_helperService, &HelperService::reload);\n"
            "}\n"));

    writeTestFile(
        sourceDirectory.path() + QStringLiteral("/src/main.cpp"),
        QStringLiteral(
            "#include \"samplewidget.h\"\n\n"
            "/**\n"
            " * @brief Запускает тестовый пример генерации карты кода.\n"
            " * @param argc Количество аргументов командной строки.\n"
            " * @param argv Аргументы командной строки.\n"
            " * @return Код завершения приложения.\n"
            " */\n"
            "int main(int argc, char* argv[])\n"
            "{\n"
            "    return argc + (argv != nullptr ? 0 : 1);\n"
            "}\n"));

    CodeMapGenerator generator;
    CodeMapGenerationResult generationResult;
    QString errorText;
    QVERIFY(generator.generate(sourceDirectory.path() + QStringLiteral("/src"), outputDirectory.path(), &generationResult, &errorText));
    QVERIFY2(errorText.isEmpty(), qPrintable(errorText));

    QFile indexFile(generationResult.indexFilePath);
    QVERIFY(indexFile.exists());
    QVERIFY(indexFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString indexText = QString::fromUtf8(indexFile.readAll());
    QVERIFY(indexText.contains(QStringLiteral("[SampleWidget](modules/sample_widget.md)")));
    QVERIFY(indexText.contains(QStringLiteral("[HelperService](modules/helper_service.md)")));
    QVERIFY(indexText.contains(QStringLiteral("[main.cpp](files/main.md)")));
    QVERIFY(indexText.contains(QStringLiteral("## Таблица модулей")));
    QVERIFY(indexText.contains(QStringLiteral("| [SampleWidget](modules/sample_widget.md) | модуль |")));
    QVERIFY(indexText.contains(QStringLiteral("## Таблица связей между модулями")));
    QVERIFY(indexText.contains(QStringLiteral("| [SampleWidget](modules/sample_widget.md) | `signal-slot` | [HelperService](modules/helper_service.md) | fileSaved -> reload |")));
    QVERIFY(indexText.contains(QStringLiteral("flowchart TB")));

    QFile sampleModuleFile(outputDirectory.path() + QStringLiteral("/modules/sample_widget.md"));
    QVERIFY(sampleModuleFile.exists());
    QVERIFY(sampleModuleFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString sampleModuleText = QString::fromUtf8(sampleModuleFile.readAll());
    QVERIFY(sampleModuleText.contains(QStringLiteral("Виджет управляет сохранением Markdown-документа")));
    QVERIFY(sampleModuleText.contains(QStringLiteral("Создаёт виджет и принимает зависимость сервиса")));
    QVERIFY(sampleModuleText.contains(QStringLiteral("| `helperService` | `HelperService*` | Сервис для обработки сигнала сохранения. |")));
    QVERIFY(sampleModuleText.contains(QStringLiteral("[HelperService](../modules/helper_service.md)")));
    QVERIFY(sampleModuleText.contains(QStringLiteral("## Таблица связей модуля")));
    QVERIFY(sampleModuleText.contains(QStringLiteral("| `include` | [HelperService](../modules/helper_service.md) | helperservice.h |")));
    QVERIFY(sampleModuleText.contains(QStringLiteral("## Таблица Qt connect-связей")));
    QVERIFY(sampleModuleText.contains(QStringLiteral("| [SampleWidget](../modules/sample_widget.md) | `fileSaved` | [HelperService](../modules/helper_service.md) | `reload` |")));
    QVERIFY(sampleModuleText.contains(QStringLiteral("fileSaved")));
    QVERIFY(sampleModuleText.contains(QStringLiteral("reload")));

    QFile mainFile(outputDirectory.path() + QStringLiteral("/files/main.md"));
    QVERIFY(mainFile.exists());
    QVERIFY(mainFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainText = QString::fromUtf8(mainFile.readAll());
    QVERIFY(mainText.contains(QStringLiteral("Запускает тестовый пример генерации карты кода")));
    QVERIFY(mainText.contains(QStringLiteral("| `argc` | `int` | Количество аргументов командной строки. |")));
}

QTEST_APPLESS_MAIN(CodeMapGeneratorTests)

#include "test_codemapgenerator.moc"
