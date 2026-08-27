#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Beachhead 2000 Parser Viewer");
    app.setOrganizationName("Beachhead2000");

    MainWindow window;
    window.show();

    if (argc > 1) {
        QString filePath = QString::fromLocal8Bit(argv[1]);
        window.loadAndDisplayFile(filePath);
    }

    return app.exec();
}
