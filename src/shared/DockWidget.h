#ifndef DOCKWIDGET_H
#define DOCKWIDGET_H

#include <QDockWidget>

class DockWidget : public QDockWidget {
    Q_OBJECT

public:
    DockWidget(const QString &title, QWidget *parent = 0, Qt::WindowFlags flags = 0)
        : QDockWidget(title, parent, flags)
    { }

private:
    void keyPressEvent(QKeyEvent *event);
};

#endif // DOCKWIDGET_H
