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


#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QRadioButton>
#include <QRegExpValidator>
#include <QScrollArea>
#include <QShortcut>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QThread>

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <VSScript.h>

#include "ScrollArea.h"
#include "WobblyException.h"
#include "WobblyWindow.h"


WobblyWindow::WobblyWindow()
    : QMainWindow()
    , import_window(nullptr)
    , splash_image(720, 480, QImage::Format_RGB32)
    , window_title(QStringLiteral("Wobbly IVTC Assistant v%1").arg(PACKAGE_VERSION))
    , project(nullptr)
    , current_frame(0)
    , pending_frame(0)
    , pending_requests(0)
    , match_pattern("ccnnc")
    , decimation_pattern("kkkkd")
    , preview(false)
    , range_start(-1)
    , range_end(-1)
    , selected_preset(-1)
    , selected_custom_list(-1)
#ifdef _WIN32
    , settings(QApplication::applicationDirPath() + "/wobbly.ini", QSettings::IniFormat)
#endif
    , vsapi(nullptr)
    , vsscript(nullptr)
    , vscore(nullptr)
    , vsnode{nullptr, nullptr}
    , vsframes{ }
{
    createUI();

    readSettings();

    try {
        initialiseVapourSynth();

        checkRequiredFilters();
    } catch (WobblyException &e) {
        show();
        errorPopup(e.what());
    }
}


void WobblyWindow::addRecentFile(const QString &path) {
    int index = -1;
    auto actions = recent_menu->actions();
    for (int i = 0; i < actions.size(); i++) {
        if (actions[i]->text().endsWith(path)) {
            index = i;
            break;
        }
    }

    if (index == 0) {
        return;
    } else if (index > 0) {
        recent_menu->removeAction(actions[index]);
        recent_menu->insertAction(actions[0], actions[index]);
    } else {
        QAction *recent = new QAction(QStringLiteral("&0. %1").arg(path), this);
        connect(recent, &QAction::triggered, recent_menu_signal_mapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));
        recent_menu_signal_mapper->setMapping(recent, path);

        recent_menu->insertAction(actions.size() ? actions[0] : 0, recent);

        if (actions.size() == 10)
            recent_menu->removeAction(actions[9]);
    }

    actions = recent_menu->actions();
    for (int i = 0; i < actions.size(); i++) {
        QString text = actions[i]->text();
        text[1] = QChar('0' + i);
        actions[i]->setText(text);

        settings.setValue(QStringLiteral("user_interface/recent%1").arg(i), text.mid(4));
    }
}


void WobblyWindow::readSettings() {
    if (settings.contains("user_interface/state"))
        restoreState(settings.value("user_interface/state").toByteArray());

    if (settings.contains("user_interface/geometry"))
        restoreGeometry(settings.value("user_interface/geometry").toByteArray());

    settings_font_spin->setValue(settings.value("user_interface/font_size", QApplication::font().pointSize()).toInt());

    settings_compact_projects_check->setChecked(settings.value("projects/compact_project_files", false).toBool());

    if (settings.contains("user_interface/colormatrix"))
        settings_colormatrix_combo->setCurrentText(settings.value("user_interface/colormatrix").toString());

    if (settings.contains("user_interface/maximum_cache_size"))
        settings_cache_spin->setValue(settings.value("user_interface/maximum_cache_size").toInt());

    settings_print_details_check->setChecked(settings.value("user_interface/print_details_on_video", true).toBool());

    settings_shortcuts_table->setRowCount(shortcuts.size());
    for (size_t i = 0; i < shortcuts.size(); i++) {
        QString settings_key = "user_interface/keys/" + shortcuts[i].description;

        if (settings.contains(settings_key))
            shortcuts[i].keys = settings.value(settings_key).toString();

        QTableWidgetItem *item = new QTableWidgetItem(shortcuts[i].keys);
        settings_shortcuts_table->setItem(i, 0, item);

        item = new QTableWidgetItem(shortcuts[i].default_keys);
        settings_shortcuts_table->setItem(i, 1, item);

        item = new QTableWidgetItem(shortcuts[i].description);
        settings_shortcuts_table->setItem(i, 2, item);
    }
    settings_shortcuts_table->resizeColumnsToContents();

    QStringList recent;
    for (int i = 9; i >= 0; i--) {
        QString settings_key = QStringLiteral("user_interface/recent%1").arg(i);
        if (settings.contains(settings_key))
            recent.push_back(settings.value(settings_key).toString());
    }
    for (int i = 0; i < recent.size(); i++)
        addRecentFile(recent[i]);
}


void WobblyWindow::writeSettings() {
    settings.setValue("user_interface/state", saveState());

    settings.setValue("user_interface/geometry", saveGeometry());
}


void WobblyWindow::errorPopup(const char *msg) {
    QMessageBox::information(this, QStringLiteral("Error"), msg);
}


void WobblyWindow::closeEvent(QCloseEvent *event) {
    // XXX Only ask if the project was modified.

    if (project) {
        QMessageBox::StandardButton answer = QMessageBox::question(this, QStringLiteral("Save?"), QStringLiteral("Save project?"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);

        if (answer == QMessageBox::Yes) {
            saveProject();
        } else if (answer == QMessageBox::No) {
            ;
        } else {
            event->ignore();
            return;
        }
    }

    writeSettings();

    cleanUpVapourSynth();

    if (project) {
        delete project;
        project = nullptr;
    }

    event->accept();
}


void WobblyWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}


void WobblyWindow::dropEvent(QDropEvent *event) {
    QList<QUrl> urls = event->mimeData()->urls();

    int first_local = -1;

    for (int i = 0; i < urls.size(); i++)
        if (urls[i].isLocalFile()) {
            first_local = i;
            break;
        }

    if (first_local == -1)
        return;

    QString path = urls[first_local].toLocalFile();

    if (path.endsWith(".json"))
        realOpenProject(path);
    else
        realOpenVideo(path);

    event->acceptProposedAction();
}


void WobblyWindow::keyPressEvent(QKeyEvent *event) {
    auto mod = event->modifiers();
    int key = event->key();

    QKeySequence sequence(mod | key);
    QString sequence_string = sequence.toString();
    //fprintf(stderr, "Sequence: '%s'\n", sequence_string.toUtf8().constData());

    for (size_t i = 0; i < shortcuts.size(); i++) {
        if (sequence_string == shortcuts[i].keys) {
            (this->*shortcuts[i].func)(); // This looks quite evil indeed.
            return;
        }
    }

    QMainWindow::keyPressEvent(event);
}


void WobblyWindow::createMenu() {
    QMenuBar *bar = menuBar();

    QMenu *p = bar->addMenu("&Project");

    struct Menu {
        const char *name;
        void (WobblyWindow::* func)();
    };

    std::vector<Menu> project_menu = {
        { "&Open project",              &WobblyWindow::openProject },
        { "Open video",                 &WobblyWindow::openVideo },
        { "&Save project",              &WobblyWindow::saveProject },
        { "Save project &as",           &WobblyWindow::saveProjectAs },
        { "Save script",                &WobblyWindow::saveScript },
        { "Save script as",             &WobblyWindow::saveScriptAs },
        { "Save timecodes",             &WobblyWindow::saveTimecodes },
        { "Save timecodes as",          &WobblyWindow::saveTimecodesAs },
        { "Save screenshot",            &WobblyWindow::saveScreenshot },
        { "Import from project",        &WobblyWindow::importFromProject },
        { nullptr,                      nullptr },
        { "&Recently opened",           nullptr },
        { nullptr,                      nullptr },
        { "&Quit",                      &WobblyWindow::quit }
    };

    for (size_t i = 0; i < project_menu.size(); i++) {
        if (project_menu[i].name && project_menu[i].func) {
            QAction *action = new QAction(project_menu[i].name, this);
            connect(action, &QAction::triggered, this, project_menu[i].func);
            p->addAction(action);
        } else if (project_menu[i].name && !project_menu[i].func) {
            // Not very nicely done.
            recent_menu = p->addMenu(project_menu[i].name);
        } else {
            p->addSeparator();
        }
    }

    recent_menu_signal_mapper = new QSignalMapper(this);

    connect(recent_menu_signal_mapper, static_cast<void (QSignalMapper::*)(const QString &)>(&QSignalMapper::mapped), [this] (const QString &path) {
        if (path.endsWith(".json"))
            realOpenProject(path);
        else
            realOpenVideo(path);
    });


    tools_menu = bar->addMenu("&Tools");


    QMenu *h = bar->addMenu("&Help");

    QAction *helpAbout = new QAction("About", this);
    QAction *helpAboutQt = new QAction("About Qt", this);

    connect(helpAbout, &QAction::triggered, [this] () {
        QMessageBox::about(this, QStringLiteral("About Wobbly"), QStringLiteral(
            "<a href='https://github.com/dubhater/Wobbly'>https://github.com/dubhater/Wobbly</a><br />"
            "<br />"
            "Copyright (c) 2015, John Smith<br />"
            "<br />"
            "Permission to use, copy, modify, and/or distribute this software for "
            "any purpose with or without fee is hereby granted, provided that the "
            "above copyright notice and this permission notice appear in all copies.<br />"
            "<br />"
            "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL "
            "WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED "
            "WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR "
            "BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES "
            "OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, "
            "WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, "
            "ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS "
            "SOFTWARE."
        ));
    });

    connect(helpAboutQt, &QAction::triggered, [this] () {
        QMessageBox::aboutQt(this);
    });

    h->addAction(helpAbout);
    h->addAction(helpAboutQt);
}


void WobblyWindow::createShortcuts() {
    // Do not use '/' or '\' in the descriptions. They are special in QSettings.
    shortcuts = {
        { "", "",                   "Open project", &WobblyWindow::openProject },
        { "", "",                   "Open video", &WobblyWindow::openVideo },
        { "", "",                   "Save project", &WobblyWindow::saveProject },
        { "", "",                   "Save project as", &WobblyWindow::saveProjectAs },
        { "", "",                   "Save script", &WobblyWindow::saveScript },
        { "", "",                   "Save script as", &WobblyWindow::saveScriptAs },
        { "", "",                   "Save timecodes", &WobblyWindow::saveTimecodes },
        { "", "",                   "Save timecodes as", &WobblyWindow::saveTimecodesAs },
        { "", "",                   "Save screenshot", &WobblyWindow::saveScreenshot },
        { "", "",                   "Import from project", &WobblyWindow::importFromProject },
        { "", "",                   "Quit", &WobblyWindow::quit },

        { "", "",                   "Show or hide frame details", &WobblyWindow::showHideFrameDetails },
        { "", "",                   "Show or hide cropping", &WobblyWindow::showHideCropping },
        { "", "",                   "Show or hide presets", &WobblyWindow::showHidePresets },
        { "", "",                   "Show or hide pattern editor", &WobblyWindow::showHidePatternEditor },
        { "", "",                   "Show or hide sections", &WobblyWindow::showHideSections },
        { "", "",                   "Show or hide custom lists", &WobblyWindow::showHideCustomLists },
        { "", "",                   "Show or hide frame rates", &WobblyWindow::showHideFrameRates },
        { "", "",                   "Show or hide frozen frames", &WobblyWindow::showHideFrozenFrames },
        { "", "",                   "Show or hide pattern guessing", &WobblyWindow::showHidePatternGuessing },
        { "", "",                   "Show or hide mic search", &WobblyWindow::showHideMicSearchWindow },
        { "", "",                   "Show or hide C match sequences window", &WobblyWindow::showHideCMatchSequencesWindow },
        { "", "",                   "Show or hide interlaced fades window", &WobblyWindow::showHideFadesWindow },

        { "", "",                   "Show or hide frame details printed on the video", &WobblyWindow::showHideFrameDetailsOnVideo },

        { "", "Left",               "Jump 1 frame back", &WobblyWindow::jump1Backward },
        { "", "Right",              "Jump 1 frame forward", &WobblyWindow::jump1Forward },
        { "", "Ctrl+Left",          "Jump 5 frames back", &WobblyWindow::jump5Backward },
        { "", "Ctrl+Right",         "Jump 5 frames forward", &WobblyWindow::jump5Forward },
        { "", "Alt+Left",           "Jump 50 frames back", &WobblyWindow::jump50Backward },
        { "", "Alt+Right",          "Jump 50 frames forward", &WobblyWindow::jump50Forward },
        { "", "Home",               "Jump to first frame", &WobblyWindow::jumpToStart },
        { "", "End",                "Jump to last frame", &WobblyWindow::jumpToEnd },
        { "", "PgDown",             "Jump 20% back", &WobblyWindow::jumpALotBackward },
        { "", "PgUp",               "Jump 20% forward", &WobblyWindow::jumpALotForward },
        { "", "Ctrl+Up",            "Jump to next section start", &WobblyWindow::jumpToNextSectionStart },
        { "", "Ctrl+Down",          "Jump to previous section start", &WobblyWindow::jumpToPreviousSectionStart },
        { "", "Up",                 "Jump to next frame with high mic", &WobblyWindow::jumpToNextMic },
        { "", "Down",               "Jump to previous frame with high mic", &WobblyWindow::jumpToPreviousMic },
        { "", "G",                  "Jump to specific frame", &WobblyWindow::jumpToFrame },
        { "", "S",                  "Cycle the current frame's match", &WobblyWindow::cycleMatchBCN },
        { "", "Ctrl+F",             "Replace current frame with next", &WobblyWindow::freezeForward },
        { "", "Shift+F",            "Replace current frame with previous", &WobblyWindow::freezeBackward },
        { "", "F",                  "Freeze current frame or a range", &WobblyWindow::freezeRange },
        { "", "Q",                  "Delete freezeframe", &WobblyWindow::deleteFreezeFrame },
        { "", "D",                  "Toggle decimation for the current frame", &WobblyWindow::toggleDecimation },
        { "", "I",                  "Start new section at current frame", &WobblyWindow::addSection },
        { "", "Ctrl+Q",             "Delete current section", &WobblyWindow::deleteSection },
        { "", "P",                  "Toggle postprocessing for the current frame or a range", &WobblyWindow::toggleCombed },
        { "", "R",                  "Reset the match(es) for the current frame or a range", &WobblyWindow::resetMatch },
        { "", "Ctrl+R",             "Reset the matches for the current section", &WobblyWindow::resetSection },
        { "", "Ctrl+S",             "Rotate the patterns and apply them to the current section", &WobblyWindow::rotateAndSetPatterns },
        { "", "",                   "Set match pattern to range", &WobblyWindow::setMatchPattern },
        { "", "",                   "Set decimation pattern to range", &WobblyWindow::setDecimationPattern },
        { "", "",                   "Set match and decimation patterns to range", &WobblyWindow::setMatchAndDecimationPatterns },
        { "", "F5",                 "Toggle preview mode", &WobblyWindow::togglePreview },
        { "", "Ctrl+Num++",         "Zoom in", &WobblyWindow::zoomIn },
        { "", "Ctrl+Num+-",         "Zoom out", &WobblyWindow::zoomOut },
        { "", "",                   "Guess current section's patterns from matches", &WobblyWindow::guessCurrentSectionPatternsFromMatches },
        { "", "",                   "Guess every section's patterns from matches", &WobblyWindow::guessProjectPatternsFromMatches },
        { "", "Ctrl+Alt+G",         "Guess current section's patterns from mics", &WobblyWindow::guessCurrentSectionPatternsFromMics },
        { "", "",                   "Guess every section's patterns from mics", &WobblyWindow::guessProjectPatternsFromMics },
        { "", "E",                  "Start a range", &WobblyWindow::startRange },
        { "", "Escape",             "Cancel a range", &WobblyWindow::cancelRange },
        { "", "",                   "Select the previous preset", &WobblyWindow::selectPreviousPreset },
        { "", "H",                  "Select the next preset", &WobblyWindow::selectNextPreset },
        { "", "",                   "Assign selected preset to the current section", &WobblyWindow::assignSelectedPresetToCurrentSection },
        { "", "Z",                  "Select the previous custom list", &WobblyWindow::selectPreviousCustomList },
        { "", "X",                  "Select the next custom list", &WobblyWindow::selectNextCustomList },
        { "", "C",                  "Add range to the selected custom list", &WobblyWindow::addRangeToSelectedCustomList }
    };

    resetShortcuts();
}


void WobblyWindow::resetShortcuts() {
    for (size_t i = 0; i < shortcuts.size(); i++)
        shortcuts[i].keys = shortcuts[i].default_keys;
}


void WobblyWindow::createFrameDetailsViewer() {
    frame_num_label = new QLabel;
    frame_num_label->setTextFormat(Qt::RichText);
    time_label = new QLabel;
    matches_label = new QLabel;
    matches_label->setTextFormat(Qt::RichText);
    matches_label->resize(QFontMetrics(matches_label->font()).width("CCCCCCCCCCCCCCCCCCCCC"), matches_label->height());
    section_label = new QLabel;
    section_label->setTextFormat(Qt::RichText);
    custom_list_label = new QLabel;
    custom_list_label->setTextFormat(Qt::RichText);
    freeze_label = new QLabel;
    decimate_metric_label = new QLabel;
    mic_label = new QLabel;
    mic_label->setTextFormat(Qt::RichText);
    combed_label = new QLabel;

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(frame_num_label);
    vbox->addWidget(time_label);
    vbox->addWidget(matches_label);
    vbox->addWidget(section_label);
    vbox->addWidget(custom_list_label);
    vbox->addWidget(freeze_label);
    vbox->addWidget(decimate_metric_label);
    vbox->addWidget(mic_label);
    vbox->addWidget(combed_label);
    vbox->addStretch(1);

    QWidget *details_widget = new QWidget;
    details_widget->setLayout(vbox);

    details_dock = new DockWidget("Frame details", this);
    details_dock->setObjectName("frame details");
    details_dock->setVisible(false);
    details_dock->setFloating(true);
    details_dock->setWidget(details_widget);
    addDockWidget(Qt::LeftDockWidgetArea, details_dock);
    tools_menu->addAction(details_dock->toggleViewAction());
    connect(details_dock, &QDockWidget::visibilityChanged, details_dock, &QDockWidget::setEnabled);
}


