#ifndef WOBBLYWINDOW_H
#define WOBBLYWINDOW_H


#include <QCloseEvent>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QSpinBox>
#include <QTableWidget>

#include <VapourSynth.h>
#include <VSScript.h>

#include "PresetTextEdit.h"
#include "WobblyProject.h"


class WobblyWindow : public QMainWindow {
    Q_OBJECT

public:
    WobblyWindow();

private:
    // Menus.

    QMenu *tools_menu;



    // Widgets.

    QLabel *frame_label;

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

    QDockWidget *crop_dock;
    QSpinBox *crop_spin[4];
    QGroupBox *crop_box;
    QSpinBox *resize_spin[2];
    QGroupBox *resize_box;

    QComboBox *preset_combo;
    PresetTextEdit *preset_edit;

    QLineEdit *match_pattern_edit;
    QLineEdit *decimation_pattern_edit;

    QTableWidget *sections_table;
    QGroupBox *short_sections_box;
    QSpinBox *short_sections_spin;
    QListWidget *section_presets_list;
    QListWidget *preset_list;

    // Other stuff.

    WobblyProject *project;
    QString project_path;

    int current_frame;

    QString match_pattern;
    QString decimation_pattern;

    bool preview;


    // VapourSynth stuff.

    const VSAPI *vsapi;
    VSScript *vsscript;
    VSCore *vscore;
    VSNodeRef *vsnode[2];
    const VSFrameRef *vsframe;


    // Functions

    void createMenu();
    void createShortcuts();
    void createFrameDetailsViewer();
    void createCropAssistant();
    void createPresetEditor();
    void createPatternEditor();
    void createSectionsEditor();
    void createUI();


    void initialiseVapourSynth();
    void cleanUpVapourSynth();
    void checkRequiredFilters();

    void initialiseSectionsList();
    void initialiseUIFromProject();

    void evaluateScript(bool final_script);
    void evaluateMainDisplayScript();
    void evaluateFinalScript();
    void displayFrame(int n);
    void updateFrameDetails();

    void errorPopup(const char *msg);

    void closeEvent(QCloseEvent *event);

    void realSaveProject(const QString &path);

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

    void freezeForward();
    void freezeBackward();
    void freezeRange();

    void deleteFreezeFrame();

    void cycleMatchPCN();

    void toggleDecimation();

    void toggleCombed();

    void addSection();
    void deleteSection();

    void openProject();
    void saveProject();
    void saveProjectAs();

    void cropChanged(int value);
    void cropToggled(bool checked);
    void resizeChanged(int value);
    void resizeToggled(bool checked);

    void presetChanged(const QString &text);
    void presetEdited();
    void presetNew();
    void presetRename();
    void presetDelete();

    void resetRange();
    void resetSection();

    void rotateAndSetPatterns();

    void matchPatternEdited(const QString &text);
    void decimationPatternEdited(const QString &text);

    void togglePreview();

    void zoomIn();
    void zoomOut();
};

#endif // WOBBLYWINDOW_H
