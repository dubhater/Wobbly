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


#ifndef CUSTOMLISTSMODEL_H
#define CUSTOMLISTSMODEL_H

#include <QAbstractTableModel>

#include "WobblyTypes.h"


class CustomListsModel : public QAbstractTableModel, private std::vector<CustomList> {
    Q_OBJECT

    enum Columns {
        NameColumn = 0,
        PresetColumn,
        PositionColumn,
        ColumnCount
    };

public:
    enum ItemDataRole {
        PositionInFilterChainRole = Qt::UserRole // QVariant type is int.
    };

    CustomListsModel(QObject *parent = Q_NULLPTR);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    using std::vector<CustomList>::at;
    using std::vector<CustomList>::size;
    using std::vector<CustomList>::cbegin;
    using std::vector<CustomList>::cend;
    using std::vector<CustomList>::reserve;

    void push_back(const CustomList &cl);

    void erase(int list_index);

    void moveCustomListUp(int list_index);

    void moveCustomListDown(int list_index);

    void setCustomListName(int list_index, const std::string &name);

    void setCustomListPreset(int list_index, const std::string &preset_name);

    void setCustomListPosition(int list_index, PositionInFilterChain position);
};

#endif // CUSTOMLISTSMODEL_H
