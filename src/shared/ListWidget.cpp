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


QList<QListWidgetItem *> ListWidget::selectedItems() const {
    auto selection = QListWidget::selectedItems();

    auto cmp = [this] (const QListWidgetItem *a, const QListWidgetItem *b) -> bool {
        return row(a) < row(b);
    };
    std::sort(selection.begin(), selection.end(), cmp);

    return selection;
}
