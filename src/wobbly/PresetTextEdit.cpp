#include "PresetTextEdit.h"

#include "PresetTextEdit.moc"


void PresetTextEdit::focusOutEvent(QFocusEvent *event) {
    emit focusLost();

    QPlainTextEdit::focusOutEvent(event);
}

