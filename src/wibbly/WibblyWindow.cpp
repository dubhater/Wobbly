#include <QButtonGroup>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimeEdit>

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <VSScript.h>

#include "WibblyWindow.h"
#include "WobblyException.h"


WibblyWindow::WibblyWindow()
    : QMainWindow()
    , current_frame(0)
    , trim_start(-1)
    , trim_end(-1)
{
    createUI();
}


void WibblyWindow::createUI() {
    setWindowTitle(QStringLiteral("Wibbly Metrics Collector v%1").arg(PACKAGE_VERSION));

    statusBar()->setSizeGripEnabled(true);

    createMainWindow();
    createVideoOutputWindow();
    createCropWindow();
    createVFMWindow();
    createTrimWindow();
    createInterlacedFadesWindow();
}


void WibblyWindow::createMenus() {
    QMenuBar *bar = menuBar();

    menu_menu = bar->addMenu("&Menu");

    QAction *quit_action = new QAction("&Quit", this);
    quit_action->setShortcut(QKeySequence("Ctrl+Q"));

    connect(quit_action, &QAction::triggered, this, &WibblyWindow::close);

    menu_menu->addSeparator();
    menu_menu->addAction(quit_action);
}


void WibblyWindow::createMainWindow() {
    createMenus();


    main_jobs_list = new ListWidget;
    main_jobs_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    main_jobs_list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QLineEdit *main_destination_edit = new QLineEdit;

    QPushButton *main_choose_button = new QPushButton("Choose");

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

    main_progress_bar = new QProgressBar;

    QPushButton *main_engage_button = new QPushButton("Engage");
    QPushButton *main_cancel_button = new QPushButton("Cancel");


    connect(main_jobs_list, &ListWidget::currentRowChanged, [this, main_destination_edit, main_steps_buttons, steps] (int currentRow) {
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
            crop_spin[i]->blockSignals(true);
            crop_spin[i]->setValue(crop_values[i]);
            crop_spin[i]->blockSignals(false);
        }

        fades_threshold_spin->blockSignals(true);
        fades_threshold_spin->setValue(job.getFadesThreshold());
        fades_threshold_spin->blockSignals(false);

        for (size_t i = 0; i < vfm_params.size(); i++) {
            if (vfm_params[i].type == VFMParamInt) {
                QSpinBox *spin = reinterpret_cast<QSpinBox *>(vfm_params[i].widget);
                spin->blockSignals(true);
                spin->setValue(job.getVFMParameterInt(vfm_params[i].name.toStdString()));
                spin->blockSignals(false);
            } else if (vfm_params[i].type == VFMParamDouble) {
                QDoubleSpinBox *spin = reinterpret_cast<QDoubleSpinBox *>(vfm_params[i].widget);
                spin->blockSignals(true);
                spin->setValue(job.getVFMParameterDouble(vfm_params[i].name.toStdString()));
                spin->blockSignals(false);
            } else if (vfm_params[i].type == VFMParamBool) {
                QCheckBox *check = reinterpret_cast<QCheckBox *>(vfm_params[i].widget);
                check->setChecked(job.getVFMParameterBool(vfm_params[i].name.toStdString()));
            }
        }
    });

    connect(main_jobs_list, &ListWidget::deletePressed, main_remove_jobs_button, &QPushButton::click);

    auto destinationChanged = [this, main_destination_edit] () {
        auto selection = main_jobs_list->selectedItems();

        for (int i = 0; i < selection.size(); i++) {
            int row = main_jobs_list->row(selection[i]);

            jobs[row].setOutputFile(main_destination_edit->text().toStdString());
        }
    };

    connect(main_destination_edit, &QLineEdit::editingFinished, destinationChanged);

    connect(main_choose_button, &QPushButton::clicked, [this, destinationChanged] () {
        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Choose destination"), QString(), QStringLiteral("Wobbly projects (*.json);;All files (*)"), nullptr, QFileDialog::DontUseNativeDialog);

        if (!path.isEmpty())
            destinationChanged();
    });

    connect(main_add_jobs_button, &QPushButton::clicked, [this] () {
        QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("Open video file"), QString(), QString(), nullptr, QFileDialog::DontUseNativeDialog);

        paths.sort();

        for (int i = 0; i < paths.size(); i++)
            if (!paths[i].isNull())
                realOpenVideo(paths[i]);
    });

    connect(main_remove_jobs_button, &QPushButton::clicked, [this] () {
        auto selection = main_jobs_list->selectedItems();

        for (int i = selection.size() - 1; i >= 0; i--) {
            int row = main_jobs_list->row(selection[i]);

            jobs.erase(jobs.cbegin() + row);
            delete main_jobs_list->takeItem(row);
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
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(main_jobs_list, 1);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(new QLabel("Destination:"));
    hbox->addWidget(main_destination_edit);
    hbox->addWidget(main_choose_button);
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
    hbox->addWidget(main_progress_bar);
    hbox->addWidget(main_engage_button);
    hbox->addWidget(main_cancel_button);

    vbox->addLayout(hbox);


    QWidget *main_widget = new QWidget;
    main_widget->setLayout(vbox);

    setCentralWidget(main_widget);
}


void WibblyWindow::createVideoOutputWindow() {
    video_frame_label = new QLabel;
    video_frame_label->setMinimumSize(720, 480);

    QScrollArea *video_frame_scroll = new QScrollArea;
    video_frame_scroll->setFrameShape(QFrame::NoFrame);
    video_frame_scroll->setFocusPolicy(Qt::NoFocus);
    video_frame_scroll->setAlignment(Qt::AlignCenter);
    video_frame_scroll->setWidgetResizable(true);
    video_frame_scroll->setWidget(video_frame_label);

    QSpinBox *video_frame_spin = new QSpinBox;
    QTimeEdit *video_time_edit = new QTimeEdit;
    video_time_edit->setDisplayFormat(QStringLiteral("hh:mm:ss.zzz"));

    video_frame_slider = new QSlider(Qt::Horizontal);
    video_frame_slider->setTracking(false);
    video_frame_slider->setFocusPolicy(Qt::NoFocus);


    // connect


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
        { nullptr, "order",      0,          1, VFMParamInt },
        { nullptr, "mchroma",    0,          1, VFMParamBool },
        { nullptr, "cthresh",    1,        255, VFMParamInt },
        { nullptr, "mi",         0,    INT_MAX, VFMParamInt },
        { nullptr, "chroma",     0,          1, VFMParamBool },
        { nullptr, "blockx",     4,        512, VFMParamInt },
        { nullptr, "blocky",     4,        512, VFMParamInt },
        { nullptr, "y0",         0,    INT_MAX, VFMParamInt },
        { nullptr, "y1",         0,    INT_MAX, VFMParamInt },
        { nullptr, "scthresh",   0,        100, VFMParamDouble },
        { nullptr, "micmatch",   0,          2, VFMParamInt }
    };

    auto parametersChanged = [this] () {
        auto selection = main_jobs_list->selectedItems();

        for (int i = 0; i < selection.size(); i++) {
            int row = main_jobs_list->row(selection[i]);

            WibblyJob &job = jobs[row];

            for (size_t j = 0; j < vfm_params.size(); j++) {
                if (vfm_params[j].type == VFMParamInt) {
                    job.setVFMParameter(vfm_params[j].name.toStdString(), reinterpret_cast<QSpinBox *>(vfm_params[j].widget)->value());
                } else if (vfm_params[j].type == VFMParamDouble) {
                    job.setVFMParameter(vfm_params[j].name.toStdString(), reinterpret_cast<QDoubleSpinBox *>(vfm_params[j].widget)->value());
                } else if (vfm_params[j].type == VFMParamBool) {
                    job.setVFMParameter(vfm_params[j].name.toStdString(), reinterpret_cast<QCheckBox *>(vfm_params[j].widget)->isChecked());
                }
            }
        }
    };

    for (size_t i = 0; i < vfm_params.size(); i++) {
        if (vfm_params[i].type == VFMParamInt) {
            QSpinBox *spin = new QSpinBox;
            spin->setPrefix(vfm_params[i].name + ": ");
            spin->setMinimum(vfm_params[i].minimum);
            if (vfm_params[i].maximum != INT_MAX)
                spin->setMaximum(vfm_params[i].maximum);

            connect(spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), parametersChanged);

            vfm_params[i].widget = spin;
        } else if (vfm_params[i].type == VFMParamDouble) {
            QDoubleSpinBox *spin = new QDoubleSpinBox;
            spin->setPrefix(vfm_params[i].name + ": ");
            spin->setMaximum(vfm_params[i].maximum);

            connect(spin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), parametersChanged);

            vfm_params[i].widget = spin;
        } else if (vfm_params[i].type == VFMParamBool) {
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


void WibblyWindow::createTrimWindow() {
    trim_ranges_list = new ListWidget;
    trim_ranges_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trim_ranges_list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QLabel *trim_start_label = new QLabel;
    QLabel *trim_end_label = new QLabel;

    QPushButton *trim_start_button = new QPushButton("Start trim");
    QPushButton *trim_end_button = new QPushButton("End trim");
    QPushButton *trim_add_button = new QPushButton("Add trim");
    QPushButton *trim_delete_button = new QPushButton("Delete trim");


    connect(trim_ranges_list, &ListWidget::deletePressed, trim_delete_button, &QPushButton::click);

    // double click on a range

    connect(trim_start_button, &QPushButton::clicked, [this, trim_start_label] () {
        trim_start = current_frame;

        trim_start_label->setText(QStringLiteral("Start: %1").arg(trim_start));
    });

    connect(trim_end_button, &QPushButton::clicked, [this, trim_end_label] () {
        trim_end = current_frame;

        trim_end_label->setText(QStringLiteral("End: %1").arg(trim_end));
    });

    connect(trim_add_button, &QPushButton::clicked, [this, trim_start_label, trim_end_label] () {
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

        int current_job = main_jobs_list->currentRow();
        main_jobs_list->setCurrentRow(-1, QItemSelectionModel::NoUpdate);
        main_jobs_list->setCurrentRow(current_job, QItemSelectionModel::NoUpdate);

        trim_start = trim_end = -1;
        trim_start_label->clear();
        trim_end_label->clear();
    });

    connect(trim_delete_button, &QPushButton::clicked, [this] () {
        auto job_selection = main_jobs_list->selectedItems();

        if (!job_selection.size())
            return;

        auto trim_selection = trim_ranges_list->selectedItems();

        if (!trim_selection.size())
            return;

        for (int i = 0; i < job_selection.size(); i++) {
            for (int j = 0; j < trim_selection.size(); j++) {
                jobs[i].deleteTrim(trim_selection[j]->data(Qt::UserRole).toInt());
            }
        }

        int current_job = main_jobs_list->currentRow();
        main_jobs_list->setCurrentRow(-1, QItemSelectionModel::NoUpdate);
        main_jobs_list->setCurrentRow(current_job, QItemSelectionModel::NoUpdate);
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
    fades_threshold_spin->setMaximum(255);


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


void WibblyWindow::realOpenVideo(const QString &path) {
    QString source_filter;

    QString extension = path.mid(path.lastIndexOf('.') + 1);

    QStringList mp4 = { "mp4", "m4v", "mov" };

    if (extension == "d2v")
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


void WibblyWindow::errorPopup(const char *msg) {
    QMessageBox::information(this, QStringLiteral("Error"), msg);
}
