#include <QApplication>

#include "WobblyWindow.h"


int main(int argv, char **args) {
    QApplication app(argv, args);

    WobblyWindow w;

    w.show();

    return app.exec();
}
