#include <QKeyEvent>

#include "TableWidget.h"


void TableWidget::keyPressEvent(QKeyEvent *event) {
    auto mod = event->modifiers();
    int key = event->key();

    if (mod == Qt::NoModifier && key == Qt::Key_Delete) {
        emit deletePressed();
    } else
        QTableWidget::keyPressEvent(event);
}
