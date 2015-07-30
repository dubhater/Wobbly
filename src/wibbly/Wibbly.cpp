#include <QApplication>

#include "WibblyWindow.h"


#ifdef WOBBLY_STATIC_QT

#ifdef _WIN32
#include <QtPlugin>
Q_IMPORT_PLUGIN (QWindowsIntegrationPlugin);
#else
#error "Not sure what to do with a static Qt on this platform. File a bug report."
#endif // _WIN32

#endif // WOBBLY_STATIC_QT


int main(int argv, char **args) {
    QApplication app(argv, args);

    app.setOrganizationName("wobbly");
    app.setApplicationName("wibbly");

    WibblyWindow w;

    w.show();

    return app.exec();
}
