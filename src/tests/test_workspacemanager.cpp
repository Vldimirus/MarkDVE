#include "workspacemanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

/**
 * @brief Класс содержит модульные тесты для менеджера рабочей папки.
 *
 * Тесты проверяют сканирование Markdown-файлов и корректное разрешение ссылок.
 */
class WorkspaceManagerTests : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief Проверяет, что рабочая папка находит Markdown-файлы и игнорирует остальные.
     */
    void openWorkspaceFindsMarkdownFiles();

    /**
     * @brief Проверяет корректное разрешение относительной ссылки на другой документ.
     */
    void resolveRelativeLinkInsideWorkspace();

    /**
     * @brief Проверяет корректное разрешение якоря внутри текущего документа.
     */
    void resolveAnchorInsideCurrentDocument();

    /**
     * @brief Проверяет блокировку ссылок на файл вне рабочей папки.
     */
    void rejectFileOutsideWorkspace();
};

/**
 * @brief Создаёт тестовую рабочую папку и проверяет поиск Markdown-файлов.
 */
void WorkspaceManagerTests::openWorkspaceFindsMarkdownFiles()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString docsDirectoryPath = temporaryDirectory.path() + QStringLiteral("/docs");
    QVERIFY(QDir().mkpath(docsDirectoryPath));

    QFile markdownFile(docsDirectoryPath + QStringLiteral("/index.md"));
    QVERIFY(markdownFile.open(QIODevice::WriteOnly | QIODevice::Text));
    markdownFile.write("# Index\n");
    markdownFile.close();

    QFile ignoredFile(temporaryDirectory.path() + QStringLiteral("/notes.txt"));
    QVERIFY(ignoredFile.open(QIODevice::WriteOnly | QIODevice::Text));
    ignoredFile.write("ignore\n");
    ignoredFile.close();

    WorkspaceManager workspaceManager;
    QVERIFY(workspaceManager.openWorkspace(temporaryDirectory.path()));
    QCOMPARE(workspaceManager.markdownFiles().size(), 1);
    QVERIFY(workspaceManager.markdownFiles().first().endsWith(QStringLiteral("/docs/index.md")));
}

/**
 * @brief Проверяет корректное разрешение относительной ссылки на другой документ.
 */
void WorkspaceManagerTests::resolveRelativeLinkInsideWorkspace()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QVERIFY(QDir().mkpath(temporaryDirectory.path() + QStringLiteral("/docs/modules")));

    QFile baseFile(temporaryDirectory.path() + QStringLiteral("/docs/index.md"));
    QVERIFY(baseFile.open(QIODevice::WriteOnly | QIODevice::Text));
    baseFile.write("[Viewer](modules/viewer.md)\n");
    baseFile.close();

    QFile targetFile(temporaryDirectory.path() + QStringLiteral("/docs/modules/viewer.md"));
    QVERIFY(targetFile.open(QIODevice::WriteOnly | QIODevice::Text));
    targetFile.write("# Viewer\n");
    targetFile.close();

    WorkspaceManager workspaceManager;
    QVERIFY(workspaceManager.openWorkspace(temporaryDirectory.path()));

    const ResolvedLink resolvedLink = workspaceManager.resolveLink(baseFile.fileName(), QStringLiteral("modules/viewer.md#tabs"));
    QVERIFY(resolvedLink.isValid);
    QCOMPARE(resolvedLink.filePath, QFileInfo(targetFile.fileName()).absoluteFilePath());
    QCOMPARE(resolvedLink.anchor, QStringLiteral("tabs"));
}

/**
 * @brief Проверяет корректное разрешение якоря внутри текущего документа.
 */
void WorkspaceManagerTests::resolveAnchorInsideCurrentDocument()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QVERIFY(QDir().mkpath(temporaryDirectory.path() + QStringLiteral("/docs")));

    QFile baseFile(temporaryDirectory.path() + QStringLiteral("/docs/index.md"));
    QVERIFY(baseFile.open(QIODevice::WriteOnly | QIODevice::Text));
    baseFile.write("[Section](#part)\n");
    baseFile.close();

    WorkspaceManager workspaceManager;
    QVERIFY(workspaceManager.openWorkspace(temporaryDirectory.path()));

    const ResolvedLink resolvedLink = workspaceManager.resolveLink(baseFile.fileName(), QStringLiteral("#part"));
    QVERIFY(resolvedLink.isValid);
    QCOMPARE(resolvedLink.filePath, QFileInfo(baseFile.fileName()).absoluteFilePath());
    QCOMPARE(resolvedLink.anchor, QStringLiteral("part"));
}

/**
 * @brief Проверяет блокировку ссылок на файл вне рабочей папки.
 */
void WorkspaceManagerTests::rejectFileOutsideWorkspace()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QVERIFY(QDir().mkpath(temporaryDirectory.path() + QStringLiteral("/docs")));

    QFile baseFile(temporaryDirectory.path() + QStringLiteral("/docs/index.md"));
    QVERIFY(baseFile.open(QIODevice::WriteOnly | QIODevice::Text));
    baseFile.write("[External](../outside.md)\n");
    baseFile.close();

    QFile outsideFile(temporaryDirectory.path() + QStringLiteral("/outside.md"));
    QVERIFY(outsideFile.open(QIODevice::WriteOnly | QIODevice::Text));
    outsideFile.write("# Outside\n");
    outsideFile.close();

    WorkspaceManager workspaceManager;
    QVERIFY(workspaceManager.openWorkspace(temporaryDirectory.path() + QStringLiteral("/docs")));

    const ResolvedLink resolvedLink = workspaceManager.resolveLink(baseFile.fileName(), QStringLiteral("../outside.md"));
    QVERIFY(!resolvedLink.isValid);
}

QTEST_APPLESS_MAIN(WorkspaceManagerTests)

#include "test_workspacemanager.moc"
