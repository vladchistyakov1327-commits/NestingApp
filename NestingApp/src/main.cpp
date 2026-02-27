#include <QApplication>
#include <QStyleFactory>
#include "MainWindow.h"
#include "NestingEngine.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("NestingApp");
    app.setApplicationVersion("2.0");
    app.setOrganizationName("MetalShop");

    // Тёмная тема Fusion
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette p;
    p.setColor(QPalette::Window,          QColor(45,45,48));
    p.setColor(QPalette::WindowText,      Qt::white);
    p.setColor(QPalette::Base,            QColor(28,28,30));
    p.setColor(QPalette::AlternateBase,   QColor(45,45,48));
    p.setColor(QPalette::ToolTipBase,     QColor(28,28,30));
    p.setColor(QPalette::ToolTipText,     Qt::white);
    p.setColor(QPalette::Text,            Qt::white);
    p.setColor(QPalette::Button,          QColor(53,53,53));
    p.setColor(QPalette::ButtonText,      Qt::white);
    p.setColor(QPalette::BrightText,      Qt::red);
    p.setColor(QPalette::Highlight,       QColor(42,130,218));
    p.setColor(QPalette::HighlightedText, Qt::black);
    p.setColor(QPalette::Disabled, QPalette::Text,       QColor(100,100,100));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100,100,100));
    app.setPalette(p);

    qRegisterMetaType<NestingResult>("NestingResult");

    MainWindow w;
    w.show();
    return app.exec();
}
