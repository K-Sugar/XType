#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QFontDatabase>
#include <QQuickImageProvider>
#include <QImage>
#include <QDebug>
#ifdef HAVE_KF6_WINDOW_SYSTEM
#include <KWindowSystem>
#include <KWindowEffects>
#endif

#include "config_store.h"
#include "engine_probe.h"
#include "reloader.h"

class GrainProvider : public QQuickImageProvider {
public:
    GrainProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString &, QSize *size, const QSize &requested) override {
        const int w = requested.width()  > 0 ? requested.width()  : 256;
        const int h = requested.height() > 0 ? requested.height() : 256;
        if (size) *size = QSize(w, h);

        QImage img(w, h, QImage::Format_ARGB32);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                uint32_t n = static_cast<uint32_t>(x * 1619 + y * 31337);
                n = (n << 13u) ^ n;
                n = n * (n * n * 15731u + 789221u) + 1376312589u;
                const uint8_t v = static_cast<uint8_t>((n >> 9u) & 0xffu);
                img.setPixel(x, y, qRgba(v, v, v, v >> 2));
            }
        }
        return img;
    }
};

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("xtype-settings");
    QGuiApplication::setApplicationVersion("0.1.0");
    QGuiApplication::setOrganizationName("xtype");

    QQuickStyle::setStyle("Basic");   // strip Breeze before engine loads

    const QString fontBase = ":/qt/qml/XType/Settings/data/fonts/";
    if (QFontDatabase::addApplicationFont(fontBase + "Inter-Variable.ttf") < 0)
        qWarning() << "xtype-settings: failed to load Inter Variable font";
    if (QFontDatabase::addApplicationFont(fontBase + "JetBrainsMono-Variable.ttf") < 0)
        qWarning() << "xtype-settings: failed to load JetBrains Mono Variable font";

    static ConfigStore  configStore;
    static EngineProbe  engineProbe;
    static Reloader     reloader;

    configStore.load();
    qmlRegisterSingletonInstance<ConfigStore> ("XType.Settings", 1, 0, "Config",   &configStore);
    qmlRegisterSingletonInstance<EngineProbe> ("XType.Settings", 1, 0, "Engine",   &engineProbe);
    qmlRegisterSingletonInstance<Reloader>    ("XType.Settings", 1, 0, "Reloader", &reloader);

    QQmlApplicationEngine engine;
    engine.addImageProvider("grain", new GrainProvider);   // must precede loadFromModule
    engine.loadFromModule("XType.Settings", "App");
    if (engine.rootObjects().isEmpty()) return -1;

#ifdef HAVE_KF6_WINDOW_SYSTEM
    if (KWindowSystem::isPlatformWayland()) {
        if (auto *win = qobject_cast<QQuickWindow*>(engine.rootObjects().first()))
            KWindowEffects::enableBlurBehind(win, true);
    }
#endif

    return app.exec();
}
