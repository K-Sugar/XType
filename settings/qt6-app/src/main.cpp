#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("xtype-settings");
    QGuiApplication::setApplicationVersion("0.1.0");
    QGuiApplication::setOrganizationName("xtype");

    QQuickStyle::setStyle("Basic");   // strip Breeze before engine loads

    QQmlApplicationEngine engine;
    engine.loadFromModule("XType.Settings", "App");
    if (engine.rootObjects().isEmpty()) return -1;
    return app.exec();
}
