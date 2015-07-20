#ifndef WOBBLYWINDOW_H
#define WOBBLYWINDOW_H


#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QSlider>
#include <QSpinBox>
#include <QStringListModel>

#include <VapourSynth.h>
#include <VSScript.h>

#include "DockWidget.h"
#include "ListWidget.h"
#include "PresetTextEdit.h"
#include "TableWidget.h"
#include "WobblyProject.h"


class WobblyWindow : public QMainWindow {
    Q_OBJECT

public:
    WobblyWindow();

private:
    // Menus.

    QMenu *tools_menu;



    // Widgets.

    DockWidget *details_dock;
    QLabel *frame_label;
    QSlider *frame_slider;

    QLabel *frame_num_label;
    QLabel *time_label;
    QLabel *matches_label;
    QLabel *section_label;
    QLabel *custom_list_label;
    QLabel *freeze_label;
    QLabel *decimate_metric_label;
    QLabel *mic_label;
    QLabel *combed_label;

    QLabel *zoom_label;

    DockWidget *crop_dock;
    QSpinBox *crop_spin[4];
    QGroupBox *crop_box;
    QCheckBox *crop_early_check;
    QSpinBox *resize_spin[2];
    QGroupBox *resize_box;
    QComboBox *resize_filter_combo;
    QGroupBox *depth_box;
    QComboBox *depth_bits_combo;
    QComboBox *depth_dither_combo;


    DockWidget *preset_dock;
    QComboBox *preset_combo;
    PresetTextEdit *preset_edit;

    DockWidget *pattern_dock;
    QLineEdit *match_pattern_edit;
    QLineEdit *decimation_pattern_edit;

    DockWidget *sections_dock;
    TableWidget *sections_table;
    QGroupBox *short_sections_box;
    QSpinBox *short_sections_spin;

    DockWidget *cl_dock;
    TableWidget *cl_table;
    QMenu *cl_send_range_menu;
    QMenu *cl_copy_range_menu;

    QButtonGroup *frame_rates_buttons;
    DockWidget *frame_rates_dock;
    TableWidget *frame_rates_table;

    DockWidget *frozen_frames_dock;
    TableWidget *frozen_frames_table;

    DockWidget *pg_dock;
    QSpinBox *pg_length_spin;
    QButtonGroup *pg_n_match_buttons;
    QButtonGroup *pg_decimate_buttons;
    TableWidget *pg_failures_table;

    DockWidget *mic_search_dock;
    QSpinBox *mic_search_minimum_spin;


    // Widget-related

    QStringListModel *presets_model;

    QImage splash_image;


    // Other stuff.

    WobblyProject *project;
    QString project_path;

    int current_frame;

    QString match_pattern;
    QString decimation_pattern;

    bool preview;

    struct Shortcut {
        QString keys;
        QString default_keys;
        QString description;
        void (WobblyWindow::* func)();
    };

    std::vector<Shortcut> shortcuts;


    // VapourSynth stuff.

    const VSAPI *vsapi;
    VSScript *vsscript;
    VSCore *vscore;
    VSNodeRef *vsnode[2];
    const VSFrameRef *vsframe;


    // Functions

    void createMenu();
    void createShortcuts();
    void resetShortcuts();
    void createFrameDetailsViewer();
    void createCropAssistant();
    void createPresetEditor();
    void createPatternEditor();
    void createSectionsEditor();
    void createCustomListsEditor();
    void createFrameRatesViewer();
    void createFrozenFramesViewer();
    void createPatternGuessingWindow();
    void createMicSearchWindow();
    void drawColorBars();
    void createUI();


    void initialiseVapourSynth();
    void cleanUpVapourSynth();
    void checkRequiredFilters();

    void initialisePresets();

    void initialiseCropAssistant();
    void initialisePresetEditor();
    void updateSectionsEditor();
    void updateCustomListsEditor();
    void updateFrameRatesViewer();
    void initialiseFrameRatesViewer();
    void initialiseFrozenFramesViewer();
    void updatePatternGuessingWindow();
    void initialisePatternGuessingWindow();
    void initialiseMicSearchWindow();
    void initialiseUIFromProject();

    void evaluateScript(bool final_script);
    void evaluateMainDisplayScript();
    void evaluateFinalScript();
    void displayFrame(int n);
    void updateFrameDetails();

    void errorPopup(const char *msg);

    void closeEvent(QCloseEvent *event);
    void keyPressEvent(QKeyEvent *event);

    void realSaveProject(const QString &path);
    void realSaveScript(const QString &path);
    void realSaveTimecodes(const QString &path);

    void jumpRelative(int offset);

    void zoom(bool in);

public slots:
    void jump1Forward();
    void jump1Backward();
    void jump5Forward();
    void jump5Backward();
    void jump50Forward();
    void jump50Backward();
    void jumpALotForward();
    void jumpALotBackward();

    void jumpToStart();
    void jumpToEnd();

    void jumpToNextSectionStart();
    void jumpToPreviousSectionStart();

    void jumpToPreviousMic();
    void jumpToNextMic();

    void jumpToFrame();

    void freezeForward();
    void freezeBackward();
    void freezeRange();

    void deleteFreezeFrame();

    void cycleMatchBCN();

    void toggleDecimation();

    void toggleCombed();

    void addSection();
    void deleteSection();

    void openProject();
    void saveProject();
    void saveProjectAs();
    void saveScript();
    void saveScriptAs();
    void saveTimecodes();
    void saveTimecodesAs();
    void saveScreenshot();
    void quit();
    void showHideFrameDetails();
    void showHideCropping();
    void showHidePresets();
    void showHidePatternEditor();
    void showHideSections();
    void showHideCustomLists();
    void showHideFrameRates();
    void showHideFrozenFrames();
    void showHidePatternGuessing();
    void showHideMicSearchWindow();

    void presetChanged(const QString &text);
    void presetEdited();
    void presetNew();
    void presetRename();
    void presetDelete();

    void resetMatch();
    void resetSection();

    void rotateAndSetPatterns();

    void matchPatternEdited(const QString &text);
    void decimationPatternEdited(const QString &text);

    void togglePreview();

    void zoomIn();
    void zoomOut();
};

#endif // WOBBLYWINDOW_H
