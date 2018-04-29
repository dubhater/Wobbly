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

    connect(this, &TableWidget::currentCellChanged, [this] (int currentRow) {
        if (currentRow < 0)
            return;

        if (columnCount() < 0)
            return;

        scrollToItem(item(currentRow, 0));
    });
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
    } else if (mod == Qt::NoModifier && key == Qt::Key_Home) {
        if (selectionModel() && model())
            selectionModel()->setCurrentIndex(model()->index(0, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    } else if (mod == Qt::NoModifier && key == Qt::Key_End) {
        if (selectionModel() && model())
            selectionModel()->setCurrentIndex(model()->index(model()->rowCount() - 1, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    } else if (mod == Qt::ShiftModifier && key == Qt::Key_Home) {
        if (selectionModel() && model()) {
            QModelIndex old_current_index = selectionModel()->currentIndex();
            QModelIndex new_current_index = model()->index(0, 0);

            selectionModel()->setCurrentIndex(new_current_index, QItemSelectionModel::NoUpdate);

            selectionModel()->select(QItemSelection(new_current_index, old_current_index), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
    } else if (mod == Qt::ShiftModifier && key == Qt::Key_End) {
        if (selectionModel() && model()) {
            QModelIndex old_current_index = selectionModel()->currentIndex();
            QModelIndex new_current_index = model()->index(model()->rowCount() - 1, 0);

            selectionModel()->setCurrentIndex(new_current_index, QItemSelectionModel::NoUpdate);

            selectionModel()->select(QItemSelection(old_current_index, new_current_index), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
    } else {
        QTableWidget::keyPressEvent(event);
    }
}
