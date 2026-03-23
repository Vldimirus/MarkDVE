#include "codemapgenerator.h"

#include <algorithm>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStringConverter>
#include <QTextStream>

namespace
{
/**
 * @brief Структура хранит разбор комментария рядом с сущностью кода.
 *
 * Структура нужна, чтобы перенести в Markdown не только краткое описание
 * функции, но и описания её параметров и возвращаемого значения.
 */
struct CodeCommentInfo
{
    QString summary;                             ///< Краткое описание сущности из `@brief` или обычного текста комментария.
    QMap<QString, QString> parameterDescriptions; ///< Описания параметров по имени, извлечённые из `@param`.
    QString returnDescription;                  ///< Описание возвращаемого значения из `@return`.
};

/**
 * @brief Структура описывает один параметр функции в карте кода.
 *
 * Структура нужна, чтобы на страницах модулей можно было показывать таблицу
 * параметров с типами и их текстовыми описаниями из комментариев.
 */
struct CodeMapParameterInfo
{
    QString type;           ///< Тип параметра из сигнатуры функции.
    QString name;           ///< Имя параметра из сигнатуры функции.
    QString description;    ///< Описание параметра из комментария к функции.
};

/**
 * @brief Структура описывает одну функцию или метод для Markdown-карты.
 *
 * Структура хранит исходную сигнатуру, модификаторы и комментарии, чтобы
 * генератор мог построить подробную страницу модуля без повторного разбора.
 */
struct CodeMapFunctionInfo
{
    QString name;                               ///< Имя функции без имени класса.
    QString signature;                          ///< Полная сигнатура функции в упрощённом однострочном виде.
    QString returnType;                         ///< Тип возвращаемого значения, если он есть у функции.
    QString description;                        ///< Краткое описание функции из комментария.
    QString returnDescription;                  ///< Описание возвращаемого значения из `@return`.
    QString accessScope;                        ///< Область доступа функции внутри класса: public/private/protected/signals/slots.
    QString declarationFilePath;                ///< Абсолютный путь к файлу, где была найдена декларация или определение.
    QList<CodeMapParameterInfo> parameters;     ///< Список параметров функции в порядке объявления.
    QStringList referencedTypes;                ///< Имена проектных типов, встреченных в типах параметров и возврата.
    bool isSignal = false;                      ///< Признак того, что функция находится в секции `signals`.
    bool isSlot = false;                        ///< Признак того, что функция объявлена как слот Qt.
    bool isConstructor = false;                 ///< Признак конструктора класса.
    bool isDestructor = false;                  ///< Признак деструктора класса.
    bool isStatic = false;                      ///< Признак статического метода.
    bool isConst = false;                       ///< Признак константного метода.
};

/**
 * @brief Структура описывает одно обнаруженное Qt-соединение `connect(...)`.
 *
 * Структура нужна, чтобы строить Mermaid-ребра и текстовые списки связей между
 * отправителем сигнала и получателем слота.
 */
struct CodeMapConnectionInfo
{
    QString ownerModuleName;    ///< Имя модуля, внутри файла которого найден вызов `connect(...)`.
    QString senderModuleName;   ///< Имя модуля-источника сигнала.
    QString senderSignalName;   ///< Имя сигнала, передаваемого из отправителя.
    QString receiverModuleName; ///< Имя модуля-получателя слота.
    QString receiverSlotName;   ///< Имя слота, принимающего сигнал.
};

/**
 * @brief Структура описывает зависимость одного модуля от другого.
 *
 * Структура нужна для генерации списков зависимостей и диаграмм Mermaid с
 * понятной подписью типа связи.
 */
struct CodeMapDependencyInfo
{
    QString targetModuleName;   ///< Имя зависимого модуля, на который ссылается текущий модуль.
    QString relationType;       ///< Тип связи: `include`, `type` или `signal-slot`.
    QString detailText;         ///< Текстовое уточнение связи для показа в Markdown.
};

/**
 * @brief Структура описывает модуль карты кода, соответствующий классу или файлу.
 *
 * Структура агрегирует все найденные сигнатуры, комментарии и связи, которые
 * затем сериализуются в отдельную Markdown-страницу.
 */
struct CodeMapModuleInfo
{
    QString displayName;                        ///< Имя модуля, показываемое пользователю в заголовках Markdown.
    QString slug;                               ///< Устойчивое имя Markdown-файла без расширения.
    QString summary;                            ///< Краткое описание модуля из комментария класса или файла.
    QString headerFilePath;                     ///< Абсолютный путь к связанному заголовочному файлу модуля.
    QString sourceFilePath;                     ///< Абсолютный путь к связанному `.cpp` файлу модуля.
    QStringList includeFilePaths;               ///< Абсолютные пути проектных include-файлов, найденных в коде модуля.
    QStringList memberTypeHints;                ///< Сырые типы полей и членов класса, используемые для построения зависимостей.
    QList<CodeMapFunctionInfo> functions;       ///< Список функций, принадлежащих модулю.
    QList<CodeMapConnectionInfo> ownedConnections; ///< Qt-связи, обнаруженные в исходниках текущего модуля.
    QList<CodeMapDependencyInfo> dependencies;  ///< Итоговый список зависимостей текущего модуля.
    QString outputRelativePath;                 ///< Относительный путь Markdown-страницы от корня каталога вывода.
    bool isFileModule = false;                  ///< Признак страницы файла без отдельного класса.
};

/**
 * @brief Структура представляет всю собранную модель проекта перед записью Markdown.
 *
 * Структура нужна, чтобы сначала собрать знания о коде из разных файлов, а
 * затем одним проходом сформировать перекрёстные ссылки и диаграммы.
 */
struct CodeMapProjectModel
{
    QString sourceRootPath;                             ///< Абсолютный путь к корню исходников, для которых строится карта.
    QMap<QString, CodeMapModuleInfo> modulesByName;     ///< Карта модулей по их отображаемому имени.
    QMap<QString, QString> pathToModuleName;            ///< Карта абсолютных путей исходников на имя соответствующего модуля.
    QMap<QString, QString> includeToModuleName;         ///< Карта имён include-файлов на имя модуля проекта.
};

/**
 * @brief Структура хранит состояние разбора заголовочного файла с классами.
 *
 * Состояние нужно, чтобы корректно переносить комментарии и режим секций
 * `signals`/`slots` между строками при линейном разборе файла.
 */
struct HeaderParseState
{
    QString currentModuleName;      ///< Имя класса, который разбирается в данный момент.
    QString currentAccessScope = QStringLiteral("private"); ///< Текущая область доступа внутри класса.
    bool currentSignalsSection = false; ///< Признак нахождения внутри секции `signals`.
    bool currentSlotsSection = false;   ///< Признак нахождения внутри секции `slots`.
    int classBraceDepth = 0;         ///< Глубина фигурных скобок текущего класса.
    bool waitingClassOpeningBrace = false; ///< Признак того, что строка `class Name` уже прочитана, но `{` ещё не встретилась.
};

/**
 * @brief Проверяет, относится ли расширение файла к поддерживаемым исходникам C/C++.
 * @param filePath Абсолютный путь к проверяемому файлу.
 * @return true, если файл имеет расширение `.h/.hpp/.c/.cpp` и подобные.
 */
bool isSupportedCodeFile(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QStringLiteral("h")
        || suffix == QStringLiteral("hh")
        || suffix == QStringLiteral("hpp")
        || suffix == QStringLiteral("hxx")
        || suffix == QStringLiteral("c")
        || suffix == QStringLiteral("cc")
        || suffix == QStringLiteral("cpp")
        || suffix == QStringLiteral("cxx");
}

/**
 * @brief Проверяет, является ли файл заголовочным.
 * @param filePath Абсолютный путь к проверяемому файлу.
 * @return true, если расширение файла соответствует заголовку C/C++.
 */
bool isHeaderFile(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QStringLiteral("h")
        || suffix == QStringLiteral("hh")
        || suffix == QStringLiteral("hpp")
        || suffix == QStringLiteral("hxx");
}

/**
 * @brief Приводит путь к устойчивому абсолютному виду.
 * @param filePath Исходный путь к файлу или каталогу.
 * @return Канонический путь, а если он недоступен — абсолютный путь.
 */
QString normalizedPath(const QString& filePath)
{
    const QFileInfo fileInfo(filePath);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    if (!canonicalPath.isEmpty()) {
        return canonicalPath;
    }

    return fileInfo.absoluteFilePath();
}

/**
 * @brief Возвращает безопасный Markdown-заголовок для автогенерируемого файла.
 * @return Готовый префикс Markdown, предупреждающий о том, что файл создан автоматически.
 */
QString generatedFileNotice()
{
    return QStringLiteral(
        "> Этот файл создан автоматически по исходному коду.\n"
        "> Ручные изменения будут перезаписаны при следующей генерации.\n\n");
}

/**
 * @brief Преобразует имя класса в `snake_case` для имени Markdown-файла.
 * @param text Исходное имя класса или идентификатор.
 * @return Строка в нижнем регистре, пригодная для использования в имени файла.
 */
QString toSnakeCase(const QString& text)
{
    QString result; ///< Накопитель символов итогового `snake_case` имени.
    for (int characterIndex = 0; characterIndex < text.size(); ++characterIndex) {
        const QChar nextCharacter = text.at(characterIndex);
        const bool insertUnderscore = characterIndex > 0
            && nextCharacter.isUpper()
            && (text.at(characterIndex - 1).isLower()
                || (characterIndex + 1 < text.size() && text.at(characterIndex + 1).isLower()));

        if (insertUnderscore && !result.endsWith(QLatin1Char('_'))) {
            result.append(QLatin1Char('_'));
        }

        if (nextCharacter.isLetterOrNumber()) {
            result.append(nextCharacter.toLower());
        } else if (!result.endsWith(QLatin1Char('_'))) {
            result.append(QLatin1Char('_'));
        }
    }

    while (result.contains(QStringLiteral("__"))) {
        result.replace(QStringLiteral("__"), QStringLiteral("_"));
    }

    result = result.trimmed();
    while (result.startsWith(QLatin1Char('_'))) {
        result.remove(0, 1);
    }
    while (result.endsWith(QLatin1Char('_'))) {
        result.chop(1);
    }

    return result;
}

/**
 * @brief Создаёт имя Markdown-файла для страницы модуля.
 * @param moduleName Отображаемое имя модуля или класса.
 * @param fallbackBaseName Базовое имя исходного файла, если `moduleName` плохо подходит для slug.
 * @return Нормализованное имя Markdown-файла без расширения.
 */
QString moduleSlug(const QString& moduleName, const QString& fallbackBaseName)
{
    const QString classBasedSlug = toSnakeCase(moduleName);
    if (!classBasedSlug.isEmpty()) {
        return classBasedSlug;
    }

    return toSnakeCase(fallbackBaseName);
}

/**
 * @brief Возвращает строку Markdown-списка с типом функции.
 * @param functionInfo Описание функции, для которой строится текстовый ярлык.
 * @return Короткий человекочитаемый вид функции: `signal`, `slot`, `constructor` и т.п.
 */
QString functionKindLabel(const CodeMapFunctionInfo& functionInfo)
{
    if (functionInfo.isSignal) {
        return QStringLiteral("signal");
    }

    if (functionInfo.isSlot) {
        return QStringLiteral("slot");
    }

    if (functionInfo.isConstructor) {
        return QStringLiteral("constructor");
    }

    if (functionInfo.isDestructor) {
        return QStringLiteral("destructor");
    }

    return QStringLiteral("function");
}

/**
 * @brief Создаёт устойчивый идентификатор узла Mermaid для модуля.
 * @param moduleName Отображаемое имя модуля.
 * @return Текстовый идентификатор узла без пробелов и спецсимволов.
 */
QString mermaidNodeId(const QString& moduleName)
{
    QString nodeId = QStringLiteral("module_") + toSnakeCase(moduleName);
    if (nodeId == QStringLiteral("module_")) {
        nodeId += QStringLiteral("unknown");
    }
    return nodeId;
}

/**
 * @brief Считывает файл целиком в UTF-8 строку.
 * @param filePath Абсолютный путь к файлу, который нужно прочитать.
 * @param text Строка для возврата содержимого файла.
 * @param errorText Строка для возврата текста ошибки чтения.
 * @return true, если файл был успешно открыт и прочитан.
 */
bool readTextFile(const QString& filePath, QString* text, QString* errorText)
{
    QFile inputFile(filePath);
    if (!inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Не удалось открыть файл для чтения: %1").arg(filePath);
        }
        return false;
    }

