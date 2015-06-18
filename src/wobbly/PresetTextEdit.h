#ifndef PRESETTEXTEDIT_H
#define PRESETTEXTEDIT_H

#include <QPlainTextEdit>

class PresetTextEdit : public QPlainTextEdit {
    Q_OBJECT

signals:
    void focusLost();

private:
    void focusOutEvent(QFocusEvent *event);
};

#endif // PRESETTEXTEDIT_H
