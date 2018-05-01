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


#include "BookmarksModel.h"

BookmarksModel::BookmarksModel(QObject *parent)
    : QAbstractTableModel(parent)
{

}


int BookmarksModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return (int)size();
}


int BookmarksModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return ColumnCount;
}


QVariant BookmarksModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole ||
        ((role == Qt::EditRole || role == Qt::ToolTipRole) && index.column() == DescriptionColumn)) {
        const Bookmark &bookmark = std::next(cbegin(), index.row())->second;

        if (index.column() == FrameColumn)
            return bookmark.frame;
        else if (index.column() == DescriptionColumn)
            return QString::fromStdString(bookmark.description);
    }

    return QVariant();
}


QVariant BookmarksModel::headerData(int section, Qt::Orientation orientation, int role) const {
    const char *column_headers[ColumnCount] = {
        "Frame",
        "Description"
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


Qt::ItemFlags BookmarksModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags f = QAbstractTableModel::flags(index);

    if (index.column() == DescriptionColumn)
        f |= Qt::ItemIsEditable;

    return f;
}


bool BookmarksModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (role == Qt::EditRole && index.column() == DescriptionColumn) {
        iterator it = std::next(begin(), index.row());

        it->second.description = value.toString().toStdString();

        emit dataChanged(index, index);

        return true;
    }

    return false;
}


void BookmarksModel::insert(const value_type &bookmark) {
    const_iterator it = lower_bound(bookmark.first);

    if (it != cend() && it->first == bookmark.first)
        return;

    int new_row = 0;
    if (size())
        new_row = (int)std::distance(cbegin(), it);

    beginInsertRows(QModelIndex(), new_row, new_row);

    BookmarkMap::insert(it, bookmark);

    endInsertRows();
}


void BookmarksModel::erase(int frame) {
    const_iterator it = find(frame);

    if (it == cend())
        return;

    int row = (int)std::distance(cbegin(), it);

    beginRemoveRows(QModelIndex(), row, row);

    BookmarkMap::erase(it);

    endRemoveRows();
}
