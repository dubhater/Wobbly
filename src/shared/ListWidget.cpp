#include <QKeyEvent>

#include "ListWidget.h"

void ListWidget::keyPressEvent(QKeyEvent *event) {
    auto mod = event->modifiers();
    int key = event->key();

    if (mod == Qt::NoModifier && key == Qt::Key_Delete) {
        emit deletePressed();
        return;
    }

    QListWidget::keyPressEvent(event);
}
