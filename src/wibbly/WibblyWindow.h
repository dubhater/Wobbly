/*

Copyright (c) 2015, John Smith

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


#ifndef WIBBLYWINDOW_H
#define WIBBLYWINDOW_H

#include <atomic>

#include <QCheckBox>
#include <QCloseEvent>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QTimeEdit>

#include <VSScript.h>

#include "DockWidget.h"
#include "ListWidget.h"
#include "ProgressDialog.h"

#include "WibblyJob.h"


enum VIVTCParameterTypes {
    VIVTCParamInt,
    VIVTCParamDouble,
    VIVTCParamBool
};


struct VIVTCParameter {
    QWidget *widget;
    QString name;
    int minimum;
    int maximum;
    int type;
};


class WibblyWindow : public QMainWindow
{
    Q_OBJECT

    // Menus.
    QMenu *menu_menu;


    // Widgets.
    ListWidget *main_jobs_list;
    QLineEdit *main_destination_edit;
    ProgressDialog *main_progress_dialog;

    DockWidget *video_dock;
    QLabel *video_frame_label;
    QSpinBox *video_frame_spin;
    QTimeEdit *video_time_edit;
    QSlider *video_frame_slider;

    DockWidget *crop_dock;
    QSpinBox *crop_spin[4];

    DockWidget *vfm_dock;
    std::vector<VIVTCParameter> vfm_params;

    std::vector<VIVTCParameter> vdecimate_params;

    DockWidget *trim_dock;
    ListWidget *trim_ranges_list;
    QLabel *trim_start_label;
    QLabel *trim_end_label;

    DockWidget *fades_dock;
    QDoubleSpinBox *fades_threshold_spin;

    DockWidget *settings_dock;
    QSpinBox *settings_font_spin;
    QCheckBox *settings_compact_projects_check;
    QCheckBox *settings_use_relative_paths_check;
    QSpinBox *settings_cache_spin;
    int settings_last_crop[4];


    // VapourSynth stuff.
    const VSAPI *vsapi;
    VSScript *vsscript;
    VSCore *vscore;
    VSNodeRef *vsnode;
    const VSVideoInfo *vsvi;


    // Other stuff.
    std::vector<WibblyJob> jobs;

    int current_frame;

    int trim_start;
    int trim_end;

    WobblyProject *current_project;
    int current_job;
    int next_frame;
    int frames_left;
    bool aborted;
    std::atomic<int> request_count;

    QString progress_dialog_label_text;
    QElapsedTimer elapsed_timer;
    QElapsedTimer update_timer;

    QSettings settings;


    // Functions.
    void initialiseVapourSynth();
    void cleanUpVapourSynth();
    void checkRequiredFilters();

    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

    void createUI();
    void createMenus();
    void createShortcuts();
    void createMainWindow();
    void createVideoOutputWindow();
    void createCropWindow();
    void createVFMWindow();
    void createVDecimateWindow();
    void createTrimWindow();
    void createInterlacedFadesWindow();
    void createSettingsWindow();

    void realOpenVideo(const QString &path);

    void evaluateFinalScript(int job_index);
    void evaluateDisplayScript();
    void displayFrame(int n);

    void readSettings();
    void writeSettings();

    void readJobs();
    void writeJobs();

    void jumpRelative(int offset);
    void jump1Backward();
    void jump1Forward();
    void jump5Backward();
    void jump5Forward();
    void jump50Backward();
    void jump50Forward();
    void jumpToStart();
    void jumpToEnd();
    void jumpALotBackward();
    void jumpALotForward();
    void selectPreviousJob();
    void selectNextJob();
    void startTrim();
    void endTrim();
    void addTrim();

public:
    WibblyWindow();

public slots:
    void vsLogPopup(int msgType, const QString &msg);
    void frameDone(void *frame_v, int n, const QString &error_msg);

    void startNextJob();

    void errorPopup(const QString &msg);
};

#endif // WIBBLYWINDOW_H
