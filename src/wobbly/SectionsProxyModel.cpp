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


#include "SectionsProxyModel.h"

SectionsProxyModel::SectionsProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , hide_sections(false)
{

}


bool SectionsProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    if (!hide_sections)
        return true;

    bool ok;
    int section_start = sourceModel()->data(sourceModel()->index(source_row, 0, source_parent)).toInt(&ok);
    if (!ok)
        return true;

    return !hidden_sections.count(section_start);
}


void SectionsProxyModel::setHideSections(bool hide) {
    if (hide != hide_sections) {
        hide_sections = hide;

        invalidateFilter();
    }
}


void SectionsProxyModel::setHiddenSections(const std::unordered_set<int> &sections) {
    hidden_sections = sections;

    invalidateFilter();
}
