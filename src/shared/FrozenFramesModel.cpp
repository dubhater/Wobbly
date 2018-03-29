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


#include "FrozenFramesModel.h"


FrozenFramesModel::FrozenFramesModel(QObject *parent)
    : QAbstractTableModel(parent)
{

}


int FrozenFramesModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return (int)size();
}


int FrozenFramesModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return ColumnCount;
}


QVariant FrozenFramesModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole) {
        const FreezeFrame &frozen = std::next(cbegin(), index.row())->second;

        if (index.column() == FirstColumn)
            return QVariant(frozen.first);
        else if (index.column() == LastColumn)
            return QVariant(frozen.last);
        else if (index.column() == ReplacementColumn)
            return QVariant(frozen.replacement);
    }

    return QVariant();
}


QVariant FrozenFramesModel::headerData(int section, Qt::Orientation orientation, int role) const {
    const char *column_headers[ColumnCount] = {
        "First",
        "Last",
        "Replacement"
    };

    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            return QVariant(QString(column_headers[section]));
        } else if (orientation == Qt::Vertical) {
            return QVariant(section + 1);
        }
    }

    return QVariant();
}


void FrozenFramesModel::insert(const std::pair<int, FreezeFrame> &freeze_frame) {
    std::map<int, FreezeFrame>::const_iterator it = lower_bound(freeze_frame.first);

    if (it != cend() && it->first == freeze_frame.first)
        return;

    int new_row = 0;
    if (size())
        new_row = (int)std::distance(cbegin(), it);

    beginInsertRows(QModelIndex(), new_row, new_row);

    std::map<int, FreezeFrame>::insert(it, freeze_frame);

    endInsertRows();
}


void FrozenFramesModel::erase(int freeze_frame) {
    std::map<int, FreezeFrame>::const_iterator it = find(freeze_frame);

    if (it == cend())
        return;

    int row = (int)std::distance(cbegin(), it);

    beginRemoveRows(QModelIndex(), row, row);

    std::map<int, FreezeFrame>::erase(it);

    endRemoveRows();
}
