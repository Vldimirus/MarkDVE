#include "workspacemanager.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QUrl>

#include <algorithm>

/**
 * @brief Открывает рабочую папку и собирает Markdown-файлы внутри неё.
 * @param workspacePath Путь к корневой папке проекта, которую нужно открыть.
 * @param errorText Строка для возврата текста ошибки, если открыть папку не удалось.
 * @return true, если рабочая папка успешно открыта и просканирована.
 */
bool WorkspaceManager::openWorkspace(const QString& workspacePath, QString* errorText)
{
    const QFileInfo workspaceInfo(workspacePath);
    if (!workspaceInfo.exists() || !workspaceInfo.isDir()) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Указанная рабочая папка не существует или не является каталогом.");
        }
        clear();
        return false;
    }

    m_workspaceRootPath = normalizedPath(workspaceInfo.absoluteFilePath());
    m_markdownFiles.clear();

    QDirIterator iterator(m_workspaceRootPath, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString nextFilePath = iterator.next();
        const QString relativePath = workspaceRelativePath(nextFilePath);
        const bool isIgnoredDirectory = relativePath.startsWith(QStringLiteral("build/"))
            || relativePath.startsWith(QStringLiteral("tmp/"))
            || relativePath.startsWith(QStringLiteral(".git/"));

        if (!isIgnoredDirectory && isMarkdownFile(nextFilePath)) {
            m_markdownFiles.append(normalizedPath(nextFilePath));
        }
    }

    std::sort(m_markdownFiles.begin(), m_markdownFiles.end(),
        [this](const QString& leftPath, const QString& rightPath) {
            const QString leftRelativePath = workspaceRelativePath(leftPath);
            const QString rightRelativePath = workspaceRelativePath(rightPath);
            const bool leftInDocs = leftRelativePath.startsWith(QStringLiteral("docs/")) || leftRelativePath == QStringLiteral("docs");
            const bool rightInDocs = rightRelativePath.startsWith(QStringLiteral("docs/")) || rightRelativePath == QStringLiteral("docs");

            if (leftInDocs != rightInDocs) {
                return leftInDocs;
            }

            return leftRelativePath.localeAwareCompare(rightRelativePath) < 0;
        });

    return true;
}

/**
 * @brief Очищает текущее состояние менеджера рабочей папки.
 *
 * Метод нужен, чтобы сбросить текущий корень проекта и список найденных документов.
 */
void WorkspaceManager::clear()
{
    m_workspaceRootPath.clear();
    m_markdownFiles.clear();
}

/**
 * @brief Возвращает абсолютный путь к текущей рабочей папке.
 * @return Абсолютный путь к корню проекта или пустая строка, если папка не открыта.
 */
QString WorkspaceManager::workspaceRootPath() const
{
    return m_workspaceRootPath;
}

/**
 * @brief Возвращает список всех найденных Markdown-файлов.
 * @return Список абсолютных путей к `.md` и `.markdown` файлам.
 */
QStringList WorkspaceManager::markdownFiles() const
{
    return m_markdownFiles;
}

/**
 * @brief Возвращает относительный путь файла внутри рабочей папки.
 * @param filePath Абсолютный путь к файлу, который нужно сделать относительным.
 * @return Относительный путь от корня рабочей папки.
 */
QString WorkspaceManager::workspaceRelativePath(const QString& filePath) const
{
    if (m_workspaceRootPath.isEmpty()) {
        return filePath;
    }

    const QDir workspaceDirectory(m_workspaceRootPath);
    return workspaceDirectory.relativeFilePath(normalizedPath(filePath));
}

/**
 * @brief Проверяет, принадлежит ли файл текущей рабочей папке и является ли Markdown-файлом.
 * @param filePath Абсолютный или относительный путь к проверяемому файлу.
 * @return true, если файл является Markdown-файлом внутри рабочей папки.
 */
bool WorkspaceManager::containsFile(const QString& filePath) const
{
    if (m_workspaceRootPath.isEmpty()) {
        return false;
    }

    const QString absolutePath = normalizedPath(QDir(m_workspaceRootPath).absoluteFilePath(filePath));
    if (!isMarkdownFile(absolutePath)) {
        return false;
    }

    return absolutePath == m_workspaceRootPath || absolutePath.startsWith(m_workspaceRootPath + QDir::separator());
}

/**
 * @brief Разрешает Markdown-ссылку относительно исходного документа.
 * @param baseFilePath Абсолютный путь к документу, внутри которого находится ссылка.
 * @param linkTarget Текст ссылки из Markdown, например `modules/viewer.md#tabs`.
 * @return Структура с разобранным путём, якорем и типом ссылки.
 */
ResolvedLink WorkspaceManager::resolveLink(const QString& baseFilePath, const QString& linkTarget) const
{
    ResolvedLink resolvedLink;
    const QString trimmedLink = linkTarget.trimmed();
    if (trimmedLink.isEmpty()) {
        return resolvedLink;
    }

    if (trimmedLink.startsWith(QStringLiteral("http://"))
        || trimmedLink.startsWith(QStringLiteral("https://"))
        || trimmedLink.startsWith(QStringLiteral("mailto:"))) {
        resolvedLink.isValid = true;
        resolvedLink.isExternal = true;
        resolvedLink.externalTarget = trimmedLink;
        return resolvedLink;
    }

    const int anchorSeparatorIndex = trimmedLink.indexOf(QLatin1Char('#'));
    const QString rawPathPart = anchorSeparatorIndex >= 0 ? trimmedLink.left(anchorSeparatorIndex) : trimmedLink;
    const QString rawAnchorPart = anchorSeparatorIndex >= 0 ? trimmedLink.mid(anchorSeparatorIndex + 1) : QString();

    QString targetFilePath;
    if (rawPathPart.isEmpty()) {
        targetFilePath = normalizedPath(baseFilePath);
    } else if (QDir::isAbsolutePath(rawPathPart)) {
        targetFilePath = normalizedPath(rawPathPart);
    } else {
        const QFileInfo baseFileInfo(baseFilePath);
        targetFilePath = normalizedPath(baseFileInfo.dir().absoluteFilePath(rawPathPart));
    }

    if (!containsFile(targetFilePath)) {
        return resolvedLink;
    }

    resolvedLink.isValid = true;
    resolvedLink.filePath = targetFilePath;
    resolvedLink.anchor = QUrl::fromPercentEncoding(rawAnchorPart.toUtf8());
    return resolvedLink;
}

/**
 * @brief Проверяет, является ли указанный путь Markdown-файлом.
 * @param filePath Путь к файлу, который нужно проверить.
 * @return true, если расширение соответствует Markdown-файлу.
 */
bool WorkspaceManager::isMarkdownFile(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown");
}

/**
 * @brief Приводит путь к устойчивому абсолютному виду.
 * @param filePath Исходный путь к файлу или папке.
 * @return Канонический путь, а если он недоступен — абсолютный путь.
 */
QString WorkspaceManager::normalizedPath(const QString& filePath)
{
    const QFileInfo fileInfo(filePath);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    if (!canonicalPath.isEmpty()) {
        return canonicalPath;
    }

    return fileInfo.absoluteFilePath();
}
