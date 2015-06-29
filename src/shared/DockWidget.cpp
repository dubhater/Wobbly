#include <QKeyEvent>

#include "DockWidget.h"


void DockWidget::keyPressEvent(QKeyEvent *event) {
    auto mod = event->modifiers();
    int key = event->key();

    if (mod == Qt::NoModifier && key == Qt::Key_Escape)
        hide();
    else
        QDockWidget::keyPressEvent(event);
}
