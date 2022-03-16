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


#ifndef COMBEDFRAMESMODEL_H
#define COMBEDFRAMESMODEL_H

#include <set>

#include <QAbstractListModel>


class CombedFramesModel : public QAbstractListModel, private std::set<int> {
    Q_OBJECT

public:
    CombedFramesModel(QObject *parent = Q_NULLPTR);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    using std::set<int>::cbegin;
    using std::set<int>::cend;
    using std::set<int>::count;
    using std::set<int>::lower_bound;
    using std::set<int>::upper_bound;
    using std::set<int>::size;
    using std::set<int>::const_iterator;

    void insert(int frame);

    void erase(int frame);

    void clear();
};

#endif // COMBEDFRAMESMODEL_H
