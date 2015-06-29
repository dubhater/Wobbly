#ifndef SECTIONSTABLEWIDGET_H
#define SECTIONSTABLEWIDGET_H

#include <QTableWidget>


class TableWidget : public QTableWidget {
    Q_OBJECT

public:
    TableWidget(QWidget *parent = 0)
        : QTableWidget(parent)
    { }

    TableWidget(int rows, int columns, QWidget *parent = 0)
        : QTableWidget(rows, columns, parent)
    { }

signals:
    void deletePressed();

private:
    void keyPressEvent(QKeyEvent *event);
};

#endif // SECTIONSTABLEWIDGET_H
