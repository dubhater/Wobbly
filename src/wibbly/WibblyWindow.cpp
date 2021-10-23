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


#include <condition_variable>
#include <mutex>

#include <QApplication>
#include <QButtonGroup>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaType>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QStatusBar>
#include <QThread>

#include <QHBoxLayout>
#include <QVBoxLayout>

#include "ScrollArea.h"
#include "WibblyWindow.h"
#include "WobblyException.h"


// To avoid duplicating the string literals passed to QSettings
#define KEY_STATE                           QStringLiteral("user_interface/state")
#define KEY_GEOMETRY                        QStringLiteral("user_interface/geometry")
#define KEY_FONT_SIZE                       QStringLiteral("user_interface/font_size")
#define KEY_MAXIMUM_CACHE_SIZE              QStringLiteral("user_interface/maximum_cache_size")
#define KEY_LAST_DIR                        QStringLiteral("user_interface/last_dir")

#define KEY_COMPACT_PROJECT_FILES           QStringLiteral("projects/compact_project_files")
#define KEY_USE_RELATIVE_PATHS              QStringLiteral("projects/use_relative_paths")

#define KEY_JOBS                            QStringLiteral("jobs")
#define KEY_COUNT                           QStringLiteral("jobs/count")
#define KEY_JOB                             QStringLiteral("jobs/job%1")
#define KEY_INPUT_FILE                      QStringLiteral("input_file")
#define KEY_SOURCE_FILTER                   QStringLiteral("source_filter")
#define KEY_OUTPUT_FILE                     QStringLiteral("output_file")
#define KEY_STEPS                           QStringLiteral("steps")
#define KEY_CROP                            QStringLiteral("crop")
#define KEY_TRIMS                           QStringLiteral("trims")
#define KEY_VFM                             QStringLiteral("vfm/")
#define KEY_VDECIMATE                       QStringLiteral("vdecimate/")
#define KEY_FADES_THRESHOLD                 QStringLiteral("fades_threshold")


std::mutex requests_mutex;
std::condition_variable requests_condition;


// QImageCleanupFunction is expected to be a cdecl function, but VSAPI::freeFrame uses stdcall.
// Thus a wrapper is needed.
void vsapiFreeFrameCdecl(void *frame) {
    vsscript_getVSApi()->freeFrame((const VSFrameRef *)frame);
}


WibblyWindow::WibblyWindow()
    : QMainWindow()
    , vsapi(nullptr)
    , vsscript(nullptr)
    , vscore(nullptr)
    , vsnode(nullptr)
    , vsvi(nullptr)
    , current_frame(0)
    , trim_start(-1)
    , trim_end(-1)
    , current_project(nullptr)
    , current_job(-1)
    , next_frame(0)
    , frames_left(0)
    , aborted(false)
    , request_count(0)
#ifdef _WIN32
    , settings(QApplication::applicationDirPath() + "/wibbly.ini", QSettings::IniFormat)
#endif
{
    createUI();

    try {
        initialiseVapourSynth();

        checkRequiredFilters();
    } catch (WobblyException &e) {
        show();
        errorPopup(e.what());
    }

    readSettings();

    readJobs();
}


void VS_CC messageHandler(int msgType, const char *msg, void *userData) {
    WibblyWindow *window = (WibblyWindow *)userData;

    Qt::ConnectionType type;
    if (QThread::currentThread() == window->thread())
        type = Qt::DirectConnection;
    else
        type = Qt::QueuedConnection;

    QMetaObject::invokeMethod(window, "vsLogPopup", type, Q_ARG(int, msgType), Q_ARG(QString, QString(msg)));
}