void WobblyWindow::createCropAssistant() {
    // Crop.
    crop_box = new QGroupBox(QStringLiteral("Crop"));
    crop_box->setCheckable(true);
    crop_box->setChecked(true);

    const char *crop_prefixes[4] = {
        "Left: ",
        "Top: ",
        "Right: ",
        "Bottom: "
    };

    for (int i = 0; i < 4; i++) {
        crop_spin[i] = new QSpinBox;
        crop_spin[i]->setRange(0, 99999);
        crop_spin[i]->setPrefix(crop_prefixes[i]);
        crop_spin[i]->setSuffix(QStringLiteral(" px"));
    }

    crop_early_check = new QCheckBox("Crop early");

    const char *resize_prefixes[2] = {
        "Width: ",
        "Height: "
    };

    // Resize.
    resize_box = new QGroupBox(QStringLiteral("Resize"));
    resize_box->setCheckable(true);
    resize_box->setChecked(false);

    for (int i = 0; i < 2; i++) {
        resize_spin[i] = new QSpinBox;
        resize_spin[i]->setRange(1, 999999);
        resize_spin[i]->setPrefix(resize_prefixes[i]);
        resize_spin[i]->setSuffix(QStringLiteral(" px"));
    }

    resize_filter_combo = new QComboBox;
    resize_filter_combo->addItems({
                                      "Point",
                                      "Bilinear",
                                      "Bicubic",
                                      "Spline16",
                                      "Spline36",
                                      "Lanczos"
                                  });
    resize_filter_combo->setCurrentIndex(3);

    // Bit depth.
    depth_box = new QGroupBox(QStringLiteral("Bit depth"));
    depth_box->setCheckable(true);
    depth_box->setChecked(false);

    depth_bits_combo = new QComboBox;
    depth_bits_combo->addItems({
                                   "8 bits",
                                   "9 bits",
                                   "10 bits",
                                   "12 bits",
                                   "16 bits, integer",
                                   "16 bits, float",
                                   "32 bits, float"
                               });
    depth_bits_combo->setCurrentIndex(0);

    depth_dither_combo = new QComboBox;
    depth_dither_combo->addItems({
                                     "No dithering",
                                     "Ordered dithering",
                                     "Random dithering",
                                     "Error diffusion"
                                 });
    depth_dither_combo->setCurrentIndex(2);


    // Crop.
    connect(crop_box, &QGroupBox::clicked, [this] (bool checked) {
        if (!project)
            return;

        project->setCropEnabled(checked);

        try {
            evaluateMainDisplayScript();
        } catch (WobblyException &) {

        }
    });

    auto cropChanged = [this] () {
        if (!project)
            return;

        project->setCrop(crop_spin[0]->value(), crop_spin[1]->value(), crop_spin[2]->value(), crop_spin[3]->value());

        try {
            evaluateMainDisplayScript();
        } catch (WobblyException &) {

        }
    };
    for (int i = 0; i < 4; i++)
        connect(crop_spin[i], static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), cropChanged);

    connect(crop_early_check, &QCheckBox::clicked, [this] (bool checked) {
        if (!project)
            return;

        project->setCropEarly(checked);
    });

    // Resize.
    connect(resize_box, &QGroupBox::toggled, [this] (bool checked) {
        if (!project)
            return;

        project->setResizeEnabled(checked);
    });

    auto resizeChanged = [this] () {
        if (!project)
            return;

        project->setResize(resize_spin[0]->value(), resize_spin[1]->value(), resize_filter_combo->currentText().toLower().toStdString());
    };
    for (int i = 0; i < 2; i++)
        connect(resize_spin[i], static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), resizeChanged);

    connect(resize_filter_combo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), [this] (const QString &text) {
        if (!project)
            return;

        project->setResize(resize_spin[0]->value(), resize_spin[1]->value(), text.toLower().toStdString());
    });

    // Bit depth.
    int index_to_bits[] = { 8, 9, 10, 12, 16, 16, 32 };
    bool index_to_float_samples[] = { false, false, false, false, false, true, true };
    const char *index_to_dither[] = { "none", "ordered", "random", "error_diffusion" };

    connect(depth_box, &QGroupBox::toggled, [this, index_to_bits, index_to_float_samples, index_to_dither] (bool checked) {
        if (!project)
            return;

        int bits_index = depth_bits_combo->currentIndex();
        int dither_index = depth_dither_combo->currentIndex();

        project->setBitDepth(index_to_bits[bits_index], index_to_float_samples[bits_index], index_to_dither[dither_index]);
        project->setBitDepthEnabled(checked);
    });

    connect(depth_bits_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this, index_to_bits, index_to_float_samples, index_to_dither] (int index) {
        if (!project)
            return;

        int dither_index = depth_dither_combo->currentIndex();

        project->setBitDepth(index_to_bits[index], index_to_float_samples[index], index_to_dither[dither_index]);
    });

    connect(depth_dither_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this, index_to_bits, index_to_float_samples, index_to_dither] (int index) {
        if (!project)
            return;

        int bits_index = depth_bits_combo->currentIndex();

        project->setBitDepth(index_to_bits[bits_index], index_to_float_samples[bits_index], index_to_dither[index]);
    });


    // Crop.
    QVBoxLayout *vbox = new QVBoxLayout;
    for (int i = 0; i < 4; i++) {
        vbox->addWidget(crop_spin[i]);
    }
    vbox->addWidget(crop_early_check);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addLayout(vbox);
    hbox->addStretch(1);

    crop_box->setLayout(hbox);

    // Resize.
    vbox = new QVBoxLayout;
    for (int i = 0; i < 2; i++) {
        vbox->addWidget(resize_spin[i]);
    }
    vbox->addWidget(resize_filter_combo);

    hbox = new QHBoxLayout;
    hbox->addLayout(vbox);
    hbox->addStretch(1);

    resize_box->setLayout(hbox);

    // Bit depth.
    vbox = new QVBoxLayout;
    vbox->addWidget(depth_bits_combo);
    vbox->addWidget(depth_dither_combo);

    hbox = new QHBoxLayout;
    hbox->addLayout(vbox);
    hbox->addStretch(1);

    depth_box->setLayout(hbox);


    vbox = new QVBoxLayout;
    vbox->addWidget(crop_box);
    vbox->addWidget(resize_box);
    vbox->addWidget(depth_box);
    vbox->addStretch(1);


    QWidget *crop_widget = new QWidget;
    crop_widget->setLayout(vbox);

    crop_dock = new DockWidget("Cropping, resizing, bit depth", this);
    crop_dock->setObjectName("crop assistant");
    crop_dock->setVisible(false);
    crop_dock->setFloating(true);
    crop_dock->setWidget(crop_widget);
    addDockWidget(Qt::RightDockWidgetArea, crop_dock);
    tools_menu->addAction(crop_dock->toggleViewAction());
    connect(crop_dock, &DockWidget::visibilityChanged, crop_dock, &DockWidget::setEnabled);
    connect(crop_dock, &DockWidget::visibilityChanged, [this] {
        if (!project)
            return;

        try {
            evaluateMainDisplayScript();
        } catch (WobblyException &) {

        }
    });
}