    QTextStream inputStream(&inputFile);
    inputStream.setEncoding(QStringConverter::Utf8);
    *text = inputStream.readAll();
    return true;
}

/**
 * @brief Записывает строку в файл UTF-8 с безопасной заменой содержимого.
 * @param filePath Абсолютный путь к целевому файлу.
 * @param text Текст, который нужно сохранить на диск.
 * @param errorText Строка для возврата текста ошибки записи.
 * @return true, если файл успешно записан и зафиксирован.
 */
bool writeTextFile(const QString& filePath, const QString& text, QString* errorText)
{
    QSaveFile outputFile(filePath);
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Не удалось открыть файл для записи: %1").arg(filePath);
        }
        return false;
    }

    QTextStream outputStream(&outputFile);
    outputStream.setEncoding(QStringConverter::Utf8);
    outputStream << text;

    if (!outputFile.commit()) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Не удалось зафиксировать запись файла: %1").arg(filePath);
        }
        return false;
    }

    return true;
}

/**
 * @brief Собирает список поддерживаемых исходных файлов внутри каталога.
 * @param sourceRootPath Абсолютный путь к корню исходников.
 * @return Отсортированный список абсолютных путей поддерживаемых файлов C/C++.
 */
QStringList collectCodeFiles(const QString& sourceRootPath)
{
    QStringList filePaths; ///< Накопитель найденных исходников C/C++ внутри каталога проекта.
    QDirIterator iterator(sourceRootPath, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString nextFilePath = iterator.next();
        const QString relativePath = QDir(sourceRootPath).relativeFilePath(nextFilePath);
        const bool isIgnoredDirectory = relativePath.startsWith(QStringLiteral("build/"))
            || relativePath.startsWith(QStringLiteral("tmp/"))
            || relativePath.startsWith(QStringLiteral(".git/"));

        if (!isIgnoredDirectory && isSupportedCodeFile(nextFilePath)) {
            filePaths.append(normalizedPath(nextFilePath));
        }
    }

    std::sort(filePaths.begin(), filePaths.end(), [](const QString& leftPath, const QString& rightPath) {
        return leftPath.localeAwareCompare(rightPath) < 0;
    });
    return filePaths;
}

/**
 * @brief Очищает маркеры C++-комментариев и возвращает чистый текст строки.
 * @param commentLine Строка комментария с символами `///`, `/**` или `*`.
 * @return Очищенный текст комментария без синтаксических маркеров.
 */
QString cleanCommentLine(const QString& commentLine)
{
    QString cleanedLine = commentLine.trimmed();
    cleanedLine.remove(QRegularExpression(QStringLiteral("^/\\*+\\s*")));
    cleanedLine.remove(QRegularExpression(QStringLiteral("^\\*+\\s*")));
    cleanedLine.remove(QRegularExpression(QStringLiteral("^///\\s*")));
    cleanedLine.remove(QRegularExpression(QStringLiteral("^//\\s*")));
    cleanedLine.remove(QRegularExpression(QStringLiteral("\\s*\\*/$")));
    return cleanedLine.trimmed();
}

/**
 * @brief Разбирает текст комментария на описание, параметры и возвращаемое значение.
 * @param rawCommentText Исходный текст комментария без привязки к конкретной сущности.
 * @return Структура с кратким описанием и описаниями параметров.
 */
CodeCommentInfo parseCommentText(const QString& rawCommentText)
{
    CodeCommentInfo parsedComment;
    const QStringList rawLines = rawCommentText.split(QLatin1Char('\n')); ///< Список строк исходного комментария для построчного разбора тегов.
    QStringList summaryLines; ///< Накопитель строк основного описания без тегов `@param` и `@return`.

    for (const QString& rawLine : rawLines) {
        const QString cleanedLine = cleanCommentLine(rawLine);
        if (cleanedLine.isEmpty()) {
            continue;
        }

        QRegularExpressionMatch briefMatch = QRegularExpression(QStringLiteral("^@brief\\s+(.+)$")).match(cleanedLine);
        if (briefMatch.hasMatch()) {
            summaryLines.append(briefMatch.captured(1).trimmed());
            continue;
        }

        QRegularExpressionMatch paramMatch = QRegularExpression(QStringLiteral("^@param\\s+([A-Za-z_][A-Za-z0-9_]*)\\s+(.+)$")).match(cleanedLine);
        if (paramMatch.hasMatch()) {
            parsedComment.parameterDescriptions.insert(paramMatch.captured(1), paramMatch.captured(2).trimmed());
            continue;
        }

        QRegularExpressionMatch returnMatch = QRegularExpression(QStringLiteral("^@return\\s+(.+)$")).match(cleanedLine);
        if (returnMatch.hasMatch()) {
            parsedComment.returnDescription = returnMatch.captured(1).trimmed();
            continue;
        }

        if (!cleanedLine.startsWith(QLatin1Char('@'))) {
            summaryLines.append(cleanedLine);
        }
    }

    parsedComment.summary = summaryLines.join(QLatin1Char(' ')).trimmed();
    return parsedComment;
}

/**
 * @brief Извлекает комментарий, расположенный непосредственно перед кодовой сущностью.
 * @param lines Список строк текущего файла.
 * @param startLineIndex Индекс строки, на которой начинается декларация.
 * @return Структура с кратким описанием и тегами параметров.
 */
CodeCommentInfo extractLeadingComment(const QStringList& lines, int startLineIndex)
{
    QStringList commentLines; ///< Накопитель строк комментария, найденных над целевой декларацией.
    int lineIndex = startLineIndex - 1; ///< Индекс текущей проверяемой строки при движении вверх по файлу.

    while (lineIndex >= 0 && lines.at(lineIndex).trimmed().isEmpty()) {
        --lineIndex;
    }

    if (lineIndex < 0) {
        return {};
    }

    if (lines.at(lineIndex).trimmed().endsWith(QStringLiteral("*/"))) {
        while (lineIndex >= 0) {
            commentLines.prepend(lines.at(lineIndex));
            if (lines.at(lineIndex).contains(QStringLiteral("/**")) || lines.at(lineIndex).contains(QStringLiteral("/*"))) {
                break;
            }
            --lineIndex;
        }
        return parseCommentText(commentLines.join(QLatin1Char('\n')));
    }

    while (lineIndex >= 0) {
        const QString trimmedLine = lines.at(lineIndex).trimmed();
        if (!trimmedLine.startsWith(QStringLiteral("///")) && !trimmedLine.startsWith(QStringLiteral("//"))) {
            break;
        }
        commentLines.prepend(lines.at(lineIndex));
        --lineIndex;
    }

    return parseCommentText(commentLines.join(QLatin1Char('\n')));
}

/**
 * @brief Подсчитывает изменение глубины фигурных скобок в строке.
 * @param text Строка исходного кода, для которой нужно посчитать `{` и `}`.
 * @return Разница между числом открывающих и закрывающих скобок.
 */
int braceDelta(const QString& text)
{
    int delta = 0; ///< Накопленная разница между открывающими и закрывающими фигурными скобками.
    for (const QChar nextCharacter : text) {
        if (nextCharacter == QLatin1Char('{')) {
            ++delta;
        } else if (nextCharacter == QLatin1Char('}')) {
            --delta;
        }
    }
    return delta;
}

/**
 * @brief Делит список параметров функции на отдельные параметры верхнего уровня.
 * @param parameterListText Текст между круглыми скобками функции.
 * @return Список отдельных параметров без деления внутри шаблонов и круглых скобок.
 */