void WibblyWindow::vsLogPopup(int msgType, const QString &msg) {
    QString message;

    if (msgType == mtFatal) {
        writeJobs();
        writeSettings();

        message += "Your work has been saved. Wibbly will now close.\n\n";
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


void WibblyWindow::initialiseVapourSynth() {
    if (!vsscript_init())
        throw WobblyException("Fatal error: failed to initialise VSScript. Your VapourSynth installation is probably broken. Python probably couldn't 'import vapoursynth'.");


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


void WibblyWindow::cleanUpVapourSynth() {
    video_frame_label->setPixmap(QPixmap());

    vsapi->freeNode(vsnode);
    vsnode = nullptr;

    vsscript_freeScript(vsscript);
    vsscript = nullptr;
}


void WibblyWindow::checkRequiredFilters() {
    struct Plugin {
        std::string id;
        std::vector<std::string> filters;
        std::string plugin_not_found;
        std::string filter_not_found;
    };

    std::vector<Plugin> plugins = {
        {
            "com.vapoursynth.dgdecodenv",
            { "DGSource" },
            "DGDecNV plugin not found.",
            ""
        },
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
            "org.ivtc.v",
            { "VFM", "VDecimate" },
            "VIVTC plugin not found.",
            ""
        },
        {
            "com.nodame.scxvid",
            { "Scxvid" },
            "SCXVID plugin not found.",
            ""
        },
        {
            "com.vapoursynth.resize",
            { "Point", "Bilinear", "Bicubic", "Spline16", "Spline36", "Lanczos" },
            "built-in resizers not found. Did you compile VapourSynth yourself?",
            "VapourSynth version is older than r29."
        },
        {
            "com.vapoursynth.std",
            { "PlaneStats" },
            "built-in filters not found. Something is borked.",
            "VapourSynth version is older than r32."
        },
        {
            "com.djatom.libp2p",
            { "Pack" },
            "LibP2P plugin not found.",
            ""
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


void WibblyWindow::closeEvent(QCloseEvent *event) {
    QMetaObject::invokeMethod(main_destination_edit, "editingFinished", Qt::DirectConnection);

    writeJobs();

    writeSettings();

    cleanUpVapourSynth();

    event->accept();
}


void WibblyWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}


void WibblyWindow::dropEvent(QDropEvent *event) {
    QList<QUrl> urls = event->mimeData()->urls();

    QStringList paths;
    paths.reserve(urls.size());

    for (int i = 0; i < urls.size(); i++)
        if (urls[i].isLocalFile())
            paths.push_back(urls[i].toLocalFile());

    paths.sort();

    for (int i = 0; i < paths.size(); i++)
        realOpenVideo(paths[i]);

    event->acceptProposedAction();
}


void WibblyWindow::createUI() {
    setAcceptDrops(true);

    setWindowTitle(QStringLiteral("Wibbly Metrics Collector v%1").arg(PACKAGE_VERSION));

    createMainWindow();
    createVideoOutputWindow();
    createCropWindow();
    createVFMWindow();
    createVDecimateWindow();
    createTrimWindow();
    createInterlacedFadesWindow();
    createSettingsWindow();
}


void WibblyWindow::createMenus() {
    QMenuBar *bar = menuBar();

    menu_menu = bar->addMenu("&Menu");

    QAction *quit_action = new QAction("&Quit", this);
    quit_action->setShortcut(QKeySequence("Ctrl+Q"));

    connect(quit_action, &QAction::triggered, this, &WibblyWindow::close);

    menu_menu->addSeparator();
    menu_menu->addAction(quit_action);


    QMenu *h = bar->addMenu("&Help");

    QAction *helpAbout = new QAction("About", this);
    QAction *helpAboutQt = new QAction("About Qt", this);

    connect(helpAbout, &QAction::triggered, [this] () {
        QMessageBox::about(this, QStringLiteral("About Wibbly"), QStringLiteral(
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


void WibblyWindow::createShortcuts() {
    struct Shortcut {
        const char *keys;
        void (WibblyWindow::* func)();
    };

    // Sequences starting with Delete prevent the list widgets from receiving the key press event.
    std::vector<Shortcut> shortcuts = {
        { "Left", &WibblyWindow::jump1Backward },
        { "Right", &WibblyWindow::jump1Forward },
        { "Ctrl+Left", &WibblyWindow::jump5Backward },
        { "Ctrl+Right", &WibblyWindow::jump5Forward },
        { "Alt+Left", &WibblyWindow::jump50Backward },
        { "Alt+Right", &WibblyWindow::jump50Forward },
        { "Ctrl+Home", &WibblyWindow::jumpToStart },
        { "Ctrl+End", &WibblyWindow::jumpToEnd },
        { "PgDown", &WibblyWindow::jumpALotBackward },
        { "PgUp", &WibblyWindow::jumpALotForward },
        { "Ctrl+Up", &WibblyWindow::selectPreviousJob },
        { "Ctrl+Down", &WibblyWindow::selectNextJob },
        { "[", &WibblyWindow::startTrim },
        { "]", &WibblyWindow::endTrim },
        { "A", &WibblyWindow::addTrim }
    };

    for (size_t i = 0; i < shortcuts.size(); i++) {
        QShortcut *s = new QShortcut(QKeySequence(shortcuts[i].keys), this);
        connect(s, &QShortcut::activated, this, shortcuts[i].func);
    }
}


void WibblyWindow::createMainWindow() {
    createMenus();
    createShortcuts();


    main_jobs_list = new ListWidget;
    main_jobs_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    main_jobs_list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    main_destination_edit = new QLineEdit;

    QPushButton *main_choose_button = new QPushButton("Choose");
    QPushButton *main_autonumber_button = new QPushButton("Autonumber");

    QPushButton *main_add_jobs_button = new QPushButton("Add jobs");
    QPushButton *main_remove_jobs_button = new QPushButton("Remove jobs");
    QPushButton *main_copy_jobs_button = new QPushButton("Copy jobs");
    QPushButton *main_move_jobs_up_button = new QPushButton("Move up");
    QPushButton *main_move_jobs_down_button = new QPushButton("Move down");

    std::map<int, QString> steps = {
        { StepTrim, "Trim" },
        { StepCrop, "Crop" },
        { StepFieldMatch, "Field matching" },
        { StepInterlacedFades, "Interlaced fades" },
        { StepDecimation, "Decimation" },
        { StepSceneChanges, "Scene changes" },
    };

    QButtonGroup *main_steps_buttons = new QButtonGroup(this);
    main_steps_buttons->setExclusive(false);

    for (auto it = steps.cbegin(); it != steps.cend(); it++) {
        main_steps_buttons->addButton(new QCheckBox(it->second), it->first);
        main_steps_buttons->button(it->first)->setChecked(true);
    }

    main_progress_dialog = new ProgressDialog;
    main_progress_dialog->setModal(true);
    main_progress_dialog->setWindowTitle(QStringLiteral("Gathering metrics..."));
    main_progress_dialog->setLabel(new QLabel);
    main_progress_dialog->reset();

    QPushButton *main_engage_button = new QPushButton("Engage");


    connect(main_jobs_list, &ListWidget::currentRowChanged, [this, main_steps_buttons, steps] (int currentRow) {
        if (currentRow < 0)
            return;

        const WibblyJob &job = jobs[currentRow];

        main_destination_edit->setText(QString::fromStdString(job.getOutputFile()));

        for (auto it = steps.cbegin(); it != steps.cend(); it++)
            main_steps_buttons->button(it->first)->setChecked(job.getSteps() & it->first);

        trim_ranges_list->clear();
        auto trims = job.getTrims();
        for (auto it = trims.cbegin(); it != trims.cend(); it++) {
            QListWidgetItem *item = new QListWidgetItem(QStringLiteral("%1,%2").arg(it->second.first).arg(it->second.last), trim_ranges_list);
            item->setData(Qt::UserRole, it->first);
        }

        const Crop &crop = job.getCrop();
        int crop_values[4] = { crop.left, crop.top, crop.right, crop.bottom };
        for (int i = 0; i < 4; i++) {
            QSignalBlocker block(crop_spin[i]);
            crop_spin[i]->setValue(crop_values[i]);
        }

        {
            QSignalBlocker block(fades_threshold_spin);
            fades_threshold_spin->setValue(job.getFadesThreshold());
        }

        for (size_t i = 0; i < vfm_params.size(); i++) {
            if (vfm_params[i].type == VIVTCParamInt) {
                QSpinBox *spin = reinterpret_cast<QSpinBox *>(vfm_params[i].widget);
                QSignalBlocker block(spin);
                spin->setValue(job.getVFMParameterInt(vfm_params[i].name.toStdString()));
            } else if (vfm_params[i].type == VIVTCParamDouble) {
                QDoubleSpinBox *spin = reinterpret_cast<QDoubleSpinBox *>(vfm_params[i].widget);
                QSignalBlocker block(spin);
                spin->setValue(job.getVFMParameterDouble(vfm_params[i].name.toStdString()));
            } else if (vfm_params[i].type == VIVTCParamBool) {
                QCheckBox *check = reinterpret_cast<QCheckBox *>(vfm_params[i].widget);
                check->setChecked(job.getVFMParameterBool(vfm_params[i].name.toStdString()));
            }
        }

        try {
            evaluateDisplayScript();
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    });

    connect(main_jobs_list, &ListWidget::deletePressed, main_remove_jobs_button, &QPushButton::click);

    auto destinationChanged = [this] () {
        auto selection = main_jobs_list->selectedItems();

        for (int i = 0; i < selection.size(); i++) {
            int row = main_jobs_list->row(selection[i]);

            jobs[row].setOutputFile(main_destination_edit->text().toStdString());
        }
    };

    connect(main_destination_edit, &QLineEdit::editingFinished, destinationChanged);

    connect(main_choose_button, &QPushButton::clicked, [this, destinationChanged] () {
        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Choose destination"), settings.value(KEY_LAST_DIR).toString(), QStringLiteral("Wobbly projects (*.json);;All files (*)"));

        if (!path.isEmpty()) {
            settings.setValue(KEY_LAST_DIR, QFileInfo(path).absolutePath());

            main_destination_edit->setText(path);
            destinationChanged();
        }
    });

    connect(main_autonumber_button, &QPushButton::clicked, [this] () {
        auto selection = main_jobs_list->selectedItems();

        int field_width = QString::number(selection.size() - 1).size();

        for (int i = 0; i < selection.size(); i++) {
            int row = main_jobs_list->row(selection[i]);

            QString output_file = QString::fromStdString(jobs[row].getOutputFile()).arg(i + 1, field_width, 10, QLatin1Char('0'));
            jobs[row].setOutputFile(output_file.toStdString());

            if (row == main_jobs_list->currentRow())
                main_destination_edit->setText(output_file);
        }
    });

    connect(main_add_jobs_button, &QPushButton::clicked, [this] () {
        QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("Open video file"), settings.value(KEY_LAST_DIR).toString());

        paths.sort();

        for (int i = 0; i < paths.size(); i++) {
            if (!paths[i].isNull()) {
                settings.setValue(KEY_LAST_DIR, QFileInfo(paths[i]).absolutePath());

                realOpenVideo(paths[i]);
            }
        }
    });

    connect(main_remove_jobs_button, &QPushButton::clicked, [this] () {
        auto selection = main_jobs_list->selectedItems();

        {
            QSignalBlocker block(main_jobs_list);

            for (int i = selection.size() - 1; i >= 0; i--) {
                int row = main_jobs_list->row(selection[i]);

                jobs.erase(jobs.cbegin() + row);
                delete main_jobs_list->takeItem(row);
            }
        }
    });

    connect(main_copy_jobs_button, &QPushButton::clicked, [this] () {
        auto selection = main_jobs_list->selectedItems();

        for (int i = selection.size() - 1; i >= 0; i--) {
            int row = main_jobs_list->row(selection[i]);

            jobs.insert(jobs.cbegin() + row + 1, jobs[row]);
            main_jobs_list->insertItem(row + 1, main_jobs_list->item(row)->text());
        }
    });

    connect(main_move_jobs_up_button, &QPushButton::clicked, [this] () {
        auto selection = main_jobs_list->selectedItems();

        for (int i = 0; i < selection.size(); i++) {
            int row = main_jobs_list->row(selection[i]);

            if (row == 0)
                return;

            std::swap(jobs[row], jobs[row - 1]);
            main_jobs_list->insertItem(row, main_jobs_list->takeItem(row - 1));
        }
    });

    connect(main_move_jobs_down_button, &QPushButton::clicked, [this] () {
        auto selection = main_jobs_list->selectedItems();

        for (int i = selection.size() - 1; i >= 0; i--) {
            int row = main_jobs_list->row(selection[i]);

            if (row == main_jobs_list->count() - 1)
                return;

            std::swap(jobs[row], jobs[row + 1]);
            main_jobs_list->insertItem(row, main_jobs_list->takeItem(row + 1));
        }
    });

    connect(main_steps_buttons, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), [this, main_steps_buttons] (int id) {
        bool checked = main_steps_buttons->button(id)->isChecked();

        auto selection = main_jobs_list->selectedItems();

        for (int i = 0; i < selection.size(); i++) {
            int row = main_jobs_list->row(selection[i]);

            int new_steps = jobs[row].getSteps();
            if (checked)
                new_steps |= id;
            else
                new_steps &= ~id;
            jobs[row].setSteps(new_steps);
        }

        if (id == StepCrop || id == StepFieldMatch || id == StepInterlacedFades) {
            try {
                evaluateDisplayScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());
            }
        }
    });

    connect(main_engage_button, &QPushButton::clicked, [this] () {
        setEnabled(false);
        QApplication::processEvents();

        QString errors;

        for (auto job = jobs.cbegin(); job != jobs.cend(); job++) {
            int index = std::distance(jobs.cbegin(), job) + 1;

            QString path = QString::fromStdString(job->getOutputFile());

            QFile file(path);

            bool opened = file.open(QIODevice::WriteOnly);
            if (!opened)
                errors += QStringLiteral("Couldn't open the destination file for job number %1 (%2). Error message: %3\n\n").arg(index).arg(path).arg(file.errorString());

            if (opened) {
                qint64 written = file.write("42");
                if (written < 0)
                    errors += QStringLiteral("Couldn't write '42' to the destination file for job number %1 (%2). Error message: %3\n\n").arg(index).arg(path).arg(file.errorString());

                file.close();
            }

            try {
                evaluateFinalScript(index - 1);
            } catch (WobblyException &e) {
                errors += e.what();
                errors += "\n\n";
            }
        }

        if (!errors.isEmpty()) {
            QMessageBox msg;
            msg.setText(QStringLiteral("Some sanity checks failed."));
            msg.setDetailedText(errors);
            msg.exec();

            setEnabled(true);
            QApplication::processEvents();

            try {
                evaluateDisplayScript();
            } catch (WobblyException &) {

            }

            return;
        }

        startNextJob();
    });

    connect(main_progress_dialog, &ProgressDialog::canceled, [this] () {
        aborted = true;

        delete current_project;
        current_project = nullptr;

        current_job = -1;

        int current_row = main_jobs_list->currentRow();
        main_jobs_list->setCurrentRow(-1, QItemSelectionModel::NoUpdate);
        main_jobs_list->setCurrentRow(current_row, QItemSelectionModel::NoUpdate);

        setEnabled(true);
    });

    connect(main_progress_dialog, &ProgressDialog::minimiseChanged, [this] (bool minimised) {
        if (minimised)
            setWindowState(windowState() | Qt::WindowMinimized);
        else
            setWindowState(windowState() & ~Qt::WindowMinimized);
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(main_jobs_list, 1);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(new QLabel("Destination:"));
    hbox->addWidget(main_destination_edit);
    hbox->addWidget(main_choose_button);
    hbox->addWidget(main_autonumber_button);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;

    QVBoxLayout *vbox2 = new QVBoxLayout;
    vbox2->addWidget(main_add_jobs_button);
    vbox2->addWidget(main_remove_jobs_button);
    vbox2->addWidget(main_copy_jobs_button);
    vbox2->addWidget(main_move_jobs_up_button);
    vbox2->addWidget(main_move_jobs_down_button);
    vbox2->addStretch(1);
    hbox->addLayout(vbox2);

    vbox2 = new QVBoxLayout;
    for (auto it = steps.cbegin(); it != steps.cend(); it++) {
        vbox2->addWidget(main_steps_buttons->button(it->first));
    }
    vbox2->addStretch(1);

    hbox->addLayout(vbox2);
    hbox->addStretch(1);

    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(main_engage_button);
    hbox->addStretch(1);

    vbox->addSpacing(10);
    vbox->addLayout(hbox);


    QWidget *main_widget = new QWidget;
    main_widget->setLayout(vbox);

    setCentralWidget(main_widget);
}


void WibblyWindow::createVideoOutputWindow() {
    video_frame_label = new QLabel;
    video_frame_label->setAlignment(Qt::AlignCenter);

    ScrollArea *video_frame_scroll = new ScrollArea;
    video_frame_scroll->setFocusPolicy(Qt::ClickFocus);
    video_frame_scroll->setAlignment(Qt::AlignCenter);
    video_frame_scroll->setWidgetResizable(true);
    video_frame_scroll->setWidget(video_frame_label);

    video_frame_spin = new QSpinBox;

    video_time_edit = new QTimeEdit;
    video_time_edit->setDisplayFormat(QStringLiteral("hh:mm:ss.zzz"));
    video_time_edit->setKeyboardTracking(false);

    video_frame_slider = new QSlider(Qt::Horizontal);
    video_frame_slider->setTracking(false);


    connect(video_frame_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &WibblyWindow::displayFrame);

    connect(video_time_edit, &QTimeEdit::timeChanged, [this] (const QTime &time) {
        if (!vsvi)
            return;

        QTime zero(0, 0, 0, 0);
        int milliseconds = zero.msecsTo(time);
        int frame = (int)(vsvi->fpsNum * milliseconds / (vsvi->fpsDen * 1000));
        displayFrame(frame);
    });

    connect(video_frame_slider, &QSlider::valueChanged, this, &WibblyWindow::displayFrame);


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(video_frame_scroll);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(video_frame_spin);
    hbox->addWidget(video_time_edit);
    hbox->addWidget(video_frame_slider);
    vbox->addLayout(hbox);


    QWidget *video_widget = new QWidget;
    video_widget->setLayout(vbox);


    video_dock = new DockWidget("Video output", this);
    video_dock->resize(720, 480);
    video_dock->setObjectName("video output window");
    video_dock->setVisible(true);
    video_dock->setFloating(true);
    video_dock->setWidget(video_widget);
    addDockWidget(Qt::RightDockWidgetArea, video_dock);
    QList<QAction *> actions = menu_menu->actions();
    menu_menu->insertAction(actions[actions.size() - 2], video_dock->toggleViewAction());
    connect(video_dock, &DockWidget::visibilityChanged, video_dock, &DockWidget::setEnabled);
}


void WibblyWindow::createCropWindow() {
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


    auto cropChanged = [this] () {
        auto selection = main_jobs_list->selectedItems();

        for (int i = 0; i < selection.size(); i++) {
            int row = main_jobs_list->row(selection[i]);

            jobs[row].setCrop(crop_spin[0]->value(), crop_spin[1]->value(), crop_spin[2]->value(), crop_spin[3]->value());
        }

        int current_row = main_jobs_list->currentRow();
        if (current_row > -1 && jobs[current_row].getSteps() & StepCrop) {
            try {
                evaluateDisplayScript();
            } catch (WobblyException &e) {
//                errorPopup(e.what());
            }
        }
    };

    for (int i = 0; i < 4; i++)
        connect(crop_spin[i], static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), cropChanged);


    QVBoxLayout *vbox = new QVBoxLayout;
    for (int i = 0; i < 4; i++)
        vbox->addWidget(crop_spin[i]);
    vbox->addStretch(1);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addLayout(vbox);
    hbox->addStretch(1);


    QWidget *crop_widget = new QWidget;
    crop_widget->setLayout(hbox);


    crop_dock = new DockWidget("Crop", this);
    crop_dock->setObjectName("crop window");
    crop_dock->setVisible(false);
    crop_dock->setFloating(true);
    crop_dock->setWidget(crop_widget);
    addDockWidget(Qt::RightDockWidgetArea, crop_dock);
    QList<QAction *> actions = menu_menu->actions();
    menu_menu->insertAction(actions[actions.size() - 2], crop_dock->toggleViewAction());
    connect(crop_dock, &DockWidget::visibilityChanged, crop_dock, &DockWidget::setEnabled);
}


void WibblyWindow::createVFMWindow() {
    vfm_params = {
        { nullptr, "order",      0,          1, VIVTCParamInt },
        { nullptr, "mchroma",    0,          1, VIVTCParamBool },
        { nullptr, "cthresh",    1,        255, VIVTCParamInt },
        { nullptr, "mi",         0,    INT_MAX, VIVTCParamInt },
        { nullptr, "chroma",     0,          1, VIVTCParamBool },
        { nullptr, "blockx",     4,        512, VIVTCParamInt },
        { nullptr, "blocky",     4,        512, VIVTCParamInt },
        { nullptr, "y0",         0,    INT_MAX, VIVTCParamInt },
        { nullptr, "y1",         0,    INT_MAX, VIVTCParamInt },
        { nullptr, "scthresh",   0,        100, VIVTCParamDouble },
        { nullptr, "micmatch",   0,          2, VIVTCParamInt }
    };

    auto parametersChanged = [this] () {
        auto selection = main_jobs_list->selectedItems();

        for (int i = 0; i < selection.size(); i++) {
            int row = main_jobs_list->row(selection[i]);

            WibblyJob &job = jobs[row];

            for (size_t j = 0; j < vfm_params.size(); j++) {
                if (vfm_params[j].type == VIVTCParamInt) {
                    job.setVFMParameter(vfm_params[j].name.toStdString(), reinterpret_cast<QSpinBox *>(vfm_params[j].widget)->value());
                } else if (vfm_params[j].type == VIVTCParamDouble) {
                    job.setVFMParameter(vfm_params[j].name.toStdString(), reinterpret_cast<QDoubleSpinBox *>(vfm_params[j].widget)->value());
                } else if (vfm_params[j].type == VIVTCParamBool) {
                    job.setVFMParameter(vfm_params[j].name.toStdString(), reinterpret_cast<QCheckBox *>(vfm_params[j].widget)->isChecked());
                }
            }
        }

        int current_row = main_jobs_list->currentRow();
        if (current_row > -1 && jobs[current_row].getSteps() & StepFieldMatch) {
            try {
                evaluateDisplayScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());
            }
        }
    };

    for (size_t i = 0; i < vfm_params.size(); i++) {
        if (vfm_params[i].type == VIVTCParamInt) {
            QSpinBox *spin = new QSpinBox;
            spin->setPrefix(vfm_params[i].name + ": ");
            spin->setMinimum(vfm_params[i].minimum);
            if (vfm_params[i].maximum != INT_MAX)
                spin->setMaximum(vfm_params[i].maximum);

            connect(spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), parametersChanged);

            vfm_params[i].widget = spin;
        } else if (vfm_params[i].type == VIVTCParamDouble) {
            QDoubleSpinBox *spin = new QDoubleSpinBox;
            spin->setPrefix(vfm_params[i].name + ": ");
            spin->setMaximum(vfm_params[i].maximum);

            connect(spin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), parametersChanged);

            vfm_params[i].widget = spin;
        } else if (vfm_params[i].type == VIVTCParamBool) {
            QCheckBox *check = new QCheckBox(vfm_params[i].name);
            check->setChecked(true);

            connect(check, &QCheckBox::clicked, parametersChanged);

            vfm_params[i].widget = check;
        }
    }


    QVBoxLayout *vbox = new QVBoxLayout;
    for (size_t i = 0; i < vfm_params.size(); i++)
        vbox->addWidget(vfm_params[i].widget);
    vbox->addStretch(1);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addLayout(vbox);
    hbox->addStretch(1);


    QWidget *vfm_widget = new QWidget;
    vfm_widget->setLayout(hbox);


    vfm_dock = new DockWidget("VFM", this);
    vfm_dock->setObjectName("vfm window");
    vfm_dock->setVisible(false);
    vfm_dock->setFloating(true);
    vfm_dock->setWidget(vfm_widget);
    addDockWidget(Qt::RightDockWidgetArea, vfm_dock);
    QList<QAction *> actions = menu_menu->actions();
    menu_menu->insertAction(actions[actions.size() - 2], vfm_dock->toggleViewAction());
    connect(vfm_dock, &DockWidget::visibilityChanged, vfm_dock, &DockWidget::setEnabled);
}


void WibblyWindow::createVDecimateWindow() {
    vdecimate_params = {
        { nullptr, "chroma",     0,          1, VIVTCParamBool },
        { nullptr, "dupthresh",  0,        100, VIVTCParamDouble },
        { nullptr, "scthresh",   0,        100, VIVTCParamDouble },
        { nullptr, "blockx",     4,        512, VIVTCParamInt },
        { nullptr, "blocky",     4,        512, VIVTCParamInt },
    };

    // An actual window later, if really necessary.
}


void WibblyWindow::createTrimWindow() {
    trim_ranges_list = new ListWidget;
    trim_ranges_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trim_ranges_list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    trim_start_label = new QLabel;
    trim_end_label = new QLabel;

    QPushButton *trim_start_button = new QPushButton("Start trim");
    QPushButton *trim_end_button = new QPushButton("End trim");
    QPushButton *trim_add_button = new QPushButton("Add trim");
    QPushButton *trim_delete_button = new QPushButton("Delete trim");


    connect(trim_ranges_list, &ListWidget::deletePressed, trim_delete_button, &QPushButton::click);

    // double click on a range

    connect(trim_start_button, &QPushButton::clicked, this, &WibblyWindow::startTrim);

    connect(trim_end_button, &QPushButton::clicked, this, &WibblyWindow::endTrim);

    connect(trim_add_button, &QPushButton::clicked, this, &WibblyWindow::addTrim);

    connect(trim_delete_button, &QPushButton::clicked, [this] () {
        auto job_selection = main_jobs_list->selectedItems();

        if (!job_selection.size())
            return;

        auto trim_selection = trim_ranges_list->selectedItems();

        if (!trim_selection.size())
            return;

        for (int i = 0; i < job_selection.size(); i++) {
            for (int j = 0; j < trim_selection.size(); j++) {
                jobs[main_jobs_list->row(job_selection[i])].deleteTrim(trim_selection[j]->data(Qt::UserRole).toInt());
            }
        }

        int current_row = main_jobs_list->currentRow();
        main_jobs_list->setCurrentRow(-1, QItemSelectionModel::NoUpdate);
        main_jobs_list->setCurrentRow(current_row, QItemSelectionModel::NoUpdate);
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(trim_ranges_list);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(trim_start_label);
    hbox->addWidget(trim_end_label);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(trim_add_button);
    hbox->addWidget(trim_delete_button);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(trim_start_button);
    hbox->addWidget(trim_end_button);
    hbox->addStretch(1);
    vbox->addLayout(hbox);


    QWidget *trim_widget = new QWidget;
    trim_widget->setLayout(vbox);


    trim_dock = new DockWidget("Trim", this);
    trim_dock->setObjectName("trim window");
    trim_dock->setVisible(false);
    trim_dock->setFloating(true);
    trim_dock->setWidget(trim_widget);
    addDockWidget(Qt::RightDockWidgetArea, trim_dock);
    QList<QAction *> actions = menu_menu->actions();
    menu_menu->insertAction(actions[actions.size() - 2], trim_dock->toggleViewAction());
    connect(trim_dock, &DockWidget::visibilityChanged, trim_dock, &DockWidget::setEnabled);
}


void WibblyWindow::createInterlacedFadesWindow() {
    fades_threshold_spin = new QDoubleSpinBox;
    fades_threshold_spin->setPrefix(QStringLiteral("Threshold: "));
    fades_threshold_spin->setMaximum(1);
    fades_threshold_spin->setDecimals(5);
    fades_threshold_spin->setSingleStep(0.0004);


    connect(fades_threshold_spin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [this] (double value) {
        auto selection = main_jobs_list->selectedItems();

        for (int i = 0; i < selection.size(); i++) {
            int row = main_jobs_list->row(selection[i]);

            jobs[row].setFadesThreshold(value);
        }
    });


    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(fades_threshold_spin);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addLayout(hbox);
    vbox->addStretch(1);


    QWidget *fades_widget = new QWidget;
    fades_widget->setLayout(vbox);


    fades_dock = new DockWidget("Interlaced fades", this);
    fades_dock->setObjectName("interlaced fades window");
    fades_dock->setVisible(false);
    fades_dock->setFloating(true);
    fades_dock->setWidget(fades_widget);
    addDockWidget(Qt::RightDockWidgetArea, fades_dock);
    QList<QAction *> actions = menu_menu->actions();
    menu_menu->insertAction(actions[actions.size() - 2], fades_dock->toggleViewAction());
    connect(fades_dock, &DockWidget::visibilityChanged, fades_dock, &DockWidget::setEnabled);
}


void WibblyWindow::createSettingsWindow() {
    settings_font_spin = new QSpinBox;
    settings_font_spin->setRange(4, 99);
    settings_font_spin->setPrefix(QStringLiteral("Font size: "));

    settings_compact_projects_check = new QCheckBox("Create compact project files");

    settings_use_relative_paths_check = new QCheckBox(QStringLiteral("Use relative paths in project files"));

    settings_cache_spin = new QSpinBox;
    settings_cache_spin->setRange(1, 99999);
    settings_cache_spin->setValue(200);
    settings_cache_spin->setPrefix(QStringLiteral("Maximum cache size: "));
    settings_cache_spin->setSuffix(QStringLiteral(" MiB"));


    connect(settings_font_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        QFont font = QApplication::font();
        font.setPointSize(value);
        QApplication::setFont(font);

        settings.setValue(KEY_FONT_SIZE, value);
    });

    connect(settings_compact_projects_check, &QCheckBox::clicked, [this] (bool checked) {
        settings.setValue(KEY_COMPACT_PROJECT_FILES, checked);
    });

    connect(settings_use_relative_paths_check, &QCheckBox::clicked, [this] (bool checked) {
        settings.setValue(KEY_USE_RELATIVE_PATHS, checked);
    });

    connect(settings_cache_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        settings.setValue(KEY_MAXIMUM_CACHE_SIZE, value);
    });


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
    hbox->addWidget(settings_cache_spin);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    vbox->addStretch(1);


    QWidget *settings_widget = new QWidget;
    settings_widget->setLayout(vbox);


    settings_dock = new DockWidget("Settings", this);
    settings_dock->setObjectName("settings window");
    settings_dock->setVisible(false);
    settings_dock->setFloating(true);
    settings_dock->setWidget(settings_widget);
    addDockWidget(Qt::RightDockWidgetArea, settings_dock);
    QList<QAction *> actions = menu_menu->actions();
    menu_menu->insertAction(actions[actions.size() - 2], settings_dock->toggleViewAction());
    connect(settings_dock, &DockWidget::visibilityChanged, settings_dock, &DockWidget::setEnabled);
}


void WibblyWindow::realOpenVideo(const QString &path) {
    QString source_filter;

    QString extension = path.mid(path.lastIndexOf('.') + 1);

    QStringList mp4 = { "mp4", "m4v", "mov" };

    if (extension == "dgi")
        source_filter = "dgdecodenv.DGSource";
    else if (extension == "d2v")
        source_filter = "d2v.Source";
    else if (mp4.contains(extension))
        source_filter = "lsmas.LibavSMASHSource";
    else
        source_filter = "lsmas.LWLibavSource";

    jobs.emplace_back();

    WibblyJob &job = jobs.back();

    job.setInputFile(path.toStdString());
    job.setSourceFilter(source_filter.toStdString());
    job.setOutputFile(QStringLiteral("%1.json").arg(path).toStdString());

    main_jobs_list->addItem(path);
}


void WibblyWindow::errorPopup(const QString &msg) {
    QMessageBox::information(this, QStringLiteral("Error"), msg);
}


void WibblyWindow::evaluateFinalScript(int job_index) {
    const WibblyJob &job = jobs[job_index];

    std::string script;

    script = job.generateFinalScript();

    if (vsscript_evaluateScript(&vsscript, script.c_str(), job.getInputFile().c_str(), efSetWorkingDir)) {
        std::string error = vsscript_getError(vsscript);
        // The traceback is mostly unnecessary noise.
        size_t traceback = error.find("Traceback");
        if (traceback != std::string::npos)
            error.insert(traceback, 1, '\n');

        throw WobblyException("Failed to evaluate final script for job number " + std::to_string(job_index + 1) + ". Error message:\n" + error);
    }

    vsapi->freeNode(vsnode);

    vsnode = vsscript_getOutput(vsscript, 0);
    if (!vsnode)
        throw WobblyException("Final script for job number " + std::to_string(job_index + 1) + " evaluated successfully, but no node found at output index 0.");

    vsvi = vsapi->getVideoInfo(vsnode);

    video_frame_label->setPixmap(QPixmap());
}


void WibblyWindow::evaluateDisplayScript() {
    int current_row = main_jobs_list->currentRow();
    if (current_row < 0)
        return;

    const WibblyJob &job = jobs[current_row];

    std::string script;

    script = job.generateDisplayScript();

    // BT 601
    script +=
            "src = vs.get_output(index=0)\n"
            // Since VapourSynth R41 get_output returns the alpha as well.
            "if isinstance(src, tuple):\n"
            "    src = src[0]\n"

            "c.query_video_format(vs.GRAY, vs.INTEGER, 32, 0, 0)\n"
            "src = c.resize.Bicubic(clip=src, format=vs.RGB24, dither_type='random', matrix_in_s='470bg', transfer_in_s='601', primaries_in_s='170m').libp2p.Pack()\n"

            "src.set_output()\n";

    script +=
            "c.max_cache_size = " + std::to_string(settings_cache_spin->value()) + "\n";

    VSMap *m = vsapi->createMap();
    if (vsscript_getVariable(vsscript, "wibbly_last_input_file", m)) {
        vsapi->propSetData(m, "wibbly_last_input_file", "", -1, paReplace);
        vsscript_setVariable(vsscript, m);
    }
    vsapi->freeMap(m);

    if (vsscript_evaluateScript(&vsscript, script.c_str(), job.getInputFile().c_str(), efSetWorkingDir)) {
        std::string error = vsscript_getError(vsscript);
        // The traceback is mostly unnecessary noise.
        size_t traceback = error.find("Traceback");
        if (traceback != std::string::npos)
            error.insert(traceback, 1, '\n');

        throw WobblyException("Failed to evaluate display script. Error message:\n" + error);
    }

    // Wait until all requests are done before freeing the node.
    std::unique_lock<std::mutex> lock(requests_mutex);
    while (request_count)
        requests_condition.wait(lock);

    vsapi->freeNode(vsnode);

    vsnode = vsscript_getOutput(vsscript, 0);
    if (!vsnode)
        throw WobblyException("Display script evaluated successfully, but no node found at output index 0.");

    vsvi = vsapi->getVideoInfo(vsnode);

    video_frame_spin->setMaximum(vsvi->numFrames - 1);

    {
        QSignalBlocker block(video_time_edit);
        video_time_edit->setTime(QTime(0, 0, 0, 0));
        if (vsvi->fpsNum && vsvi->fpsDen) {
            int milliseconds = (int)(((vsvi->numFrames - 1) * vsvi->fpsDen * 1000 / vsvi->fpsNum) % 1000);
            int seconds_total = (int)((vsvi->numFrames - 1) * vsvi->fpsDen / vsvi->fpsNum);
            int seconds = seconds_total % 60;
            int minutes = (seconds_total / 60) % 60;
            int hours = seconds_total / 3600;
            video_time_edit->setMaximumTime(QTime(hours, minutes, seconds, milliseconds));
        }
    }

    video_frame_slider->setMaximum(vsvi->numFrames - 1);
    video_frame_slider->setPageStep(vsvi->numFrames * 20 / 100);

    displayFrame(current_frame);
}


void WibblyWindow::displayFrame(int n) {
    if (!vsnode)
        return;

    if (n < 0)
        n = 0;
    if (n >= vsvi->numFrames)
        n = vsvi->numFrames - 1;

    std::vector<char> error(1024);
    if (n == vsvi->numFrames - 1)
        // Workaround for bug in d2vsource: https://github.com/dwbuiten/d2vsource/issues/12
        vsapi->freeFrame(vsapi->getFrame(n - 1, vsnode, error.data(), 1024));
    const VSFrameRef *frame = vsapi->getFrame(n, vsnode, error.data(), 1024);

    if (!frame)
        throw WobblyException(std::string("Failed to retrieve frame. Error message: ") + error.data());

    const uint8_t *ptr = vsapi->getReadPtr(frame, 0);
    int width = vsapi->getFrameWidth(frame, 0);
    int height = vsapi->getFrameHeight(frame, 0);
    int stride = vsapi->getStride(frame, 0);
    QPixmap pixmap = QPixmap::fromImage(QImage(ptr, width, height, stride, QImage::Format_RGB32, vsapiFreeFrameCdecl, (void *)frame));

    video_frame_label->setPixmap(pixmap);

    current_frame = n;

    {
        QSignalBlocker block(video_frame_spin);
        video_frame_spin->setValue(n);
    }

    if (vsvi->fpsNum && vsvi->fpsDen) {
        int milliseconds = (int)((n * vsvi->fpsDen * 1000 / vsvi->fpsNum) % 1000);
        int seconds_total = (int)(n * vsvi->fpsDen / vsvi->fpsNum);
        int seconds = seconds_total % 60;
        int minutes = (seconds_total / 60) % 60;
        int hours = seconds_total / 3600;

        {
            QSignalBlocker block(video_time_edit);
            video_time_edit->setTime(QTime(hours, minutes, seconds, milliseconds));
        }
    }

    {
        QSignalBlocker block(video_frame_slider);
        video_frame_slider->setValue(n);
    }
}


void VS_CC frameDoneCallback(void *userData, const VSFrameRef *f, int n, VSNodeRef *, const char *errorMsg) {
    WibblyWindow *window = (WibblyWindow *)userData;

    // Qt::DirectConnection = frameDone runs in the worker threads
    // Qt::QueuedConnection = frameDone runs in the GUI thread
    QMetaObject::invokeMethod(window, "frameDone", Qt::DirectConnection, Q_ARG(void *, (void *)f), Q_ARG(int, n), Q_ARG(QString, QString(errorMsg)));
}


// Always runs in the GUI thread.
void WibblyWindow::startNextJob() {
    current_job++;

    if (current_job == (int)jobs.size()) {
        // No more jobs.
        current_job = -1;

        int current_row = main_jobs_list->currentRow();
        main_jobs_list->setCurrentRow(-1, QItemSelectionModel::NoUpdate);
        main_jobs_list->setCurrentRow(current_row, QItemSelectionModel::NoUpdate);

        QApplication::alert(this, 0);

        // Re-enable the user interface.
        setEnabled(true);

        return;
    }

    setEnabled(false);

    const WibblyJob &job = jobs[current_job];

    try {
        evaluateFinalScript(current_job);
    } catch (WobblyException &e) {
        errorPopup(e.what());
        return;
    }

    QString input_file = QString::fromStdString(job.getInputFile());
    if (settings_use_relative_paths_check->isChecked())
        input_file = QFileInfo(input_file).fileName();

    current_project = new WobblyProject(false, input_file.toStdString(), job.getSourceFilter(), vsvi->fpsNum, vsvi->fpsDen, vsvi->width, vsvi->height, vsvi->numFrames);

    auto trims = job.getTrims();
    for (auto it = trims.cbegin(); it != trims.cend(); it++)
        current_project->addTrim(it->second.first, it->second.last);

    if (!trims.size())
        current_project->addTrim(0, vsvi->numFrames - 1);

    int steps = job.getSteps();

    if (steps & StepFieldMatch) {
        for (size_t i = 0; i < vfm_params.size(); i++) {
            if (vfm_params[i].type == VIVTCParamInt) {
                current_project->setVFMParameter(vfm_params[i].name.toStdString(), job.getVFMParameterInt(vfm_params[i].name.toStdString()));
            } else if (vfm_params[i].type == VIVTCParamDouble) {
                current_project->setVFMParameter(vfm_params[i].name.toStdString(), job.getVFMParameterDouble(vfm_params[i].name.toStdString()));
            } else if (vfm_params[i].type == VIVTCParamBool) {
                current_project->setVFMParameter(vfm_params[i].name.toStdString(), (int)job.getVFMParameterBool(vfm_params[i].name.toStdString()));
            }
        }
    }

    if (steps & StepDecimation) {
        for (size_t i = 0; i < vdecimate_params.size(); i++) {
            if (vdecimate_params[i].type == VIVTCParamInt) {
                current_project->setVDecimateParameter(vdecimate_params[i].name.toStdString(), job.getVDecimateParameterInt(vdecimate_params[i].name.toStdString()));
            } else if (vdecimate_params[i].type == VIVTCParamDouble) {
                current_project->setVDecimateParameter(vdecimate_params[i].name.toStdString(), job.getVDecimateParameterDouble(vdecimate_params[i].name.toStdString()));
            } else if (vdecimate_params[i].type == VIVTCParamBool) {
                current_project->setVDecimateParameter(vdecimate_params[i].name.toStdString(), (int)job.getVDecimateParameterBool(vdecimate_params[i].name.toStdString()));
            }
        }
    }

    if (!(steps & StepFieldMatch || steps & StepInterlacedFades || steps & StepDecimation || steps & StepSceneChanges)) {
        // No metrics to collect. Just create the project file and move on.
        try {
            current_project->writeProject(job.getOutputFile(), settings_compact_projects_check->isChecked());

            delete current_project;
            current_project = nullptr;
        } catch (WobblyException &e) {
            errorPopup(e.what());

            delete current_project;
            current_project = nullptr;
        }

        QApplication::processEvents();

        // A little recursion, but surely there won't be enough jobs to make it a problem.
        startNextJob();

        return;
    }

    progress_dialog_label_text = QStringLiteral("Job %1/%2:\n%3").arg(current_job + 1).arg(jobs.size()).arg(QString::fromStdString(job.getOutputFile()));

    main_progress_dialog->setLabelText(progress_dialog_label_text + "\n\n");
    main_progress_dialog->setMinimum(0);
    main_progress_dialog->setMaximum(vsvi->numFrames);
    main_progress_dialog->setValue(0);

    const VSCoreInfo *info = vsapi->getCoreInfo(vscore);
    int requests = std::min(info->numThreads, vsvi->numFrames);

    aborted = false;

    frames_left = vsvi->numFrames;

    next_frame = 0;
    elapsed_timer.start();
    update_timer.start();
    for (int i = 0; i < requests; i++) {
        ++request_count;
        vsapi->getFrameAsync(next_frame, vsnode, frameDoneCallback, (void *)this);
        next_frame++;
    }
}


// Runs in the worker threads, so don't touch the GUI directly.
// The worker threads are queued up inside VapourSynth, so they run one at a time.
void WibblyWindow::frameDone(void *frame_v, int n, const QString &error_msg) {
    const VSFrameRef *frame = (const VSFrameRef *)frame_v;

    if (aborted) {
        vsapi->freeFrame(frame);
    } else {
        if (frame) {
            const VSMap *props = vsapi->getFramePropsRO(frame);

            int err;

            const char match_chars[] = { 'p', 'c', 'n', 'b', 'u' };
            int64_t match = vsapi->propGetInt(props, "VFMMatch", 0, &err);
            if (!err)
                current_project->setOriginalMatch(n, match_chars[match]);

            if (vsapi->propGetInt(props, "_Combed", 0, &err))
                current_project->addCombedFrame(n);

            if (vsapi->propNumElements(props, "VFMMics") == 5) {
                const int64_t *mics = vsapi->propGetIntArray(props, "VFMMics", &err);
                current_project->setMics(n, mics[0], mics[1], mics[2], mics[3], mics[4]);
            }

            if (vsapi->propGetInt(props, "_SceneChangePrev", 0, &err))
                current_project->addSection(n);

            int64_t decimate_metric = vsapi->propGetInt(props, "VDecimateMaxBlockDiff", 0, &err);
            if (!err)
                current_project->setDecimateMetric(n, decimate_metric);

            if (vsapi->propGetInt(props, "VDecimateDrop", 0, &err))
                current_project->addDecimatedFrame(n);

            double field_difference = vsapi->propGetFloat(props, "WibblyFieldDifference", 0, &err);
            if (field_difference > jobs[current_job].getFadesThreshold())
                current_project->addInterlacedFade(n, field_difference);

            vsapi->freeFrame(frame);

            if (next_frame < vsvi->numFrames) {
                ++request_count;
                vsapi->getFrameAsync(next_frame, vsnode, frameDoneCallback, (void *)this);
                next_frame++;
            }

            frames_left--;

            // Speed and time remaining updated every five seconds,
            // or as long as it takes to process a frames, whichever is larger.
            if (update_timer.elapsed() >= 5000) {
                update_timer.start();

                qint64 elapsed_milliseconds = elapsed_timer.elapsed();
                double frames_per_second = (double)(vsvi->numFrames - frames_left) * 1000 / elapsed_milliseconds;
                int seconds_left = (int)(frames_left / frames_per_second);
                int minutes_left = seconds_left / 60;
                seconds_left = seconds_left % 60;
                int hours_left = minutes_left / 60;
                minutes_left = minutes_left % 60;

                QMetaObject::invokeMethod(
                            main_progress_dialog,
                            "setLabelText",
                            Qt::QueuedConnection,
                            Q_ARG(QString, QStringLiteral("%1\n\n%2 fps, %3:%4:%5 to finish this job")
                                  .arg(progress_dialog_label_text)
                                  .arg(frames_per_second, 0, 'f', 2)
                                  .arg(hours_left, 2, 10, QLatin1Char('0'))
                                  .arg(minutes_left, 2, 10, QLatin1Char('0'))
                                  .arg(seconds_left, 2, 10, QLatin1Char('0'))));
            }

            QMetaObject::invokeMethod(
                        main_progress_dialog,
                        "setValue",
                        Qt::QueuedConnection,
                        Q_ARG(int, vsvi->numFrames - frames_left));

            if (frames_left == 0) {
                try {
                    current_project->resetRangeMatches(0, vsvi->numFrames - 1);

                    // If the project was successfully saved earlier, this will probably work.
                    current_project->writeProject(jobs[current_job].getOutputFile(), settings_compact_projects_check->isChecked());

                    delete current_project;
                    current_project = nullptr;

                    QMetaObject::invokeMethod(this, "startNextJob", Qt::QueuedConnection);
                } catch (WobblyException &e) {
                    QMetaObject::invokeMethod(this, "errorPopup", Qt::QueuedConnection, Q_ARG(QString, QString(e.what())));

                    aborted = true;
                    current_job = -1;
                    QMetaObject::invokeMethod(this, "setEnabled", Qt::QueuedConnection, Q_ARG(bool, true));

                    delete current_project;
                    current_project = nullptr;
                }
            }
        } else {
            aborted = true;

            delete current_project;
            current_project = nullptr;

            QMetaObject::invokeMethod(this, "errorPopup", Qt::QueuedConnection, Q_ARG(QString, QStringLiteral("Job number %1: failed to retrieve frame number %2. Error message:\n\n%3").arg(current_job).arg(n).arg(error_msg)));
        }
    }

    --request_count;

    if (request_count == 0) {
        std::lock_guard<std::mutex> lock(requests_mutex);
        requests_condition.notify_one();
    }
}


void WibblyWindow::readSettings() {
    if (settings.contains(KEY_STATE))
        restoreState(settings.value(KEY_STATE).toByteArray());

    if (settings.contains(KEY_GEOMETRY))
        restoreGeometry(settings.value(KEY_GEOMETRY).toByteArray());

    settings_font_spin->setValue(settings.value(KEY_FONT_SIZE, QApplication::font().pointSize()).toInt());

    settings_compact_projects_check->setChecked(settings.value(KEY_COMPACT_PROJECT_FILES, false).toBool());

    settings_use_relative_paths_check->setChecked(settings.value(KEY_USE_RELATIVE_PATHS, false).toBool());

    if (settings.contains(KEY_MAXIMUM_CACHE_SIZE))
        settings_cache_spin->setValue(settings.value(KEY_MAXIMUM_CACHE_SIZE).toInt());
}


void WibblyWindow::writeSettings() {
    settings.setValue(KEY_STATE, saveState());

    settings.setValue(KEY_GEOMETRY, saveGeometry());
}


void WibblyWindow::readJobs() {
    int job_count = settings.value(KEY_COUNT, 0).toInt();

    if (!job_count)
        return;

    jobs.resize(job_count);

    int field_width = QString::number(jobs.size() - 1).size();

    for (auto job = jobs.begin(); job != jobs.end(); job++) {
        QString key = KEY_JOB.arg(std::distance(jobs.begin(), job), field_width, 10, QLatin1Char('0'));

        job->setInputFile(settings.value(key + KEY_INPUT_FILE).toString().toStdString());

        job->setSourceFilter(settings.value(key + KEY_SOURCE_FILTER).toString().toStdString());

        job->setOutputFile(settings.value(key + KEY_OUTPUT_FILE).toString().toStdString());

        job->setSteps(settings.value(key + KEY_STEPS).toInt());

        QList<QVariant> crop_list = settings.value(key + KEY_CROP).toList();
        if (crop_list.size() == 4)
            job->setCrop(crop_list[0].toInt(), crop_list[1].toInt(), crop_list[2].toInt(), crop_list[3].toInt());

        QList<QVariant> trim_list = settings.value(key + KEY_TRIMS).toList();
        for (int i = 0; i < trim_list.size(); i += 2)
            job->addTrim(trim_list[i].toInt(), trim_list[i + 1].toInt());

        for (auto param = vfm_params.cbegin(); param != vfm_params.cend(); param++) {
            if (param->type == VIVTCParamInt)
                job->setVFMParameter(param->name.toStdString(), settings.value(key + KEY_VFM + param->name).toInt());
            else if (param->type == VIVTCParamDouble)
                job->setVFMParameter(param->name.toStdString(), settings.value(key + KEY_VFM + param->name).toDouble());
            else if (param->type == VIVTCParamBool)
                job->setVFMParameter(param->name.toStdString(), settings.value(key + KEY_VFM + param->name).toBool());
        }

        for (auto param = vdecimate_params.cbegin(); param != vdecimate_params.cend(); param++) {
            if (param->type == VIVTCParamInt)
                job->setVDecimateParameter(param->name.toStdString(), settings.value(key + KEY_VDECIMATE + param->name).toInt());
            else if (param->type == VIVTCParamDouble)
                job->setVDecimateParameter(param->name.toStdString(), settings.value(key + KEY_VDECIMATE + param->name).toDouble());
            else if (param->type == VIVTCParamBool)
                job->setVDecimateParameter(param->name.toStdString(), settings.value(key + KEY_VDECIMATE + param->name).toBool());
        }

        job->setFadesThreshold(settings.value(key + KEY_FADES_THRESHOLD).toDouble());

        main_jobs_list->addItem(QString::fromStdString(job->getInputFile()));
    }

    main_jobs_list->setCurrentRow(0);
}


void WibblyWindow::writeJobs() {
    settings.remove(KEY_JOBS);

    if (!jobs.size())
        return;

    settings.setValue(KEY_COUNT, (int)jobs.size());

    int field_width = QString::number(jobs.size() - 1).size();

    for (auto job = jobs.cbegin(); job != jobs.cend(); job++) {
        QString key = KEY_JOB.arg(std::distance(jobs.cbegin(), job), field_width, 10, QLatin1Char('0'));

        settings.setValue(key + KEY_INPUT_FILE, QString::fromStdString(job->getInputFile()));

        settings.setValue(key + KEY_SOURCE_FILTER, QString::fromStdString(job->getSourceFilter()));

        settings.setValue(key + KEY_OUTPUT_FILE, QString::fromStdString(job->getOutputFile()));

        settings.setValue(key + KEY_STEPS, job->getSteps());

        const Crop &crop = job->getCrop();
        QList<QVariant> crop_list = { crop.left, crop.top, crop.right, crop.bottom };
        settings.setValue(key + KEY_CROP, crop_list);

        const auto &trims = job->getTrims();
        QList<QVariant> trim_list;
        trim_list.reserve(trims.size() * 2);
        for (auto it = trims.cbegin(); it != trims.cend(); it++) {
            trim_list.push_back(it->second.first);
            trim_list.push_back(it->second.last);
        }
        settings.setValue(key + KEY_TRIMS, trim_list);

        for (auto param = vfm_params.cbegin(); param != vfm_params.cend(); param++) {
            if (param->type == VIVTCParamInt)
                settings.setValue(key + KEY_VFM + param->name, job->getVFMParameterInt(param->name.toStdString()));
            else if (param->type == VIVTCParamDouble)
                settings.setValue(key + KEY_VFM + param->name, job->getVFMParameterDouble(param->name.toStdString()));
            else if (param->type == VIVTCParamBool)
                settings.setValue(key + KEY_VFM + param->name, job->getVFMParameterBool(param->name.toStdString()));
        }

        for (auto param = vdecimate_params.cbegin(); param != vdecimate_params.cend(); param++) {
            if (param->type == VIVTCParamInt)
                settings.setValue(key + KEY_VDECIMATE + param->name, job->getVDecimateParameterInt(param->name.toStdString()));
            else if (param->type == VIVTCParamDouble)
                settings.setValue(key + KEY_VDECIMATE + param->name, job->getVDecimateParameterDouble(param->name.toStdString()));
            else if (param->type == VIVTCParamBool)
                settings.setValue(key + KEY_VDECIMATE + param->name, job->getVDecimateParameterBool(param->name.toStdString()));
        }

        settings.setValue(key + KEY_FADES_THRESHOLD, job->getFadesThreshold());
    }
}


void WibblyWindow::jumpRelative(int offset) {
    int target = current_frame + offset;

    displayFrame(target);
}


void WibblyWindow::jump1Backward() {
    jumpRelative(-1);
}


void WibblyWindow::jump1Forward() {
    jumpRelative(1);
}


void WibblyWindow::jump5Backward() {
    jumpRelative(-5);
}


void WibblyWindow::jump5Forward() {
    jumpRelative(5);
}


void WibblyWindow::jump50Backward() {
    jumpRelative(-50);
}


void WibblyWindow::jump50Forward() {
    jumpRelative(50);
}


void WibblyWindow::jumpALotBackward() {
    int twenty_percent = vsvi->numFrames * 20 / 100;

    jumpRelative(-twenty_percent);
}


void WibblyWindow::jumpALotForward() {
    int twenty_percent = vsvi->numFrames * 20 / 100;

    jumpRelative(twenty_percent);
}


void WibblyWindow::jumpToStart() {
    jumpRelative(0 - current_frame);
}


void WibblyWindow::jumpToEnd() {
    jumpRelative(vsvi->numFrames - current_frame);
}


void WibblyWindow::selectPreviousJob() {
    if (!main_jobs_list->count())
        return;

    int current_row = main_jobs_list->currentRow();
    if (current_row > 0)
        main_jobs_list->setCurrentRow(current_row - 1);
}


void WibblyWindow::selectNextJob() {
    int count = main_jobs_list->count();
    if (!count)
        return;

    int current_row = main_jobs_list->currentRow();
    if (current_row < count - 1)
        main_jobs_list->setCurrentRow(current_row + 1);
}


void WibblyWindow::startTrim() {
    trim_start = current_frame;

    trim_start_label->setText(QStringLiteral("Start: %1").arg(trim_start));
}


void WibblyWindow::endTrim() {
    trim_end = current_frame;

    trim_end_label->setText(QStringLiteral("End: %1").arg(trim_end));
}


void WibblyWindow::addTrim() {
    if (trim_start == -1 || trim_end == -1)
        return;

    auto selection = main_jobs_list->selectedItems();

    if (!selection.size())
        return;

    if (trim_start > trim_end)
        std::swap(trim_start, trim_end);

    for (int i = 0; i < selection.size(); i++) {
        int row = main_jobs_list->row(selection[i]);

        try {
            jobs[row].addTrim(trim_start, trim_end);
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }

    int current_row = main_jobs_list->currentRow();
    main_jobs_list->setCurrentRow(-1, QItemSelectionModel::NoUpdate);
    main_jobs_list->setCurrentRow(current_row, QItemSelectionModel::NoUpdate);

    trim_start = trim_end = -1;
    trim_start_label->clear();
    trim_end_label->clear();
}
