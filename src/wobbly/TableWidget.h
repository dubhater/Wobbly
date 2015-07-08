#ifndef SECTIONSTABLEWIDGET_H
#define SECTIONSTABLEWIDGET_H

#include <QTableWidget>


class TableWidget : public QTableWidget {
    Q_OBJECT

public:
    TableWidget(int rows, int columns, QWidget *parent = 0);
    QList<QTableWidgetSelectionRange> selectedRanges() const;

signals:
    void deletePressed();

private:
    void keyPressEvent(QKeyEvent *event);
};

#endif // SECTIONSTABLEWIDGET_H