QStringList splitParameters(const QString& parameterListText)
{
    QStringList parameters; ///< Накопитель отдельных параметров функции в исходном порядке.
    QString currentParameter; ///< Буфер текущего параметра до обнаружения разделяющей запятой верхнего уровня.
    int angleDepth = 0; ///< Глубина вложенности угловых скобок шаблонных параметров.
    int roundDepth = 0; ///< Глубина вложенности круглых скобок, чтобы не делить function-pointer параметры.
    int squareDepth = 0; ///< Глубина вложенности квадратных скобок массивов.

    for (const QChar nextCharacter : parameterListText) {
        if (nextCharacter == QLatin1Char('<')) {
            ++angleDepth;
        } else if (nextCharacter == QLatin1Char('>') && angleDepth > 0) {
            --angleDepth;
        } else if (nextCharacter == QLatin1Char('(')) {
            ++roundDepth;
        } else if (nextCharacter == QLatin1Char(')') && roundDepth > 0) {
            --roundDepth;
        } else if (nextCharacter == QLatin1Char('[')) {
            ++squareDepth;
        } else if (nextCharacter == QLatin1Char(']') && squareDepth > 0) {
            --squareDepth;
        } else if (nextCharacter == QLatin1Char(',') && angleDepth == 0 && roundDepth == 0 && squareDepth == 0) {
            parameters.append(currentParameter.trimmed());
            currentParameter.clear();
            continue;
        }

        currentParameter.append(nextCharacter);
    }

    if (!currentParameter.trimmed().isEmpty()) {
        parameters.append(currentParameter.trimmed());
    }

    return parameters;
}

/**
 * @brief Разбирает один параметр функции на тип и имя.
 * @param parameterText Исходный текст параметра из сигнатуры.
 * @param parameterDescriptions Описания параметров из комментария функции по именам.
 * @return Структура с разобранным параметром.
 */
CodeMapParameterInfo parseParameter(const QString& parameterText, const QMap<QString, QString>& parameterDescriptions)
{
    CodeMapParameterInfo parameterInfo;
    QString normalizedParameter = parameterText.trimmed(); ///< Временная строка параметра без лишних пробелов и default-значения.

    const int defaultValueIndex = normalizedParameter.indexOf(QLatin1Char('='));
    if (defaultValueIndex >= 0) {
        normalizedParameter = normalizedParameter.left(defaultValueIndex).trimmed();
    }

    if (normalizedParameter.isEmpty() || normalizedParameter == QStringLiteral("void")) {
        return parameterInfo;
    }

    QRegularExpressionMatch nameMatch = QRegularExpression(QStringLiteral("([A-Za-z_][A-Za-z0-9_]*)(\\s*\\[\\s*\\])?$")).match(normalizedParameter);
    if (nameMatch.hasMatch()) {
        parameterInfo.name = nameMatch.captured(1);
        parameterInfo.type = normalizedParameter.left(nameMatch.capturedStart(1)).trimmed();
        if (nameMatch.captured(2).contains(QLatin1Char('['))) {
            parameterInfo.type = (parameterInfo.type + QStringLiteral(" []")).trimmed();
        }
    } else {
        parameterInfo.type = normalizedParameter;
    }

    parameterInfo.description = parameterDescriptions.value(parameterInfo.name);
    return parameterInfo;
}

/**
 * @brief Извлекает имена проектных типов из произвольного текстового типа C++.
 * @param typeText Исходный текст типа параметра, поля или возвращаемого значения.
 * @param knownModuleNames Множество имён модулей проекта, считающихся валидными типами.
 * @return Список имён модулей, которые встретились внутри указанного типа.
 */
