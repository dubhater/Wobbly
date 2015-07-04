#ifndef LISTWIDGET_H
#define LISTWIDGET_H

#include <QListWidget>

class ListWidget : public QListWidget {
    Q_OBJECT

signals:
    void deletePressed();

private:
    void keyPressEvent(QKeyEvent *event);
};

#endif // LISTWIDGET_H
