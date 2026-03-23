#pragma once

#include <QString>
#include <QStringList>

/**
 * @brief Структура описывает итог генерации Markdown-карты кода.
 *
 * Структура нужна, чтобы главное окно могло показать пользователю путь к
 * корневому документу и сводную статистику по созданной карте.
 */
struct CodeMapGenerationResult
{
    QString indexFilePath;     ///< Абсолютный путь к корневому файлу `index.md`, созданному генератором.
    int classModuleCount = 0;  ///< Количество созданных страниц модулей-классов.
    int fileModuleCount = 0;   ///< Количество созданных страниц файлов без отдельного класса.
    QStringList warnings;      ///< Список предупреждений генератора, не прерывающих создание карты.
};

/**
 * @brief Генератор строит Markdown-карту C/C++-кода с перекрёстными ссылками.
 *
 * Класс сканирует исходники, извлекает классы, функции, параметры, комментарии
 * и связи между модулями, а затем создаёт готовую иерархию `.md` файлов.
 */
class CodeMapGenerator
{
public:
    /**
     * @brief Создаёт Markdown-карту кода по указанной папке исходников.
     * @param sourceRootPath Абсолютный путь к каталогу с исходными `.h/.hpp/.cpp` файлами.
     * @param outputRootPath Абсолютный путь к каталогу, куда нужно записать готовую карту.
     * @param result Структура для возврата путей и статистики успешной генерации.
     * @param errorText Строка для возврата текста ошибки, если генерация не удалась.
     * @return true, если карта кода успешно построена и сохранена на диск.
     */
    bool generate(const QString& sourceRootPath, const QString& outputRootPath, CodeMapGenerationResult* result = nullptr, QString* errorText = nullptr) const;
};
