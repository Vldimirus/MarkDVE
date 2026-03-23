#pragma once

#include <QString>
#include <QStringList>

/**
 * @brief Структура описывает результат разбора Markdown-ссылки внутри рабочей папки.
 *
 * Структура нужна, чтобы единообразно передавать в контроллер путь к файлу,
 * якорь внутри документа и признак внешней ссылки.
 */
struct ResolvedLink
{
    bool isValid = false;               ///< Признак того, что ссылка успешно разобрана.
    bool isExternal = false;            ///< Признак того, что ссылка указывает на внешний ресурс.
    QString filePath;                   ///< Абсолютный путь к Markdown-файлу внутри рабочей папки.
    QString anchor;                     ///< Якорь внутри Markdown-документа без символа '#'.
    QString externalTarget;             ///< Внешний URL для открытия системным обработчиком.
};

/**
 * @brief Класс управляет рабочей папкой проекта и навигацией по Markdown-файлам.
 *
 * Класс хранит путь к корню проекта, собирает список Markdown-файлов и
 * умеет разрешать ссылки относительно текущего документа.
 */
class WorkspaceManager
{
public:
    /**
     * @brief Открывает рабочую папку и собирает Markdown-файлы внутри неё.
     * @param workspacePath Путь к корневой папке проекта, которую нужно открыть.
     * @param errorText Строка для возврата текста ошибки, если открыть папку не удалось.
     * @return true, если рабочая папка успешно открыта и просканирована.
     */
    bool openWorkspace(const QString& workspacePath, QString* errorText = nullptr);

    /**
     * @brief Очищает текущее состояние менеджера рабочей папки.
     *
     * Метод нужен, чтобы сбросить текущий корень проекта и список найденных документов.
     */
    void clear();

    /**
     * @brief Возвращает абсолютный путь к текущей рабочей папке.
     * @return Абсолютный путь к корню проекта или пустая строка, если папка не открыта.
     */
    QString workspaceRootPath() const;

    /**
     * @brief Возвращает список всех найденных Markdown-файлов.
     * @return Список абсолютных путей к `.md` и `.markdown` файлам.
     */
    QStringList markdownFiles() const;

    /**
     * @brief Возвращает относительный путь файла внутри рабочей папки.
     * @param filePath Абсолютный путь к файлу, который нужно сделать относительным.
     * @return Относительный путь от корня рабочей папки.
     */
    QString workspaceRelativePath(const QString& filePath) const;

    /**
     * @brief Проверяет, принадлежит ли файл текущей рабочей папке и является ли Markdown-файлом.
     * @param filePath Абсолютный или относительный путь к проверяемому файлу.
     * @return true, если файл является Markdown-файлом внутри рабочей папки.
     */
    bool containsFile(const QString& filePath) const;

    /**
     * @brief Разрешает Markdown-ссылку относительно исходного документа.
     * @param baseFilePath Абсолютный путь к документу, внутри которого находится ссылка.
     * @param linkTarget Текст ссылки из Markdown, например `modules/viewer.md#tabs`.
     * @return Структура с разобранным путём, якорем и типом ссылки.
     */
    ResolvedLink resolveLink(const QString& baseFilePath, const QString& linkTarget) const;

private:
    /**
     * @brief Проверяет, является ли указанный путь Markdown-файлом.
     * @param filePath Путь к файлу, который нужно проверить.
     * @return true, если расширение соответствует Markdown-файлу.
     */
    static bool isMarkdownFile(const QString& filePath);

    /**
     * @brief Приводит путь к устойчивому абсолютному виду.
     * @param filePath Исходный путь к файлу или папке.
     * @return Канонический путь, а если он недоступен — абсолютный путь.
     */
    static QString normalizedPath(const QString& filePath);

    QString m_workspaceRootPath;        ///< Абсолютный путь к корню открытой рабочей папки.
    QStringList m_markdownFiles;        ///< Отсортированный список Markdown-файлов внутри рабочей папки.
};

