#include "mainwindow.h"

#include <QApplication>

/**
 * @brief Запускает графическое приложение просмотра и редактирования Markdown.
 * @param argc Количество аргументов командной строки, переданных приложению.
 * @param argv Массив аргументов командной строки, переданных приложению.
 * @return Код завершения приложения Qt.
 */
int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("MarkDVE"));
    application.setApplicationDisplayName(QStringLiteral("MarkDVE"));
    application.setOrganizationName(QStringLiteral("MarkDVE"));

    MainWindow mainWindow;
    mainWindow.show();

    return application.exec();
}

