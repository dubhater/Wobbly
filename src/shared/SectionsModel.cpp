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


#include "SectionsModel.h"

SectionsModel::SectionsModel(QObject *parent)
    : QAbstractTableModel(parent)
{

}


int SectionsModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return (int)size();
}


int SectionsModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return ColumnCount;
}


QVariant SectionsModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole) {
        const Section &section = std::next(cbegin(), index.row())->second;

        if (index.column() == StartColumn)
            return section.start;
        else if (index.column() == PresetsColumn) {
            QString presets;

            if (section.presets.size()) {
                presets = QString::fromStdString(section.presets[0]);
                for (size_t i = 1; i < section.presets.size(); i++)
                    presets += "," + QString::fromStdString(section.presets[i]);
            }

            return presets;
        }
    }

    return QVariant();
}


QVariant SectionsModel::headerData(int section, Qt::Orientation orientation, int role) const {
    const char *column_headers[ColumnCount] = {
        "Start",
        "Presets"
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


void SectionsModel::insert(const value_type &section) {
    SectionMap::const_iterator it = lower_bound(section.first);

    if (it != cend() && it->first == section.first)
        return;

    int new_row = 0;
    if (size())
        new_row = (int)std::distance(cbegin(), it);

    beginInsertRows(QModelIndex(), new_row, new_row);

    SectionMap::insert(it, section);

    endInsertRows();
}


void SectionsModel::erase(int section_start) {
    SectionMap::const_iterator it = find(section_start);

    if (it == cend())
        return;

    int row = (int)std::distance(cbegin(), it);

    beginRemoveRows(QModelIndex(), row, row);

    SectionMap::erase(it);

    endRemoveRows();
}


void SectionsModel::setSectionPresetName(int section_start, size_t preset_index, const std::string &preset_name) {
    SectionMap::iterator it = find(section_start);

    it->second.presets[preset_index] = preset_name;

    int row = (int)std::distance(begin(), it);

    QModelIndex cell = index(row, PresetsColumn);
    emit dataChanged(cell, cell);
}


void SectionsModel::appendSectionPreset(int section_start, const std::string &preset_name) {
    SectionMap::iterator it = find(section_start);

    it->second.presets.push_back(preset_name);

    int row = (int)std::distance(begin(), it);

    QModelIndex cell = index(row, PresetsColumn);
    emit dataChanged(cell, cell);
}


void SectionsModel::deleteSectionPreset(int section_start, size_t preset_index) {
    SectionMap::iterator it = find(section_start);

    it->second.presets.erase(it->second.presets.cbegin() + preset_index);

    int row = (int)std::distance(begin(), it);

    QModelIndex cell = index(row, PresetsColumn);
    emit dataChanged(cell, cell);
}


void SectionsModel::setSectionPresets(int section_start, const std::vector<std::string> &presets) {
    SectionMap::iterator it = find(section_start);

    it->second.presets = presets;

    int row = (int)std::distance(begin(), it);

    QModelIndex cell = index(row, PresetsColumn);
    emit dataChanged(cell, cell);
}


void SectionsModel::moveSectionPresetUp(int section_start, size_t preset_index) {
    if (preset_index == 0)
        return;

    SectionMap::iterator it = find(section_start);

    std::swap(it->second.presets[preset_index - 1], it->second.presets[preset_index]);

    int row = (int)std::distance(begin(), it);

    QModelIndex cell = index(row, PresetsColumn);
    emit dataChanged(cell, cell);
}


void SectionsModel::moveSectionPresetDown(int section_start, size_t preset_index) {
    SectionMap::iterator it = find(section_start);

    if (preset_index == it->second.presets.size() - 1)
        return;

    std::swap(it->second.presets[preset_index], it->second.presets[preset_index + 1]);

    int row = (int)std::distance(begin(), it);

    QModelIndex cell = index(row, PresetsColumn);
    emit dataChanged(cell, cell);
}
