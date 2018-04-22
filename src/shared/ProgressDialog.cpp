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

#include <QWindowStateChangeEvent>

#include "ProgressDialog.h"

ProgressDialog::ProgressDialog()
    : QProgressDialog()
{
    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::WindowMinimizeButtonHint;
    flags &= ~Qt::WindowContextHelpButtonHint;
    setWindowFlags(flags);
}


void ProgressDialog::changeEvent(QEvent *e) {
    if (e->type() == QEvent::WindowStateChange) {
        QWindowStateChangeEvent *state_change_event = (QWindowStateChangeEvent *)e;

        Qt::WindowStates old_state = state_change_event->oldState();
        Qt::WindowStates new_state = windowState();

        if ((old_state & Qt::WindowMinimized) != (new_state & Qt::WindowMinimized))
            emit minimiseChanged(new_state & Qt::WindowMinimized);
    }

    QProgressDialog::changeEvent(e);
}
