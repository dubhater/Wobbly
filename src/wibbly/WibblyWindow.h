#ifndef WIBBLYWINDOW_H
#define WIBBLYWINDOW_H

#include <QMainWindow>
#include <QProgressBar>

#include "ListWidget.h"

class WibblyWindow : public QMainWindow
{
    Q_OBJECT

    // Widgets.
    ListWidget *main_jobs_list;
    QProgressBar *main_progress_bar;


    // VapourSynth stuff.


    // Other stuff.


    // Functions.
    void createUI();
    void createMainWindow();
    void createVideoOutputWindow();
    void createCropWindow();
    void createVFMWindow();
    void createVDecimateWindow();

public:
    WibblyWindow();

public slots:
};

#endif // WIBBLYWINDOW_H
