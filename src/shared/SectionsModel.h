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


#ifndef SECTIONSMODEL_H
#define SECTIONSMODEL_H

#include <QAbstractTableModel>

#include "WobblyTypes.h"


class SectionsModel : public QAbstractTableModel, private SectionMap {
    Q_OBJECT

public:
    enum Columns {
        StartColumn = 0,
        PresetsColumn,
        ColumnCount
    };

    SectionsModel(QObject *parent = Q_NULLPTR);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    using SectionMap::cbegin;
    using SectionMap::cend;
    using SectionMap::upper_bound;
    using SectionMap::count;

    void insert(const value_type &section);

    void erase(int section_start);

    void setSectionPresetName(int section_start, size_t preset_index, const std::string &preset_name);

    void appendSectionPreset(int section_start, const std::string &preset_name);

    void deleteSectionPreset(int section_start, size_t preset_index);

    void moveSectionPresetUp(int section_start, size_t preset_index);

    void moveSectionPresetDown(int section_start, size_t preset_index);
};

#endif // SECTIONSMODEL_H
