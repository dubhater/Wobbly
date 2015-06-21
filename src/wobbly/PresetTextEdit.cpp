#include "PresetTextEdit.h"


void PresetTextEdit::focusOutEvent(QFocusEvent *event) {
    emit focusLost();

    QPlainTextEdit::focusOutEvent(event);
}