QStringList referencedProjectTypes(const QString& typeText, const QSet<QString>& knownModuleNames)
{
    QSet<QString> referencedTypes; ///< Множество найденных имён проектных типов без повторов.
    const QRegularExpression tokenExpression(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*\\b"));
    QRegularExpressionMatchIterator tokenIterator = tokenExpression.globalMatch(typeText);
    while (tokenIterator.hasNext()) {
        const QString token = tokenIterator.next().captured(0);
        if (knownModuleNames.contains(token)) {
            referencedTypes.insert(token);
        }
    }

    QStringList result = referencedTypes.values();
    std::sort(result.begin(), result.end(), [](const QString& leftValue, const QString& rightValue) {
        return leftValue.localeAwareCompare(rightValue) < 0;
    });
    return result;
}

/**
 * @brief Разбирает текст сигнатуры функции в структурированное представление.
 * @param signatureText Нормализованный однострочный текст сигнатуры функции.
 * @param ownerClassName Имя класса-владельца для определения конструктора и деструктора.
 * @param accessScope Строка области доступа функции внутри класса.
 * @param commentInfo Комментарий функции, из которого берутся описания параметров.
 * @return Структура с разобранной функцией или пустая структура, если сигнатуру распознать не удалось.
 */
CodeMapFunctionInfo parseFunctionSignature(
    const QString& signatureText,
    const QString& ownerClassName,
    const QString& accessScope,
    const CodeCommentInfo& commentInfo)
{
    CodeMapFunctionInfo functionInfo;
    QString normalizedSignature = signatureText.simplified(); ///< Упрощённая однострочная сигнатура функции без лишних переводов строк.

    normalizedSignature.remove(QRegularExpression(QStringLiteral("\\s*=\\s*(0|default|delete)\\s*;?$")));
    normalizedSignature.remove(QRegularExpression(QStringLiteral(";+$")));

    const int openingBracketIndex = normalizedSignature.indexOf(QLatin1Char('('));
    const int closingBracketIndex = normalizedSignature.lastIndexOf(QLatin1Char(')'));
    if (openingBracketIndex <= 0 || closingBracketIndex <= openingBracketIndex) {
        return functionInfo;
    }

    const QString prefixText = normalizedSignature.left(openingBracketIndex).trimmed(); ///< Часть сигнатуры до параметров, содержащая тип возврата и имя функции.
    QString suffixText = normalizedSignature.mid(closingBracketIndex + 1).trimmed(); ///< Часть сигнатуры после списка параметров с `const` и служебными модификаторами.
    const QString parameterListText = normalizedSignature.mid(openingBracketIndex + 1, closingBracketIndex - openingBracketIndex - 1); ///< Исходный текст списка параметров функции.

    QRegularExpressionMatch nameMatch = QRegularExpression(QStringLiteral("([~A-Za-z_][A-Za-z0-9_:~]*)$")).match(prefixText);
    if (!nameMatch.hasMatch()) {
        return functionInfo;
    }

    QString qualifiedName = nameMatch.captured(1); ///< Имя функции с возможным префиксом `Class::`, найденное в сигнатуре.
    const int scopeSeparatorIndex = qualifiedName.lastIndexOf(QStringLiteral("::"));
    if (scopeSeparatorIndex >= 0) {
        qualifiedName = qualifiedName.mid(scopeSeparatorIndex + 2);
    }

    functionInfo.name = qualifiedName;
    functionInfo.signature = normalizedSignature;
    functionInfo.accessScope = accessScope;
    functionInfo.description = commentInfo.summary;
    functionInfo.returnDescription = commentInfo.returnDescription;
    functionInfo.isConstructor = !ownerClassName.isEmpty() && qualifiedName == ownerClassName;
    functionInfo.isDestructor = !ownerClassName.isEmpty() && qualifiedName == QStringLiteral("~%1").arg(ownerClassName);
    functionInfo.isStatic = prefixText.contains(QStringLiteral(" static ")) || prefixText.startsWith(QStringLiteral("static "));
    functionInfo.isConst = suffixText.startsWith(QStringLiteral("const"))
        || suffixText.contains(QStringLiteral(" const "))
        || suffixText.endsWith(QStringLiteral(" const"));

    QString returnType = prefixText.left(nameMatch.capturedStart(1)).trimmed(); ///< Строка предполагаемого возвращаемого типа без имени функции.
    returnType.remove(QRegularExpression(QStringLiteral("^(virtual|static|explicit|inline|constexpr|friend)\\s+")));
    returnType = returnType.trimmed();
    if (!functionInfo.isConstructor && !functionInfo.isDestructor) {
        functionInfo.returnType = returnType;
    }

    const QStringList parameterTexts = splitParameters(parameterListText);
    for (const QString& nextParameterText : parameterTexts) {
        CodeMapParameterInfo parameterInfo = parseParameter(nextParameterText, commentInfo.parameterDescriptions);
        if (!parameterInfo.type.isEmpty() || !parameterInfo.name.isEmpty()) {
            functionInfo.parameters.append(parameterInfo);
        }
    }

    return functionInfo;
}

/**
 * @brief Возвращает или создаёт модуль класса по имени.
 * @param model Общая модель проекта, в которой нужно получить модуль.
 * @param className Имя класса, для которого требуется модуль.
 * @param filePath Абсолютный путь к файлу, из которого был извлечён класс.
 * @return Ссылка на созданную или уже существующую структуру модуля.
 */
CodeMapModuleInfo& ensureClassModule(CodeMapProjectModel* model, const QString& className, const QString& filePath)
{
    CodeMapModuleInfo& moduleInfo = model->modulesByName[className];
    moduleInfo.displayName = className;
    moduleInfo.isFileModule = false;

    const QFileInfo fileInfo(filePath);
    if (moduleInfo.slug.isEmpty()) {
        moduleInfo.slug = moduleSlug(className, fileInfo.completeBaseName());
    }

    if (isHeaderFile(filePath)) {
        moduleInfo.headerFilePath = filePath;
    } else {
        moduleInfo.sourceFilePath = filePath;
    }

    model->pathToModuleName.insert(filePath, className);
    model->includeToModuleName.insert(fileInfo.fileName(), className);
    model->includeToModuleName.insert(QDir(model->sourceRootPath).relativeFilePath(filePath), className);
    return moduleInfo;
}

/**
 * @brief Возвращает или создаёт модуль файла без отдельного класса.
 * @param model Общая модель проекта, в которой нужно получить модуль файла.
 * @param filePath Абсолютный путь к исходному файлу без собственного класса.
 * @return Ссылка на созданную или уже существующую структуру файлового модуля.
 */
CodeMapModuleInfo& ensureFileModule(CodeMapProjectModel* model, const QString& filePath)
{
    const QFileInfo fileInfo(filePath);
    const QString moduleName = fileInfo.fileName();
    CodeMapModuleInfo& moduleInfo = model->modulesByName[moduleName];
    moduleInfo.displayName = moduleName;
    moduleInfo.isFileModule = true;
    moduleInfo.slug = toSnakeCase(fileInfo.completeBaseName());
    moduleInfo.sourceFilePath = filePath;

    model->pathToModuleName.insert(filePath, moduleName);
    return moduleInfo;
}

/**
 * @brief Определяет модуль по исходному файлу на основе полного пути и имени файла.
 * @param model Общая модель проекта с уже обнаруженными модулями.
 * @param filePath Абсолютный путь к заголовку или `.cpp` файлу.
 * @return Имя найденного модуля или пустая строка, если модуль ещё не сопоставлен.
 */
QString findModuleByFile(const CodeMapProjectModel& model, const QString& filePath)
{
    const QString normalizedFilePath = normalizedPath(filePath);
    if (model.pathToModuleName.contains(normalizedFilePath)) {
        return model.pathToModuleName.value(normalizedFilePath);
    }

    const QFileInfo fileInfo(normalizedFilePath);
    const QString sameBaseHeaderPath = normalizedPath(fileInfo.dir().absoluteFilePath(fileInfo.completeBaseName() + QStringLiteral(".h")));
    if (model.pathToModuleName.contains(sameBaseHeaderPath)) {
        return model.pathToModuleName.value(sameBaseHeaderPath);
    }

    const QString sameBaseHppPath = normalizedPath(fileInfo.dir().absoluteFilePath(fileInfo.completeBaseName() + QStringLiteral(".hpp")));
    if (model.pathToModuleName.contains(sameBaseHppPath)) {
        return model.pathToModuleName.value(sameBaseHppPath);
    }

    return QString();
}

/**
 * @brief Добавляет модулю путь `#include`, если он ещё не был записан.
 * @param moduleInfo Модуль, в который нужно добавить зависимость include.
 * @param includeFilePath Абсолютный путь к подключаемому проектному файлу.
 */
void appendUniqueInclude(CodeMapModuleInfo* moduleInfo, const QString& includeFilePath)
{
    if (!includeFilePath.isEmpty() && !moduleInfo->includeFilePaths.contains(includeFilePath)) {
        moduleInfo->includeFilePaths.append(includeFilePath);
    }
}

/**
 * @brief Разбирает заголовочный файл и добавляет в модель классы, методы и поля.
 * @param filePath Абсолютный путь к заголовку, который нужно разобрать.
 * @param model Общая модель проекта, в которую добавляются найденные сущности.
 * @param warnings Список предупреждений генератора, куда можно добавить мягкие ошибки разбора.
 */
void parseHeaderFile(const QString& filePath, CodeMapProjectModel* model, QStringList* warnings)
{
    QString fileText;
    QString errorText;
    if (!readTextFile(filePath, &fileText, &errorText)) {
        warnings->append(errorText);
        return;
    }

    const QStringList lines = fileText.split(QLatin1Char('\n'));
    QStringList headerIncludePaths; ///< Список проектных include-файлов заголовка, который затем будет перенесён в модуль класса.
    for (const QString& nextLine : lines) {
        const QRegularExpressionMatch includeMatch = QRegularExpression(QStringLiteral("^\\s*#include\\s+\"([^\"]+)\"")).match(nextLine.trimmed());
        if (includeMatch.hasMatch()) {
            headerIncludePaths.append(normalizedPath(QFileInfo(filePath).dir().absoluteFilePath(includeMatch.captured(1))));
        }
    }

    HeaderParseState parseState;
    int lineIndex = 0; ///< Индекс текущей строки заголовка при последовательном разборе.

    while (lineIndex < lines.size()) {
        const QString rawLine = lines.at(lineIndex);
        const QString trimmedLine = rawLine.trimmed();

        if (parseState.waitingClassOpeningBrace && trimmedLine.contains(QLatin1Char('{'))) {
            parseState.classBraceDepth += braceDelta(trimmedLine);
            parseState.waitingClassOpeningBrace = false;
            ++lineIndex;
            continue;
        }

        if (parseState.currentModuleName.isEmpty()) {
            const QRegularExpressionMatch classMatch = QRegularExpression(QStringLiteral("^class\\s+([A-Za-z_][A-Za-z0-9_]*)\\b")).match(trimmedLine);
            const bool isForwardDeclaration = classMatch.hasMatch() && trimmedLine.endsWith(QLatin1Char(';')) && !trimmedLine.contains(QLatin1Char('{'));

            if (classMatch.hasMatch() && !isForwardDeclaration) {
                const QString className = classMatch.captured(1);
                CodeMapModuleInfo& moduleInfo = ensureClassModule(model, className, filePath);
                const CodeCommentInfo classComment = extractLeadingComment(lines, lineIndex);
                if (moduleInfo.summary.isEmpty() && !classComment.summary.isEmpty()) {
                    moduleInfo.summary = classComment.summary;
                }
                for (const QString& includeFilePath : headerIncludePaths) {
                    appendUniqueInclude(&moduleInfo, includeFilePath);
                }

                parseState.currentModuleName = className;
                parseState.currentAccessScope = QStringLiteral("private");
                parseState.currentSignalsSection = false;
                parseState.currentSlotsSection = false;
                parseState.classBraceDepth = braceDelta(trimmedLine);
                parseState.waitingClassOpeningBrace = !trimmedLine.contains(QLatin1Char('{'));
            }

            ++lineIndex;
            continue;
        }

        CodeMapModuleInfo& currentModule = model->modulesByName[parseState.currentModuleName];

        if (trimmedLine == QStringLiteral("signals:")) {
            parseState.currentAccessScope = QStringLiteral("signals");
            parseState.currentSignalsSection = true;
            parseState.currentSlotsSection = false;
            parseState.classBraceDepth += braceDelta(trimmedLine);
            ++lineIndex;
            continue;
        }

        const QRegularExpressionMatch accessMatch = QRegularExpression(QStringLiteral("^(public|protected|private)(\\s+slots)?\\s*:$")).match(trimmedLine);
        if (accessMatch.hasMatch()) {
            parseState.currentAccessScope = accessMatch.captured(1);
            parseState.currentSignalsSection = false;
            parseState.currentSlotsSection = accessMatch.captured(2).contains(QStringLiteral("slots"));
            if (parseState.currentSlotsSection) {
                parseState.currentAccessScope += QStringLiteral(" slots");
            }
            parseState.classBraceDepth += braceDelta(trimmedLine);
            ++lineIndex;
            continue;
        }

        const bool looksLikeFunctionStart = parseState.classBraceDepth > 0
            && trimmedLine.contains(QLatin1Char('('))
            && !trimmedLine.startsWith(QStringLiteral("#"))
            && !trimmedLine.startsWith(QStringLiteral("if "))
            && !trimmedLine.startsWith(QStringLiteral("for "))
            && !trimmedLine.startsWith(QStringLiteral("while "))
            && !trimmedLine.startsWith(QStringLiteral("switch "));

        if (looksLikeFunctionStart) {
            const int declarationStartLine = lineIndex; ///< Строка начала декларации функции для извлечения комментария над ней.
            QString declarationText = trimmedLine; ///< Накопленный текст декларации функции, который может занимать несколько строк.
            while (!declarationText.contains(QLatin1Char(';')) && lineIndex + 1 < lines.size()) {
                ++lineIndex;
                declarationText += QLatin1Char(' ');
                declarationText += lines.at(lineIndex).trimmed();
                if (declarationText.contains(QLatin1Char('{'))) {
                    break;
                }
            }

            if (!declarationText.contains(QLatin1Char('{')) && declarationText.contains(QLatin1Char(';'))) {
                const CodeCommentInfo functionComment = extractLeadingComment(lines, declarationStartLine);
                CodeMapFunctionInfo functionInfo = parseFunctionSignature(
                    declarationText,
                    parseState.currentModuleName,
                    parseState.currentAccessScope,
                    functionComment);

                if (!functionInfo.name.isEmpty()) {
                    functionInfo.declarationFilePath = filePath;
                    functionInfo.isSignal = parseState.currentSignalsSection;
                    functionInfo.isSlot = parseState.currentSlotsSection;
                    currentModule.functions.append(functionInfo);
                }
            }

            parseState.classBraceDepth += braceDelta(declarationText);
            ++lineIndex;
            continue;
        }

        const bool looksLikeFieldDeclaration = parseState.classBraceDepth > 0
            && trimmedLine.endsWith(QLatin1Char(';'))
            && !trimmedLine.contains(QLatin1Char('('))
            && !trimmedLine.endsWith(QStringLiteral(":"))
            && !trimmedLine.startsWith(QStringLiteral("using "))
            && !trimmedLine.startsWith(QStringLiteral("typedef "))
            && !trimmedLine.startsWith(QStringLiteral("friend "))
            && !trimmedLine.startsWith(QStringLiteral("enum "))
            && !trimmedLine.startsWith(QStringLiteral("class "));

        if (looksLikeFieldDeclaration) {
            const QRegularExpressionMatch fieldMatch = QRegularExpression(QStringLiteral("^(.+?)\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*(?:=.+)?;")).match(trimmedLine);
            if (fieldMatch.hasMatch()) {
                currentModule.memberTypeHints.append(fieldMatch.captured(1).trimmed());
            }
        }

        parseState.classBraceDepth += braceDelta(trimmedLine);
        if (!parseState.waitingClassOpeningBrace && parseState.classBraceDepth <= 0) {
            parseState = HeaderParseState();
        }

        ++lineIndex;
    }
}

/**
 * @brief Возвращает модуль, который следует считать владельцем `.cpp` файла.
 * @param filePath Абсолютный путь к `.cpp` файлу.
 * @param model Общая модель проекта с уже найденными классами.
 * @return Ссылка на модуль-владелец файла, при необходимости созданный как файловый модуль.
 */
CodeMapModuleInfo& ownerModuleForSourceFile(const QString& filePath, CodeMapProjectModel* model)
{
    const QString knownModuleName = findModuleByFile(*model, filePath);
    if (!knownModuleName.isEmpty()) {
        CodeMapModuleInfo& moduleInfo = model->modulesByName[knownModuleName];
        if (moduleInfo.sourceFilePath.isEmpty()) {
            moduleInfo.sourceFilePath = filePath;
        }
        model->pathToModuleName.insert(filePath, knownModuleName);
        return moduleInfo;
    }

    return ensureFileModule(model, filePath);
}

/**
 * @brief Разбирает исходный `.cpp` файл и извлекает include, connect и free functions.
 * @param filePath Абсолютный путь к `.cpp` файлу, который нужно разобрать.
 * @param model Общая модель проекта, в которую добавляются связи и функции файловых модулей.
 * @param warnings Список предупреждений, пополняемый мягкими ошибками чтения.
 */
void parseSourceFile(const QString& filePath, CodeMapProjectModel* model, QStringList* warnings)
{
    QString fileText;
    QString errorText;
    if (!readTextFile(filePath, &fileText, &errorText)) {
        warnings->append(errorText);
        return;
    }

    CodeMapModuleInfo& ownerModule = ownerModuleForSourceFile(filePath, model);
    const QStringList lines = fileText.split(QLatin1Char('\n'));
    int braceDepth = 0; ///< Текущая глубина фигурных скобок файла для распознавания функций верхнего уровня.
    int lineIndex = 0; ///< Индекс текущей строки `.cpp` файла.

    while (lineIndex < lines.size()) {
        const QString rawLine = lines.at(lineIndex);
        const QString trimmedLine = rawLine.trimmed();

        const QRegularExpressionMatch includeMatch = QRegularExpression(QStringLiteral("^#include\\s+\"([^\"]+)\"")).match(trimmedLine);
        if (includeMatch.hasMatch()) {
            const QString includeAbsolutePath = normalizedPath(QFileInfo(filePath).dir().absoluteFilePath(includeMatch.captured(1)));
            appendUniqueInclude(&ownerModule, includeAbsolutePath);
        }

        if (trimmedLine.contains(QStringLiteral("connect("))) {
            QString connectText = trimmedLine; ///< Накопленный текст вызова `connect(...)`, который может занимать несколько строк.
            int parenthesisBalance = 0; ///< Баланс круглых скобок внутри вызова `connect(...)` для определения его конца.
            for (const QChar nextCharacter : connectText) {
                if (nextCharacter == QLatin1Char('(')) {
                    ++parenthesisBalance;
                } else if (nextCharacter == QLatin1Char(')')) {
                    --parenthesisBalance;
                }
            }

            while (parenthesisBalance > 0 && lineIndex + 1 < lines.size()) {
                ++lineIndex;
                const QString continuationLine = lines.at(lineIndex).trimmed();
                connectText += QLatin1Char(' ');
                connectText += continuationLine;
                for (const QChar nextCharacter : continuationLine) {
                    if (nextCharacter == QLatin1Char('(')) {
                        ++parenthesisBalance;
                    } else if (nextCharacter == QLatin1Char(')')) {
                        --parenthesisBalance;
                    }
                }
            }

            QList<QPair<QString, QString>> methodReferences; ///< Список найденных `Class::method` внутри вызова `connect(...)`.
            QRegularExpressionMatchIterator referenceIterator = QRegularExpression(QStringLiteral("&([A-Za-z_][A-Za-z0-9_]*)::([A-Za-z_][A-Za-z0-9_]*)")).globalMatch(connectText);
            while (referenceIterator.hasNext()) {
                const QRegularExpressionMatch nextReference = referenceIterator.next();
                methodReferences.append({nextReference.captured(1), nextReference.captured(2)});
            }

            if (methodReferences.size() >= 2) {
                const QString senderClassName = methodReferences.at(0).first;
                const QString receiverClassName = methodReferences.at(1).first;
                if (model->modulesByName.contains(senderClassName) && model->modulesByName.contains(receiverClassName)) {
                    CodeMapConnectionInfo connectionInfo;
                    connectionInfo.ownerModuleName = ownerModule.displayName;
                    connectionInfo.senderModuleName = senderClassName;
                    connectionInfo.senderSignalName = methodReferences.at(0).second;
                    connectionInfo.receiverModuleName = receiverClassName;
                    connectionInfo.receiverSlotName = methodReferences.at(1).second;
                    ownerModule.ownedConnections.append(connectionInfo);
                }
            }
        }

        const bool canStartTopLevelFunction = ownerModule.isFileModule
            && braceDepth == 0
            && trimmedLine.contains(QLatin1Char('('))
            && !trimmedLine.startsWith(QStringLiteral("#"))
            && !trimmedLine.startsWith(QStringLiteral("if "))
            && !trimmedLine.startsWith(QStringLiteral("for "))
            && !trimmedLine.startsWith(QStringLiteral("while "))
            && !trimmedLine.startsWith(QStringLiteral("switch "))
            && !trimmedLine.startsWith(QStringLiteral("return "));

        if (canStartTopLevelFunction) {
            const int functionStartLine = lineIndex; ///< Строка, на которой начинается свободная функция верхнего уровня.
            QString declarationText = trimmedLine; ///< Накопленный текст заголовка функции верхнего уровня до первой `{`.
            while (!declarationText.contains(QLatin1Char('{')) && lineIndex + 1 < lines.size()) {
                ++lineIndex;
                declarationText += QLatin1Char(' ');
                declarationText += lines.at(lineIndex).trimmed();
            }

            const bool isDefinition = declarationText.contains(QLatin1Char('{')) && !declarationText.contains(QStringLiteral("::"));
            if (isDefinition) {
                declarationText = declarationText.left(declarationText.indexOf(QLatin1Char('{'))).trimmed();
                const CodeCommentInfo functionComment = extractLeadingComment(lines, functionStartLine);
                CodeMapFunctionInfo functionInfo = parseFunctionSignature(
                    declarationText,
                    QString(),
                    QStringLiteral("file"),
                    functionComment);

                if (!functionInfo.name.isEmpty()) {
                    functionInfo.declarationFilePath = filePath;
                    ownerModule.functions.append(functionInfo);
                }
            }
        }

        braceDepth += braceDelta(trimmedLine);
        ++lineIndex;
    }
}

/**
 * @brief Преобразует include в имя проектного модуля, если оно известно модели.
 * @param includeFilePath Абсолютный путь к include-файлу проекта.
 * @param model Общая модель проекта с картой include-имён.
 * @return Имя целевого модуля проекта или пустая строка, если include не распознан.
 */
QString includeTargetModuleName(const QString& includeFilePath, const CodeMapProjectModel& model)
{
    const QFileInfo includeInfo(includeFilePath);
    const QString relativeIncludePath = QDir(model.sourceRootPath).relativeFilePath(includeFilePath);

    if (model.includeToModuleName.contains(relativeIncludePath)) {
        return model.includeToModuleName.value(relativeIncludePath);
    }

    if (model.includeToModuleName.contains(includeInfo.fileName())) {
        return model.includeToModuleName.value(includeInfo.fileName());
    }

    return QString();
}

/**
 * @brief Добавляет модулю зависимость, если такой записи ещё нет.
 * @param moduleInfo Модуль, в который нужно добавить зависимость.
 * @param targetModuleName Имя зависимого целевого модуля.
 * @param relationType Тип связи: `include`, `type` или `signal-slot`.
 * @param detailText Дополнительный текст пояснения связи.
 */
void appendDependency(CodeMapModuleInfo* moduleInfo, const QString& targetModuleName, const QString& relationType, const QString& detailText)
{
    if (targetModuleName.isEmpty() || targetModuleName == moduleInfo->displayName) {
        return;
    }

    const auto dependencyExists = std::any_of(
        moduleInfo->dependencies.cbegin(),
        moduleInfo->dependencies.cend(),
        [&targetModuleName, &relationType, &detailText](const CodeMapDependencyInfo& dependencyInfo) {
            return dependencyInfo.targetModuleName == targetModuleName
                && dependencyInfo.relationType == relationType
                && dependencyInfo.detailText == detailText;
        });

    if (!dependencyExists) {
        CodeMapDependencyInfo dependencyInfo;
        dependencyInfo.targetModuleName = targetModuleName;
        dependencyInfo.relationType = relationType;
        dependencyInfo.detailText = detailText;
        moduleInfo->dependencies.append(dependencyInfo);
    }
}

/**
 * @brief Заполняет для каждого модуля итоговый список межмодульных зависимостей.
 * @param model Полностью собранная модель проекта с функциями, include и connect.
 */
void buildDependencies(CodeMapProjectModel* model)
{
    const QSet<QString> knownModuleNames = QSet<QString>(model->modulesByName.keyBegin(), model->modulesByName.keyEnd());

    for (auto moduleIterator = model->modulesByName.begin(); moduleIterator != model->modulesByName.end(); ++moduleIterator) {
        CodeMapModuleInfo& moduleInfo = moduleIterator.value();

        for (const QString& includeFilePath : moduleInfo.includeFilePaths) {
            const QString targetModuleName = includeTargetModuleName(includeFilePath, *model);
            appendDependency(&moduleInfo, targetModuleName, QStringLiteral("include"), QFileInfo(includeFilePath).fileName());
        }

        for (const QString& nextMemberType : moduleInfo.memberTypeHints) {
            const QStringList typeNames = referencedProjectTypes(nextMemberType, knownModuleNames);
            for (const QString& nextTypeName : typeNames) {
                appendDependency(&moduleInfo, nextTypeName, QStringLiteral("type"), nextMemberType);
            }
        }

        for (CodeMapFunctionInfo& functionInfo : moduleInfo.functions) {
            const QStringList returnTypeReferences = referencedProjectTypes(functionInfo.returnType, knownModuleNames);
            for (const QString& nextReferencedType : returnTypeReferences) {
                functionInfo.referencedTypes.append(nextReferencedType);
                appendDependency(&moduleInfo, nextReferencedType, QStringLiteral("type"), functionInfo.signature);
            }

            for (const CodeMapParameterInfo& parameterInfo : functionInfo.parameters) {
                const QStringList parameterTypeReferences = referencedProjectTypes(parameterInfo.type, knownModuleNames);
                for (const QString& nextReferencedType : parameterTypeReferences) {
                    functionInfo.referencedTypes.append(nextReferencedType);
                    appendDependency(&moduleInfo, nextReferencedType, QStringLiteral("type"), functionInfo.signature);
                }
            }

            functionInfo.referencedTypes.removeDuplicates();
        }

        for (const CodeMapConnectionInfo& connectionInfo : moduleInfo.ownedConnections) {
            appendDependency(
                &model->modulesByName[connectionInfo.senderModuleName],
                connectionInfo.receiverModuleName,
                QStringLiteral("signal-slot"),
                QStringLiteral("%1 -> %2").arg(connectionInfo.senderSignalName, connectionInfo.receiverSlotName));
        }

        std::sort(moduleInfo.dependencies.begin(), moduleInfo.dependencies.end(),
            [](const CodeMapDependencyInfo& leftDependency, const CodeMapDependencyInfo& rightDependency) {
                if (leftDependency.targetModuleName != rightDependency.targetModuleName) {
                    return leftDependency.targetModuleName.localeAwareCompare(rightDependency.targetModuleName) < 0;
                }

                if (leftDependency.relationType != rightDependency.relationType) {
                    return leftDependency.relationType.localeAwareCompare(rightDependency.relationType) < 0;
                }

                return leftDependency.detailText.localeAwareCompare(rightDependency.detailText) < 0;
            });
    }
}

/**
 * @brief Назначает относительные пути Markdown-файлов для всех страниц карты.
 * @param model Модель проекта, в которой нужно заполнить `outputRelativePath`.
 */
void assignOutputPaths(CodeMapProjectModel* model)
{
    for (auto moduleIterator = model->modulesByName.begin(); moduleIterator != model->modulesByName.end(); ++moduleIterator) {
        CodeMapModuleInfo& moduleInfo = moduleIterator.value();
        moduleInfo.outputRelativePath = moduleInfo.isFileModule
            ? QStringLiteral("files/%1.md").arg(moduleInfo.slug)
            : QStringLiteral("modules/%1.md").arg(moduleInfo.slug);
    }
}

/**
 * @brief Возвращает ссылку на Markdown-страницу целевого модуля относительно текущей страницы.
 * @param targetModule Целевой модуль, на который нужно сослаться.
 * @return Относительный путь ссылки на Markdown-файл целевого модуля.
 */
QString relativeLinkFromModulePage(const CodeMapModuleInfo& targetModule)
{
    return QStringLiteral("../%1").arg(targetModule.outputRelativePath);
}

/**
 * @brief Возвращает короткое русское имя типа узла карты кода.
 * @param moduleInfo Модуль, для которого нужно вернуть человекочитаемый тип.
 * @return Строка `модуль` для классовых страниц или `файл` для файловых страниц.
 */
QString moduleKindText(const CodeMapModuleInfo& moduleInfo)
{
    return moduleInfo.isFileModule ? QStringLiteral("файл") : QStringLiteral("модуль");
}

/**
 * @brief Создаёт Markdown-таблицу всех модулей проекта.
 * @param model Полная модель проекта с вычисленными модулями и зависимостями.
 * @return Markdown-строка с таблицей модулей, их типа, количества функций и числа связей.
 */
QString buildProjectModuleTable(const CodeMapProjectModel& model)
{
    QStringList tableLines;
    tableLines << QStringLiteral("| Модуль | Тип | Функций | Связей | Страница |");
    tableLines << QStringLiteral("| --- | --- | ---: | ---: | --- |");

    for (auto moduleIterator = model.modulesByName.cbegin(); moduleIterator != model.modulesByName.cend(); ++moduleIterator) {
        const CodeMapModuleInfo& moduleInfo = moduleIterator.value();
        tableLines << QStringLiteral("| [%1](%2) | %3 | %4 | %5 | `%6` |")
            .arg(moduleInfo.displayName,
                moduleInfo.outputRelativePath,
                moduleKindText(moduleInfo),
                QString::number(moduleInfo.functions.size()),
                QString::number(moduleInfo.dependencies.size()),
                moduleInfo.outputRelativePath);
    }

    return tableLines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

/**
 * @brief Создаёт Markdown-таблицу всех межмодульных связей проекта.
 * @param model Полная модель проекта с вычисленными зависимостями.
 * @return Markdown-строка с таблицей связей между модулями.
 */
QString buildProjectDependencyTable(const CodeMapProjectModel& model)
{
    QStringList tableLines;
    tableLines << QStringLiteral("| Откуда | Тип связи | Куда | Детали |");
    tableLines << QStringLiteral("| --- | --- | --- | --- |");

    for (auto moduleIterator = model.modulesByName.cbegin(); moduleIterator != model.modulesByName.cend(); ++moduleIterator) {
        const CodeMapModuleInfo& moduleInfo = moduleIterator.value();
        for (const CodeMapDependencyInfo& dependencyInfo : moduleInfo.dependencies) {
            if (!model.modulesByName.contains(dependencyInfo.targetModuleName)) {
                continue;
            }

            const CodeMapModuleInfo& targetModule = model.modulesByName.value(dependencyInfo.targetModuleName);
            const QString detailText = dependencyInfo.detailText.isEmpty() ? QStringLiteral("—") : dependencyInfo.detailText;
            tableLines << QStringLiteral("| [%1](%2) | `%3` | [%4](%5) | %6 |")
                .arg(moduleInfo.displayName,
                    moduleInfo.outputRelativePath,
                    dependencyInfo.relationType,
                    targetModule.displayName,
                    targetModule.outputRelativePath,
                    detailText);
        }
    }

    if (tableLines.size() == 2) {
        return QStringLiteral("Связи между модулями не обнаружены.\n");
    }

    return tableLines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

/**
 * @brief Создаёт Markdown-таблицу локальных связей выбранного модуля.
 * @param moduleInfo Модуль, для которого нужно построить таблицу зависимостей.
 * @param model Полная модель проекта с доступом к известным модулям.
 * @return Markdown-строка с таблицей связей текущего модуля.
 */
QString buildModuleDependencyTable(const CodeMapModuleInfo& moduleInfo, const CodeMapProjectModel& model)
{
    if (moduleInfo.dependencies.isEmpty()) {
        return QStringLiteral("Связи модуля не обнаружены.\n");
    }

    QStringList tableLines;
    tableLines << QStringLiteral("| Тип связи | Целевой модуль | Детали |");
    tableLines << QStringLiteral("| --- | --- | --- |");

    for (const CodeMapDependencyInfo& dependencyInfo : moduleInfo.dependencies) {
        if (!model.modulesByName.contains(dependencyInfo.targetModuleName)) {
            continue;
        }

        const CodeMapModuleInfo& targetModule = model.modulesByName.value(dependencyInfo.targetModuleName);
        const QString detailText = dependencyInfo.detailText.isEmpty() ? QStringLiteral("—") : dependencyInfo.detailText;
        tableLines << QStringLiteral("| `%1` | [%2](%3) | %4 |")
            .arg(dependencyInfo.relationType,
                targetModule.displayName,
                relativeLinkFromModulePage(targetModule),
                detailText);
    }

    return tableLines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

/**
 * @brief Создаёт Markdown-таблицу локальных Qt `connect(...)`, найденных в модуле.
 * @param moduleInfo Модуль, для которого нужно вывести таблицу соединений.
 * @param model Полная модель проекта с доступом к именам целевых модулей.
 * @return Markdown-строка с таблицей локальных сигнал-слот связей.
 */
QString buildConnectionTable(const CodeMapModuleInfo& moduleInfo, const CodeMapProjectModel& model)
{
    if (moduleInfo.ownedConnections.isEmpty()) {
        return QStringLiteral("Qt connect-связи в исходниках модуля не обнаружены.\n");
    }

    QStringList tableLines;
    tableLines << QStringLiteral("| Отправитель | Сигнал | Получатель | Слот |");
    tableLines << QStringLiteral("| --- | --- | --- | --- |");

    for (const CodeMapConnectionInfo& connectionInfo : moduleInfo.ownedConnections) {
        if (!model.modulesByName.contains(connectionInfo.senderModuleName) || !model.modulesByName.contains(connectionInfo.receiverModuleName)) {
            continue;
        }

        const CodeMapModuleInfo& senderModule = model.modulesByName.value(connectionInfo.senderModuleName);
        const CodeMapModuleInfo& receiverModule = model.modulesByName.value(connectionInfo.receiverModuleName);
        tableLines << QStringLiteral("| [%1](%2) | `%3` | [%4](%5) | `%6` |")
            .arg(senderModule.displayName,
                relativeLinkFromModulePage(senderModule),
                connectionInfo.senderSignalName,
                receiverModule.displayName,
                relativeLinkFromModulePage(receiverModule),
                connectionInfo.receiverSlotName);
    }

    return tableLines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

/**
 * @brief Создаёт Markdown-блок Mermaid с графом зависимостей проекта.
 * @param model Полная модель проекта с вычисленными связями между модулями.
 * @return Markdown-строка с Mermaid-графом или пояснение об отсутствии связей.
 */
QString buildProjectMermaidGraph(const CodeMapProjectModel& model)
{
    QStringList graphLines;
    graphLines << QStringLiteral("```mermaid");
    graphLines << QStringLiteral("flowchart TB");
    graphLines << QStringLiteral("    classDef classModule fill:#EAF2FF,stroke:#1A73E8,stroke-width:1px,color:#202124;");
    graphLines << QStringLiteral("    classDef fileModule fill:#F1F3F4,stroke:#5F6368,stroke-width:1px,color:#202124;");

    bool hasClassModules = false; ///< Признак наличия классовых модулей для отдельного Mermaid-подграфа.
    bool hasFileModules = false; ///< Признак наличия файловых модулей для отдельного Mermaid-подграфа.
    for (auto moduleIterator = model.modulesByName.cbegin(); moduleIterator != model.modulesByName.cend(); ++moduleIterator) {
        hasClassModules = hasClassModules || !moduleIterator.value().isFileModule;
        hasFileModules = hasFileModules || moduleIterator.value().isFileModule;
    }

    if (hasClassModules) {
        graphLines << QStringLiteral("    subgraph class_modules[\"Классовые модули\"]");
        for (auto moduleIterator = model.modulesByName.cbegin(); moduleIterator != model.modulesByName.cend(); ++moduleIterator) {
            const CodeMapModuleInfo& moduleInfo = moduleIterator.value();
            if (moduleInfo.isFileModule) {
                continue;
            }

            graphLines << QStringLiteral("        %1[\"%2<br/>функций: %3<br/>связей: %4\"]")
                .arg(mermaidNodeId(moduleInfo.displayName), moduleInfo.displayName, QString::number(moduleInfo.functions.size()), QString::number(moduleInfo.dependencies.size()));
            graphLines << QStringLiteral("        class %1 classModule").arg(mermaidNodeId(moduleInfo.displayName));
        }
        graphLines << QStringLiteral("    end");
    }

    if (hasFileModules) {
        graphLines << QStringLiteral("    subgraph file_modules[\"Файловые узлы\"]");
        for (auto moduleIterator = model.modulesByName.cbegin(); moduleIterator != model.modulesByName.cend(); ++moduleIterator) {
            const CodeMapModuleInfo& moduleInfo = moduleIterator.value();
            if (!moduleInfo.isFileModule) {
                continue;
            }

            graphLines << QStringLiteral("        %1[\"%2<br/>функций: %3<br/>связей: %4\"]")
                .arg(mermaidNodeId(moduleInfo.displayName), moduleInfo.displayName, QString::number(moduleInfo.functions.size()), QString::number(moduleInfo.dependencies.size()));
            graphLines << QStringLiteral("        class %1 fileModule").arg(mermaidNodeId(moduleInfo.displayName));
        }
        graphLines << QStringLiteral("    end");
    }

    QSet<QString> seenEdges; ///< Множество уже добавленных рёбер Mermaid для защиты от дублей.
    for (auto moduleIterator = model.modulesByName.cbegin(); moduleIterator != model.modulesByName.cend(); ++moduleIterator) {
        const CodeMapModuleInfo& moduleInfo = moduleIterator.value();
        for (const CodeMapDependencyInfo& dependencyInfo : moduleInfo.dependencies) {
            if (!model.modulesByName.contains(dependencyInfo.targetModuleName)) {
                continue;
            }

            const QString edgeKey = QStringLiteral("%1|%2|%3").arg(moduleInfo.displayName, dependencyInfo.targetModuleName, dependencyInfo.relationType);
            if (seenEdges.contains(edgeKey)) {
                continue;
            }

            seenEdges.insert(edgeKey);
            graphLines << QStringLiteral("    %1 -->|%2| %3")
                .arg(mermaidNodeId(moduleInfo.displayName), dependencyInfo.relationType, mermaidNodeId(dependencyInfo.targetModuleName));
        }
    }

    graphLines << QStringLiteral("```");
    return graphLines.join(QLatin1Char('\n'));
}

/**
 * @brief Создаёт Mermaid-граф локальных зависимостей одного модуля.
 * @param moduleInfo Модуль, для которого строится локальная диаграмма.
 * @param model Полная модель проекта с доступом к целевым модулям.
 * @return Markdown-строка с Mermaid-графом локальных связей модуля.
 */
QString buildModuleMermaidGraph(const CodeMapModuleInfo& moduleInfo, const CodeMapProjectModel& model)
{
    QStringList graphLines;
    graphLines << QStringLiteral("```mermaid");
    graphLines << QStringLiteral("flowchart TB");
    graphLines << QStringLiteral("    classDef currentModule fill:#EAF2FF,stroke:#1A73E8,stroke-width:2px,color:#202124;");
    graphLines << QStringLiteral("    classDef linkedModule fill:#F8F9FA,stroke:#5F6368,stroke-width:1px,color:#202124;");
    graphLines << QStringLiteral("    %1[\"%2<br/>функций: %3<br/>связей: %4\"]")
        .arg(mermaidNodeId(moduleInfo.displayName), moduleInfo.displayName, QString::number(moduleInfo.functions.size()), QString::number(moduleInfo.dependencies.size()));
    graphLines << QStringLiteral("    class %1 currentModule").arg(mermaidNodeId(moduleInfo.displayName));

    QSet<QString> targetNames; ///< Множество целевых модулей для локального графа без повторов.
    for (const CodeMapDependencyInfo& dependencyInfo : moduleInfo.dependencies) {
        targetNames.insert(dependencyInfo.targetModuleName);
    }

    for (const QString& targetModuleName : targetNames) {
        if (!model.modulesByName.contains(targetModuleName)) {
            continue;
        }

        const CodeMapModuleInfo& targetModule = model.modulesByName.value(targetModuleName);
        graphLines << QStringLiteral("    %1[\"%2<br/>функций: %3<br/>связей: %4\"]")
            .arg(mermaidNodeId(targetModuleName), targetModuleName, QString::number(targetModule.functions.size()), QString::number(targetModule.dependencies.size()));
        graphLines << QStringLiteral("    class %1 linkedModule").arg(mermaidNodeId(targetModuleName));
    }

    for (const CodeMapDependencyInfo& dependencyInfo : moduleInfo.dependencies) {
        if (!model.modulesByName.contains(dependencyInfo.targetModuleName)) {
            continue;
        }

        graphLines << QStringLiteral("    %1 -->|%2| %3")
            .arg(mermaidNodeId(moduleInfo.displayName), dependencyInfo.relationType, mermaidNodeId(dependencyInfo.targetModuleName));
    }

    graphLines << QStringLiteral("```");
    return graphLines.join(QLatin1Char('\n'));
}

/**
 * @brief Создаёт Markdown-таблицу параметров функции.
 * @param functionInfo Функция, для которой нужно построить таблицу параметров.
 * @return Markdown-строка с таблицей параметров или сообщение об их отсутствии.
 */
QString buildParameterTable(const CodeMapFunctionInfo& functionInfo)
{
    if (functionInfo.parameters.isEmpty()) {
        return QStringLiteral("Параметры отсутствуют.\n");
    }

    QStringList lines;
    lines << QStringLiteral("| Имя | Тип | Описание |");
    lines << QStringLiteral("| --- | --- | --- |");

    for (const CodeMapParameterInfo& parameterInfo : functionInfo.parameters) {
        const QString parameterDescription = parameterInfo.description.isEmpty()
            ? QStringLiteral("Описание параметра отсутствует")
            : parameterInfo.description;
        lines << QStringLiteral("| `%1` | `%2` | %3 |")
            .arg(parameterInfo.name.isEmpty() ? QStringLiteral("—") : parameterInfo.name,
                parameterInfo.type.isEmpty() ? QStringLiteral("—") : parameterInfo.type,
                parameterDescription);
    }

    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

/**
 * @brief Создаёт Markdown-секцию со списком зависимостей модуля.
 * @param moduleInfo Модуль, для которого нужно вывести зависимости.
 * @param model Полная модель проекта для разрешения ссылок на страницы модулей.
 * @return Markdown-строка со списком зависимостей.
 */
QString buildDependencySection(const CodeMapModuleInfo& moduleInfo, const CodeMapProjectModel& model)
{
    if (moduleInfo.dependencies.isEmpty()) {
        return QStringLiteral("Зависимости не обнаружены.\n");
    }

    QStringList lines;
    for (const CodeMapDependencyInfo& dependencyInfo : moduleInfo.dependencies) {
        if (!model.modulesByName.contains(dependencyInfo.targetModuleName)) {
            continue;
        }

        const CodeMapModuleInfo& targetModule = model.modulesByName.value(dependencyInfo.targetModuleName);
        const QString targetLink = relativeLinkFromModulePage(targetModule);
        const QString detailText = dependencyInfo.detailText.isEmpty() ? QStringLiteral("без уточнения") : dependencyInfo.detailText;
        lines << QStringLiteral("- [%1](%2) — `%3`, %4")
            .arg(targetModule.displayName, targetLink, dependencyInfo.relationType, detailText);
    }

    if (lines.isEmpty()) {
        return QStringLiteral("Зависимости не обнаружены.\n");
    }

    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

/**
 * @brief Создаёт Markdown-секцию локальных Qt `connect(...)`, найденных в модуле.
 * @param moduleInfo Модуль, для которого нужно вывести список соединений.
 * @param model Полная модель проекта с доступом к именам целевых модулей.
 * @return Markdown-строка с локальными сигнал-слот связями.
 */
QString buildConnectionSection(const CodeMapModuleInfo& moduleInfo, const CodeMapProjectModel& model)
{
    if (moduleInfo.ownedConnections.isEmpty()) {
        return QStringLiteral("Qt connect-связи в исходниках модуля не обнаружены.\n");
    }

    QStringList lines;
    for (const CodeMapConnectionInfo& connectionInfo : moduleInfo.ownedConnections) {
        if (!model.modulesByName.contains(connectionInfo.senderModuleName) || !model.modulesByName.contains(connectionInfo.receiverModuleName)) {
            continue;
        }

        const CodeMapModuleInfo& senderModule = model.modulesByName.value(connectionInfo.senderModuleName);
        const CodeMapModuleInfo& receiverModule = model.modulesByName.value(connectionInfo.receiverModuleName);
        lines << QStringLiteral("- [%1](%2)::`%3` -> [%4](%5)::`%6`")
            .arg(senderModule.displayName,
                relativeLinkFromModulePage(senderModule),
                connectionInfo.senderSignalName,
                receiverModule.displayName,
                relativeLinkFromModulePage(receiverModule),
                connectionInfo.receiverSlotName);
    }

    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

/**
 * @brief Создаёт Markdown-страницу одного модуля или файлового узла.
 * @param moduleInfo Модуль, для которого нужно построить страницу.
 * @param model Полная модель проекта с доступом к зависимостям и ссылкам.
 * @return Готовый Markdown-текст страницы указанного модуля.
 */
QString buildModuleMarkdown(const CodeMapModuleInfo& moduleInfo, const CodeMapProjectModel& model)
{
    QString markdownText;
    QTextStream markdownStream(&markdownText);
    markdownStream << "# " << moduleInfo.displayName << "\n\n";
    markdownStream << generatedFileNotice();
    markdownStream << "- Тип: " << (moduleInfo.isFileModule ? QStringLiteral("файл") : QStringLiteral("модуль")) << "\n";
    if (!moduleInfo.headerFilePath.isEmpty()) {
        markdownStream << "- Заголовок: `" << QDir(model.sourceRootPath).relativeFilePath(moduleInfo.headerFilePath) << "`\n";
    }
    if (!moduleInfo.sourceFilePath.isEmpty()) {
        markdownStream << "- Реализация: `" << QDir(model.sourceRootPath).relativeFilePath(moduleInfo.sourceFilePath) << "`\n";
    }
    markdownStream << '\n';

    markdownStream << "## Кратко\n\n";
    markdownStream << (moduleInfo.summary.isEmpty() ? QStringLiteral("Описание отсутствует.") : moduleInfo.summary) << "\n\n";

    markdownStream << "## Таблица связей модуля\n\n";
    markdownStream << buildModuleDependencyTable(moduleInfo, model) << "\n\n";

    markdownStream << "## Mermaid-диаграмма связей\n\n";
    markdownStream << buildModuleMermaidGraph(moduleInfo, model) << "\n\n";

    markdownStream << "## Функции\n\n";
    if (moduleInfo.functions.isEmpty()) {
        markdownStream << "Функции не обнаружены.\n\n";
    } else {
        for (const CodeMapFunctionInfo& functionInfo : moduleInfo.functions) {
            markdownStream << "### `" << functionInfo.signature << "`\n\n";
            markdownStream << "- Вид: `" << functionKindLabel(functionInfo) << "`\n";
            markdownStream << "- Доступ: `" << functionInfo.accessScope << "`\n";
            markdownStream << "- Исходник: `" << QDir(model.sourceRootPath).relativeFilePath(functionInfo.declarationFilePath) << "`\n";
            if (!functionInfo.returnType.isEmpty()) {
                markdownStream << "- Возвращает: `" << functionInfo.returnType << "`\n";
            }
            if (!functionInfo.referencedTypes.isEmpty()) {
                markdownStream << "- Используемые проектные типы: ";
                for (int typeIndex = 0; typeIndex < functionInfo.referencedTypes.size(); ++typeIndex) {
                    const QString& typeName = functionInfo.referencedTypes.at(typeIndex);
                    const CodeMapModuleInfo& targetModule = model.modulesByName.value(typeName);
                    if (typeIndex > 0) {
                        markdownStream << ", ";
                    }
                    markdownStream << "[" << targetModule.displayName << "](" << relativeLinkFromModulePage(targetModule) << ")";
                }
                markdownStream << "\n";
            }
            markdownStream << "\n";
            markdownStream << (functionInfo.description.isEmpty() ? QStringLiteral("Описание отсутствует.") : functionInfo.description) << "\n\n";
            markdownStream << "#### Параметры\n\n";
            markdownStream << buildParameterTable(functionInfo) << "\n";
            if (!functionInfo.returnType.isEmpty()) {
                markdownStream << "#### Возвращаемое значение\n\n";
                markdownStream << (functionInfo.returnDescription.isEmpty()
                    ? QStringLiteral("Описание возвращаемого значения отсутствует.")
                    : functionInfo.returnDescription)
                    << "\n\n";
            }
        }
    }

    markdownStream << "## Зависимости\n\n";
    markdownStream << buildDependencySection(moduleInfo, model) << "\n";

    markdownStream << "## Таблица Qt connect-связей\n\n";
    markdownStream << buildConnectionTable(moduleInfo, model);
    return markdownText;
}

/**
 * @brief Создаёт Markdown-корень карты кода со ссылками на все модули.
 * @param model Полная модель проекта с уже вычисленными модулями и связями.
 * @return Готовый Markdown-текст корневого файла `index.md`.
 */
QString buildIndexMarkdown(const CodeMapProjectModel& model)
{
    QString markdownText;
    QTextStream markdownStream(&markdownText);
    markdownStream << "# Карта кода\n\n";
    markdownStream << generatedFileNotice();
    markdownStream << "## Источник\n\n";
    markdownStream << "`" << model.sourceRootPath << "`\n\n";

    markdownStream << "## Таблица модулей\n\n";
    markdownStream << buildProjectModuleTable(model) << "\n\n";

    markdownStream << "## Таблица связей между модулями\n\n";
    markdownStream << buildProjectDependencyTable(model) << "\n\n";

    markdownStream << "## Mermaid-диаграмма модулей\n\n";
    markdownStream << buildProjectMermaidGraph(model) << "\n\n";

    markdownStream << "## Модули\n\n";
    for (auto moduleIterator = model.modulesByName.cbegin(); moduleIterator != model.modulesByName.cend(); ++moduleIterator) {
        const CodeMapModuleInfo& moduleInfo = moduleIterator.value();
        if (moduleInfo.isFileModule) {
            continue;
        }

        markdownStream << "- [" << moduleInfo.displayName << "](" << moduleInfo.outputRelativePath << ")";
        if (!moduleInfo.summary.isEmpty()) {
            markdownStream << " — " << moduleInfo.summary;
        }
        markdownStream << "\n";
    }

    markdownStream << "\n## Файлы без отдельного класса\n\n";
    bool hasFileModules = false; ///< Признак того, что проект содержит `.cpp` файлы без собственного класса.
    for (auto moduleIterator = model.modulesByName.cbegin(); moduleIterator != model.modulesByName.cend(); ++moduleIterator) {
        const CodeMapModuleInfo& moduleInfo = moduleIterator.value();
        if (!moduleInfo.isFileModule) {
            continue;
        }

        hasFileModules = true;
        markdownStream << "- [" << moduleInfo.displayName << "](" << moduleInfo.outputRelativePath << ")";
        if (!moduleInfo.summary.isEmpty()) {
            markdownStream << " — " << moduleInfo.summary;
        }
        markdownStream << "\n";
    }

    if (!hasFileModules) {
        markdownStream << "Файлы без отдельного класса не обнаружены.\n";
    }

    return markdownText;
}

/**
 * @brief Готовит каталог вывода: создаёт его или очищает для полной пересборки.
 * @param outputRootPath Абсолютный путь к каталогу вывода Markdown-карты.
 * @param errorText Строка для возврата текста ошибки подготовки каталога.
 * @return true, если каталог вывода успешно создан или очищен.
 */
bool prepareOutputDirectory(const QString& outputRootPath, QString* errorText)
{
    QDir outputDirectory(outputRootPath);
    if (outputDirectory.exists() && !outputDirectory.removeRecursively()) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Не удалось очистить каталог вывода: %1").arg(outputRootPath);
        }
        return false;
    }

    if (!QDir().mkpath(outputRootPath)) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Не удалось создать каталог вывода: %1").arg(outputRootPath);
        }
        return false;
    }

    if (!QDir().mkpath(QDir(outputRootPath).filePath(QStringLiteral("modules")))
        || !QDir().mkpath(QDir(outputRootPath).filePath(QStringLiteral("files")))) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Не удалось создать подкаталоги `modules` и `files` в каталоге вывода.");
        }
        return false;
    }

    return true;
}

/**
 * @brief Записывает всю Markdown-иерархию модели в каталог вывода.
 * @param model Полная модель проекта, для которой уже назначены пути Markdown-страниц.
 * @param outputRootPath Абсолютный путь к корню каталога вывода.
 * @param errorText Строка для возврата текста ошибки записи.
 * @return true, если все файлы Markdown были успешно сохранены.
 */
bool writeMarkdownHierarchy(const CodeMapProjectModel& model, const QString& outputRootPath, QString* errorText)
{
    if (!writeTextFile(QDir(outputRootPath).filePath(QStringLiteral("index.md")), buildIndexMarkdown(model), errorText)) {
        return false;
    }

    for (auto moduleIterator = model.modulesByName.cbegin(); moduleIterator != model.modulesByName.cend(); ++moduleIterator) {
        const CodeMapModuleInfo& moduleInfo = moduleIterator.value();
        const QString absoluteMarkdownPath = QDir(outputRootPath).filePath(moduleInfo.outputRelativePath);
        if (!writeTextFile(absoluteMarkdownPath, buildModuleMarkdown(moduleInfo, model), errorText)) {
            return false;
        }
    }

    return true;
}
}

/**
 * @brief Создаёт Markdown-карту кода по указанной папке исходников.
 * @param sourceRootPath Абсолютный путь к каталогу с исходными `.h/.hpp/.cpp` файлами.
 * @param outputRootPath Абсолютный путь к каталогу, куда нужно записать готовую карту.
 * @param result Структура для возврата путей и статистики успешной генерации.
 * @param errorText Строка для возврата текста ошибки, если генерация не удалась.
 * @return true, если карта кода успешно построена и сохранена на диск.
 */
bool CodeMapGenerator::generate(
    const QString& sourceRootPath,
    const QString& outputRootPath,
    CodeMapGenerationResult* result,
    QString* errorText) const
{
    const QString normalizedSourceRoot = normalizedPath(sourceRootPath);
    const QString normalizedOutputRoot = normalizedPath(outputRootPath);
    const QFileInfo sourceInfo(normalizedSourceRoot);

    if (!sourceInfo.exists() || !sourceInfo.isDir()) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Папка исходного кода не существует или не является каталогом.");
        }
        return false;
    }

    if (normalizedSourceRoot == normalizedOutputRoot) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Папка вывода не должна совпадать с папкой исходного кода.");
        }
        return false;
    }

    if (normalizedSourceRoot.startsWith(normalizedOutputRoot + QDir::separator())) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Папка вывода не должна быть родительской папкой для исходников.");
        }
        return false;
    }

    QStringList warnings; ///< Список мягких предупреждений генератора, возвращаемый в результат без остановки процесса.
    CodeMapProjectModel projectModel;
    projectModel.sourceRootPath = normalizedSourceRoot;

    if (!prepareOutputDirectory(normalizedOutputRoot, errorText)) {
        return false;
    }

    const QStringList codeFiles = collectCodeFiles(normalizedSourceRoot);
    if (codeFiles.isEmpty()) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("В указанной папке не найдено поддерживаемых файлов `.h/.hpp/.cpp`.");
        }
        return false;
    }

    for (const QString& nextFilePath : codeFiles) {
        if (isHeaderFile(nextFilePath)) {
            parseHeaderFile(nextFilePath, &projectModel, &warnings);
        }
    }

    for (const QString& nextFilePath : codeFiles) {
        if (!isHeaderFile(nextFilePath)) {
            parseSourceFile(nextFilePath, &projectModel, &warnings);
        }
    }

    buildDependencies(&projectModel);
    assignOutputPaths(&projectModel);

    if (!writeMarkdownHierarchy(projectModel, normalizedOutputRoot, errorText)) {
        return false;
    }

    if (result != nullptr) {
        result->indexFilePath = QDir(normalizedOutputRoot).filePath(QStringLiteral("index.md"));
        result->warnings = warnings;
        result->classModuleCount = std::count_if(
            projectModel.modulesByName.cbegin(),
            projectModel.modulesByName.cend(),
            [](const CodeMapModuleInfo& moduleInfo) {
                return !moduleInfo.isFileModule;
            });
        result->fileModuleCount = std::count_if(
            projectModel.modulesByName.cbegin(),
            projectModel.modulesByName.cend(),
            [](const CodeMapModuleInfo& moduleInfo) {
                return moduleInfo.isFileModule;
            });
    }

    return true;
}
