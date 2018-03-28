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


#include "CombedFramesModel.h"

CombedFramesModel::CombedFramesModel(QObject *parent)
    : QAbstractListModel(parent)
{

}


int CombedFramesModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return (int)size();
}


QVariant CombedFramesModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole) {
        return QVariant(*std::next(cbegin(), index.row()));
    }

    return QVariant();
}


QVariant CombedFramesModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            if (section == 0) {
                return QVariant("Frame");
            }
        } else if (orientation == Qt::Vertical) {
            return QVariant(section + 1);
        }
    }

    return QVariant();
}


void CombedFramesModel::insert(int frame) {
    std::set<int>::const_iterator it = lower_bound(frame);

    if (it != cend() && *it == frame)
        return;

    int new_row = 0;
    if (size())
        new_row = (int)std::distance(cbegin(), it);

    beginInsertRows(QModelIndex(), new_row, new_row);

    std::set<int>::insert(it, frame);

    endInsertRows();
}


void CombedFramesModel::erase(int frame) {
    std::set<int>::const_iterator it = find(frame);

    if (it == cend())
        return;

    int row = (int)std::distance(cbegin(), it);

    beginRemoveRows(QModelIndex(), row, row);

    std::set<int>::erase(it);

    endRemoveRows();
}


void CombedFramesModel::clear() {
    if (!size())
        return;

    beginRemoveRows(QModelIndex(), 0, size() - 1);

    std::set<int>::clear();

    endRemoveRows();
}
