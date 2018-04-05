/*

Copyright (c) 2018, John Smith

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


#include "FrameRangesModel.h"

FrameRangesModel::FrameRangesModel(QObject *parent)
    : QAbstractTableModel(parent)
{

}


int FrameRangesModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return (int)size();
}


int FrameRangesModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return ColumnCount;
}


QVariant FrameRangesModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole) {
        const FrameRange &range = std::next(cbegin(), index.row())->second;

        if (index.column() == FirstColumn)
            return range.first;
        else if (index.column() == LastColumn)
            return range.last;
    }

    return QVariant();
}


QVariant FrameRangesModel::headerData(int section, Qt::Orientation orientation, int role) const {
    const char *column_headers[ColumnCount] = {
        "First",
        "Last"
    };

    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            return QString(column_headers[section]);
        } else if (orientation == Qt::Vertical) {
            return section + 1;
        }
    }

    return QVariant();
}


void FrameRangesModel::insert(const std::pair<int, FrameRange> &range) {
    const_iterator it = lower_bound(range.first);

    if (it != cend() && it->first == range.first)
        return;

    int new_row = 0;
    if (size())
        new_row = (int)std::distance(cbegin(), it);

    beginInsertRows(QModelIndex(), new_row, new_row);

    std::map<int, FrameRange>::insert(it, range);

    endInsertRows();
}


void FrameRangesModel::erase(int frame) {
    const_iterator it = find(frame);

    if (it == cend())
        return;

    int row = (int)std::distance(cbegin(), it);

    beginRemoveRows(QModelIndex(), row, row);

    std::map<int, FrameRange>::erase(it);

    endRemoveRows();
}
