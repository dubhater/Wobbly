#include <QKeyEvent>

#include "TableWidget.h"


TableWidget::TableWidget(int rows, int columns, QWidget *parent)
    : QTableWidget(rows, columns, parent)
{
    setAutoScroll(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setTabKeyNavigation(false);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    if (columns) {
        QStringList labels;
        labels.reserve(columns);
        for (int i = 0; i < columns; i++)
            labels.push_back(QString::number(i + 1));
        setHorizontalHeaderLabels(labels);
        // If there are no labels set, horizontalHeaderItem() returns 0,
        // even though the table does have visible header items. (Qt 5.4.2)
        for (int i = 0; i < columnCount(); i++)
            horizontalHeaderItem(i)->setTextAlignment(Qt::AlignLeft);
    }
}


QList<QTableWidgetSelectionRange> TableWidget::selectedRanges() const {
    auto selection = QTableWidget::selectedRanges();

    auto cmp = [] (const QTableWidgetSelectionRange &a, const QTableWidgetSelectionRange &b) -> bool {
        return a.topRow() < b.topRow();
    };
    std::sort(selection.begin(), selection.end(), cmp);

    return selection;
}


void TableWidget::keyPressEvent(QKeyEvent *event) {
    auto mod = event->modifiers();
    int key = event->key();

    if (mod == Qt::NoModifier && key == Qt::Key_Delete) {
        emit deletePressed();
    } else
        QTableWidget::keyPressEvent(event);
}
