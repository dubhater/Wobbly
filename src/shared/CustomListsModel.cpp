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


#include "CustomListsModel.h"

CustomListsModel::CustomListsModel(QObject *parent)
    : QAbstractTableModel(parent)
{

}


int CustomListsModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return (int)size();
}


int CustomListsModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return ColumnCount;
}


QVariant CustomListsModel::data(const QModelIndex &index, int role) const {
    const CustomList &cl = at(index.row());

    if (role == Qt::DisplayRole) {
        if (index.column() == NameColumn)
            return QString::fromStdString(cl.name);
        else if (index.column() == PresetColumn)
            return QString::fromStdString(cl.preset);
        else if (index.column() == PositionColumn) {
            const char *positions[3] = {
                "Post source",
                "Post field match",
                "Post decimate"
            };
            return QVariant(positions[cl.position]);
        }
    } else if (role == PositionInFilterChainRole) {
        if (index.column() == PositionColumn)
            return QVariant(cl.position);
    }

    return QVariant();
}


QVariant CustomListsModel::headerData(int section, Qt::Orientation orientation, int role) const {
    const char *column_headers[ColumnCount] = {
        "Name",
        "Preset",
        "Position"
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


void CustomListsModel::push_back(const CustomList &cl) {
    beginInsertRows(QModelIndex(), size(), size());

    CustomListVector::push_back(cl);

    endInsertRows();
}


void CustomListsModel::erase(int list_index) {
    beginRemoveRows(QModelIndex(), list_index, list_index);

    CustomListVector::erase(cbegin() + list_index);

    endRemoveRows();
}


void CustomListsModel::moveCustomListUp(int list_index) {
    if (beginMoveRows(QModelIndex(), list_index, list_index, QModelIndex(), list_index - 1)) {
        std::swap(at(list_index - 1), at(list_index));

        endMoveRows();
    }
}


void CustomListsModel::moveCustomListDown(int list_index) {
    if (beginMoveRows(QModelIndex(), list_index, list_index, QModelIndex(), list_index + 2)) {
        std::swap(at(list_index), at(list_index + 1));

        endMoveRows();
    }
}


void CustomListsModel::setCustomListName(int list_index, const std::string &name) {
    at(list_index).name = name;

    QModelIndex cell = index(list_index, NameColumn);
    emit dataChanged(cell, cell);
}


void CustomListsModel::setCustomListPreset(int list_index, const std::string &preset_name) {
    at(list_index).preset = preset_name;

    QModelIndex cell = index(list_index, PresetColumn);
    emit dataChanged(cell, cell);
}


void CustomListsModel::setCustomListPosition(int list_index, PositionInFilterChain position) {
    at(list_index).position = position;

    QModelIndex cell = index(list_index, PositionColumn);
    emit dataChanged(cell, cell);
}