void WobblyWindow::createPresetEditor() {
    preset_combo = new QComboBox;
    preset_combo->setModel(presets_model);

    preset_edit = new PresetTextEdit;
    preset_edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    preset_edit->setTabChangesFocus(true);
    preset_edit->setToolTip(QStringLiteral(
                "The preset is a Python function. It takes a single parameter, called 'clip'.\n"
                "Filter that and assign the result to the same variable.\n"
                "The return statement will be added automatically.\n"
                "The VapourSynth core object is called 'c'."
    ));

    QPushButton *new_button = new QPushButton(QStringLiteral("New"));
    QPushButton *rename_button = new QPushButton(QStringLiteral("Rename"));
    QPushButton *delete_button = new QPushButton(QStringLiteral("Delete"));

    connect(preset_combo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this, &WobblyWindow::presetChanged);

    connect(preset_edit, &PresetTextEdit::focusLost, this, &WobblyWindow::presetEdited);

    connect(new_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        bool ok = false;
        QString preset_name;

        while (!ok) {
            preset_name = QInputDialog::getText(this, QStringLiteral("New preset"), QStringLiteral("Use only letters, numbers, and the underscore character.\nThe first character cannot be a number."), QLineEdit::Normal, preset_name);

            if (!preset_name.isEmpty()) {
                try {
                    project->addPreset(preset_name.toStdString());

                    QStringList presets = presets_model->stringList();

                    // The "selected" preset has nothing to do with what preset is currently displayed in preset_combo.
                    int selected_index = getSelectedPreset();

                    QString selected;
                    if (selected_index > -1)
                        selected = presets[selected_index];

                    updatePresets();

                    if (selected_index > -1)
                        setSelectedPreset(presets.indexOf(selected));

                    preset_combo->setCurrentText(preset_name);

                    presetChanged(preset_name);

                    ok = true;
                } catch (WobblyException &e) {
                    errorPopup(e.what());
                }
            } else
                ok = true;
        }
    });

    connect(rename_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        if (preset_combo->currentIndex() == -1)
            return;

        bool ok = false;
        QString preset_name = preset_combo->currentText();

        while (!ok) {
            preset_name = QInputDialog::getText(this, QStringLiteral("Rename preset"), QStringLiteral("Use only letters, numbers, and the underscore character.\nThe first character cannot be a number."), QLineEdit::Normal, preset_name);

            if (!preset_name.isEmpty() && preset_name != preset_combo->currentText()) {
                try {
                    project->renamePreset(preset_combo->currentText().toStdString(), preset_name.toStdString());

                    updatePresets();

                    int index = presets_model->stringList().indexOf(preset_name);
                    setSelectedPreset(index);

                    preset_combo->setCurrentText(preset_name);

                    updateCustomListsEditor();

                    updateSectionsEditor();

                    updateFrameDetails();

                    ok = true;
                } catch (WobblyException &e) {
                    errorPopup(e.what());
                }
            } else
                ok = true;
        }
    });

    connect(delete_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        int index = preset_combo->currentIndex();
        if (index == -1)
            return;

        const std::string &preset = preset_combo->currentText().toStdString();

        bool preset_in_use = project->isPresetInUse(preset);

        if (preset_in_use) {
            QMessageBox::StandardButton answer = QMessageBox::question(this, QStringLiteral("Delete preset?"), QStringLiteral("Preset '%1' is in use. Delete anyway?").arg(preset.c_str()), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (answer == QMessageBox::No)
                return;
        }

        project->deletePreset(preset);

        updatePresets();

        setSelectedPreset(selected_preset);

        if (preset_combo->count()) {
            index = std::min(index, preset_combo->count() - 1);
            preset_combo->setCurrentIndex(index);
        }
        presetChanged(preset_combo->currentText());

        if (preset_in_use) {
            updateSectionsEditor();

            updateCustomListsEditor();
        }
    });


    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(new_button);
    hbox->addWidget(rename_button);
    hbox->addWidget(delete_button);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(preset_combo);
    vbox->addWidget(preset_edit);
    vbox->addLayout(hbox);

    QWidget *preset_widget = new QWidget;
    preset_widget->setLayout(vbox);


    preset_dock = new DockWidget("Presets", this);
    preset_dock->setObjectName("preset editor");
    preset_dock->setVisible(false);
    preset_dock->setFloating(true);
    preset_dock->setWidget(preset_widget);
    addDockWidget(Qt::RightDockWidgetArea, preset_dock);
    tools_menu->addAction(preset_dock->toggleViewAction());
    connect(preset_dock, &DockWidget::visibilityChanged, preset_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createPatternEditor() {
    match_pattern_edit = new QLineEdit(match_pattern);
    decimation_pattern_edit = new QLineEdit(decimation_pattern);

    QRegExpValidator *match_validator = new QRegExpValidator(QRegExp("[pcn]{5,}"), this);
    QRegExpValidator *decimation_validator = new QRegExpValidator(QRegExp("[dk]{5,}"), this);

    match_pattern_edit->setValidator(match_validator);
    decimation_pattern_edit->setValidator(decimation_validator);

    QPushButton *fill_24fps_button = new QPushButton(QStringLiteral("&24 fps"));
    QPushButton *fill_30fps_button = new QPushButton(QStringLiteral("&30 fps"));


    connect(match_pattern_edit, &QLineEdit::textEdited, [this] (const QString &text) {
        match_pattern = text;
    });

    connect(decimation_pattern_edit, &QLineEdit::textEdited, [this] (const QString &text) {
        decimation_pattern = text;
    });

    connect(fill_24fps_button, &QPushButton::clicked, [this] () {
        match_pattern = "ccnnc";
        match_pattern_edit->setText(match_pattern);

        decimation_pattern = "kkkkd";
        decimation_pattern_edit->setText(decimation_pattern);
    });

    connect(fill_30fps_button, &QPushButton::clicked, [this] () {
        match_pattern = "ccccc";
        match_pattern_edit->setText(match_pattern);

        decimation_pattern = "kkkkk";
        decimation_pattern_edit->setText(decimation_pattern);
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(new QLabel(QStringLiteral("Match pattern:")));
    vbox->addWidget(match_pattern_edit);
    vbox->addWidget(new QLabel(QStringLiteral("Decimation pattern:")));
    vbox->addWidget(decimation_pattern_edit);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(fill_24fps_button);
    hbox->addWidget(fill_30fps_button);
    hbox->addStretch(1);

    vbox->addLayout(hbox);
    vbox->addStretch(1);

    QWidget *pattern_widget = new QWidget;
    pattern_widget->setLayout(vbox);


    pattern_dock = new DockWidget("Pattern editor", this);
    pattern_dock->setObjectName("pattern editor");
    pattern_dock->setVisible(false);
    pattern_dock->setFloating(true);
    pattern_dock->setWidget(pattern_widget);
    addDockWidget(Qt::RightDockWidgetArea, pattern_dock);
    tools_menu->addAction(pattern_dock->toggleViewAction());
    connect(pattern_dock, &DockWidget::visibilityChanged, pattern_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createSectionsEditor() {
    sections_table = new TableWidget(0, 2, this);
    sections_table->setHorizontalHeaderLabels({ "Start", "Presets" });

    QPushButton *delete_sections_button = new QPushButton("Delete");

    short_sections_box = new QGroupBox("Show only short sections");
    short_sections_box->setCheckable(true);
    short_sections_box->setChecked(false);

    short_sections_spin = new QSpinBox;
    short_sections_spin->setValue(10);
    short_sections_spin->setPrefix(QStringLiteral("Maximum: "));
    short_sections_spin->setSuffix(QStringLiteral(" frames"));

    ListWidget *section_presets_list = new ListWidget;
    section_presets_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    section_presets_list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QPushButton *move_preset_up_button = new QPushButton("Move up");
    QPushButton *move_preset_down_button = new QPushButton("Move down");
    QPushButton *remove_preset_button = new QPushButton("Remove");

    QListView *preset_list = new QListView;
    preset_list->setModel(presets_model);
    preset_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    preset_list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QPushButton *append_presets_button = new QPushButton("Append");


    connect(sections_table, &TableWidget::cellDoubleClicked, [this] (int row, int column) {
        (void)column;
        QTableWidgetItem *item = sections_table->item(row, 0);
        bool ok;
        int frame = item->text().toInt(&ok);
        if (ok)
            requestFrames(frame);
    });

    connect(sections_table, &TableWidget::currentCellChanged, [this, section_presets_list] (int currentRow) {
        if (currentRow < 0)
            return;

        section_presets_list->clear();
        bool ok;
        int frame = sections_table->item(currentRow, 0)->text().toInt(&ok);
        if (ok) {
            const Section *section = project->findSection(frame);
            for (auto it = section->presets.cbegin(); it != section->presets.cend(); it++)
                section_presets_list->addItem(QString::fromStdString(*it));
        }
    });

    connect(sections_table, &TableWidget::deletePressed, delete_sections_button, &QPushButton::click);

    connect(delete_sections_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        auto selection = sections_table->selectedRanges();

        for (int i = selection.size() - 1; i >= 0; i--) {
            for (int j = selection[i].bottomRow(); j >= selection[i].topRow(); j--) {
                bool ok;
                int frame = sections_table->item(j, 0)->text().toInt(&ok);
                if (ok && frame != 0) {
                    project->deleteSection(frame);
                    sections_table->removeRow(j);
                }
            }
        }

        if (sections_table->rowCount())
            sections_table->selectRow(sections_table->currentRow());

        if (selection.size())
            updateFrameDetails();
    });

    connect(short_sections_box, &QGroupBox::clicked, [this] (bool checked) {
        if (!project)
            return;

        (void)checked;

        updateSectionsEditor();
    });

    connect(short_sections_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        if (!project)
            return;

        (void)value;

        updateSectionsEditor();
    });

    connect(section_presets_list, &ListWidget::deletePressed, remove_preset_button, &QPushButton::click);

    connect(move_preset_up_button, &QPushButton::clicked, [this, section_presets_list] () {
        if (!project)
            return;

        int sections_row = sections_table->currentRow();
        auto selected_presets = section_presets_list->selectedItems();
        bool ok;
        int frame = sections_table->item(sections_row, 0)->text().toInt(&ok);
        if (ok && selected_presets.size()) {
            for (int i = 0; i < selected_presets.size(); i++) {
                int preset_row = section_presets_list->row(selected_presets[i]);
                if (preset_row == 0)
                    break;
                section_presets_list->insertItem(preset_row - 1, section_presets_list->takeItem(preset_row));
                section_presets_list->item(preset_row)->setSelected(false);
                section_presets_list->item(preset_row - 1)->setSelected(true);
            }

            Section *section = project->findSection(frame);
            QString presets;
            for (size_t i = 0; i < section->presets.size(); i++) {
                section->presets[i] = section_presets_list->item(i)->text().toStdString();

                if (i > 0)
                    presets += ",";
                presets += QString::fromStdString(section->presets[i]);
            }
            QTableWidgetItem *item = new QTableWidgetItem(presets);
            sections_table->setItem(sections_row, 1, item);

            updateFrameDetails();
        }
    });

    connect(move_preset_down_button, &QPushButton::clicked, [this, section_presets_list] () {
        if (!project)
            return;

        int sections_row = sections_table->currentRow();
        auto selected_presets = section_presets_list->selectedItems();
        bool ok;
        int frame = sections_table->item(sections_row, 0)->text().toInt(&ok);
        if (ok && selected_presets.size()) {
            for (int i = selected_presets.size() - 1; i >= 0; i--) {
                int preset_row = section_presets_list->row(selected_presets[i]);
                if (preset_row == section_presets_list->count() - 1)
                    break;
                section_presets_list->insertItem(preset_row + 1, section_presets_list->takeItem(preset_row));
                section_presets_list->item(preset_row)->setSelected(false);
                section_presets_list->item(preset_row + 1)->setSelected(true);
            }

            Section *section = project->findSection(frame);
            QString presets;
            for (size_t i = 0; i < section->presets.size(); i++) {
                section->presets[i] = section_presets_list->item(i)->text().toStdString();

                if (i > 0)
                    presets += ",";
                presets += QString::fromStdString(section->presets[i]);
            }
            QTableWidgetItem *item = new QTableWidgetItem(presets);
            sections_table->setItem(sections_row, 1, item);

            updateFrameDetails();
        }
    });

    connect(remove_preset_button, &QPushButton::clicked, [this, section_presets_list] () {
        if (!project)
            return;

        int sections_row = sections_table->currentRow();
        auto selected_presets = section_presets_list->selectedItems();
        bool ok;
        int frame = sections_table->item(sections_row, 0)->text().toInt(&ok);
        if (ok && selected_presets.size()) {
            Section *section = project->findSection(frame);
            for (int i = selected_presets.size() - 1; i >= 0; i--) {
                section->presets.erase(section->presets.cbegin() + section_presets_list->row(selected_presets[i]));
            }
            section_presets_list->clear();

            QString presets;
            for (size_t i = 0; i < section->presets.size(); i++) {
                section_presets_list->addItem(QString::fromStdString(section->presets[i]));
                if (i > 0)
                    presets += ",";
                presets += QString::fromStdString(section->presets[i]);
            }
            QTableWidgetItem *item = new QTableWidgetItem(presets);
            sections_table->setItem(sections_row, 1, item);

            updateFrameDetails();
        }
    });

    connect(append_presets_button, &QPushButton::clicked, [this, section_presets_list, preset_list] () {
        if (!project)
            return;

        auto selected_presets = preset_list->selectionModel()->selectedRows();
        auto selected_sections = sections_table->selectedItems();

        if (selected_presets.size()) {
            QStringList presets = presets_model->stringList();

            for (auto section = selected_sections.cbegin(); section != selected_sections.cend(); section++)
                for (auto model_index = selected_presets.cbegin(); model_index != selected_presets.cend(); model_index++) {
                    bool ok;
                    int frame = (*section)->text().toInt(&ok);
                    if (ok) {
                        const QString &preset = presets[model_index->row()];

                        project->setSectionPreset(frame, preset.toStdString());
                        section_presets_list->addItem(preset);

                        QTableWidgetItem *presets_item = sections_table->item((*section)->row(), 1);
                        if (!presets_item) {
                            presets_item = new QTableWidgetItem;
                            sections_table->setItem((*section)->row(), 1, presets_item);
                        }
                        QString presets_text = presets_item->text();
                        if (presets_text.size())
                            presets_text += ",";
                        presets_item->setText(presets_text + preset);
                    }
                }
            if (selected_sections.size())
                updateFrameDetails();
        }
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(new QLabel("Sections:"));
    vbox->addWidget(sections_table, 1);

    QVBoxLayout *vbox2 = new QVBoxLayout;
    vbox2->addWidget(delete_sections_button);
    vbox2->addStretch(1);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(short_sections_spin);
    hbox->addStretch(1);
    short_sections_box->setLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addLayout(vbox2);
    hbox->addWidget(short_sections_box);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    QHBoxLayout *hbox2 = new QHBoxLayout;
    hbox2->addLayout(vbox);

    vbox = new QVBoxLayout;
    vbox->addWidget(new QLabel("Section's presets:"));
    vbox->addWidget(section_presets_list, 1);

    vbox2 = new QVBoxLayout;
    vbox2->addWidget(move_preset_up_button);
    vbox2->addWidget(move_preset_down_button);
    vbox2->addWidget(remove_preset_button);
    hbox = new QHBoxLayout;
    hbox->addLayout(vbox2);
    hbox->addStretch(1);
    vbox->addLayout(hbox);
    hbox2->addLayout(vbox);

    vbox = new QVBoxLayout;
    vbox->addWidget(new QLabel("Available presets:"));
    vbox->addWidget(preset_list, 1);

    hbox = new QHBoxLayout;
    hbox->addWidget(append_presets_button);
    hbox->addStretch(1);
    vbox->addLayout(hbox);
    hbox2->addLayout(vbox);


    QWidget *sections_widget = new QWidget;
    sections_widget->setLayout(hbox2);


    sections_dock = new DockWidget("Sections", this);
    sections_dock->setObjectName("sections editor");
    sections_dock->setVisible(false);
    sections_dock->setFloating(true);
    sections_dock->setWidget(sections_widget);
    addDockWidget(Qt::RightDockWidgetArea, sections_dock);
    tools_menu->addAction(sections_dock->toggleViewAction());
    connect(sections_dock, &DockWidget::visibilityChanged, sections_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createCustomListsEditor() {
    cl_table = new TableWidget(0, 3, this);
    cl_table->setHorizontalHeaderLabels({ "Name", "Preset", "Position" });


    QPushButton *cl_new_button = new QPushButton("New");
    QPushButton *cl_rename_button = new QPushButton("Rename");
    QPushButton *cl_delete_button = new QPushButton("Delete");
    QPushButton *cl_move_up_button = new QPushButton("Move up");
    QPushButton *cl_move_down_button = new QPushButton("Move down");


    QComboBox *cl_presets_box = new QComboBox;
    cl_presets_box->setModel(presets_model);


    QGroupBox *cl_position_box = new QGroupBox("Position in the filter chain");

    const char *positions[] = {
        "Post source",
        "Post field match",
        "Post decimate"
    };

    QButtonGroup *cl_position_group = new QButtonGroup(this);
    for (int i = 0; i < 3; i++)
        cl_position_group->addButton(new QRadioButton(positions[i]), i);
    cl_position_group->button(PostSource)->setChecked(true);


    ListWidget *cl_ranges_list = new ListWidget;
    cl_ranges_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    cl_ranges_list->setSelectionMode(QAbstractItemView::ExtendedSelection);


    QPushButton *cl_delete_range_button = new QPushButton("Delete");

    QPushButton *cl_send_range_button = new QPushButton("Send to list");
    cl_send_range_menu = new QMenu(this);
    cl_send_range_button->setMenu(cl_send_range_menu);

    QPushButton *cl_copy_range_button = new QPushButton("Copy to list");
    cl_copy_range_menu = new QMenu(this);
    cl_copy_range_button->setMenu(cl_copy_range_menu);


    connect(cl_table, &TableWidget::deletePressed, cl_delete_button, &QPushButton::click);

    connect(cl_table, &TableWidget::currentCellChanged, [this, cl_position_group, cl_presets_box, cl_ranges_list] (int currentRow) {
        if (currentRow < 0)
            return;

        auto cl = project->getCustomLists();

        if (!cl.size())
            return;

        // When deleting a custom list, currentRow has the wrong value somehow.
        if (currentRow >= (int)cl.size())
            currentRow = cl.size() - 1;

        cl_position_group->button(cl[currentRow].position)->setChecked(true);

        cl_presets_box->setCurrentText(QString::fromStdString(cl[currentRow].preset));

        cl_ranges_list->clear();
        for (auto it = cl[currentRow].ranges.cbegin(); it != cl[currentRow].ranges.cend(); it++) {
            QListWidgetItem *item = new QListWidgetItem(QStringLiteral("%1,%2").arg(it->second.first).arg(it->second.last));
            item->setData(Qt::UserRole, it->second.first);
            cl_ranges_list->addItem(item);
        }
    });

    connect(cl_new_button, &QPushButton::clicked, [this, positions] () {
        if (!project)
            return;

        bool ok = false;
        QString cl_name;

        while (!ok) {
            cl_name = QInputDialog::getText(
                        this,
                        QStringLiteral("New custom list"),
                        QStringLiteral("Use only letters, numbers, and the underscore character.\nThe first character cannot be a number."),
                        QLineEdit::Normal,
                        cl_name);

            if (!cl_name.isEmpty()) {
                try {
                    project->addCustomList(cl_name.toStdString());

                    updateCustomListsEditor();

                    ok = true;
                } catch (WobblyException &e) {
                    errorPopup(e.what());
                }
            } else
                ok = true;
        }
    });

    connect(cl_rename_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        int cl_index = cl_table->currentRow();
        if (cl_index < 0)
            return;

        QString old_name = cl_table->item(cl_index, 0)->text();

        bool ok = false;
        QString new_name = old_name;

        while (!ok) {
            new_name = QInputDialog::getText(
                        this,
                        QStringLiteral("Rename custom list"),
                        QStringLiteral("Use only letters, numbers, and the underscore character.\nThe first character cannot be a number."),
                        QLineEdit::Normal,
                        new_name);

            if (!new_name.isEmpty()) {
                try {
                    project->renameCustomList(old_name.toStdString(), new_name.toStdString());

                    if (cl_index == getSelectedCustomList())
                        setSelectedCustomList(cl_index);

                    updateCustomListsEditor();

                    updateFrameDetails();

                    ok = true;
                } catch (WobblyException &e) {
                    errorPopup(e.what());
                }
            } else
                ok = true;
        }
    });

    connect(cl_delete_button, &QPushButton::clicked, [this, cl_ranges_list] () {
        if (!project)
            return;

        auto selection = cl_table->selectedRanges();

        for (int i = selection.size() - 1; i >= 0; i--) {
            for (int j = selection[i].bottomRow(); j >= selection[i].topRow(); j--) {
                project->deleteCustomList(j);

                if (j == selected_custom_list)
                    setSelectedCustomList(selected_custom_list);
                else if (j < selected_custom_list)
                    selected_custom_list--;
            }
        }

        updateCustomListsEditor();

        if (cl_table->rowCount())
            cl_table->selectRow(cl_table->currentRow());

        if (!cl_table->rowCount())
            cl_ranges_list->clear();

        updateFrameDetails();
    });

    connect(cl_move_up_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        auto selection = cl_table->selectedRanges();
        if (selection.isEmpty())
            return;

        if (selection.first().topRow() == 0)
            return;

        for (int i = 0; i < selection.size(); i++) {
            for (int j = selection[i].topRow(); j <= selection[i].bottomRow(); j++) {
                project->moveCustomListUp(j);

                if (j == selected_custom_list + 1)
                    selected_custom_list++;
            }
        }

        updateCustomListsEditor();

        for (int i = 0; i < selection.size(); i++) {
            QTableWidgetSelectionRange range(selection[i].topRow() - 1, 0, selection[i].bottomRow() - 1, 2);
            cl_table->setRangeSelected(range, true);
        }

        updateFrameDetails();
    });

    connect(cl_move_down_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        auto selection = cl_table->selectedRanges();
        if (selection.isEmpty())
            return;

        if (selection.last().bottomRow() == cl_table->rowCount() - 1)
            return;

        for (int i = selection.size() - 1; i >= 0; i--) {
            for (int j = selection[i].bottomRow(); j >= selection[i].topRow(); j--) {
                project->moveCustomListDown(j);

                if (j == selected_custom_list - 1)
                    selected_custom_list--;
            }
        }

        updateCustomListsEditor();

        for (int i = 0; i < selection.size(); i++) {
            QTableWidgetSelectionRange range(selection[i].topRow() + 1, 0, selection[i].bottomRow() + 1, 2);
            cl_table->setRangeSelected(range, true);
        }

        updateFrameDetails();
    });

    connect(cl_presets_box, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), [this] (const QString &text) {
        if (!project)
            return;

        int cl_index = cl_table->currentRow();
        if (cl_index < 0)
            return;

        project->setCustomListPreset(cl_index, text.toStdString());

        cl_table->item(cl_index, 1)->setText(text);
    });

    connect(cl_position_group, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), [this, positions] (int id) {
        if (!project)
            return;

        int cl_index = cl_table->currentRow();
        if (cl_index < 0)
            return;

        project->setCustomListPosition(cl_index, (PositionInFilterChain)id);

        cl_table->item(cl_index, 2)->setText(positions[id]);
    });

    connect(cl_ranges_list, &ListWidget::deletePressed, cl_delete_range_button, &QPushButton::click);

    connect(cl_ranges_list, &ListWidget::itemDoubleClicked, [this] (QListWidgetItem *item) {
        if (!project)
            return;

        requestFrames(item->data(Qt::UserRole).toInt());
    });

    connect(cl_delete_range_button, &QPushButton::clicked, [this, cl_ranges_list] () {
        if (!project)
            return;

        int cl_index = cl_table->currentRow();
        if (cl_index < 0)
            return;

        auto selected_ranges = cl_ranges_list->selectedItems();
        for (int i = 0; i < selected_ranges.size(); i++) {
            project->deleteCustomListRange(cl_index, selected_ranges[i]->data(Qt::UserRole).toInt());
            delete selected_ranges[i];
        }

        updateFrameDetails();
    });

    connect(cl_send_range_menu, &QMenu::triggered, [this, cl_ranges_list, cl_delete_range_button] (QAction *action) {
        if (!project)
            return;

        int cl_src_index = cl_table->currentRow();
        if (cl_src_index < 0)
            return;

        int cl_dst_index = action->data().toInt();
        if (cl_src_index == cl_dst_index)
            return;

        auto selected_ranges = cl_ranges_list->selectedItems();
        for (int i = 0; i < selected_ranges.size(); i++) {
            auto range = project->findCustomListRange(cl_src_index, selected_ranges[i]->data(Qt::UserRole).toInt());
            project->addCustomListRange(cl_dst_index, range->first, range->last);
        }

        cl_delete_range_button->click();
    });

    connect(cl_copy_range_menu, &QMenu::triggered, [this, cl_ranges_list] (QAction *action) {
        if (!project)
            return;

        int cl_src_index = cl_table->currentRow();
        if (cl_src_index < 0)
            return;

        int cl_dst_index = action->data().toInt();
        if (cl_src_index == cl_dst_index)
            return;

        auto selected_ranges = cl_ranges_list->selectedItems();
        for (int i = 0; i < selected_ranges.size(); i++) {
            auto range = project->findCustomListRange(cl_src_index, selected_ranges[i]->data(Qt::UserRole).toInt());
            project->addCustomListRange(cl_dst_index, range->first, range->last);
        }
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(cl_table);

    QVBoxLayout *vbox2 = new QVBoxLayout;
    vbox2->addWidget(cl_new_button);
    vbox2->addWidget(cl_rename_button);
    vbox2->addWidget(cl_delete_button);
    vbox2->addWidget(cl_move_up_button);
    vbox2->addWidget(cl_move_down_button);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addLayout(vbox2);

    vbox2 = new QVBoxLayout;
    for (int i = 0; i < 3; i++)
        vbox2->addWidget(cl_position_group->button(i));
    cl_position_box->setLayout(vbox2);

    vbox2 = new QVBoxLayout;
    vbox2->addWidget(cl_presets_box);
    vbox2->addWidget(cl_position_box);

    hbox->addLayout(vbox2);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    QHBoxLayout *hbox2 = new QHBoxLayout;
    hbox2->addLayout(vbox);

    vbox = new QVBoxLayout;
    vbox->addWidget(cl_ranges_list);

    hbox = new QHBoxLayout;
    hbox->addWidget(cl_delete_range_button);
    hbox->addWidget(cl_send_range_button);
    hbox->addWidget(cl_copy_range_button);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    hbox2->addLayout(vbox);


    QWidget *cl_widget = new QWidget;
    cl_widget->setLayout(hbox2);


    cl_dock = new DockWidget("Custom lists", this);
    cl_dock->setObjectName("custom lists editor");
    cl_dock->setVisible(false);
    cl_dock->setFloating(true);
    cl_dock->setWidget(cl_widget);
    addDockWidget(Qt::RightDockWidgetArea, cl_dock);
    tools_menu->addAction(cl_dock->toggleViewAction());
    connect(cl_dock, &DockWidget::visibilityChanged, cl_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createFrameRatesViewer() {
    QGroupBox *frame_rates_group = new QGroupBox("Show rates");
    frame_rates_group->setCheckable(false);

    frame_rates_buttons = new QButtonGroup(this);
    frame_rates_buttons->setExclusive(false);
    const char *rates[] = { "&30", "2&4", "1&8", "1&2", "&6" };
    for (int i = 0; i < 5; i++)
        frame_rates_buttons->addButton(new QCheckBox(rates[i] + QStringLiteral(" fps")), i);

    frame_rates_table = new TableWidget(0, 3, this);
    frame_rates_table->setHorizontalHeaderLabels({ "Start", "End", "Frame rate" });


    connect(frame_rates_buttons, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), [this] () {
        if (!project)
            return;

        std::array<bool, 5> shown_rates;
        for (int i = 0; i < 5; i++)
            shown_rates[i] = frame_rates_buttons->button(i)->isChecked();

        project->setShownFrameRates(shown_rates);

        updateFrameRatesViewer();
    });


    connect(frame_rates_table, &TableWidget::cellDoubleClicked, [this] (int row) {
        QTableWidgetItem *item = frame_rates_table->item(row, 0);
        bool ok;
        int frame = item->text().toInt(&ok);
        if (ok)
            requestFrames(frame);
    });


    QHBoxLayout *hbox = new QHBoxLayout;
    for (int i = 0; i < 5; i++)
        hbox->addWidget(frame_rates_buttons->button(i));

    frame_rates_group->setLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(frame_rates_group);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addLayout(hbox);
    vbox->addWidget(frame_rates_table);


    QWidget *frame_rates_widget = new QWidget;
    frame_rates_widget->setLayout(vbox);


    frame_rates_dock = new DockWidget("Frame rates", this);
    frame_rates_dock->setObjectName("frame rates viewer");
    frame_rates_dock->setVisible(false);
    frame_rates_dock->setFloating(true);
    frame_rates_dock->setWidget(frame_rates_widget);
    addDockWidget(Qt::RightDockWidgetArea, frame_rates_dock);
    tools_menu->addAction(frame_rates_dock->toggleViewAction());
    connect(frame_rates_dock, &DockWidget::visibilityChanged, frame_rates_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createFrozenFramesViewer() {
    frozen_frames_table = new TableWidget(0, 3, this);
    frozen_frames_table->setHorizontalHeaderLabels({ "First", "Last", "Replacement" });

    QPushButton *delete_button = new QPushButton("Delete");


    connect(frozen_frames_table, &TableWidget::cellDoubleClicked, [this] (int row) {
        QTableWidgetItem *item = frozen_frames_table->item(row, 0);
        bool ok;
        int frame = item->text().toInt(&ok);
        if (ok)
            requestFrames(frame);
    });

    connect(frozen_frames_table, &TableWidget::deletePressed, delete_button, &QPushButton::click);

    connect(delete_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        auto selection = frozen_frames_table->selectedRanges();

        for (int i = selection.size() - 1; i >= 0; i--) {
            for (int j = selection[i].bottomRow(); j >= selection[i].topRow(); j--) {
                bool ok;
                int frame = frozen_frames_table->item(j, 0)->text().toInt(&ok);
                if (ok) {
                    project->deleteFreezeFrame(frame);
                    frozen_frames_table->removeRow(j);
                }
            }
        }

        if (frozen_frames_table->rowCount())
            frozen_frames_table->selectRow(frozen_frames_table->currentRow());

        if (selection.size()) {
            try {
                evaluateMainDisplayScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());
            }
        }
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(frozen_frames_table);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(delete_button);
    hbox->addStretch(1);
    vbox->addLayout(hbox);


    QWidget *frozen_frames_widget = new QWidget;
    frozen_frames_widget->setLayout(vbox);


    frozen_frames_dock = new DockWidget("Frozen frames", this);
    frozen_frames_dock->setObjectName("frozen frames viewer");
    frozen_frames_dock->setVisible(false);
    frozen_frames_dock->setFloating(true);
    frozen_frames_dock->setWidget(frozen_frames_widget);
    addDockWidget(Qt::RightDockWidgetArea, frozen_frames_dock);
    tools_menu->addAction(frozen_frames_dock->toggleViewAction());
    connect(frozen_frames_dock, &DockWidget::visibilityChanged, frozen_frames_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createPatternGuessingWindow() {
    QGroupBox *pg_methods_group = new QGroupBox(QStringLiteral("Guessing method"));

    std::map<int, QString> guessing_methods = {
        { PatternGuessingFromMatches, "From matches" },
        { PatternGuessingFromMics, "From mics" }
    };
    pg_methods_buttons = new QButtonGroup(this);
    for (auto it = guessing_methods.cbegin(); it != guessing_methods.cend(); it++)
        pg_methods_buttons->addButton(new QRadioButton(it->second), it->first);
    pg_methods_buttons->button(PatternGuessingFromMatches)->setChecked(true);

    pg_length_spin = new QSpinBox;
    pg_length_spin->setMaximum(999);
    pg_length_spin->setPrefix(QStringLiteral("Minimum length: "));
    pg_length_spin->setSuffix(QStringLiteral(" frames"));
    pg_length_spin->setValue(10);
    pg_length_spin->setToolTip(QStringLiteral("Sections shorter than this will be skipped."));

    QGroupBox *pg_n_match_group = new QGroupBox(QStringLiteral("Use third N match"));

    const char *third_n_match[] = {
        "Always",
        "Never",
        "If it has lower mic"
    };
    pg_n_match_buttons = new QButtonGroup(this);
    for (int i = 0; i < 3; i++)
        pg_n_match_buttons->addButton(new QRadioButton(third_n_match[i]), i);
    pg_n_match_buttons->button(UseThirdNMatchNever)->setChecked(true);

    pg_n_match_buttons->button(UseThirdNMatchAlways)->setToolTip(QStringLiteral(
        "Always generate 'ccnnn' matches.\n"
        "\n"
        "Sometimes helps with field-blended hard telecine."));
    pg_n_match_buttons->button(UseThirdNMatchNever)->setToolTip(QStringLiteral(
        "Always generate 'cccnn' matches.\n"
        "\n"
        "Good for clean hard telecine."));
    pg_n_match_buttons->button(UseThirdNMatchIfPrettier)->setToolTip(QStringLiteral(
        "Generate 'ccnnn' matches if they result in a lower mic than\n"
        "with 'cccnn' matches (per cycle).\n"
        "\n"
        "Use with field-blended hard telecine."));

    QGroupBox *pg_decimate_group = new QGroupBox(QStringLiteral("Decimate"));

    const char *decimate[] = {
        "First duplicate",
        "Second duplicate",
        "Duplicate with higher mic per cycle",
        "Duplicate with higher mic per section"
    };
    pg_decimate_buttons = new QButtonGroup(this);
    for (int i = 0; i < 4; i++)
        pg_decimate_buttons->addButton(new QRadioButton(decimate[i]), i);
    pg_decimate_buttons->button(DropFirstDuplicate)->setChecked(true);

    pg_decimate_buttons->button(DropFirstDuplicate)->setToolTip(QStringLiteral(
        "Always drop the first duplicate. The first duplicate may have\n"
        "more compression artifacts than the second one.\n"
        "\n"
        "Use with clean hard telecine."));
    pg_decimate_buttons->button(DropSecondDuplicate)->setToolTip(QStringLiteral(
        "Always drop the second duplicate.\n"
        "\n"
        "Use with clean hard telecine."));
    pg_decimate_buttons->button(DropUglierDuplicatePerCycle)->setToolTip(QStringLiteral(
        "Drop the duplicate that is more likely to be combed in each cycle.\n"
        "\n"
        "When the first duplicate happens to be the last frame in the cycle,\n"
        "this will be done per section, to avoid creating unwanted 18 fps and\n"
        "30 fps cycles.\n"
        "\n"
        "Use with field-blended hard telecine."));
    pg_decimate_buttons->button(DropUglierDuplicatePerSection)->setToolTip(QStringLiteral(
        "Drop the duplicate that is more likely to be combed, on average,\n"
        "in the entire section.\n"
        "\n"
        "Use with field-blended hard telecine."));

    QGroupBox *pg_use_patterns_group = new QGroupBox(QStringLiteral("Use patterns"));

    std::map<int, QString> use_patterns = {
        { PatternCCCNN, "CCCNN" },
        { PatternCCNNN, "CCNNN" },
        { PatternCCCCC, "CCCCC" }
    };
    pg_use_patterns_buttons = new QButtonGroup(this);
    pg_use_patterns_buttons->setExclusive(false);
    for (auto it = use_patterns.cbegin(); it != use_patterns.cend(); it++) {
        pg_use_patterns_buttons->addButton(new QCheckBox(it->second), it->first);
        pg_use_patterns_buttons->button(it->first)->setChecked(true);
    }

    QPushButton *pg_process_section_button = new QPushButton(QStringLiteral("Process current section"));

    QPushButton *pg_process_project_button = new QPushButton(QStringLiteral("Process project"));

    pg_failures_table = new TableWidget(0, 2, this);
    pg_failures_table->setHorizontalHeaderLabels({ "Section", "Reason for failure" });


    connect(pg_use_patterns_buttons, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled), [this] (int id, bool checked) {
        if (id == PatternCCCNN && !checked && !pg_use_patterns_buttons->button(PatternCCNNN)->isChecked())
            pg_use_patterns_buttons->button(PatternCCNNN)->setChecked(true);
        else if (id == PatternCCNNN && !checked && !pg_use_patterns_buttons->button(PatternCCCNN)->isChecked())
            pg_use_patterns_buttons->button(PatternCCCNN)->setChecked(true);
    });

    connect(pg_process_section_button, &QPushButton::clicked, [this] () {
        if (pg_methods_buttons->checkedId() == PatternGuessingFromMatches)
            guessCurrentSectionPatternsFromMatches();
        else
            guessCurrentSectionPatternsFromMics();
    });

    connect(pg_process_project_button, &QPushButton::clicked, [this] () {
        if (pg_methods_buttons->checkedId() == PatternGuessingFromMatches)
            guessProjectPatternsFromMatches();
        else
            guessProjectPatternsFromMics();
    });

    connect(pg_failures_table, &TableWidget::cellDoubleClicked, [this] (int row) {
        QTableWidgetItem *item = pg_failures_table->item(row, 0);
        bool ok;
        int frame = item->text().toInt(&ok);
        if (ok)
            requestFrames(frame);
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    for (auto it = guessing_methods.cbegin(); it != guessing_methods.cend(); it++)
        vbox->addWidget(pg_methods_buttons->button(it->first));
    pg_methods_group->setLayout(vbox);

    vbox = new QVBoxLayout;
    for (int i = 0; i < 3; i++)
        vbox->addWidget(pg_n_match_buttons->button(i));
    pg_n_match_group->setLayout(vbox);

    vbox = new QVBoxLayout;
    for (int i = 0; i < 4; i++)
        vbox->addWidget(pg_decimate_buttons->button(i));
    pg_decimate_group->setLayout(vbox);

    vbox = new QVBoxLayout;
    for (auto it = use_patterns.cbegin(); it != use_patterns.cend(); it++)
        vbox->addWidget(pg_use_patterns_buttons->button(it->first));
    pg_use_patterns_group->setLayout(vbox);

    vbox = new QVBoxLayout;

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(pg_methods_group);
    hbox->addWidget(pg_length_spin);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(pg_n_match_group);
    hbox->addWidget(pg_decimate_group);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    // Kind of awful to put it here, but replaceWidget() only works the first two times it's called. (wtf?)
    // Or maybe the second time it removes "from", but doesn't insert "to"?
    connect(pg_methods_buttons, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled), [this, pg_n_match_group, pg_use_patterns_group, hbox] (int id) {
        QWidget *from = pg_n_match_group;
        QWidget *to = pg_use_patterns_group;

        if (id == PatternGuessingFromMatches)
            std::swap(from, to);

        int index = hbox->indexOf(from);
        if (index > -1) {
            hbox->removeWidget(from);
            from->hide();
            hbox->insertWidget(index, to);
            to->show();
        }
    });

    hbox = new QHBoxLayout;
    hbox->addWidget(pg_process_section_button);
    hbox->addWidget(pg_process_project_button);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    vbox->addWidget(pg_failures_table);


    QWidget *pg_widget = new QWidget;
    pg_widget->setLayout(vbox);


    pg_dock = new DockWidget("Pattern guessing", this);
    pg_dock->setObjectName("pattern guessing window");
    pg_dock->setVisible(false);
    pg_dock->setFloating(true);
    pg_dock->setWidget(pg_widget);
    addDockWidget(Qt::RightDockWidgetArea, pg_dock);
    tools_menu->addAction(pg_dock->toggleViewAction());
    connect(pg_dock, &DockWidget::visibilityChanged, pg_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createMicSearchWindow() {
    mic_search_minimum_spin = new QSpinBox;
    mic_search_minimum_spin->setRange(0, 256);
    mic_search_minimum_spin->setPrefix(QStringLiteral("Minimum: "));

    QPushButton *mic_search_previous_button = new QPushButton("Jump to previous");
    QPushButton *mic_search_next_button = new QPushButton("Jump to next");


    connect(mic_search_minimum_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        if (!project)
            return;

        project->setMicSearchMinimum(value);
    });

    connect(mic_search_previous_button, &QPushButton::clicked, this, &WobblyWindow::jumpToPreviousMic);

    connect(mic_search_next_button, &QPushButton::clicked, this, &WobblyWindow::jumpToNextMic);


    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(mic_search_minimum_spin);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(mic_search_previous_button);
    hbox->addWidget(mic_search_next_button);
    hbox->addStretch(1);

    vbox->addLayout(hbox);
    vbox->addStretch(1);


    QWidget *mic_search_widget = new QWidget;
    mic_search_widget->setLayout(vbox);


    mic_search_dock = new DockWidget("Mic search", this);
    mic_search_dock->setObjectName("mic search window");
    mic_search_dock->setVisible(false);
    mic_search_dock->setFloating(true);
    mic_search_dock->setWidget(mic_search_widget);
    addDockWidget(Qt::RightDockWidgetArea, mic_search_dock);
    tools_menu->addAction(mic_search_dock->toggleViewAction());
    connect(mic_search_dock, &DockWidget::visibilityChanged, mic_search_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createCMatchSequencesWindow() {
    c_match_minimum_spin = new QSpinBox;
    c_match_minimum_spin->setRange(4, 999);
    c_match_minimum_spin->setPrefix(QStringLiteral("Minimum: "));
    c_match_minimum_spin->setSuffix(QStringLiteral(" frames"));

    c_match_sequences_table = new TableWidget(0, 2, this);
    c_match_sequences_table->setHorizontalHeaderLabels({ "Start", "Length" });


    connect(c_match_minimum_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        if (!project)
            return;

        project->setCMatchSequencesMinimum(value);

        updateCMatchSequencesWindow();
    });

    connect(c_match_sequences_table, &TableWidget::cellDoubleClicked, [this] (int row) {
        QTableWidgetItem *item = c_match_sequences_table->item(row, 0);
        bool ok;
        int frame = item->text().toInt(&ok);
        if (ok)
            requestFrames(frame);
    });


    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(c_match_minimum_spin);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addLayout(hbox);

    vbox->addWidget(c_match_sequences_table);


    QWidget *c_match_sequences_widget = new QWidget;
    c_match_sequences_widget->setLayout(vbox);


    c_match_sequences_dock = new DockWidget("C match sequences", this);
    c_match_sequences_dock->setObjectName("c match sequences window");
    c_match_sequences_dock->setVisible(false);
    c_match_sequences_dock->setFloating(true);
    c_match_sequences_dock->setWidget(c_match_sequences_widget);
    addDockWidget(Qt::RightDockWidgetArea, c_match_sequences_dock);
    tools_menu->addAction(c_match_sequences_dock->toggleViewAction());
    connect(c_match_sequences_dock, &DockWidget::visibilityChanged, c_match_sequences_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createFadesWindow() {
    fades_gaps_spin = new QSpinBox;
    fades_gaps_spin->setRange(0, 100);
    fades_gaps_spin->setValue(1);
    fades_gaps_spin->setPrefix(QStringLiteral("Ignore gaps of "));
    fades_gaps_spin->setSuffix(QStringLiteral(" frames or fewer"));

    fades_table = new TableWidget(0, 2, this);
    fades_table->setHorizontalHeaderLabels({ "Start", "End" });


    connect(fades_gaps_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] () {
        if (!project)
            return;

        updateFadesWindow();
    });

    connect(fades_table, &TableWidget::cellDoubleClicked, [this] (int row, int column) {
        QTableWidgetItem *item = fades_table->item(row, column);
        bool ok;
        int frame = item->text().toInt(&ok);
        if (ok)
            requestFrames(frame);
    });


    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(fades_gaps_spin);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addLayout(hbox);
    vbox->addWidget(fades_table);


    QWidget *fades_widget = new QWidget;
    fades_widget->setLayout(vbox);


    fades_dock = new DockWidget("Interlaced fades", this);
    fades_dock->setObjectName("interlaced fades window");
    fades_dock->setVisible(false);
    fades_dock->setFloating(true);
    fades_dock->setWidget(fades_widget);
    addDockWidget(Qt::RightDockWidgetArea, fades_dock);
    tools_menu->addAction(fades_dock->toggleViewAction());
    connect(fades_dock, &DockWidget::visibilityChanged, fades_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createSettingsWindow() {
    settings_font_spin = new QSpinBox;
    settings_font_spin->setRange(4, 99);
    settings_font_spin->setPrefix(QStringLiteral("Font size: "));

    settings_compact_projects_check = new QCheckBox("Create compact project files");

    settings_colormatrix_combo = new QComboBox;
    settings_colormatrix_combo->addItems({
                                             "BT 601",
                                             "BT 709",
                                             "BT 2020 NCL",
                                             "BT 2020 CL"
                                         });
    settings_colormatrix_combo->setCurrentIndex(1);

    settings_cache_spin = new QSpinBox;
    settings_cache_spin->setRange(1, 99999);
    settings_cache_spin->setValue(200);
    settings_cache_spin->setPrefix(QStringLiteral("Maximum cache size: "));
    settings_cache_spin->setSuffix(QStringLiteral(" MiB"));

    settings_print_details_check = new QCheckBox(QStringLiteral("Print frame details on top of the video"));

    settings_shortcuts_table = new TableWidget(0, 3, this);
    settings_shortcuts_table->setHorizontalHeaderLabels({ "Current", "Default", "Description" });

    QLineEdit *settings_shortcut_edit = new QLineEdit;

    QPushButton *settings_reset_shortcuts_button = new QPushButton("Reset selected shortcuts");


    connect(settings_font_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        QFont font = QApplication::font();
        font.setPointSize(value);
        QApplication::setFont(font);

        settings.setValue("user_interface/font_size", value);
    });

    connect(settings_compact_projects_check, &QCheckBox::clicked, [this] (bool checked) {
        settings.setValue("projects/compact_project_files", checked);
    });

    connect(settings_colormatrix_combo, &QComboBox::currentTextChanged, [this] (const QString &text) {
        settings.setValue("user_interface/colormatrix", text);

        if (!project)
            return;

        try {
            evaluateScript(preview);
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    });

    connect(settings_cache_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        settings.setValue("user_interface/maximum_cache_size", value);
    });

    connect(settings_print_details_check, &QCheckBox::toggled, [this] (bool checked) {
        settings.setValue("user_interface/print_details_on_video", checked);

        updateFrameDetails();
    });

    connect(settings_shortcuts_table, &TableWidget::cellDoubleClicked, settings_shortcut_edit, static_cast<void (QLineEdit::*)()>(&QLineEdit::setFocus));

    connect(settings_shortcuts_table, &TableWidget::currentCellChanged, [this, settings_shortcut_edit] (int currentRow) {
        if (currentRow < 0)
            return;

        settings_shortcut_edit->setText(shortcuts[currentRow].keys);
    });

    connect(settings_shortcut_edit, &QLineEdit::textEdited, [this] (const QString &text) {
        int row = settings_shortcuts_table->currentRow();
        if (row < 0)
            return;

        QString keys;
        QKeySequence key_sequence(text);
        if (key_sequence.count() <= 1)
            keys = key_sequence.toString();
        shortcuts[row].keys = keys;
        settings_shortcuts_table->item(row, 0)->setText(keys);
        settings_shortcuts_table->resizeColumnToContents(0);
    });

    connect(settings_shortcut_edit, &QLineEdit::editingFinished, [this] () {
        int row = settings_shortcuts_table->currentRow();
        if (row < 0)
            return;

        // XXX No duplicate shortcuts.

        QString settings_key = "user_interface/keys/" + shortcuts[row].description;

        if (shortcuts[row].keys == shortcuts[row].default_keys)
            settings.remove(settings_key);
        else
            settings.setValue(settings_key, shortcuts[row].keys);
    });

    connect(settings_reset_shortcuts_button, &QPushButton::clicked, [this, settings_shortcut_edit] () {
        int current_row = settings_shortcuts_table->currentRow();

        auto selection = settings_shortcuts_table->selectedRanges();

        for (int i = 0; i < selection.size(); i++) {
            for (int j = selection[i].topRow(); j <= selection[i].bottomRow(); j++) {
                if (shortcuts[j].keys != shortcuts[j].default_keys) {
                    shortcuts[j].keys = shortcuts[j].default_keys;

                    settings_shortcuts_table->item(j, 0)->setText(shortcuts[j].keys);

                    if (j == current_row)
                        settings_shortcut_edit->setText(shortcuts[j].keys);

                    settings.remove("user_interface/keys/" + shortcuts[j].description);
                }
            }
        }
    });


    QTabWidget *settings_tabs = new QTabWidget;

    QVBoxLayout *vbox = new QVBoxLayout;

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(settings_font_spin);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(settings_compact_projects_check);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(new QLabel("Colormatrix:"));
    hbox->addWidget(settings_colormatrix_combo);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(settings_cache_spin);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(settings_print_details_check);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    vbox->addStretch(1);

    QWidget *settings_general_widget = new QWidget;
    settings_general_widget->setLayout(vbox);
    settings_tabs->addTab(settings_general_widget, "General");

    vbox = new QVBoxLayout;
    vbox->addWidget(settings_shortcuts_table);

    hbox = new QHBoxLayout;
    hbox->addWidget(new QLabel("Edit shortcut:"));
    hbox->addWidget(settings_shortcut_edit);
    hbox->addWidget(settings_reset_shortcuts_button);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    QWidget *settings_shortcuts_widget = new QWidget;
    settings_shortcuts_widget->setLayout(vbox);
    settings_tabs->addTab(settings_shortcuts_widget, "Keyboard shortcuts");


    settings_dock = new DockWidget("Settings", this);
    settings_dock->setObjectName("settings window");
    settings_dock->setVisible(false);
    settings_dock->setFloating(true);
    settings_dock->setWidget(settings_tabs);
    addDockWidget(Qt::RightDockWidgetArea, settings_dock);
    tools_menu->addAction(settings_dock->toggleViewAction());
    connect(settings_dock, &DockWidget::visibilityChanged, settings_dock, &DockWidget::setEnabled);
}


void WobblyWindow::drawColorBars() {
    auto drawRect = [this] (int left, int top, int width, int height, int red, int green, int blue) {
        uint8_t *ptr = splash_image.bits();
        int stride = splash_image.bytesPerLine();
        ptr += stride * top;

        for (int x = left; x < left + width; x++) {
            ptr[x*4] = blue;
            ptr[x*4 + 1] = green;
            ptr[x*4 + 2] = red;
            ptr[x*4 + 3] = 255;
        }

        for (int y = 1; y < height; y++)
            memcpy(ptr + y * stride + left * 4, ptr + left * 4, width * 4);
    };

    auto drawGrayHorizontalGradient = [this] (int left, int top, int width, int height, int start, int end) {
        uint8_t *ptr = splash_image.bits();
        int stride = splash_image.bytesPerLine();
        ptr += stride * top;

        for (int x = left; x < left + width; x++) {
            float weight_end = (x - left) / (float)width;
            float weight_start = 1.0f - weight_end;

            int value = start * weight_start + end * weight_end;

            ptr[x*4] = value;
            ptr[x*4 + 1] = value;
            ptr[x*4 + 2] = value;
            ptr[x*4 + 3] = 255;
        }

        for (int y = 1; y < height; y++)
            memcpy(ptr + y * stride + left * 4, ptr + left * 4, width * 4);
    };

    drawRect(  0,   0,  90, 280, 104, 104, 104);
    drawRect( 90,   0,  77, 280, 180, 180, 180);
    drawRect(167,   0,  77, 280, 180, 180,  16);
    drawRect(244,   0,  77, 280,  16, 180, 180);
    drawRect(321,   0,  78, 280,  16, 180,  16);
    drawRect(399,   0,  77, 280, 180,  16, 180);
    drawRect(476,   0,  77, 280, 180,  16,  16);
    drawRect(553,   0,  77, 280,  16,  16, 180);
    drawRect(630,   0,  90, 280, 104, 104, 104);

    drawRect(  0, 280,  90,  40,  16, 235, 235);
    drawRect( 90, 280,  77,  40, 235, 235, 235);
    drawRect(167, 280, 463,  40, 180, 180, 180);
    drawRect(630, 280,  90,  40,  16,  16, 235);

    drawRect(  0, 320,  90,  40, 235, 235,  16);
    drawRect( 90, 320,  77,  40,  16,  16,  16);
    drawGrayHorizontalGradient(167, 320, 386, 40, 17, 234);
    drawRect(553, 320,  77,  40, 235, 235, 235);
    drawRect(630, 320,  90,  40, 235,  16,  16);

    drawRect(  0, 360,  90, 120,  49,  49,  49);
    drawRect( 90, 360, 116, 120,  16,  16,  16);
    drawRect(206, 360, 154, 120, 235, 235, 235);
    drawRect(360, 360,  64, 120,  16,  16,  16);
    drawRect(424, 360,  25, 120,  12,  12,  12);
    drawRect(449, 360,  27, 120,  16,  16,  16);
    drawRect(476, 360,  25, 120,  20,  20,  20);
    drawRect(501, 360,  27, 120,  16,  16,  16);
    drawRect(528, 360,  25, 120,  25,  25,  25);
    drawRect(553, 360,  77, 120,  16,  16,  16);
    drawRect(630, 360,  90, 120,  49,  49,  49);
}


void WobblyWindow::createUI() {
    setAcceptDrops(true);

    createMenu();
    createShortcuts();

    setWindowTitle(window_title);

    statusBar()->setSizeGripEnabled(true);

    selected_preset_label = new QLabel(QStringLiteral("Selected preset: "));
    selected_custom_list_label = new QLabel(QStringLiteral("Selected custom list: "));
    zoom_label = new QLabel(QStringLiteral("Zoom: 1x"));
    statusBar()->addPermanentWidget(selected_preset_label);
    statusBar()->addPermanentWidget(selected_custom_list_label);
    statusBar()->addPermanentWidget(zoom_label);

    drawColorBars();

    tab_bar = new QTabBar;
    tab_bar->addTab(QStringLiteral("Source"));
    tab_bar->addTab(QStringLiteral("Preview"));
    tab_bar->setExpanding(false);
    tab_bar->setEnabled(false);
    tab_bar->setFocusPolicy(Qt::NoFocus);

    frame_label = new FrameLabel;
    frame_label->setAlignment(Qt::AlignCenter);
    frame_label->setPixmap(QPixmap::fromImage(splash_image));

    for (int i = 0; i < NUM_THUMBNAILS; i++) {
        thumb_labels[i] = new QLabel;
        thumb_labels[i]->setAlignment(Qt::AlignCenter);
        thumb_labels[i]->setPixmap(QPixmap::fromImage(splash_image.scaled(splash_image.size() / 5, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
    }

    ScrollArea *frame_scroll = new ScrollArea;
    frame_scroll->resize(720, 480);
    frame_scroll->setFrameShape(QFrame::NoFrame);
    frame_scroll->setFocusPolicy(Qt::NoFocus);
    frame_scroll->setAlignment(Qt::AlignCenter);
    frame_scroll->setWidgetResizable(true);
    frame_scroll->setWidget(frame_label);

    overlay_label = new OverlayLabel;
    overlay_label->setAttribute(Qt::WA_TransparentForMouseEvents);

    frame_slider = new QSlider(Qt::Horizontal);
    frame_slider->setTracking(false);
    frame_slider->setFocusPolicy(Qt::NoFocus);


    connect(tab_bar, &QTabBar::currentChanged, this, &WobblyWindow::togglePreview);


    connect(frame_label, &FrameLabel::pixmapSizeChanged, overlay_label, &OverlayLabel::setFramePixmapSize);


    connect(frame_slider, &QSlider::valueChanged, [this] (int value) {
        if (!project)
            return;

        requestFrames(value);
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(tab_bar);

    vbox->addWidget(frame_scroll);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addStretch(1);
    for (int i = 0; i < NUM_THUMBNAILS; i++)
        hbox->addWidget(thumb_labels[i]);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    vbox->addWidget(frame_slider);

    QWidget *central_widget = new QWidget;
    central_widget->setLayout(vbox);

    setCentralWidget(central_widget);

    vbox = new QVBoxLayout;
    vbox->addWidget(overlay_label);
    frame_scroll->setLayout(vbox);


    presets_model = new QStringListModel(this);


    createFrameDetailsViewer();
    createCropAssistant();
    createPresetEditor();
    createPatternEditor();
    createSectionsEditor();
    createCustomListsEditor();
    createFrameRatesViewer();
    createFrozenFramesViewer();
    createPatternGuessingWindow();
    createMicSearchWindow();
    createCMatchSequencesWindow();
    createFadesWindow();
    createSettingsWindow();
}


void VS_CC messageHandler(int msgType, const char *msg, void *userData) {
    WobblyWindow *window = (WobblyWindow *)userData;

    Qt::ConnectionType type;
    if (QThread::currentThread() == window->thread())
        type = Qt::DirectConnection;
    else
        type = Qt::BlockingQueuedConnection;

    QMetaObject::invokeMethod(window, "vsLogPopup", type, Q_ARG(int, msgType), Q_ARG(QString, QString(msg)));
}


void WobblyWindow::vsLogPopup(int msgType, const QString &msg) {
    QString message;

    if (msgType == mtFatal) {
        if (project) {
            if (project_path.isEmpty())
                project_path = video_path + ".json";

            realSaveProject(project_path);

            message += "Your work has been saved to '" + project_path + "'. ";
        }
        writeSettings();

        message += "Wobbly will now close.\n\n";
    }

    message += "Message type: ";

    if (msgType == mtFatal) {
        message += "fatal";
    } else if (msgType == mtCritical) {
        message += "critical";
    } else if (msgType == mtWarning) {
        message += "warning";
    } else if (msgType == mtDebug) {
        message += "debug";
    } else {
        message += "unknown";
    }

    message += ". Message: ";
    message += msg;

    QMessageBox::information(this, QStringLiteral("vsLog"), message);
}


void WobblyWindow::initialiseVapourSynth() {
    if (!vsscript_init())
        throw WobblyException("Fatal error: failed to initialise VSScript. Your VapourSynth installation is probably broken.");
    
    
    vsapi = vsscript_getVSApi();
    if (!vsapi)
        throw WobblyException("Fatal error: failed to acquire VapourSynth API struct. Did you update the VapourSynth library but not the Python module (or the other way around)?");
    
    vsapi->setMessageHandler(messageHandler, (void *)this);

    if (vsscript_createScript(&vsscript))
        throw WobblyException(std::string("Fatal error: failed to create VSScript object. Error message: ") + vsscript_getError(vsscript));

    vscore = vsscript_getCore(vsscript);
    if (!vscore)
        throw WobblyException("Fatal error: failed to retrieve VapourSynth core object.");
}


void WobblyWindow::cleanUpVapourSynth() {
    frame_label->setPixmap(QPixmap());
    for (int i = 0; i < NUM_THUMBNAILS; i++) {
        thumb_labels[i]->setPixmap(QPixmap());
        vsapi->freeFrame(vsframes[i]);
        vsframes[i] = nullptr;
    }

    for (int i = 0; i < 2; i++) {
        vsapi->freeNode(vsnode[i]);
        vsnode[i] = nullptr;
    }

    vsscript_freeScript(vsscript);
    vsscript = nullptr;
}


void WobblyWindow::checkRequiredFilters() {
    struct Plugin {
        std::string id;
        std::vector<std::string> filters;
        std::string plugin_not_found;
        std::string filter_not_found;
    };

    std::vector<Plugin> plugins = {
        {
            "com.sources.d2vsource",
            { "Source" },
            "d2vsource plugin not found.",
            ""
        },
        {
            "systems.innocent.lsmas",
            { "LibavSMASHSource", "LWLibavSource" },
            "L-SMASH-Works plugin not found.",
            ""
        },
        {
            "com.nodame.fieldhint",
            { "FieldHint" },
            "FieldHint plugin not found.",
            "FieldHint plugin is too old."
        },
        {
            "com.vapoursynth.std",
            { "FreezeFrames", "DeleteFrames" },
            "VapourSynth standard filter library not found. This should never happen.",
            "VapourSynth version is older than r24."
        },
        {
            "com.vapoursynth.resize",
            { "Point", "Bilinear", "Bicubic", "Spline16", "Spline36", "Lanczos" },
            "built-in resizers not found. Did you compile VapourSynth yourself?",
            "VapourSynth version is older than r29."
        }
    };

    std::string error;

    for (size_t i = 0; i < plugins.size(); i++) {
        VSPlugin *plugin = vsapi->getPluginById(plugins[i].id.c_str(), vscore);
        if (!plugin) {
            error += "Fatal error: ";
            error += plugins[i].plugin_not_found;
            error += "\n";
        } else {
            VSMap *map = vsapi->getFunctions(plugin);
            for (auto it = plugins[i].filters.cbegin(); it != plugins[i].filters.cend(); it++) {
                if (vsapi->propGetType(map, it->c_str()) == ptUnset) {
                    error += "Fatal error: plugin '";
                    error += plugins[i].id;
                    error += "' found but it lacks filter '";
                    error += *it;
                    error += "'.";
                    if (plugins[i].filter_not_found.size()) {
                        error += " Likely reason: ";
                        error += plugins[i].filter_not_found;
                    }
                    error += "\n";
                }
            }
        }
    }

    if (error.size())
        throw WobblyException(error);
}


void WobblyWindow::updatePresets() {
    // Presets.
    const auto &presets = project->getPresets();
    QStringList preset_list;
    preset_list.reserve(presets.size());
    for (size_t i = 0; i < presets.size(); i++) {
        preset_list.append(QString::fromStdString(presets[i]));
    }
    presets_model->setStringList(preset_list);
}


void WobblyWindow::updateGeometry() {
    const std::string &state = project->getUIState();
    if (state.size())
        restoreState(QByteArray::fromBase64(QByteArray(state.c_str(), state.size())));

    const std::string &geometry = project->getUIGeometry();
    if (geometry.size())
        restoreGeometry(QByteArray::fromBase64(QByteArray(geometry.c_str(), geometry.size())));
}

void WobblyWindow::updateWindowTitle() {
    setWindowTitle(QStringLiteral("%1 - %2").arg(window_title).arg(project_path.isEmpty() ? video_path : project_path));
}


void WobblyWindow::initialiseCropAssistant() {
    // Crop.
    for (int i = 0; i < 4; i++)
        crop_spin[i]->blockSignals(true);

    const Crop &crop = project->getCrop();
    crop_spin[0]->setValue(crop.left);
    crop_spin[1]->setValue(crop.top);
    crop_spin[2]->setValue(crop.right);
    crop_spin[3]->setValue(crop.bottom);

    for (int i = 0; i < 4; i++)
        crop_spin[i]->blockSignals(false);

    crop_box->setChecked(project->isCropEnabled());
    crop_early_check->setChecked(project->isCropEarly());


    // Resize.
    for (int i = 0; i < 2; i++)
        resize_spin[i]->blockSignals(true);

    const Resize &resize = project->getResize();
    resize_spin[0]->setValue(resize.width);
    resize_spin[1]->setValue(resize.height);

    for (int i = 0; i < 2; i++)
        resize_spin[i]->blockSignals(false);

    resize_box->blockSignals(true);
    resize_box->setChecked(project->isResizeEnabled());
    resize_box->blockSignals(false);

    QString filter = QString::fromStdString(resize.filter);
    filter[0] = filter[0].toUpper();
    resize_filter_combo->setCurrentText(filter);


    // Bit depth.
    std::unordered_map<int, int> bits_to_index[2] = {
        { { 8, 0 }, { 9, 1 }, {10, 2 }, { 12, 3 }, { 16, 4 } },
        { { 16, 5 }, { 32, 6 } }
    };
    std::unordered_map<std::string, int> dither_to_index = {
        { "none", 0 },
        { "ordered", 1 },
        { "random", 2 },
        { "error_diffusion", 3 }
    };
    const Depth &depth = project->getBitDepth();
    depth_box->blockSignals(true);
    depth_box->setChecked(depth.enabled);
    depth_box->blockSignals(false);
    depth_bits_combo->setCurrentIndex(bits_to_index[(int)depth.float_samples][depth.bits]);
    depth_dither_combo->setCurrentIndex(dither_to_index[depth.dither]);
}


void WobblyWindow::initialisePresetEditor() {
    if (preset_combo->count()) {
        preset_combo->setCurrentIndex(0);
        presetChanged(preset_combo->currentText());
    }
}


void WobblyWindow::updateSectionsEditor() {
    auto selection = sections_table->selectedRanges();

    int row_count_before = sections_table->rowCount();

    int current_row = sections_table->currentRow();

    sections_table->setRowCount(0);
    int rows = 0;
    const Section *section = project->findSection(0);
    while (section) {
        if (!short_sections_box->isChecked() || project->getSectionEnd(section->start) - section->start <= short_sections_spin->value()) {
            QTableWidgetItem *item;

            rows++;
            sections_table->setRowCount(rows);

            item = new QTableWidgetItem(QString::number(section->start));
            sections_table->setItem(rows - 1, 0, item);

            if (section->presets.size()) {
                QString presets = QString::fromStdString(section->presets[0]);
                for (size_t i = 1; i < section->presets.size(); i++)
                    presets += "," + QString::fromStdString(section->presets[i]);
                item = new QTableWidgetItem(presets);
                sections_table->setItem(rows - 1, 1, item);
            }
        }

        section = project->findNextSection(section->start);
    }

    sections_table->resizeColumnsToContents();

    int row_count_after = sections_table->rowCount();

    if (row_count_before == row_count_after) {
        if (current_row > -1)
            sections_table->setCurrentCell(current_row, 0);

        for (int i = 0; i < selection.size(); i++)
            sections_table->setRangeSelected(selection[i], true);
    } else if (row_count_after)
        sections_table->selectRow(0);
}


void WobblyWindow::updateCustomListsEditor() {
    auto selection = cl_table->selectedRanges();

    int row_count_before = cl_table->rowCount();

    int current_row = cl_table->currentRow();

    cl_table->setRowCount(0);

    cl_copy_range_menu->clear();
    cl_send_range_menu->clear();

    auto cl = project->getCustomLists();

    cl_table->setRowCount(cl.size());

    for (size_t i = 0; i < cl.size(); i++) {
        QString cl_name = QString::fromStdString(cl[i].name);

        QTableWidgetItem *item = new QTableWidgetItem(cl_name);
        cl_table->setItem(i, 0, item);

        item = new QTableWidgetItem(QString::fromStdString(cl[i].preset));
        cl_table->setItem(i, 1, item);

        const char *positions[] = {
            "Post source",
            "Post field match",
            "Post decimate"
        };
        item = new QTableWidgetItem(positions[cl[i].position]);
        cl_table->setItem(i, 2, item);

        QAction *copy_action = cl_copy_range_menu->addAction(cl_name);
        QAction *send_action = cl_send_range_menu->addAction(cl_name);
        copy_action->setData((int)i);
        send_action->setData((int)i);
    }

    cl_table->resizeColumnsToContents();

    int row_count_after = cl_table->rowCount();

    if (row_count_before == row_count_after) {
        if (current_row > -1)
            cl_table->setCurrentCell(current_row, 0);

        for (int i = 0; i < selection.size(); i++)
            cl_table->setRangeSelected(selection[i], true);
    } else if (row_count_after)
        cl_table->selectRow(0);
}


void WobblyWindow::updateFrameRatesViewer() {
    auto selection = frame_rates_table->selectedRanges();

    int row_count_before = frame_rates_table->rowCount();

    frame_rates_table->setRowCount(0);

    auto ranges = project->getDecimationRanges();

    int rows = 0;
    for (size_t i = 0; i < ranges.size(); i++) {
        if (frame_rates_buttons->button(ranges[i].num_dropped)->isChecked()) {
            rows++;
            frame_rates_table->setRowCount(rows);

            QTableWidgetItem *item = new QTableWidgetItem(QString::number(ranges[i].start));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            frame_rates_table->setItem(rows - 1, 0, item);

            int end;
            if (i < ranges.size() - 1)
                end = ranges[i + 1].start - 1;
            else
                end = project->getNumFrames(PostSource) - 1;

            item = new QTableWidgetItem(QString::number(end));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            frame_rates_table->setItem(rows - 1, 1, item);

            const char *rates[] = {
                "30000/1001",
                "24000/1001",
                "18000/1001",
                "12000/1001",
                "6000/1001"
            };
            item = new QTableWidgetItem(rates[ranges[i].num_dropped]);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            frame_rates_table->setItem(rows - 1, 2, item);
        }
    }

    frame_rates_table->resizeColumnsToContents();

    int row_count_after = frame_rates_table->rowCount();

    if (row_count_before == row_count_after) {
        for (int i = 0; i < selection.size(); i++)
            frame_rates_table->setRangeSelected(selection[i], true);
    } else if (row_count_after)
        frame_rates_table->selectRow(0);
}


void WobblyWindow::initialiseFrameRatesViewer() {
    auto rates = project->getShownFrameRates();

    for (int i = 0; i < 5; i++)
        frame_rates_buttons->button(i)->setChecked(rates[i]);

    updateFrameRatesViewer();
}


void WobblyWindow::updateFrozenFramesViewer() {
    frozen_frames_table->setRowCount(0);

    const auto &ff = project->getFreezeFrames();

    frozen_frames_table->setRowCount(ff.size());

    for (size_t i = 0; i < ff.size(); i++) {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(ff[i].first));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        frozen_frames_table->setItem(i, 0, item);

        item = new QTableWidgetItem(QString::number(ff[i].last));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        frozen_frames_table->setItem(i, 1, item);

        item = new QTableWidgetItem(QString::number(ff[i].replacement));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        frozen_frames_table->setItem(i, 2, item);
    }

    frozen_frames_table->resizeColumnsToContents();

    if (ff.size())
        frozen_frames_table->selectRow(0);
}


void WobblyWindow::updatePatternGuessingWindow() {
    pg_failures_table->setRowCount(0);

    const char *reasons[] = {
        "Section too short",
        "Ambiguous pattern"
    };

    auto pg = project->getPatternGuessing();

    pg_failures_table->setRowCount(pg.failures.size());

    int rows = 0;

    for (auto it = pg.failures.cbegin(); it != pg.failures.cend(); it++) {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(it->second.start));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pg_failures_table->setItem(rows, 0, item);

        item = new QTableWidgetItem(reasons[it->second.reason]);
        pg_failures_table->setItem(rows, 1, item);

        rows++;
    }

    pg_failures_table->resizeColumnsToContents();
}


void WobblyWindow::initialisePatternGuessingWindow() {
    auto pg = project->getPatternGuessing();

    if (pg.failures.size()) {
        pg_methods_buttons->button(pg.method)->setChecked(true);

        pg_length_spin->setValue(pg.minimum_length);

        pg_n_match_buttons->button(pg.third_n_match)->setChecked(true);

        pg_decimate_buttons->button(pg.decimation)->setChecked(true);

        auto buttons = pg_use_patterns_buttons->buttons();
        for (int i = 0; i < buttons.size(); i++)
            buttons[i]->setChecked(pg.use_patterns & pg_use_patterns_buttons->id(buttons[i]));
    }

    updatePatternGuessingWindow();
}


void WobblyWindow::initialiseMicSearchWindow() {
    mic_search_minimum_spin->blockSignals(true);
    mic_search_minimum_spin->setValue(project->getMicSearchMinimum());
    mic_search_minimum_spin->blockSignals(false);
}


void WobblyWindow::updateCMatchSequencesWindow() {
    const auto &sequences = project->getCMatchSequences(c_match_minimum_spin->value());

    c_match_sequences_table->setRowCount(0);
    c_match_sequences_table->setRowCount(sequences.size());

    int row = 0;
    for (auto it = sequences.cbegin(); it != sequences.cend(); it++) {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(it->first));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        c_match_sequences_table->setItem(row, 0, item);

        item = new QTableWidgetItem(QString::number(it->second));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        c_match_sequences_table->setItem(row, 1, item);

        row++;
    }

    if (sequences.size())
        c_match_sequences_table->selectRow(0);

    c_match_sequences_table->resizeColumnsToContents();
}


void WobblyWindow::initialiseCMatchSequencesWindow() {
    c_match_minimum_spin->blockSignals(true);
    c_match_minimum_spin->setValue(project->getCMatchSequencesMinimum());
    c_match_minimum_spin->blockSignals(false);

    updateCMatchSequencesWindow();
}


void WobblyWindow::updateFadesWindow() {
    auto fades = project->getInterlacedFades();

    int ignore_gaps = fades_gaps_spin->value();

    std::vector<FrameRange> fades_ranges;

    auto it = fades.cbegin();
    if (it == fades.cend()) {
        fades_table->setRowCount(0);
        return;
    }

    int start = it->first;
    int end = start;

    it++;
    for ( ; it != fades.cend(); it++) {
        if (it->first - end - 1 > ignore_gaps) {
            fades_ranges.push_back({ start, end });
            start = it->first;
            end = start;
        } else {
            end = it->first;
        }
        if (it == (fades.cend()--))
            fades_ranges.push_back({ start, end });
    }

    fades_table->setRowCount(fades_ranges.size());

    for (auto range = fades_ranges.cbegin(); range != fades_ranges.cend(); range++) {
        int row = std::distance(fades_ranges.cbegin(), range);

        QTableWidgetItem *item = new QTableWidgetItem(QString::number(range->first));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fades_table->setItem(row, 0, item);

        item = new QTableWidgetItem(QString::number(range->last));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fades_table->setItem(row, 1, item);
    }
}


void WobblyWindow::initialiseUIFromProject() {
    updateWindowTitle();

    tab_bar->setEnabled(true);

    frame_slider->setRange(0, project->getNumFrames(PostSource) - 1);
    frame_slider->setPageStep(project->getNumFrames(PostSource) * 20 / 100);

    // Zoom.
    zoom_label->setText(QStringLiteral("Zoom: %1x").arg(project->getZoom()));

    updateGeometry();

    updatePresets();

    initialiseCropAssistant();
    initialisePresetEditor();
    updateSectionsEditor();
    updateCustomListsEditor();
    initialiseFrameRatesViewer();
    updateFrozenFramesViewer();
    initialisePatternGuessingWindow();
    initialiseMicSearchWindow();
    initialiseCMatchSequencesWindow();
    updateFadesWindow();
}


void WobblyWindow::realOpenProject(const QString &path) {
    WobblyProject *tmp = new WobblyProject(true);

    try {
        tmp->readProject(path.toStdString());

        project_path = path;
        video_path.clear();

        if (project)
            delete project;
        project = tmp;

        addRecentFile(path);

        current_frame = project->getLastVisitedFrame();

        initialiseUIFromProject();

        vsscript_clearOutput(vsscript, 1);

        evaluateMainDisplayScript();
    } catch (WobblyException &e) {
        errorPopup(e.what());

        if (project == tmp)
            project = nullptr;
        delete tmp;
    }
}


void WobblyWindow::openProject() {
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Wobbly project"), settings.value("user_interface/last_dir").toString(), QStringLiteral("Wobbly projects (*.json);;All files (*)"), nullptr, QFileDialog::DontUseNativeDialog);

    if (!path.isNull()) {
        settings.setValue("user_interface/last_dir", QFileInfo(path).absolutePath());

        realOpenProject(path);
    }
}


void WobblyWindow::realOpenVideo(const QString &path) {
    try {
        QString source_filter;

        QString extension = path.mid(path.lastIndexOf('.') + 1);

        QStringList mp4 = { "mp4", "m4v", "mov" };

        if (extension == "d2v")
            source_filter = "d2v.Source";
        else if (mp4.contains(extension))
            source_filter = "lsmas.LibavSMASHSource";
        else
            source_filter = "lsmas.LWLibavSource";

        QString script = QStringLiteral(
                    "import vapoursynth as vs\n"
                    "\n"
                    "c = vs.get_core()\n"
                    "\n"
                    "c.%1(r'%2').set_output()\n");
        script = script.arg(source_filter).arg(path);

        if (vsscript_evaluateScript(&vsscript, script.toUtf8().constData(), QFileInfo(path).dir().path().toUtf8().constData(), efSetWorkingDir)) {
            std::string error = vsscript_getError(vsscript);
            // The traceback is mostly unnecessary noise.
            size_t traceback = error.find("Traceback");
            if (traceback != std::string::npos)
                error.insert(traceback, 1, '\n');

            throw WobblyException("Can't extract basic information from the video file: script evaluation failed. Error message:\n" + error);
        }

        VSNodeRef *node = vsscript_getOutput(vsscript, 0);
        if (!node)
            throw WobblyException("Can't extract basic information from the video file: script evaluated successfully, but no node found at output index 0.");

        VSVideoInfo vi = *vsapi->getVideoInfo(node);

        vsapi->freeNode(node);

        if (project)
            delete project;

        project = new WobblyProject(true, path.toStdString(), source_filter.toStdString(), vi.fpsNum, vi.fpsDen, vi.width, vi.height, vi.numFrames);
        project->addTrim(0, vi.numFrames - 1);

        video_path = path;
        project_path.clear();

        initialiseUIFromProject();

        vsscript_clearOutput(vsscript, 1);

        evaluateMainDisplayScript();

        addRecentFile(path);
    } catch(WobblyException &e) {
        errorPopup(e.what());

        project = nullptr;
    }
}


void WobblyWindow::openVideo() {
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open video file"), settings.value("user_interface/last_dir").toString(), QString(), nullptr, QFileDialog::DontUseNativeDialog);

    if (!path.isNull()) {
        settings.setValue("user_interface/last_dir", QFileInfo(path).absolutePath());

        realOpenVideo(path);
    }
}


void WobblyWindow::realSaveProject(const QString &path) {
    if (!project)
        return;

    // The currently selected preset might not have been stored in the project yet.
    presetEdited();

    project->setLastVisitedFrame(current_frame);

    const QByteArray &state = saveState().toBase64();
    const QByteArray &geometry = saveGeometry().toBase64();
    project->setUIState(std::string(state.constData(), state.size()));
    project->setUIGeometry(std::string(geometry.constData(), geometry.size()));

    project->writeProject(path.toStdString(), settings_compact_projects_check->isChecked());

    project_path = path;
    video_path.clear();

    updateWindowTitle();

    addRecentFile(path);
}


void WobblyWindow::saveProject() {
    try {
        if (!project)
            throw WobblyException("Can't save the project because none has been loaded.");

        if (project_path.isEmpty())
            saveProjectAs();
        else
            realSaveProject(project_path);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::saveProjectAs() {
    try {
        if (!project)
            throw WobblyException("Can't save the project because none has been loaded.");

        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Wobbly project"), settings.value("user_interface/last_dir").toString(), QStringLiteral("Wobbly projects (*.json);;All files (*)"), nullptr, QFileDialog::DontUseNativeDialog);

        if (!path.isNull()) {
            settings.setValue("user_interface/last_dir", QFileInfo(path).absolutePath());

            realSaveProject(path);
        }
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::realSaveScript(const QString &path) {
    // The currently selected preset might not have been stored in the project yet.
    presetEdited();

    std::string script = project->generateFinalScript();

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly))
        throw WobblyException("Couldn't open script '" + path.toStdString() + "'. Error message: " + file.errorString().toStdString());

    file.write(script.c_str(), script.size());
}


void WobblyWindow::saveScript() {
    try {
        if (!project)
            throw WobblyException("Can't save the script because no project has been loaded.");

        QString path;
        if (project_path.isEmpty())
            path = video_path;
        else
            path = project_path;
        path += ".py";

        realSaveScript(path);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::saveScriptAs() {
    try {
        if (!project)
            throw WobblyException("Can't save the script because no project has been loaded.");

        QString dir;
        if (project_path.isEmpty())
            dir = video_path;
        else
            dir = project_path;
        dir += ".py";

        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save script"), dir, QStringLiteral("VapourSynth scripts (*.py *.vpy);;All files (*)"), nullptr, QFileDialog::DontUseNativeDialog);

        if (!path.isNull()) {
            settings.setValue("user_interface/last_dir", QFileInfo(path).absolutePath());

            realSaveScript(path);
        }
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::realSaveTimecodes(const QString &path) {
    std::string tc = project->generateTimecodesV1();

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly))
        throw WobblyException("Couldn't open timecodes file '" + path.toStdString() + "'. Error message: " + file.errorString().toStdString());

    file.write(tc.c_str(), tc.size());
}


void WobblyWindow::saveTimecodes() {
    try {
        if (!project)
            throw WobblyException("Can't save the timecodes because no project has been loaded.");

        QString path;
        if (project_path.isEmpty())
            path = video_path;
        else
            path = project_path;
        path += ".vfr.txt";

        realSaveTimecodes(path);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::saveTimecodesAs() {
    try {
        if (!project)
            throw WobblyException("Can't save the timecodes because no project has been loaded.");

        QString dir;
        if (project_path.isEmpty())
            dir = video_path;
        else
            dir = project_path;
        dir += ".vfr.txt";

        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save timecodes"), dir, QStringLiteral("Timecodes v1 files (*.txt);;All files (*)"), nullptr, QFileDialog::DontUseNativeDialog);

        if (!path.isNull()) {
            settings.setValue("user_interface/last_dir", QFileInfo(path).absolutePath());

            realSaveTimecodes(path);
        }
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::saveScreenshot() {
    QString path;
    if (project_path.isEmpty() && video_path.isEmpty())
        path = "wobbly";
    else if (project_path.isEmpty())
        path = video_path;
    else
        path = project_path;
    path += ".png";

    path = QFileDialog::getSaveFileName(this, QStringLiteral("Save screenshot"), path, QStringLiteral("PNG images (*.png);;All files (*)"), nullptr, QFileDialog::DontUseNativeDialog);

    if (!path.isNull()) {
        settings.setValue("user_interface/last_dir", QFileInfo(path).absolutePath());

        frame_label->pixmap()->save(path, "png");
    }
}


QString getPreviousInSeries(const QString &path) {
    QFileInfo info(path);
    QDir dir = info.dir();
    QStringList files = dir.entryList(QDir::Files, QDir::Name);

    QString this_project = info.fileName();

    for (int i = 0; i < files.size(); ) {
        if (files[i].size() != this_project.size())
            files.removeAt(i);
        else
            i++;
    }

    for (int i = 0; i < files.size(); ) {
        if (files[i] == this_project) {
            i++;
            continue;
        }

        bool belongs = true;

        for (int j = 0; j < this_project.size(); j++) {
            if (!((this_project[j].isDigit() && files[i][j].isDigit()) || (this_project[j] == files[i][j]))) {
                belongs = false;
                break;
            }
        }

        if (!belongs)
            files.removeAt(i);
        else
            i++;
    }

    QString previous_name;

    int index = files.indexOf(this_project);
    if (index > 0)
        previous_name = dir.absoluteFilePath(files[index - 1]);

    return previous_name;
}


void WobblyWindow::importFromProject() {
    if (!import_window) {
        QString previous_name;

        if (!project_path.isEmpty())
            previous_name = getPreviousInSeries(project_path);

        ImportedThings import_things;
        import_things.geometry = true;
        import_things.zoom = true;
        import_things.presets = true;
        import_things.custom_lists = true;
        import_things.crop = true;
        import_things.resize = true;
        import_things.bit_depth = true;
        import_things.mic_search = true;

        import_window = new ImportWindow(previous_name, import_things, this);

        connect(import_window, &ImportWindow::import, [this] (const QString &file_name, const ImportedThings &imports) {
            if (!project)
                return;

            try {
                project->importFromOtherProject(file_name.toStdString(), imports);

                initialiseUIFromProject();

                requestFrames(current_frame);

                import_window->hide();
            } catch (WobblyException &e) {
                errorPopup(e.what());
            }
        });

        connect(import_window, &ImportWindow::previousWanted, [this] () {
            QString prev;

            if (!project_path.isEmpty())
                prev = getPreviousInSeries(project_path);

            import_window->setFileName(prev);
        });
    }

    import_window->show();
    import_window->raise();
    import_window->activateWindow();
}


void WobblyWindow::quit() {
    close();
}


void WobblyWindow::showHideFrameDetails() {
    details_dock->setVisible(!details_dock->isVisible());
}


void WobblyWindow::showHideCropping() {
    crop_dock->setVisible(!crop_dock->isVisible());
}


void WobblyWindow::showHidePresets() {
    preset_dock->setVisible(!preset_dock->isVisible());
}


void WobblyWindow::showHidePatternEditor() {
    pattern_dock->setVisible(!pattern_dock->isVisible());
}


void WobblyWindow::showHideSections() {
    sections_dock->setVisible(!sections_dock->isVisible());
}


void WobblyWindow::showHideCustomLists() {
    cl_dock->setVisible(!cl_dock->isVisible());
}


void WobblyWindow::showHideFrameRates() {
    frame_rates_dock->setVisible(!frame_rates_dock->isVisible());
}


void WobblyWindow::showHideFrozenFrames() {
    frozen_frames_dock->setVisible(!frozen_frames_dock->isVisible());
}


void WobblyWindow::showHidePatternGuessing() {
    pg_dock->setVisible(!pg_dock->isVisible());
}


void WobblyWindow::showHideMicSearchWindow() {
    mic_search_dock->setVisible(!mic_search_dock->isVisible());
}


void WobblyWindow::showHideCMatchSequencesWindow() {
    c_match_sequences_dock->setVisible(!c_match_sequences_dock->isVisible());
}


void WobblyWindow::showHideFadesWindow() {
    fades_dock->setVisible(!fades_dock->isVisible());
}


void WobblyWindow::showHideFrameDetailsOnVideo() {
    settings_print_details_check->setChecked(!settings_print_details_check->isChecked());
}


void WobblyWindow::evaluateScript(bool final_script) {
    std::string script;

    if (final_script)
        script = project->generateFinalScript();
    else
        script = project->generateMainDisplayScript();

    QString m = settings_colormatrix_combo->currentText();
    std::string matrix = "709";
    std::string transfer = "709";
    std::string primaries = "709";

    if (m == "BT 601") {
        matrix = "470bg";
        transfer = "601";
        primaries = "170m";
    } else if (m == "BT 709") {
        matrix = "709";
        transfer = "709";
        primaries = "709";
    } else if (m == "BT 2020 NCL") {
        matrix = "2020ncl";
        transfer = "709";
        primaries = "2020";
    } else if (m == "BT 2020 CL") {
        matrix = "2020cl";
        transfer = "709";
        primaries = "2020";
    }

    script +=
            "src = vs.get_output(index=0)\n"

            // Since VapourSynth R41 get_output returns the alpha as well.
            "if isinstance(src, tuple):\n"
            "    src = src[0]\n"

            "if src.format is None:\n"
            "    raise vs.Error('The output clip has unknown format. Wobbly cannot display such clips.')\n"

            // Workaround for bug in the resizers in VapourSynth R29 and R30.
            // Remove at some point after R31.
            "src = c.std.SetFrameProp(clip=src, prop='_FieldBased', delete=True)\n";

    if (crop_dock->isVisible() && project->isCropEnabled()) {
        script += "src = c.std.CropRel(clip=src, left=";
        script += std::to_string(crop_spin[0]->value()) + ", top=";
        script += std::to_string(crop_spin[1]->value()) + ", right=";
        script += std::to_string(crop_spin[2]->value()) + ", bottom=";
        script += std::to_string(crop_spin[3]->value()) + ")\n";

        script +=
                "src = c.resize.Bicubic(clip=src, format=vs.RGB24, dither_type='random', matrix_in_s='" + matrix + "', transfer_in_s='" + transfer + "', primaries_in_s='" + primaries + "')\n";

        script += "src = c.std.AddBorders(clip=src, left=";
        script += std::to_string(crop_spin[0]->value()) + ", top=";
        script += std::to_string(crop_spin[1]->value()) + ", right=";
        script += std::to_string(crop_spin[2]->value()) + ", bottom=";
        script += std::to_string(crop_spin[3]->value()) + ", color=[224, 81, 255])\n";

        script += "src = c.resize.Bicubic(clip=src, format=vs.COMPATBGR32)\n";
    } else {
        script +=
            "src = c.resize.Bicubic(clip=src, format=vs.COMPATBGR32, dither_type='random', matrix_in_s='" + matrix + "', transfer_in_s='" + transfer + "', primaries_in_s='" + primaries + "')\n";
    }

    script +=
            "src.set_output()\n";

    script +=
            "c.max_cache_size = " + std::to_string(settings_cache_spin->value()) + "\n";

    if (vsscript_evaluateScript(&vsscript, script.c_str(), QFileInfo(project_path).dir().path().toUtf8().constData(), efSetWorkingDir)) {
        std::string error = vsscript_getError(vsscript);
        // The traceback is mostly unnecessary noise.
        size_t traceback = error.find("Traceback");
        if (traceback != std::string::npos)
            error.insert(traceback, 1, '\n');

        throw WobblyException("Failed to evaluate " + std::string(final_script ? "final" : "main display") + " script. Error message:\n" + error);
    }

    int node_index = (int)final_script;

    vsapi->freeNode(vsnode[node_index]);

    vsnode[node_index] = vsscript_getOutput(vsscript, 0);
    if (!vsnode[node_index])
        throw WobblyException(std::string(final_script ? "Final" : "Main display") + " script evaluated successfully, but no node found at output index 0.");

    requestFrames(current_frame);
}


void WobblyWindow::evaluateMainDisplayScript() {
    evaluateScript(false);
}


void WobblyWindow::evaluateFinalScript() {
    evaluateScript(true);
}


void VS_CC frameDoneCallback(void *userData, const VSFrameRef *f, int n, VSNodeRef *node, const char *errorMsg) {
    WobblyWindow *window = (WobblyWindow *)userData;

    // Qt::DirectConnection = frameDone runs in the worker threads
    // Qt::QueuedConnection = frameDone runs in the GUI thread
    QMetaObject::invokeMethod(window, "frameDone", Qt::QueuedConnection,
                              Q_ARG(void *, (void *)f),
                              Q_ARG(int, n),
                              Q_ARG(void *, (void *)node),
                              Q_ARG(QString, QString(errorMsg)));
    // Pass a copy of the error message because the pointer won't be valid after this function returns.
}


void WobblyWindow::requestFrames(int n) {
    if (!vsnode[(int)preview])
        return;

    n = std::max(0, std::min(n, project->getNumFrames(PostSource) - 1));

    current_frame = n;

    frame_slider->blockSignals(true);
    frame_slider->setValue(n);
    frame_slider->blockSignals(false);

    updateFrameDetails();

    if (pending_requests)
        return;

    pending_frame = n;

    int frame_num = n;
    int last_frame = project->getNumFrames(PostSource) - 1;
    if (preview) {
        frame_num = project->frameNumberAfterDecimation(n);
        last_frame = project->getNumFrames(PostDecimate) - 1;
    }

    // XXX Make it generic.
    if (frame_num == 0)
        thumb_labels[0]->setPixmap(QPixmap::fromImage(splash_image.scaled(splash_image.size() / 5, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
    if (frame_num == last_frame)
        thumb_labels[2]->setPixmap(QPixmap::fromImage(splash_image.scaled(splash_image.size() / 5, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));

    for (int i = std::max(0, frame_num - NUM_THUMBNAILS / 2); i < std::min(frame_num + NUM_THUMBNAILS / 2 + 1, last_frame + 1); i++) {
        pending_requests++;
        vsapi->getFrameAsync(i, vsnode[(int)preview], frameDoneCallback, (void *)this);
    }
}


// Runs in the GUI thread.
void WobblyWindow::frameDone(void *framev, int n, void *nodev, const QString &errorMsg) {
    const VSFrameRef *frame = (const VSFrameRef *)framev;
    VSNodeRef *node = (VSNodeRef *)nodev;

    pending_requests--;

    if (!frame) {
        errorPopup(QStringLiteral("Failed to retrieve frame %1. Error message: %2").arg(n).arg(errorMsg).toUtf8().constData());
        return;
    }

    const uint8_t *ptr = vsapi->getReadPtr(frame, 0);
    int width = vsapi->getFrameWidth(frame, 0);
    int height = vsapi->getFrameHeight(frame, 0);
    int stride = vsapi->getStride(frame, 0);
    QImage image(ptr, width, height, stride, QImage::Format_RGB32);
    QPixmap pixmap = QPixmap::fromImage(image.mirrored(false, true));

    int offset;
    if (node == vsnode[0])
        offset = n - pending_frame;
    else
        offset = n - project->frameNumberAfterDecimation(pending_frame);

    if (offset == 0) {
        int zoom = project->getZoom();
        frame_label->setPixmap(pixmap.scaled(width * zoom, height * zoom, Qt::IgnoreAspectRatio, Qt::FastTransformation));
    }

    pixmap = pixmap.scaled(pixmap.size() / 5, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    thumb_labels[offset + NUM_THUMBNAILS / 2]->setPixmap(pixmap);
    // Must free the frame only after replacing the pixmap.
    vsapi->freeFrame(vsframes[offset + NUM_THUMBNAILS / 2]);
    vsframes[offset + NUM_THUMBNAILS / 2] = frame;

    if (!pending_requests && pending_frame != current_frame)
        requestFrames(current_frame);
}


void WobblyWindow::updateFrameDetails() {
    if (!project)
        return;

    QString frame("Frame: ");

    if (!preview)
        frame += "<b>";

    frame += QString::number(current_frame);

    if (!preview)
        frame += "</b>";

    frame += " | ";

    if (preview)
        frame += "<b>";

    frame += QString::number(project->frameNumberAfterDecimation(current_frame));

    if (preview)
        frame += "</b>";

    frame_num_label->setText(frame);


    time_label->setText(QString::fromStdString("Time: " + project->frameToTime(current_frame)));


    int matches_start = std::max(0, current_frame - 10);
    int matches_end = std::min(current_frame + 10, project->getNumFrames(PostSource) - 1);

    QString matches("Matches: ");
    for (int i = matches_start; i <= matches_end; i++) {
        char match = project->getMatch(i);

        bool is_decimated = project->isDecimatedFrame(i);

        if (i % 5 == 0)
            matches += "<u>";

        if (is_decimated)
            matches += "<s>";

        if (i == current_frame)
            match += 'C' - 'c';
        matches += match;

        if (is_decimated)
            matches += "</s>";

        if (i % 5 == 0)
            matches += "</u>";
    }
    matches_label->setText(matches);


    if (project->isCombedFrame(current_frame))
        combed_label->setText(QStringLiteral("Combed"));
    else
        combed_label->clear();


    decimate_metric_label->setText(QStringLiteral("DMetric: ") + QString::number(project->getDecimateMetric(current_frame)));


    int match_index = matchCharToIndex(project->getMatch(current_frame));
    QString mics("Mics: ");
    for (int i = 0; i < 5; i++) {
        if (i == match_index)
            mics += "<b>";

        mics += QStringLiteral("%1 ").arg((int)project->getMics(current_frame)[i]);

        if (i == match_index)
            mics += "</b>";
    }
    mic_label->setText(mics);


    const Section *current_section = project->findSection(current_frame);
    int section_start = current_section->start;
    int section_end = project->getSectionEnd(section_start) - 1;

    QString presets;
    for (auto it = current_section->presets.cbegin(); it != current_section->presets.cend(); it++) {
        if (!presets.isEmpty())
            presets += "\n";
        presets += QString::fromStdString(*it);
    }

    if (presets.isNull())
        presets = "&lt;none&gt;";

    section_label->setText(QStringLiteral("Section: [%1,%2]<br />Presets:<br />%3").arg(section_start).arg(section_end).arg(presets));


    QString custom_lists;
    const std::vector<CustomList> &lists = project->getCustomLists();
    for (size_t i =  0; i < lists.size(); i++) {
        const FrameRange *range = project->findCustomListRange(i, current_frame);
        if (range) {
            if (!custom_lists.isEmpty())
                custom_lists += "\n";
            custom_lists += QStringLiteral("%1: [%2,%3]").arg(QString::fromStdString(lists[i].name)).arg(range->first).arg(range->last);
        }
    }

    if (custom_lists.isNull())
        custom_lists = "&lt;none&gt;";

    custom_list_label->setText(QStringLiteral("Custom lists:<br />%1").arg(custom_lists));


    const FreezeFrame *freeze = project->findFreezeFrame(current_frame);
    if (freeze)
        freeze_label->setText(QStringLiteral("Frozen: [%1,%2,%3]").arg(freeze->first).arg(freeze->last).arg(freeze->replacement));
    else
        freeze_label->clear();


    if (settings_print_details_check->isChecked()) {
        QString drawn_text = frame_num_label->text() + "<br />";
        drawn_text += time_label->text() + "<br />";
        drawn_text += matches_label->text() + "<br />";
        drawn_text += section_label->text() + "<br />";
        drawn_text += custom_list_label->text() + "<br />";
        drawn_text += freeze_label->text() + "<br />";
        drawn_text += decimate_metric_label->text() + "<br />";
        drawn_text += mic_label->text() + "<br />";
        drawn_text += combed_label->text();

        overlay_label->setText(drawn_text);
    } else {
        overlay_label->clear();
    }
}


void WobblyWindow::jumpRelative(int offset) {
    if (!project)
        return;

    int target = current_frame + offset;

    if (target < 0)
        target = 0;
    if (target >= project->getNumFrames(PostSource))
        target = project->getNumFrames(PostSource) - 1;

    if (preview) {
        int skip = offset < 0 ? -1 : 1;

        while (true) {
            if (target == 0 || target == project->getNumFrames(PostSource) - 1)
                skip = -skip;

            if (!project->isDecimatedFrame(target))
                break;

            target += skip;
        }
    }

    requestFrames(target);
}


void WobblyWindow::jump1Backward() {
    jumpRelative(-1);
}


void WobblyWindow::jump1Forward() {
    jumpRelative(1);
}


void WobblyWindow::jump5Backward() {
    jumpRelative(-5);
}


void WobblyWindow::jump5Forward() {
    jumpRelative(5);
}


void WobblyWindow::jump50Backward() {
    jumpRelative(-50);
}


void WobblyWindow::jump50Forward() {
    jumpRelative(50);
}


void WobblyWindow::jumpALotBackward() {
    if (!project)
        return;

    int twenty_percent = project->getNumFrames(PostSource) * 20 / 100;

    jumpRelative(-twenty_percent);
}


void WobblyWindow::jumpALotForward() {
    if (!project)
        return;

    int twenty_percent = project->getNumFrames(PostSource) * 20 / 100;

    jumpRelative(twenty_percent);
}


void WobblyWindow::jumpToStart() {
    jumpRelative(0 - current_frame);
}


void WobblyWindow::jumpToEnd() {
    if (!project)
        return;

    jumpRelative(project->getNumFrames(PostSource) - current_frame);
}


void WobblyWindow::jumpToNextSectionStart() {
    if (!project)
        return;

    const Section *next_section = project->findNextSection(current_frame);

    if (next_section)
        jumpRelative(next_section->start - current_frame);
}


void WobblyWindow::jumpToPreviousSectionStart() {
    if (!project)
        return;

    if (current_frame == 0)
        return;

    const Section *section = project->findSection(current_frame);
    if (section->start == current_frame)
        section = project->findSection(current_frame - 1);

    jumpRelative(section->start - current_frame);
}


void WobblyWindow::jumpToPreviousMic() {
    if (!project)
        return;

    int frame = project->getPreviousFrameWithMic(mic_search_minimum_spin->value(), current_frame);
    if (frame != -1)
        requestFrames(frame);
}


void WobblyWindow::jumpToNextMic() {
    if (!project)
        return;

    int frame = project->getNextFrameWithMic(mic_search_minimum_spin->value(), current_frame);
    if (frame != -1)
        requestFrames(frame);
}


void WobblyWindow::jumpToFrame() {
    if (!project)
        return;

    bool ok;
    int frame = QInputDialog::getInt(this, QStringLiteral("Jump to frame"), QStringLiteral("Destination frame:"), current_frame, 0, project->getNumFrames(PostSource) - 1, 1, &ok);
    if (ok)
        requestFrames(frame);
}


void WobblyWindow::cycleMatchBCN() {
    if (!project)
        return;

    project->cycleMatchBCN(current_frame);

    updateCMatchSequencesWindow();

    try {
        evaluateMainDisplayScript();
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::freezeForward() {
    if (!project)
        return;

    if (current_frame == project->getNumFrames(PostSource) - 1)
        return;

    try {
        project->addFreezeFrame(current_frame, current_frame, current_frame + 1);

        evaluateMainDisplayScript();

        updateFrozenFramesViewer();
    } catch (WobblyException &e) {
        errorPopup(e.what());
        //statusBar()->showMessage(QStringLiteral("Couldn't freeze forward."), 5000);
    }
}


void WobblyWindow::freezeBackward() {
    if (!project)
        return;

    if (current_frame == 0)
        return;

    try {
        project->addFreezeFrame(current_frame, current_frame, current_frame - 1);

        evaluateMainDisplayScript();

        updateFrozenFramesViewer();
    } catch (WobblyException &e) {
        errorPopup(e.what());
        //statusBar()->showMessage(QStringLiteral("Couldn't freeze backward."), 5000);
    }
}


void WobblyWindow::freezeRange() {
    if (!project)
        return;

    static FreezeFrame ff = { -1, -1, -1 };

    if (ff.first == -1) {
        if (range_start == -1) {
            ff.first = current_frame;
            ff.last = current_frame;
        } else {
            finishRange();

            ff.first = range_start;
            ff.last = range_end;

            cancelRange();
        }

        freeze_label->setText(QStringLiteral("Freezing [%1,%2]").arg(ff.first).arg(ff.last));
    } else if (ff.replacement == -1) {
        ff.replacement = current_frame;
        try {
            project->addFreezeFrame(ff.first, ff.last, ff.replacement);

            evaluateMainDisplayScript();

            updateFrozenFramesViewer();
        } catch (WobblyException &e) {
            updateFrameDetails();

            errorPopup(e.what());
            //statusBar()->showMessage(QStringLiteral("Couldn't freeze range."), 5000);
        }

        ff = { -1, -1, -1 };
    }
}


void WobblyWindow::deleteFreezeFrame() {
    if (!project)
        return;

    const FreezeFrame *ff = project->findFreezeFrame(current_frame);
    if (ff) {
        project->deleteFreezeFrame(ff->first);

        updateFrozenFramesViewer();

        try {
            evaluateMainDisplayScript();
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }
}


void WobblyWindow::toggleDecimation() {
    if (!project)
        return;

    if (project->isDecimatedFrame(current_frame))
        project->deleteDecimatedFrame(current_frame);
    else
        project->addDecimatedFrame(current_frame);

    updateFrameDetails();

    updateFrameRatesViewer();
}


void WobblyWindow::toggleCombed() {
    if (!project)
        return;

    int start, end;

    if (range_start == -1) {
        start = current_frame;
        end = current_frame;
    } else {
        finishRange();

        start = range_start;
        end = range_end;

        cancelRange();
    }

    if (project->isCombedFrame(current_frame))
        for (int i = start; i <= end; i++)
            project->deleteCombedFrame(i);
    else
        for (int i = start; i <= end; i++)
            project->addCombedFrame(i);

    updateFrameDetails();
}


void WobblyWindow::addSection() {
    if (!project)
        return;

    const Section *section = project->findSection(current_frame);
    if (section->start != current_frame) {
        project->addSection(current_frame);

        updateSectionsEditor();

        updateFrameDetails();
    }
}


void WobblyWindow::deleteSection() {
    if (!project)
        return;

    const Section *section = project->findSection(current_frame);
    if (section->start != 0) {
        project->deleteSection(section->start);

        auto items = sections_table->findItems(QString::number(section->start), Qt::MatchFixedString);
        if (items.size() == 1)
            sections_table->removeRow(items[0]->row());

        updateFrameDetails();
    }
}


void WobblyWindow::presetChanged(const QString &text) {
    if (!project)
        return;

    if (text.isEmpty())
        preset_edit->setPlainText(QString());
    else
        preset_edit->setPlainText(QString::fromStdString(project->getPresetContents(text.toStdString())));
}


void WobblyWindow::presetEdited() {
    if (!project)
        return;

    if (preset_combo->currentIndex() == -1)
        return;

    project->setPresetContents(preset_combo->currentText().toStdString(), preset_edit->toPlainText().toStdString());
}


void WobblyWindow::resetMatch() {
    if (!project)
        return;

    int start, end;

    if (range_start == -1) {
        start = current_frame;
        end = current_frame;
    } else {
        finishRange();

        start = range_start;
        end = range_end;

        cancelRange();
    }


    project->resetRangeMatches(start, end);

    updateCMatchSequencesWindow();

    try {
        evaluateMainDisplayScript();
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::resetSection() {
    if (!project)
        return;

    const Section *section = project->findSection(current_frame);

    project->resetSectionMatches(section->start);

    updateCMatchSequencesWindow();

    try {
        evaluateMainDisplayScript();
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::rotateAndSetPatterns() {
    if (!project)
        return;

    int size = match_pattern.size();
    match_pattern.prepend(match_pattern[size - 1]);
    match_pattern.truncate(size);
    match_pattern_edit->setText(match_pattern);

    size = decimation_pattern.size();
    decimation_pattern.prepend(decimation_pattern[size - 1]);
    decimation_pattern.truncate(size);
    decimation_pattern_edit->setText(decimation_pattern);

    const Section *section = project->findSection(current_frame);

    project->setSectionMatchesFromPattern(section->start, match_pattern.toStdString());
    project->setSectionDecimationFromPattern(section->start, decimation_pattern.toStdString());

    updateFrameRatesViewer();

    updateCMatchSequencesWindow();

    try {
        evaluateMainDisplayScript();
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::setMatchPattern() {
    if (!project)
        return;

    if (range_start == -1)
        return;

    finishRange();

    project->setRangeMatchesFromPattern(range_start, range_end, match_pattern.toStdString());

    cancelRange();

    updateCMatchSequencesWindow();

    try {
        evaluateMainDisplayScript();
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::setDecimationPattern() {
    if (!project)
        return;

    if (range_start == -1)
        return;

    finishRange();

    project->setRangeDecimationFromPattern(range_start, range_end, decimation_pattern.toStdString());

    cancelRange();

    updateFrameRatesViewer();

    try {
        evaluateMainDisplayScript();
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::setMatchAndDecimationPatterns() {
    if (!project)
        return;

    if (range_start == -1)
        return;

    finishRange();

    project->setRangeMatchesFromPattern(range_start, range_end, match_pattern.toStdString());
    project->setRangeDecimationFromPattern(range_start, range_end, decimation_pattern.toStdString());

    cancelRange();

    updateFrameRatesViewer();

    updateCMatchSequencesWindow();

    try {
        evaluateMainDisplayScript();
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::guessCurrentSectionPatternsFromMics() {
    if (!project)
        return;

    int section_start = project->findSection(current_frame)->start;

    int use_patterns = 0;
    auto buttons = pg_use_patterns_buttons->buttons();
    for (int i = 0; i < buttons.size(); i++)
        if (buttons[i]->isChecked())
            use_patterns |= pg_use_patterns_buttons->id(buttons[i]);

    bool success;

    try {
        success = project->guessSectionPatternsFromMics(section_start, pg_length_spin->value(), use_patterns, pg_decimate_buttons->checkedId());
    } catch (WobblyException &e) {
        errorPopup(e.what());
        return;
    }

    updatePatternGuessingWindow();

    if (success) {
        updateFrameRatesViewer();

        updateCMatchSequencesWindow();

        try {
            evaluateMainDisplayScript();
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }
}


void WobblyWindow::guessProjectPatternsFromMics() {

    if (!project)
        return;

    int use_patterns = 0;
    auto buttons = pg_use_patterns_buttons->buttons();
    for (int i = 0; i < buttons.size(); i++)
        if (buttons[i]->isChecked())
            use_patterns |= pg_use_patterns_buttons->id(buttons[i]);

    try {
        project->guessProjectPatternsFromMics(pg_length_spin->value(), use_patterns, pg_decimate_buttons->checkedId());

        updatePatternGuessingWindow();

        updateFrameRatesViewer();

        updateCMatchSequencesWindow();

        evaluateMainDisplayScript();
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::guessCurrentSectionPatternsFromMatches() {
    if (!project)
        return;

    int section_start = project->findSection(current_frame)->start;

    bool success = project->guessSectionPatternsFromMatches(section_start, pg_length_spin->value(), pg_n_match_buttons->checkedId(), pg_decimate_buttons->checkedId());

    updatePatternGuessingWindow();

    if (success) {
        updateFrameRatesViewer();

        updateCMatchSequencesWindow();

        try {
            evaluateMainDisplayScript();
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }
}


void WobblyWindow::guessProjectPatternsFromMatches() {
    if (!project)
        return;

    project->guessProjectPatternsFromMatches(pg_length_spin->value(), pg_n_match_buttons->checkedId(), pg_decimate_buttons->checkedId());

    updatePatternGuessingWindow();

    updateFrameRatesViewer();

    updateCMatchSequencesWindow();

    try {
        evaluateMainDisplayScript();
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::togglePreview() {
    if (!project)
        return;

    preview = !preview;

    try {
        if (preview) {
            presetEdited();

            evaluateFinalScript();
        } else {
            evaluateMainDisplayScript();
        }
    } catch (WobblyException &e) {
        errorPopup(e.what());
        preview = !preview;
    }

    tab_bar->blockSignals(true);
    tab_bar->setCurrentIndex((int)preview);
    tab_bar->blockSignals(false);
}


void WobblyWindow::zoom(bool in) {
    if (!project)
        return;

    int zoom = project->getZoom();
    if ((!in && zoom > 1) || (in && zoom < 8)) {
        zoom += in ? 1 : -1;
        project->setZoom(zoom);
        try {
            requestFrames(current_frame);
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }

    zoom_label->setText(QStringLiteral("Zoom: %1x").arg(zoom));
}


void WobblyWindow::zoomIn() {
    zoom(true);
}


void WobblyWindow::zoomOut() {
    zoom(false);
}


void WobblyWindow::startRange() {
    if (!project)
        return;

    range_start = current_frame;

    statusBar()->showMessage(QStringLiteral("Range start: %1").arg(range_start), 0);
}


void WobblyWindow::finishRange() {
    if (!project)
        return;

    range_end = current_frame;

    if (range_start > range_end)
        std::swap(range_start, range_end);
}


void WobblyWindow::cancelRange() {
    if (!project)
        return;

    range_start = range_end = -1;

    statusBar()->clearMessage();
}


int WobblyWindow::getSelectedPreset() const {
    return selected_preset;
}


void WobblyWindow::setSelectedPreset(int index) {
    QStringList presets = presets_model->stringList();

    if (index >= presets.size())
        index = presets.size() - 1;

    selected_preset = index;

    selected_preset_label->setText("Selected preset: " + (selected_preset > -1 ? presets[selected_preset] : ""));
}


void WobblyWindow::selectPreviousPreset() {
    if (!project)
        return;

    QStringList presets = presets_model->stringList();

    int index = getSelectedPreset();

    if (presets.size() == 0) {
        index = -1;
    } else if (presets.size() == 1) {
        index = 0;
    } else {
        if (index == -1) {
            index = presets.size() - 1;
        } else {
            if (index == 0)
                index = presets.size();
            index--;
        }
    }

    setSelectedPreset(index);
}


void WobblyWindow::selectNextPreset() {
    if (!project)
        return;

    QStringList presets = presets_model->stringList();

    int index = getSelectedPreset();

    if (presets.size() == 0) {
        index = -1;
    } else if (presets.size() == 1) {
        index = 0;
    } else {
        if (index == -1) {
            index = 0;
        } else {
            index = (index + 1) % presets.size();
        }
    }

    setSelectedPreset(index);
}


int WobblyWindow::getSelectedCustomList() const {
    return selected_custom_list;
}


void WobblyWindow::setSelectedCustomList(int index) {
    if (!project)
        return;

    auto cl = project->getCustomLists();

    if (index >= (int)cl.size())
        index = cl.size() - 1;

    selected_custom_list = index;

    selected_custom_list_label->setText(QStringLiteral("Selected custom list: ") + (selected_custom_list > -1 ? cl[selected_custom_list].name.c_str() : ""));
}


void WobblyWindow::selectPreviousCustomList() {
    if (!project)
        return;

    const auto &cl = project->getCustomLists();

    int index = getSelectedCustomList();

    if (cl.size() == 0) {
        index = -1;
    } else if (cl.size() == 1) {
        index = 0;
    } else {
        if (index == -1) {
            index = cl.size() - 1;
        } else {
            if (index == 0)
                index = cl.size();
            index--;
        }
    }

    setSelectedCustomList(index);
}


void WobblyWindow::selectNextCustomList() {
    if (!project)
        return;

    const auto &cl = project->getCustomLists();

    int index = getSelectedCustomList();

    if (cl.size() == 0) {
        index = -1;
    } else if (cl.size() == 1) {
        index = 0;
    } else {
        if (index == -1) {
            index = 0;
        } else {
            index = (index + 1) % cl.size();
        }
    }

    setSelectedCustomList(index);
}


void WobblyWindow::assignSelectedPresetToCurrentSection() {
    if (!project)
        return;

    if (selected_preset == -1)
        return;

    QStringList presets = presets_model->stringList();

    int section_start = project->findSection(current_frame)->start;
    project->setSectionPreset(section_start, presets[selected_preset].toStdString());

    updateFrameDetails();

    updateSectionsEditor();
}


void WobblyWindow::addRangeToSelectedCustomList() {
    if (!project)
        return;

    if (selected_custom_list == -1)
        return;

    int start, end;

    if (range_start == -1) {
        start = current_frame;
        end = current_frame;
    } else {
        finishRange();

        start = range_start;
        end = range_end;

        cancelRange();
    }

    try {
        project->addCustomListRange(selected_custom_list, start, end);

        updateFrameDetails();

        updateCustomListsEditor();
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}
