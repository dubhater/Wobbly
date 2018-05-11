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


#include "UndoAction.h"


QString UndoAction::description() const {
    switch (type) {
    case AddFreezeFrame:
        return QStringLiteral("add freezeframe [%1,%2,%3]")
                .arg(freezeframe.first)
                .arg(freezeframe.last)
                .arg(freezeframe.replacement);
        break;
    case DeleteFreezeFrame:
        return QStringLiteral("delete freezeframe [%1,%2,%3]")
                .arg(freezeframe.first)
                .arg(freezeframe.last)
                .arg(freezeframe.replacement);
        break;
    case DeleteManyFreezeFrames:
        return QStringLiteral("delete %1 freezeframes")
                .arg(ints.size());
        break;
    case AddRangeToCustomList:
        return QStringLiteral("add [%1,%2] to custom list '%3'")
                .arg(first_frame)
                .arg(last_frame)
                .arg(QString::fromStdString(cl_name));
        break;
    }
}
