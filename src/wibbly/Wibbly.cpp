/*

Copyright (c) 2015, John Smith

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted, provided that the
above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/


#include <QApplication>
#include <QFileInfo>

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

#ifdef _WIN32
    QString wibbly_ini = QApplication::applicationDirPath() + "/wibbly.ini";

    if (!QFileInfo::exists(wibbly_ini)) {
        // Migrate the settings from the registry to ini file next to wibbly.exe

        QSettings old_settings;

        QStringList keys = old_settings.allKeys();
        if (keys.size()) {
            QSettings new_settings(wibbly_ini, QSettings::IniFormat);

            for (const QString &key : keys)
                new_settings.setValue(key, old_settings.value(key));

            old_settings.clear();
        }
    }
#endif

    WibblyWindow w;

    w.show();

    return app.exec();
}
