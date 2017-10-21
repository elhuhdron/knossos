/*
 *  This file is a part of KNOSSOS.
 *
 *  (C) Copyright 2007-2016
 *  Max-Planck-Gesellschaft zur Foerderung der Wissenschaften e.V.
 *
 *  KNOSSOS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 of
 *  the License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *  For further information, visit https://knossostool.org
 *  or contact knossos-team@mpimf-heidelberg.mpg.de
 */

#include "file_io.h"
#include "GuiConstants.h"
#include "htmlmacros.h"
#include "loader.h"
#include "mainwindow.h"
#include "network.h"
#include "scriptengine/scripting.h"
#include "skeleton/node.h"
#include "skeleton/skeleton_dfs.h"
#include "skeleton/skeletonizer.h"
#include "stateInfo.h"
#include "version.h"
#include "viewer.h"
#include "viewports/viewportbase.h"
#include "widgetcontainer.h"

#include <PythonQt/PythonQt.h>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPropertyAnimation>
#include <QRegExp>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringList>
#include <QToolButton>

class CoordinateSpin : public QSpinBox {
public:
    CoordinateSpin(const QString & prefix, MainWindow & mainWindow) : QSpinBox(&mainWindow) {
        setPrefix(prefix);
        setRange(0, 1000000);//allow min 0 as bogus value, we don’t adjust the max anyway
        setValue(0 + 1);//inintialize for {0, 0, 0}
        QObject::connect(this, &QSpinBox::editingFinished, &mainWindow, &MainWindow::coordinateEditingFinished);
    }
    virtual void fixup(QString & input) const override {
        input = QString::number(0);//let viewer reset the value
    }
};

template<typename Menu, typename Receiver, typename Slot>
QAction & addApplicationShortcut(Menu & menu, const QIcon & icon, const QString & caption, const Receiver * receiver, const Slot slot, const QKeySequence & keySequence) {
    auto & action = *menu.addAction(icon, caption);
    action.setShortcut(keySequence);
    action.setShortcutContext(Qt::ApplicationShortcut);
    QObject::connect(&action, &QAction::triggered, receiver, slot);
    return action;
}

MainWindow::MainWindow(QWidget * parent) : QMainWindow{parent}, evilHack{[this](){ state->mainWindow = this; return true; }()}, widgetContainer{this} {// make state->mainWindow available to widgets
    updateTitlebar();
    this->setWindowIcon(QIcon(":/resources/icons/logo.ico"));

    skeletonFileHistory.reserve(FILE_DIALOG_HISTORY_MAX_ENTRIES);
    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::resetData, [this]() { setSynapseState(SynapseState::Off); });
    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::guiModeLoaded, [this]() { setProofReadingUI(Session::singleton().guiMode == GUIMode::ProofReading); });
    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::lockedToNode, [this](const std::uint64_t nodeID) {
        nodeLockingLabel.setText(tr("Locked to node %1").arg(nodeID));
        nodeLockingLabel.show();
    });
    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::unlockedNode, [this]() { nodeLockingLabel.hide(); });
    QObject::connect(&widgetContainer.datasetLoadWidget, &DatasetLoadWidget::datasetChanged, [this](bool showOverlays) {
        const auto currentMode = workModeModel.at(modeCombo.currentIndex()).first;
        std::map<AnnotationMode, QString> rawModes = workModes;
        AnnotationMode defaultMode = AnnotationMode::Mode_Tracing;
        if (!showOverlays) {
             rawModes = {{AnnotationMode::Mode_Tracing, workModes[AnnotationMode::Mode_Tracing]}, {AnnotationMode::Mode_TracingAdvanced, workModes[AnnotationMode::Mode_TracingAdvanced]}};
        }
        else if (Session::singleton().guiMode == GUIMode::ProofReading) {
            rawModes = {{AnnotationMode::Mode_Merge, workModes[AnnotationMode::Mode_Merge]}, {AnnotationMode::Mode_Paint, workModes[AnnotationMode::Mode_Paint]}};
            defaultMode = AnnotationMode::Mode_Merge;
        }
        workModeModel.recreate(rawModes);
        setWorkMode((rawModes.find(currentMode) != std::end(rawModes))? currentMode : defaultMode);

        widgetContainer.annotationWidget.setSegmentationVisibility(showOverlays);
    });
    QObject::connect(&widgetContainer.datasetLoadWidget, &DatasetLoadWidget::updateDatasetCompression,  this, &MainWindow::updateCompressionRatioDisplay);
    QObject::connect(&widgetContainer.snapshotWidget, &SnapshotWidget::snapshotVpSizeRequest, [this](SnapshotOptions & o) { viewport(o.vp)->takeSnapshotVpSize(o); });
    QObject::connect(&widgetContainer.snapshotWidget, &SnapshotWidget::snapshotDatasetSizeRequest, [this](SnapshotOptions & o) { viewport(o.vp)->takeSnapshotDatasetSize(o); });
    QObject::connect(&widgetContainer.snapshotWidget, &SnapshotWidget::snapshotRequest, [this](const SnapshotOptions & o) { viewport(o.vp)->takeSnapshot(o); });
    QObject::connect(&Segmentation::singleton(), &Segmentation::appendedRow, this, &MainWindow::notifyUnsavedChanges);
    QObject::connect(&Segmentation::singleton(), &Segmentation::changedRow, this, &MainWindow::notifyUnsavedChanges);
    QObject::connect(&Segmentation::singleton(), &Segmentation::removedRow, this, &MainWindow::notifyUnsavedChanges);
    QObject::connect(&Segmentation::singleton(), &Segmentation::todosLeftChanged, this, &MainWindow::updateTodosLeft);

    QObject::connect(&Session::singleton(), &Session::autoSaveSignal, [this](){ save(); });

    createToolbars();
    createMenus();
    setCentralWidget(new QWidget(this));

    createViewports();
    setAcceptDrops(true);

    statusBar()->setSizeGripEnabled(false);
    GUIModeLabel.setVisible(false);
    statusBar()->addWidget(&GUIModeLabel);
    networkProgressBar.setVisible(false);
    networkProgressBar.setToolTip("Network progress");
    networkProgressBar.setTextVisible(false); // under windows percentage is shown next to progress bar (instead of on top of it) and is not visible on the dark status bar.
    networkProgressAbortButton.setVisible(false);
    networkProgressAbortButton.setToolTip("Abort network operation");
    cursorPositionLabel.setVisible(false);
    QObject::connect(&Network::singleton(), &Network::startedNetworkRequest, [this](QNetworkReply &
                 #ifndef Q_OS_UNIX
                     reply
                 #endif
                     ) {
        networkProgressBar.setVisible(true);
    #ifndef Q_OS_UNIX // On Unix QNetworkReply::abort() crashes…
        networkProgressAbortButton.setVisible(true);
        QObject::connect(&networkProgressAbortButton, &QPushButton::clicked, &reply, &QNetworkReply::abort);
    #endif
    });
    QObject::connect(&Network::singleton(), &Network::finishedNetworkRequest, [this]() {
        networkProgressBar.setValue(0);// prevent (visible) resets when a new max is set
        networkProgressBar.setHidden(true);
        networkProgressAbortButton.setHidden(true);
    });
    QObject::connect(&Network::singleton(), &Network::progressChanged, [this](int bytesFinished, int bytesTotal) {
        networkProgressBar.setMaximum(bytesTotal);
        networkProgressBar.setValue(bytesFinished);
    });
    statusBar()->addWidget(&networkProgressBar);
    statusBar()->addWidget(&networkProgressAbortButton);
    statusBar()->addWidget(&cursorPositionLabel);

    activityAnimation.addAnimation(new QPropertyAnimation(&activityLabel, "minimumHeight"));
    activityAnimation.addAnimation(new QPropertyAnimation(&activityLabel, "maximumHeight"));
    for (int i = 0; i < activityAnimation.animationCount(); ++i) {
        QPropertyAnimation * animation = static_cast<QPropertyAnimation *>(activityAnimation.animationAt(i));
        animation->setDuration(500);
        animation->setStartValue(0);
        animation->setEndValue(activityLabel.sizeHint().height());
    }

    QObject::connect(&activityTimer, &QTimer::timeout, [this]() { activityAnimation.start(); });
    QObject::connect(&activityAnimation, &QParallelAnimationGroup::finished, [this]() {
        if (activityAnimation.direction() == QAbstractAnimation::Forward) {
            activityAnimation.setDirection(QAbstractAnimation::Backward);
            activityTimer.setSingleShot(true);
            activityTimer.start(1000);
        } else {
            activityLabel.setVisible(false);
        }
    });
    activityLabel.setVisible(false);
    statusBar()->addWidget(&activityLabel);

    nodeLockingLabel.setVisible(false);
    statusBar()->addPermanentWidget(&nodeLockingLabel);
    synapseStateLabel.setVisible(false);
    statusBar()->addPermanentWidget(&synapseStateLabel);
    segmentStateLabel.setToolTip("Press [a] once: Next placed node will not be connected to the previous one.\n"
                                 "Press [a] twice: Segments are disabled entirely.\n"
                                 "Press [a] thrice: Segments are turned back on.");
    statusBar()->addPermanentWidget(&segmentStateLabel);
    statusBar()->addPermanentWidget(&unsavedChangesLabel);
    statusBar()->addPermanentWidget(&annotationTimeLabel);

    QObject::connect(&Session::singleton(), &Session::annotationTimeChanged, &annotationTimeLabel, &QLabel::setText);

    createGlobalAction(state->mainWindow, Qt::Key_F7, [](){
        state->viewer->gpuRendering = !state->viewer->gpuRendering;
    });

    addDockWidget(Qt::RightDockWidgetArea, &cheatsheet);
    QObject::connect(&cheatsheet, &Cheatsheet::anchorClicked, [this](const QUrl & link) {
        auto anchor = link.toString();
        auto & annoWidget = widgetContainer.annotationWidget;
        auto & prefWidget = widgetContainer.preferencesWidget;
        if(anchor.endsWith(ANNOTATION_SEG)) {
            annoWidget.tabs.setCurrentIndex(1); annoWidget.show(); annoWidget.raise();
        } else if (anchor.endsWith(ANNOTATION_SKEL)) {
            annoWidget.tabs.setCurrentIndex(0); annoWidget.show(); annoWidget.raise();
        } else if(anchor.endsWith(PREF_NODE)) {
            prefWidget.tabs.setCurrentIndex(1); prefWidget.show(); prefWidget.raise();
        } else if(anchor.endsWith(PREF_SEG)) {
            prefWidget.tabs.setCurrentIndex(2); prefWidget.show(); prefWidget.raise();
        } else if(anchor.endsWith(PREF_TREE)) {
            prefWidget.tabs.setCurrentIndex(0); prefWidget.show(); prefWidget.raise();
        } else if(anchor.endsWith(NAVIGATION)) {
            prefWidget.tabs.setCurrentIndex(5); prefWidget.show(); prefWidget.raise();
        }
    });
}

void MainWindow::updateCursorLabel(const Coordinate & position, const ViewportType vpType) {
    cursorPositionLabel.setHidden(vpType == VIEWPORT_SKELETON || vpType == VIEWPORT_UNDEFINED);
    cursorPositionLabel.setText(QString("%1, %2, %3").arg(position.x + 1).arg(position.y + 1).arg(position.z + 1));
}

void MainWindow::resetTextureProperties() {
    //reset viewerState texture properties
    forEachOrthoVPDo([](ViewportOrtho & orthoVP) {
        orthoVP.texture.size = state->viewerState->texEdgeLength;
        orthoVP.texture.texUnitsPerDataPx = (1.0 / orthoVP.texture.size) / Dataset::current().magnification;
        orthoVP.texture.FOV = 1;
        orthoVP.texture.usedSizeInCubePixels = (state->M - 1) * Dataset::current().cubeEdgeLength;
        if (orthoVP.viewportType == VIEWPORT_ARBITRARY) {
            orthoVP.texture.usedSizeInCubePixels /= std::sqrt(2);
        }
    });
}

void MainWindow::createViewports() {
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setMajorVersion(2);
    format.setMinorVersion(0);
    format.setSamples(state->viewerState->sampleBuffers);
    format.setDepthBufferSize(24);
//    format.setSwapInterval(0);
//    format.setSwapBehavior(QSurfaceFormat::SingleBuffer);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setOption(QSurfaceFormat::DeprecatedFunctions);
    if (ViewportBase::oglDebug) {
        format.setOption(QSurfaceFormat::DebugContext);
    }
    QSurfaceFormat::setDefaultFormat(format);

    viewportXY = std::unique_ptr<ViewportOrtho>(new ViewportOrtho(centralWidget(), VIEWPORT_XY));
    viewportXZ = std::unique_ptr<ViewportOrtho>(new ViewportOrtho(centralWidget(), VIEWPORT_XZ));
    viewportZY = std::unique_ptr<ViewportOrtho>(new ViewportOrtho(centralWidget(), VIEWPORT_ZY));
    viewportArb = std::unique_ptr<ViewportArb>(new ViewportArb(centralWidget(), VIEWPORT_ARBITRARY));
    viewport3D = std::unique_ptr<Viewport3D>(new Viewport3D(centralWidget(), VIEWPORT_SKELETON));
    state->viewer->viewportXY = viewportXY.get();
    state->viewer->viewportXZ = viewportXZ.get();
    state->viewer->viewportZY = viewportZY.get();
    state->viewer->viewportArb = viewportArb.get();
    resetTextureProperties();
    forEachVPDo([this](ViewportBase & vp) {
        QObject::connect(&vp, &ViewportBase::cursorPositionChanged, this, &MainWindow::updateCursorLabel);
        QObject::connect(&vp, &ViewportBase::snapshotTriggered, &this->widgetContainer.snapshotWidget, &SnapshotWidget::openForVP);
        QObject::connect(&vp, &ViewportBase::snapshotFinished, &widgetContainer.snapshotWidget, &SnapshotWidget::showViewer);
    });
}

ViewportBase * MainWindow::viewport(const ViewportType vpType) {
    return (viewportXY->viewportType == vpType)? static_cast<ViewportBase *>(viewportXY.get()) :
           (viewportXZ->viewportType == vpType)? static_cast<ViewportBase *>(viewportXZ.get()) :
           (viewportZY->viewportType == vpType)? static_cast<ViewportBase *>(viewportZY.get()) :
           (viewportArb->viewportType == vpType)? static_cast<ViewportBase *>(viewportArb.get()) :
           static_cast<ViewportBase *>(viewport3D.get());
}

ViewportOrtho * MainWindow::viewportOrtho(const ViewportType vpType) {
    return (viewportXY->viewportType == vpType)? viewportXY.get() :
           (viewportXZ->viewportType == vpType)? viewportXZ.get() :
           (viewportZY->viewportType == vpType)? viewportZY.get() :
           viewportArb.get();
}

void MainWindow::createToolbars() {
    basicToolbar.setObjectName(basicToolbar.windowTitle()); // unique name for QMainWindow::saveState()/restoreState()
    defaultToolbar.setObjectName(defaultToolbar.windowTitle()); // unique name for QMainWindow::saveState()/restoreState()

    basicToolbar.setMovable(false);
    basicToolbar.setFloatable(false);
    basicToolbar.setIconSize(QSize(24, 24));

    basicToolbar.addAction(QIcon(":/resources/icons/open-annotation.png"), "Open Annotation", this, SLOT(openSlot()));
    basicToolbar.addAction(QIcon(":/resources/icons/save-annotation.png"), "Save Annotation", this, SLOT(saveSlot()));
    basicToolbar.addSeparator();
    workModeModel.recreate(workModes);
    modeCombo.setModel(&workModeModel);
    modeCombo.setCurrentIndex(static_cast<int>(AnnotationMode::Mode_Tracing));
    basicToolbar.addWidget(&modeCombo);
    QObject::connect(&modeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this](int index) { setWorkMode(workModeModel.at(index).first); });
    modeCombo.setToolTip("<b>Select a work mode:</b><br/>"
                         "<b>" + workModes[AnnotationMode::Mode_MergeTracing] + ":</b> Merge segmentation objects by tracing<br/>"
                         "<b>" + workModes[AnnotationMode::Mode_Selection] + ":</b> Skeleton manipulation and segmentation selection<br/>"
                         "<b>" + workModes[AnnotationMode::Mode_Merge] + ":</b> Segmentation by merging objects<br/>"
                         "<b>" + workModes[AnnotationMode::Mode_Paint] + ":</b> Segmentation by painting<br/>"
                         "<b>" + workModes[AnnotationMode::Mode_Tracing] + ":</b> Skeletonization on one tree<br/>"
                         "<b>" + workModes[AnnotationMode::Mode_TracingAdvanced] + ":</b> Unrestricted skeletonization<br/>");
    basicToolbar.addSeparator();
    basicToolbar.addAction(QIcon(":/resources/icons/edit-copy.png"), "Copy", this, SLOT(copyClipboardCoordinates()));
    basicToolbar.addAction(QIcon(":/resources/icons/edit-paste.png"), "Paste", this, SLOT(pasteClipboardCoordinates()));

    xField = new CoordinateSpin("x: ", *this);
    yField = new CoordinateSpin("y: ", *this);
    zField = new CoordinateSpin("z: ", *this);

    basicToolbar.addWidget(xField);
    basicToolbar.addWidget(yField);
    basicToolbar.addWidget(zField);

    defaultToolbar.setIconSize(QSize(24, 24));

    addToolBar(&basicToolbar);
    addToolBar(&defaultToolbar);

    auto createToolToggleButton = [&](auto && widget, const QString & icon, const QString & tooltip){
        auto button = new QToolButton();
        button->setIcon(QIcon(icon));
        button->setToolTip(tooltip);
        button->setCheckable(true);
        defaultToolbar.addWidget(button);
        QObject::connect(button, &QToolButton::clicked, &widget, &std::remove_reference<decltype(widget)>::type::setVisible);// button → visibility
        QObject::connect(&widget, &std::remove_reference<decltype(widget)>::type::visibilityChanged, button, &QToolButton::setChecked);// visibility → button
        return button;
    };
    createToolToggleButton(widgetContainer.preferencesWidget, ":/resources/icons/preferences.png", "Preferences");
    defaultToolbar.addSeparator();
    createToolToggleButton(widgetContainer.taskManagementWidget, ":/resources/icons/tasks-management.png", "Task Management");
    createToolToggleButton(widgetContainer.annotationWidget, ":/resources/icons/annotation.png", "Annotation");
    createToolToggleButton(widgetContainer.zoomWidget, ":/resources/icons/zoom.png", "Zoom");
    createToolToggleButton(widgetContainer.snapshotWidget, ":/resources/icons/snapshot.png", "Snapshot");
    defaultToolbar.addSeparator();
    createToolToggleButton(widgetContainer.pythonInterpreterWidget, ":/resources/icons/python.png", "Python Interpreter");

    defaultToolbar.addSeparator();

    auto resetVPsButton = new QPushButton("Reset vp positions", this);
    resetVPsButton->setToolTip("Reset viewport positions and sizes");
    defaultToolbar.addWidget(resetVPsButton);
    QObject::connect(resetVPsButton, &QPushButton::clicked, this, &MainWindow::resetViewports);

    defaultToolbar.addWidget(new QLabel(" Loader pending: "));
    loaderProgress = new QLabel();
    defaultToolbar.addWidget(loaderProgress);
    loaderLastProgress = 0;
    loaderProgress->setFixedWidth(25);
    loaderProgress->setAlignment(Qt::AlignCenter);
    QObject::connect(&Loader::Controller::singleton(), &Loader::Controller::progress, this, &MainWindow::updateLoaderProgress);

    // segmentation task mode toolbar
    auto prevBtn = new QPushButton("< Last");
    auto nextBtn = new QPushButton("(N)ext >");
    auto splitBtn = new QPushButton("Split required >");
    prevBtn->setToolTip("Go back to last task.");
    nextBtn->setToolTip("Mark current task as finished and go to next one.");
    nextBtn->setShortcut(QKeySequence(Qt::Key_N));
    QObject::connect(prevBtn, &QPushButton::clicked, [](bool) { Segmentation::singleton().selectPrevTodoObject(); });
    QObject::connect(nextBtn, &QPushButton::clicked, [](bool) { Segmentation::singleton().selectNextTodoObject(); });
    QObject::connect(splitBtn, &QPushButton::clicked, [](bool) { Segmentation::singleton().markSelectedObjectForSplitting(state->viewerState->currentPosition); });

    segJobModeToolbar.addWidget(prevBtn);
    segJobModeToolbar.addWidget(nextBtn);
    segJobModeToolbar.addWidget(splitBtn);
    segJobModeToolbar.addSeparator();
    segJobModeToolbar.addWidget(&todosLeftLabel);
}

void MainWindow::updateLoaderProgress(int refCount) {
    if ((refCount % 5 > 0) && (loaderLastProgress > 0)) {
        return;
    }
    loaderLastProgress = refCount;
    QPalette pal;
    pal.setColor(QPalette::WindowText, Qt::black);
    pal.setColor(loaderProgress->backgroundRole(), QColor(refCount > 0 ? Qt::red : Qt::green).lighter());
    loaderProgress->setAutoFillBackground(true);
    loaderProgress->setPalette(pal);
    loaderProgress->setText(QString::number(refCount));
}

void MainWindow::setProofReadingUI(const bool on) {
    const auto currentMode = workModeModel.at(modeCombo.currentIndex()).first;
    if (on) {
        workModeModel.recreate({{AnnotationMode::Mode_Merge, workModes[AnnotationMode::Mode_Merge]}, {AnnotationMode::Mode_Paint, workModes[AnnotationMode::Mode_Paint]}});
        if (currentMode == AnnotationMode::Mode_Merge || currentMode == AnnotationMode::Mode_Paint) {
            setWorkMode(currentMode);
        } else {
            setWorkMode(AnnotationMode::Mode_Merge);
        }
        state->viewer->saveSettings();
        resetViewports();
    } else {
        workModeModel.recreate(workModes);
        setWorkMode(currentMode);
        state->viewer->loadSettings();
    }

    modeSwitchSeparator->setVisible(on);
    setMergeModeAction->setVisible(on);
    setPaintModeAction->setVisible(on);
    GUIModeLabel.setText(on ? "Proof Reading Mode" : "");
    GUIModeLabel.setVisible(on);
}

void MainWindow::setJobModeUI(bool enabled) {
    if(enabled) {
        setWorkMode(AnnotationMode::Mode_MergeSimple);
        menuBar()->hide();
        widgetContainer.hideAll();
        removeToolBar(&defaultToolbar);
        addToolBar(&segJobModeToolbar);
        segJobModeToolbar.show(); // toolbar is hidden by removeToolBar
        forEachVPDo([] (ViewportBase & vp) { vp.hide(); });
        viewportXY.get()->resize(centralWidget()->height() - DEFAULT_VP_MARGIN, centralWidget()->height() - DEFAULT_VP_MARGIN);
    } else {
        menuBar()->show();
        removeToolBar(&segJobModeToolbar);
        addToolBar(&defaultToolbar);
        defaultToolbar.show();
        resetViewports();
    }
}

void MainWindow::updateTodosLeft() {
    int todosLeft = Segmentation::singleton().todolist().size();
    auto & job = Segmentation::singleton().job;

    if(todosLeft > 0) {
        todosLeftLabel.setText(QString("<font color='#FF573E'>  %1 more left</font>").arg(todosLeft));
    }
    else if(Session::singleton().annotationMode.testFlag(AnnotationMode::Mode_MergeSimple)) {
        todosLeftLabel.setText(QString("<font color='#00D36F'>  %1 more left</font>").arg(todosLeft));
        // submit work
        QRegExp regex("\\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,4}\\b");
        if(regex.exactMatch(job.submitPath)) { // submission by email
            QMessageBox msgBox(QMessageBox::Information,
                               "Good Job, you're done!", QString("Please save your work and send it to:\n%0").arg(job.submitPath));
            msgBox.exec();
            return;
        }
        // submission by upload
        QMessageBox msgBox(QMessageBox::Question,
                           "Good Job, you're done!", "Submit your work now to receive a verification?",
                           QMessageBox::Yes | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Yes);
        if(msgBox.exec() == QMessageBox::Yes) {
            auto jobFilename = "final_" + QFileInfo(Session::singleton().annotationFilename).fileName();
            auto finishedJobPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/segmentationJobs/" + jobFilename;
            annotationFileSave(finishedJobPath);
            Network::singleton().submitSegmentationJob(finishedJobPath);
        }
    }
}

void MainWindow::notifyUnsavedChanges() {
    Session::singleton().unsavedChanges = true;
    updateTitlebar();
}

void MainWindow::updateTitlebar() {
    const auto & session = Session::singleton();
    QString title = QString("%1 %2 showing ").arg(qApp->applicationDisplayName()).arg(KVERSION);
    if (!session.annotationFilename.isEmpty()) {
        title.append(Session::singleton().annotationFilename);
    } else {
        title.append("no annotation file");
    }
    unsavedChangesLabel.setToolTip("");
    title.append("[*]");// setWindowModified needs this
    setWindowModified(session.unsavedChanges);
    if (session.unsavedChanges) {
        auto autosave = tr("<font color='#FF573E'>(autosave: off)</font>");
        if (session.autoSaveTimer.isActive()) {
            autosave = tr("<font color='#00D36F'>(autosave: on)</font>");
            const auto minutes = session.autoSaveTimer.remainingTime() / 1000 / 60;
            const auto seconds = session.autoSaveTimer.remainingTime() / 1000 % 60;
            unsavedChangesLabel.setToolTip(tr("Next autosave in %1:%2 min").arg(minutes).arg(seconds, 2, 10, QChar('0')));
        }
        unsavedChangesLabel.setText(tr("unsaved changes ") + autosave);
    } else {
        unsavedChangesLabel.setText("saved");
    }
    //don’t display if there are no changes and no file is loaded
    if (session.unsavedChanges == false && session.annotationFilename.isEmpty()) {
        unsavedChangesLabel.hide();
        annotationTimeLabel.hide();
    } else {
        unsavedChangesLabel.show();
        annotationTimeLabel.show();
    }

    if(session.autoSaveTimer.isActive() == false) {
        title.append(" Autosave: OFF");
    }

    setWindowTitle(title);
}

void MainWindow::updateRecentFile(const QString & fileName) {
    int pos = skeletonFileHistory.indexOf(fileName);
    if (pos != -1) {//move to front if already existing
        skeletonFileHistory.move(pos, 0);
    } else {
        if (skeletonFileHistory.size() == FILE_DIALOG_HISTORY_MAX_ENTRIES) {//shrink if necessary
           skeletonFileHistory.pop_back();
        }
        skeletonFileHistory.push_front(fileName);
    }
    //update the menu
    int i = 0;
    for (const auto & path : skeletonFileHistory) {
        historyEntryActions[i]->setText(path);
        historyEntryActions[i]->setVisible(!path.isEmpty());
        ++i;
    }
}

void MainWindow::createMenus() {
    menuBar()->addMenu(&fileMenu);
    fileMenu.addAction(QIcon(":/resources/icons/menubar/choose-dataset.png"), tr("Choose Dataset …"), &widgetContainer.datasetLoadWidget, SLOT(show()));
    fileMenu.addSeparator();
    addApplicationShortcut(fileMenu, QIcon(":/resources/icons/menubar/create-annotation.png"), tr("Create New Annotation"), this, &MainWindow::newAnnotationSlot, QKeySequence::New);
    addApplicationShortcut(fileMenu, QIcon(":/resources/icons/menubar/open-annotation.png"), tr("Open Annotation …"), this, &MainWindow::openSlot, QKeySequence::Open);
    auto & recentfileMenu = *fileMenu.addMenu(QIcon(":/resources/icons/menubar/open-recent.png"), tr("Recent Annotation File(s)"));
    int i = 0;
    for (auto & elem : historyEntryActions) {
        elem = recentfileMenu.addAction(QIcon(":/resources/icons/menubar/open-recent.png"), "");
        elem->setVisible(false);
        QObject::connect(elem, &QAction::triggered, [this, i](){
            openFileDispatch({skeletonFileHistory.at(i)});
        });
        ++i;
    }
    addApplicationShortcut(fileMenu, QIcon(":/resources/icons/menubar/save-annotation.png"), tr("Save Annotation"), this, &MainWindow::saveSlot, QKeySequence::Save);
    addApplicationShortcut(fileMenu, QIcon(":/resources/icons/menubar/save-annotation-as.png"), tr("Save Annotation as …"), this, &MainWindow::saveAsSlot, QKeySequence::SaveAs);
    fileMenu.addSeparator();
    fileMenu.addAction(tr("Export to nml..."), this, SLOT(exportToNml()));
    fileMenu.addSeparator();
    addApplicationShortcut(fileMenu, QIcon(":/resources/icons/menubar/quit.png"), tr("Quit"), this, &MainWindow::close, QKeySequence::Quit);

    compressionToggleAction = &addApplicationShortcut(actionMenu, QIcon(), tr("Toggle Dataset Compression: None"), this, [this]() {
        static Dataset::CubeType originalType;
        if (Dataset::current().type != Dataset::CubeType::RAW_UNCOMPRESSED) {
            originalType = Dataset::current().type;
            Dataset::current().type = Dataset::CubeType::RAW_UNCOMPRESSED;
        } else {
            Dataset::current().type = originalType;
        }
        state->viewer->updateDatasetMag();
        updateCompressionRatioDisplay();
    }, Qt::Key_F4);
    actionMenu.addSeparator();
    //advanced skeleton
    const QString segStateString = segmentState == SegmentState::On ? tr("On") : tr("Off");
    toggleSegmentsAction = &addApplicationShortcut(actionMenu, QIcon(), tr("Segments: ") + segStateString, this, &MainWindow::toggleSegments, Qt::Key_A);
    newTreeAction = &addApplicationShortcut(actionMenu, QIcon(), tr("New Tree"), this, &MainWindow::newTreeSlot, Qt::Key_C);
    //skeleton
    pushBranchAction = &addApplicationShortcut(actionMenu, QIcon(), tr("Push Branch Node"), this, &MainWindow::pushBranchNodeSlot, Qt::Key_B);
    popBranchAction = &addApplicationShortcut(actionMenu, QIcon(), tr("Pop Branch Node"), this, &MainWindow::popBranchNodeSlot, Qt::Key_J);
    actionMenu.addSeparator();

    createSynapse = &addApplicationShortcut(actionMenu, QIcon(), tr("Create Synapse"), this, []() {
        if(state->skeletonState->selectedNodes.size() < 2) {
            if (state->skeletonState->activeNode != nullptr) {
                state->viewer->window->toggleSynapseState(); //update statusbar
                Skeletonizer::singleton().continueSynapse();
            }
        } else if(state->skeletonState->selectedNodes.size() == 2) {
            Skeletonizer::singleton().addSynapseFromNodes(state->skeletonState->selectedNodes);
        }
    }, Qt::ShiftModifier + Qt::Key_C);

    swapSynapticNodes = &addApplicationShortcut(actionMenu, QIcon(), tr("Reverse Synapse Direction"),
                                                this, [this](){ widgetContainer.annotationWidget.skeletonTab.reverseSynapseDirection(this); },
                                                Qt::ShiftModifier + Qt::ControlModifier + Qt::Key_C);

    actionMenu.addSeparator();
    clearSkeletonAction = actionMenu.addAction(QIcon(":/resources/icons/menubar/trash.png"), "Clear Skeleton", this, SLOT(clearSkeletonSlot()));
    actionMenu.addSeparator();
    //segmentation
    auto setOverlayOpacity = [](int value) {
        Segmentation::singleton().alpha = static_cast<uint8_t>(std::max(0, std::min(255, static_cast<int>(Segmentation::singleton().alpha) + value)));
    };
    increaseOpacityAction = &addApplicationShortcut(actionMenu, QIcon(), tr("Increase Overlay Opacity"), this, [&]() { setOverlayOpacity(10); emit overlayOpacityChanged(); }, Qt::Key_Plus);
    decreaseOpacityAction = &addApplicationShortcut(actionMenu, QIcon(), tr("Decrease Overlay Opacity"), this, [&]() { setOverlayOpacity(-10); emit overlayOpacityChanged(); }, Qt::Key_Minus);
    enlargeBrushAction = &addApplicationShortcut(actionMenu, QIcon(), tr("Increase Brush Size (Shift + Scroll)"), &Segmentation::singleton(),
                                                 []() { Segmentation::singleton().brush.setRadius(Segmentation::singleton().brush.getRadius() + 1); }, Qt::SHIFT + Qt::Key_Plus);
    shrinkBrushAction = &addApplicationShortcut(actionMenu, QIcon(), tr("Decrease Brush Size (Shift + Scroll)"), &Segmentation::singleton(),
                                                []() { Segmentation::singleton().brush.setRadius(Segmentation::singleton().brush.getRadius() - 1); }, Qt::SHIFT + Qt::Key_Minus);

    actionMenu.addSeparator();
    clearMergelistAction = actionMenu.addAction(QIcon(":/resources/icons/menubar/trash.png"), "Clear Merge List", &Segmentation::singleton(), SLOT(clear()));
    //proof reading mode
    modeSwitchSeparator = actionMenu.addSeparator();
    setMergeModeAction = &addApplicationShortcut(actionMenu, QIcon(), tr("Switch to Segmentation Merge Mode"), this, [this]() { setWorkMode(AnnotationMode::Mode_Merge); }, Qt::Key_1);
    setPaintModeAction = &addApplicationShortcut(actionMenu, QIcon(), tr("Switch to Paint Mode"), this, [this]() { setWorkMode(AnnotationMode::Mode_Paint); }, Qt::Key_2);

    menuBar()->addMenu(&actionMenu);

    auto viewMenu = menuBar()->addMenu("&Navigation");

    addApplicationShortcut(*viewMenu, QIcon(), tr("Jump to Active Node"), &Skeletonizer::singleton(), []() {
        auto meshPriority = !state->skeletonState->meshLastClickCurrentlyVisited || !state->skeletonState->activeNode;
        if (state->skeletonState->meshLastClickInformation && meshPriority) {
            state->viewer->setPosition(state->skeletonState->meshLastClickInformation.get().coord);
            state->skeletonState->meshLastClickCurrentlyVisited = true;
        } else if (state->skeletonState->activeNode) {
            Skeletonizer::singleton().jumpToNode(*state->skeletonState->activeNode);
            state->skeletonState->meshLastClickCurrentlyVisited = false;
        }
    }, Qt::Key_S);
    addApplicationShortcut(*viewMenu, QIcon(), tr("Forward-traverse Tree"), &Skeletonizer::singleton(), []() { Skeletonizer::singleton().goToNode(NodeGenerator::Direction::Forward); }, Qt::Key_X);
    addApplicationShortcut(*viewMenu, QIcon(), tr("Backward-traverse Tree"), &Skeletonizer::singleton(), []() { Skeletonizer::singleton().goToNode(NodeGenerator::Direction::Backward); }, Qt::SHIFT + Qt::Key_X);
    addApplicationShortcut(*viewMenu, QIcon(), tr("Next Node in Table"), this, [this](){widgetContainer.annotationWidget.skeletonTab.jumpToNextNode(true);}, Qt::Key_N);
    addApplicationShortcut(*viewMenu, QIcon(), tr("Previous Node in Table"), this, [this](){widgetContainer.annotationWidget.skeletonTab.jumpToNextNode(false);}, Qt::Key_P);
    addApplicationShortcut(*viewMenu, QIcon(), tr("Next Tree in Table"), this, [this](){widgetContainer.annotationWidget.skeletonTab.jumpToNextTree(true);}, Qt::Key_Z);
    addApplicationShortcut(*viewMenu, QIcon(), tr("Previous Tree in Table"), this, [this](){widgetContainer.annotationWidget.skeletonTab.jumpToNextTree(false);}, Qt::SHIFT + Qt::Key_Z);

    viewMenu->addSeparator();

    viewMenu->addAction(QIcon(":/resources/icons/menubar/preferences.png"), "Navigation Settings", [this](){
        widgetContainer.preferencesWidget.setVisible(true);
        widgetContainer.preferencesWidget.tabs.setCurrentIndex(5);

    });

    auto commentsMenu = menuBar()->addMenu("&Comment Shortcuts");
    auto addCommentShortcut = [&](const int number, const QKeySequence key){
        auto & action = addApplicationShortcut(*commentsMenu, QIcon(), "", this, [this, number](){
            if (placeComment(number-1)) {
                activityLabel.setText(tr("Added comment: ") + CommentSetting::comments[number - 1].text);
                activityLabel.setVisible(true);
                activityAnimation.setDirection(QAbstractAnimation::Forward);
                activityAnimation.start();
            }
        }, key);
        commentActions.push_back(&action);
        action.setEnabled(false);
    };
    for (int number = 1; number < 11; ++number) {
        addCommentShortcut(number, QKeySequence(QString("Ctrl+%1").arg(number%10)));
    }

    commentsMenu->addSeparator();
    commentsMenu->addAction(QIcon(":/resources/icons/menubar/preferences.png"), "Comment Settings", [this](){
        widgetContainer.annotationWidget.setVisible(true);
        widgetContainer.annotationWidget.tabs.setCurrentIndex(2);
    });

    auto preferenceMenu = menuBar()->addMenu("&Preferences");
    preferenceMenu->addAction(tr("Load Custom Preferences"), this, SLOT(loadCustomPreferencesSlot()));
    preferenceMenu->addAction(tr("Save Custom Preferences"), this, SLOT(saveCustomPreferencesSlot()));
    preferenceMenu->addAction(tr("Reset to Default Preferences"), this, SLOT(defaultPreferencesSlot()));
    preferenceMenu->addSeparator();
    preferenceMenu->addAction(QIcon(":/resources/icons/menubar/preferences.png"), "Preferences", &widgetContainer.preferencesWidget, SLOT(show()));

    auto windowMenu = menuBar()->addMenu("&Windows");
    windowMenu->addAction(QIcon(":/resources/icons/menubar/tasks-management.png"), tr("Task Management"), &widgetContainer.taskManagementWidget, SLOT(updateAndRefreshWidget()));
    windowMenu->addAction(QIcon(":/resources/icons/menubar/annotation.png"), tr("Annotation"), &widgetContainer.annotationWidget, SLOT(show()));
    windowMenu->addAction(QIcon(":/resources/icons/menubar/zoom.png"), tr("Zoom"), &widgetContainer.zoomWidget, SLOT(show()));
    windowMenu->addAction(QIcon(":/resources/icons/menubar/snapshot.png"), tr("Take a Snapshot"), &widgetContainer.snapshotWidget, SLOT(show()));

    auto scriptingMenu = menuBar()->addMenu("&Scripting");
    scriptingMenu->addAction("Properties", this, SLOT(pythonPropertiesSlot()));
    scriptingMenu->addAction("Run File", this, SLOT(pythonFileSlot()));
    scriptingMenu->addAction("Plugin Manager", this, SLOT(pythonPluginMgrSlot()));
    scriptingMenu->addAction(QIcon(":/resources/icons/menubar/python.png"), "Interpreter", this, SLOT(pythonInterpreterSlot()));
    pluginMenu = scriptingMenu->addMenu("Plugins");
    refreshPluginMenu();

    auto & helpMenu = *menuBar()->addMenu("&Help");

    cheatsheetAction = helpMenu.addAction(QIcon(), tr("Cheatsheet"));
    cheatsheetAction->setCheckable(true);
    QObject::connect(cheatsheetAction, &QAction::triggered, [this](const bool checked) { cheatsheet.setVisible(checked); } );
    QObject::connect(&cheatsheet, &QDockWidget::visibilityChanged, [this](const bool visible) { cheatsheetAction->setChecked(visible); });

    addApplicationShortcut(helpMenu, QIcon(), tr("Documentation … "), this, []() { QDesktopServices::openUrl({MainWindow::docUrl}); }, Qt::Key_F1);
    helpMenu.addAction(QIcon(":/resources/icons/menubar/about.png"), "About", &widgetContainer.aboutDialog, &AboutDialog::show);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (Session::singleton().unsavedChanges) {
         QMessageBox question(this);
         question.setIcon(QMessageBox::Question);
         question.setText(tr("Really quit KNOSSOS despite changes?"));
         question.setInformativeText(tr("All unsaved changes will be lost."));
         question.addButton(tr("&Quit"), QMessageBox::AcceptRole);
         const QPushButton * const cancelButton = question.addButton(QMessageBox::Cancel);
         question.exec();
         if (question.clickedButton() == cancelButton) {
             event->ignore();
             return;//we changed our mind – we dont want to quit anymore
         }
    }
    EmitOnCtorDtor eocd(&SignalRelay::Signal_MainWindow_closeEvent, state->signalRelay, event);
    state->quitSignal = true;
    QApplication::processEvents();//ensure everything’s done
    Loader::Controller::singleton().suspendLoader();
    saveSettings();
    // event loop will stop after this
    QApplication::closeAllWindows();// generates – otherwise missing – hideEvents for saveGeometry
    event->accept();
}

//file menu functionality
bool MainWindow::openFileDispatch(QStringList fileNames, const bool mergeAll, const bool silent) {
    if (fileNames.empty()) {
        return false;
    }

    bool mergeSegmentation = mergeAll;
    bool mergeSkeleton = mergeAll;
    bool existingSegmentation = Segmentation::singleton().hasObjects();
    bool existingSkeleton = !state->skeletonState->trees.empty();
    bool existingBoth = existingSegmentation && existingSkeleton;
    if (!mergeAll && (existingSegmentation || existingSkeleton)) {
        QMessageBox prompt(this);
        prompt.setIcon(QMessageBox::Question);
        prompt.setText(QObject::tr("Which action would you like to choose?"));
        QString text = tr("There is existing: <ul>")
            + (existingSegmentation ? tr("<li>Segmentation data</li>") : tr(""))
            + (existingSkeleton ? tr("<li>Skeleton data</li>") : tr(""))
            + tr("</ul>")
            + tr("which can be merged or overwritten.");
        prompt.setInformativeText(text);
        const auto * overrideAllButton = prompt.addButton(QObject::tr("&Overwrite%1").arg(existingBoth ? " Both" : ""), QMessageBox::AcceptRole);
        const auto * keepAllButton = existingBoth ? prompt.addButton(QObject::tr("&Keep Both"), QMessageBox::AcceptRole) : nullptr;
        const auto * segmenationKeepButton = existingSegmentation ? prompt.addButton(QObject::tr("Keep%1 Seg&menation").arg(existingBoth ? " only" : ""), QMessageBox::AcceptRole) : nullptr;
        const auto * skeletonKeepButton = existingSkeleton ? prompt.addButton(QObject::tr("Keep%1 &Skeleton").arg(existingBoth ? " only" : ""), QMessageBox::AcceptRole) : nullptr;
        const auto * cancelButton = prompt.addButton(QMessageBox::Cancel);
        prompt.exec();
        if (prompt.clickedButton() == cancelButton) {
            return false;
        }
        mergeSegmentation = prompt.clickedButton() == keepAllButton || prompt.clickedButton() == segmenationKeepButton;
        mergeSkeleton = prompt.clickedButton() == keepAllButton || prompt.clickedButton() == skeletonKeepButton;
        if (prompt.clickedButton() == overrideAllButton) {
            Session::singleton().clearAnnotation();
            updateTitlebar();
        } else {
            if (!mergeSegmentation) {
                Segmentation::singleton().clear();
            }
            if (!mergeSkeleton) {
                Skeletonizer::singleton().clearSkeleton();
            }
        }
    }

    bool multipleFiles = fileNames.size() > 1;
    Session::singleton().guiMode = GUIMode::None; // always reset to default gui
    auto nmlEndIt = std::stable_partition(std::begin(fileNames), std::end(fileNames), [](const QString & elem){
        return QFileInfo(elem).suffix() == "nml";
    });

    auto nmls = std::vector<QString>(std::begin(fileNames), nmlEndIt);
    auto zips = std::vector<QString>(nmlEndIt, std::end(fileNames));
    try {
        QSignalBlocker blocker{Skeletonizer::singleton()};
        for (const auto & filename : nmls) {
            const QString treeCmtOnMultiLoad = multipleFiles ? QFileInfo(filename).fileName() : "";
            QFile file(filename);
            state->viewer->skeletonizer->loadXmlSkeleton(file, mergeSkeleton, treeCmtOnMultiLoad);
            updateRecentFile(filename);
            mergeSkeleton = multipleFiles; //multiple files have to be merged
        }
        for (const auto & filename : zips) {
            const QString treeCmtOnMultiLoad = multipleFiles ? QFileInfo(filename).fileName() : "";
            annotationFileLoad(filename, mergeSkeleton, treeCmtOnMultiLoad);
            updateRecentFile(filename);
            mergeSkeleton = multipleFiles; //multiple files have to be merged
        }
    } catch (std::runtime_error & error) {
        if (!silent) {
            QMessageBox fileErrorBox(this);
            fileErrorBox.setIcon(QMessageBox::Warning);
            fileErrorBox.setText(tr("Loading failed."));
            fileErrorBox.setInformativeText(tr("One of the files could not be loaded successfully."));
            fileErrorBox.setDetailedText(error.what());
            fileErrorBox.exec();
        }

        Session::singleton().unsavedChanges = false;// meh, don’t ask
        newAnnotationSlot();
        if (silent) {
            throw;
        }
        return false;
    }
    Skeletonizer::singleton().resetData();

    Session::singleton().unsavedChanges = multipleFiles || mergeSkeleton || mergeSegmentation; //merge implies changes
    if (!mergeSkeleton && !mergeSegmentation) { // if an annotation was already open don't change its filename, otherwise…
        // if multiple files are loaded, let KNOSSOS generate a new filename. Otherwise either an .nml or a .k.zip was loaded
        Session::singleton().annotationFilename = multipleFiles ? "" : !nmls.empty() ? nmls.front() : zips.front();
    }
    updateTitlebar();

    if (Session::singleton().annotationMode.testFlag(AnnotationMode::Mode_MergeSimple)) { // we need to apply job mode here to ensure that all necessary parts are loaded by now.
        setJobModeUI(true);
        Segmentation::singleton().startJobMode();
    }

    state->viewer->window->widgetContainer.zoomWidget.update();

    return true;
}

bool MainWindow::newAnnotationSlot() {
    if (Session::singleton().unsavedChanges) {
        QMessageBox question(this);
        question.setIcon(QMessageBox::Question);
        question.setText(tr("Create new annotation despite changes?"));
        question.setInformativeText(tr("Creating a new annotation will make you lose what you’ve done."));
        question.addButton(tr("&Abandon changes – Start from scratch"), QMessageBox::AcceptRole);
        const auto * const cancelButton = question.addButton(QMessageBox::Cancel);
        question.exec();
        if (question.clickedButton() == cancelButton) {
            return false;
        }
    }
    Session::singleton().clearAnnotation();
    updateTitlebar();
    return true;
}

/**
  * opens the file dialog and receives a skeleton file name path. If the file dialog is not cancelled
  * the skeletonFileHistory queue is updated with the file name entry. The history entries are compared to the
  * selected file names. If the file is already loaded it will not be put into the queue
  *
  */
void MainWindow::openSlot() {
#ifdef Q_OS_MAC
    QString choices = "KNOSSOS annotation file(s) (*.zip *.nml)";
#else
    QString choices = "KNOSSOS annotation file(s) (*.k.zip *.nml)";
#endif
    const QStringList fileNames = state->viewer->suspend([this, &choices]{
        return QFileDialog::getOpenFileNames(this, "Open annotation file(s)", openFileDirectory, choices);
    });
    if (fileNames.empty() == false) {
        openFileDirectory = QFileInfo(fileNames.front()).absolutePath();
        openFileDispatch(fileNames);
    }
}

void MainWindow::saveSlot() {
    auto annotationFilename = Session::singleton().annotationFilename;
    if (annotationFilename.isEmpty()) {
        saveAsSlot();
    } else {
        save(annotationFilename);
    }
}

void MainWindow::saveAsSlot() {
    const auto & suggestedFile = saveFileDirectory.isEmpty() ? annotationFileDefaultPath() : saveFileDirectory + '/' + annotationFileDefaultName();

    QString fileName = state->viewer->suspend([this, suggestedFile]{
#ifdef Q_OS_MAC
        return QFileDialog::getSaveFileName(this, "Save the KNOSSSOS Annotation file", suggestedFile);
#else
        return QFileDialog::getSaveFileName(this, "Save the KNOSSSOS annotation file", suggestedFile, "KNOSSOS annotation file (*.k.zip)");
#endif
    });

    if (!fileName.isEmpty()) {
        const auto prevFilename = fileName;
        QRegExp kzipRegex(R"regex((\.k)|(\.zip))regex"); // any occurance of .k and .zip
        if (fileName.contains(kzipRegex)) {
            fileName.remove(kzipRegex);

            if (prevFilename != fileName + ".k.zip") {
                const auto message = tr("The supplied filename has been changed: \n%1 to\n%2.k.zip").arg(prevFilename).arg(fileName);
                QMessageBox::information(this, tr("Fixed filename"), message);
            }
        }
        saveFileDirectory = QFileInfo(fileName).absolutePath();
        save(fileName + ".k.zip", false, false);
    }
}

void MainWindow::save(QString filename, const bool silent, const bool allocIncrement)
try {
    if (filename.isEmpty()) {
        filename = annotationFileDefaultPath();
    } else {// to prevent update of the initial default path
        if (filename.endsWith(".nml")) {
            filename.chop(4);
            filename += ".k.zip";
        }
        if (allocIncrement && Session::singleton().autoFilenameIncrementBool) {
            int index = skeletonFileHistory.indexOf(filename);
            filename = updatedFileName(filename);
            if (index != -1) {//replace old filename with updated one
                skeletonFileHistory.replace(index, filename);
                historyEntryActions[index]->setText(filename);
            }
        }
    }
    annotationFileSave(filename);
    Session::singleton().annotationFilename = filename;
    updateRecentFile(filename);
    updateTitlebar();
} catch (std::runtime_error & error) {
    if (silent) {
        throw;
    } else {
        QMessageBox errorBox(this);
        errorBox.setIcon(QMessageBox::Critical);
        errorBox.setText("File save failed");
        errorBox.setInformativeText(filename);
        errorBox.setDetailedText(error.what());
        errorBox.exec();
    }
}

void MainWindow::exportToNml() {
    if (state->skeletonState->trees.empty()) {
        QMessageBox::information(this, "No Save", "No skeleton was found. Not saving!");
        return;
    }
    auto info = QFileInfo(Session::singleton().annotationFilename);
    auto defaultpath = annotationFileDefaultPath();
    defaultpath.chop(6);
    defaultpath += ".nml";
    const auto & suggestedFilepath = Session::singleton().annotationFilename.isEmpty() ? defaultpath : info.absoluteDir().path() + "/" + info.baseName() + ".nml";
    auto filename = state->viewer->suspend([this, &suggestedFilepath]{
        return QFileDialog::getSaveFileName(this, "Export to skeleton file", suggestedFilepath, "KNOSSOS skeleton file (*.nml)");
    });
    if(filename.isEmpty() == false) {
        if(filename.endsWith(".nml") == false) {
            filename += ".nml";
        }
        nmlExport(filename);
    }
}

void MainWindow::setWorkMode(AnnotationMode workMode) {
    if (workModes.find(workMode) == std::end(workModes)) {
        workMode = AnnotationMode::Mode_Tracing;
    }
    modeCombo.setCurrentIndex(workModeModel.indexOf(workMode));
    auto & mode = Session::singleton().annotationMode;
    mode = workMode;
    const bool trees = mode.testFlag(AnnotationMode::Mode_TracingAdvanced) || mode.testFlag(AnnotationMode::Mode_MergeTracing);
    const bool skeleton = mode.testFlag(AnnotationMode::Mode_Tracing) || mode.testFlag(AnnotationMode::Mode_TracingAdvanced) || mode.testFlag(AnnotationMode::Mode_MergeTracing);
    const bool segmentation = mode.testFlag(AnnotationMode::Mode_Paint) || mode.testFlag(AnnotationMode::Mode_Merge) || mode.testFlag(AnnotationMode::Mode_MergeSimple) || mode.testFlag(AnnotationMode::Mode_MergeTracing) || mode.testFlag(AnnotationMode::Mode_Selection);
    toggleSegmentsAction->setVisible(mode.testFlag(AnnotationMode::Mode_TracingAdvanced));
    segmentStateLabel.setVisible(mode.testFlag(AnnotationMode::Mode_TracingAdvanced));
    if (mode.testFlag(AnnotationMode::Mode_TracingAdvanced)) {
        setSegmentState(segmentState);
    } else if (mode.testFlag(AnnotationMode::Mode_Tracing)) {
        setSegmentState(SegmentState::On);
    }
    newTreeAction->setVisible(trees);
    pushBranchAction->setVisible(mode.testFlag(AnnotationMode::NodeEditing));
    popBranchAction->setVisible(mode.testFlag(AnnotationMode::NodeEditing));
    createSynapse->setVisible(mode.testFlag(AnnotationMode::Mode_TracingAdvanced));
    swapSynapticNodes->setVisible((mode.testFlag(AnnotationMode::Mode_TracingAdvanced)));
    clearSkeletonAction->setVisible(skeleton && !mode.testFlag(AnnotationMode::Mode_MergeTracing));
    increaseOpacityAction->setVisible(segmentation);
    decreaseOpacityAction->setVisible(segmentation);
    enlargeBrushAction->setVisible(mode.testFlag(AnnotationMode::Brush));
    shrinkBrushAction->setVisible(mode.testFlag(AnnotationMode::Brush));
    clearMergelistAction->setVisible(segmentation && !mode.testFlag(AnnotationMode::Mode_MergeTracing));

    if (mode.testFlag(AnnotationMode::Mode_MergeTracing) && state->skeletonState->activeNode != nullptr) {// sync subobject and node selection
        Skeletonizer::singleton().selectObjectForNode(*state->skeletonState->activeNode);
    }

    cheatsheet.load(workMode);
}

void MainWindow::setSegmentState(const SegmentState newState) {
    segmentState = newState;
    QString stateName = "";
    QString nextState = "";
    switch(segmentState) {
    case SegmentState::On:
        stateName = tr("<font color='#00D36F'>On</font>");
        nextState = tr("off once");
        Session::singleton().annotationMode |= AnnotationMode::LinkedNodes;
        break;
    case SegmentState::Off_Once:
        stateName = tr("<font color='darkGoldenRod'>Off once</font>");
        nextState = tr("off");
        Session::singleton().annotationMode &= ~QFlags<AnnotationMode>(AnnotationMode::LinkedNodes);
        break;
    case SegmentState::Off:
        stateName = tr("<font color='#34A4FF'>Off</font>");
        nextState = tr("on");
        Session::singleton().annotationMode &= ~QFlags<AnnotationMode>(AnnotationMode::LinkedNodes);
        break;
    }
    toggleSegmentsAction->setText(tr("Turn segments %1").arg(nextState));
    segmentStateLabel.setText(tr("Segments (toggle with a): ") + stateName);
}

void MainWindow::toggleSegments() {
    switch(segmentState) {
    case SegmentState::On:
        setSegmentState(SegmentState::Off_Once);
        break;
    case SegmentState::Off_Once:
        setSegmentState(SegmentState::Off);
        break;
    case SegmentState::Off:
        setSegmentState(SegmentState::On);
        break;
    }
}

void MainWindow::setSynapseState(const SynapseState newState) {
    synapseState = newState;
    switch(synapseState) {
    case SynapseState::Off:
        synapseStateLabel.setText(tr(""));
        break;
    case SynapseState::SynapticCleft:
        synapseStateLabel.setText(tr("Synapse mode: <font color='#00D36F'>trace cleft</font>"));
        break;
    case SynapseState::PostSynapse:
        synapseStateLabel.setText(tr("Synapse mode: <font color='#34A4FF'>set post synapse</font>"));
        break;
    }
    newTreeAction->setText((synapseState == SynapseState::SynapticCleft) ? "New Tree / Finish Synapse" : "New Tree");
    createSynapse->setEnabled(synapseState == SynapseState::Off); // Only allow new synapse if there is none active atm.
    synapseStateLabel.setHidden(synapseStateLabel.text().isEmpty());
}

void MainWindow::toggleSynapseState() {
    switch(synapseState) {
    case SynapseState::Off:
        setSynapseState(SynapseState::SynapticCleft);
        break;
    case SynapseState::SynapticCleft:
        setSynapseState(SynapseState::PostSynapse);
        break;
    case SynapseState::PostSynapse:
        setSynapseState(SynapseState::Off);
        break;
    }
}

void MainWindow::clearSkeletonSlot() {
    if(Session::singleton().unsavedChanges || !state->skeletonState->trees.empty()) {
        QMessageBox question(this);
        question.setIcon(QMessageBox::Question);
        question.setText(tr("Do you really want to clear the skeleton?"));
        question.setInformativeText(tr("This erases all trees and their nodes and cannot be undone."));
        QPushButton const * const ok = question.addButton(tr("Clear skeleton"), QMessageBox::AcceptRole);
        question.addButton(QMessageBox::Cancel);
        question.exec();
        if (question.clickedButton() == ok) {
            Skeletonizer::singleton().clearSkeleton();
        }
    }
}

void MainWindow::loadCustomPreferences(const QString & fileName) {
    QSettings globalSettings;
    QSettings settingsToLoad(fileName, QSettings::IniFormat);
    for (auto && key : settingsToLoad.allKeys()) {
        globalSettings.setValue(key, settingsToLoad.value(key));
    }
    loadSettings();
}

/* preference menu functionality */
void MainWindow::loadCustomPreferencesSlot() {
    const QString fileName = state->viewer->suspend([this]{
        return QFileDialog::getOpenFileName(this, "Open custom preferences file", QDir::homePath(), "KNOSSOS GUI preferences file (*.ini)");
    });
    if (!fileName.isNull()) {
        loadCustomPreferences(fileName);
    }
}

void MainWindow::saveCustomPreferencesSlot() {
    saveSettings();
    QSettings settings;
    QString originSettings = settings.fileName();

    const QString fileName = state->viewer->suspend([this]{
        return QFileDialog::getSaveFileName(this, "Save custom preferences file as", QDir::homePath(), "KNOSSOS GUI preferences file (*.ini)");
    });
    if(!fileName.isEmpty()) {
        QFile file;
        file.setFileName(originSettings);
        file.copy(fileName);
    }
}

void MainWindow::defaultPreferencesSlot() {
    QMessageBox question{QApplication::activeWindow()};
    question.setIcon(QMessageBox::Question);
    question.setWindowTitle("Confirmation required");
    question.setText("Do you really want to load the default preferences?");
    QPushButton *yes = question.addButton(QMessageBox::Yes);
    question.addButton(QMessageBox::No);
    question.exec();

    if(question.clickedButton() == yes) {
        clearSettings();
        loadSettings();
        state->viewer->loadTreeLUT();
        state->viewer->defaultDatasetLUT();
        this->setGeometry(QApplication::desktop()->availableGeometry().topLeft().x() + 20,
                          QApplication::desktop()->availableGeometry().topLeft().y() + 50, 1024, 800);
    }
}

/* toolbar slots */

void MainWindow::copyClipboardCoordinates() {
    const auto content = QString("%1, %2, %3").arg(xField->value()).arg(yField->value()).arg(zField->value());
    QApplication::clipboard()->setText(content);
}

void MainWindow::pasteClipboardCoordinates(){
    const QString clipboardContent = QApplication::clipboard()->text();

    const QRegExp clipboardRegEx(R"regex((\d+)\D+(\d+)\D+(\d+))regex");//match 3 groups of digits separated by non-digits
    if (clipboardRegEx.indexIn(clipboardContent) != -1) {//also fails if clipboard is empty
        const auto x = clipboardRegEx.cap(1).toInt();//index 0 is the whole matched text
        const auto y = clipboardRegEx.cap(2).toInt();
        const auto z = clipboardRegEx.cap(3).toInt();

        xField->setValue(x);
        yField->setValue(y);
        zField->setValue(z);

        coordinateEditingFinished();
    } else {
        qDebug() << "Unable to extract coordinates from clipboard content" << clipboardContent;
    }
}

void MainWindow::coordinateEditingFinished() {
    floatCoordinate inputCoord{xField->value() - 1.f, yField->value() - 1.f, zField->value() - 1.f};
    state->viewer->userMove(inputCoord - state->viewerState->currentPosition, USERMOVE_NEUTRAL);
}

void MainWindow::saveSettings() {
    QSettings settings;
    settings.beginGroup(MAIN_WINDOW);
    settings.setValue(GEOMETRY, saveGeometry());
    settings.setValue(STATE, saveState());

    state->viewer->saveSettings();

    settings.setValue(ANNOTATION_MODE, static_cast<int>(workModeModel.at(modeCombo.currentIndex()).first));

    int i = 0;
    for (const auto & path : skeletonFileHistory) {
        settings.setValue(QString("loaded_file%1").arg(i+1), path);
        ++i;
    }

    settings.setValue(OPEN_FILE_DIALOG_DIRECTORY, openFileDirectory);
    settings.setValue(SAVE_FILE_DIALOG_DIRECTORY, saveFileDirectory);

    settings.setValue(GUI_MODE, static_cast<int>(Session::singleton().guiMode));

    settings.endGroup();

    widgetContainer.datasetLoadWidget.saveSettings();
    widgetContainer.zoomWidget.saveSettings();
    widgetContainer.preferencesWidget.saveSettings();
    widgetContainer.annotationWidget.saveSettings();
    widgetContainer.pythonPropertyWidget.saveSettings();
    widgetContainer.pythonInterpreterWidget.saveSettings();
    widgetContainer.snapshotWidget.saveSettings();
    widgetContainer.taskManagementWidget.taskLoginWidget.saveSettings();
}

void MainWindow::loadSettings() {
    QSettings settings;
    settings.beginGroup(MAIN_WINDOW);
    if (!restoreGeometry(settings.value(GEOMETRY).toByteArray())) {
        resize(1024, 768);// initial default size
    }
    restoreState(settings.value(STATE).toByteArray());
    cheatsheetAction->setChecked(cheatsheet.isVisibleTo(this));

    auto autosaveLocation = QFileInfo(annotationFileDefaultPath()).dir().absolutePath();
    QDir().mkpath(autosaveLocation);

    openFileDirectory = settings.value(OPEN_FILE_DIALOG_DIRECTORY, autosaveLocation).toString();

    saveFileDirectory = settings.value(SAVE_FILE_DIALOG_DIRECTORY, autosaveLocation).toString();

    setWorkMode(static_cast<AnnotationMode>(settings.value(ANNOTATION_MODE, static_cast<int>(AnnotationMode::Mode_Tracing)).toInt()));

    for (int no = 10; no != 0; --no) {//reverse, because new ones are added in front
        updateRecentFile(settings.value(QString("loaded_file%1").arg(no), "").toString());
    }

    setSegmentState(static_cast<SegmentState>(settings.value(SEGMENT_STATE, static_cast<int>(SegmentState::On)).toInt()));

    Session::singleton().guiMode = static_cast<GUIMode>(settings.value(GUI_MODE, static_cast<int>(GUIMode::None)).toInt());

    settings.endGroup();

    widgetContainer.annotationWidget.loadSettings();
    widgetContainer.preferencesWidget.loadSettings();
    widgetContainer.datasetLoadWidget.loadSettings();
    widgetContainer.zoomWidget.loadSettings();
    widgetContainer.pythonInterpreterWidget.loadSettings();
    widgetContainer.pythonPropertyWidget.loadSettings();
    widgetContainer.snapshotWidget.loadSettings();

    show();
    activateWindow();// prevent mainwin in background in gnome when other widgets are also visible
    state->viewer->loadSettings();// size vps after show() for proper maximized space
    setProofReadingUI(Session::singleton().guiMode == GUIMode::ProofReading);
    forEachVPDo([](ViewportBase & vp) { // show undocked vps after mainwindow
        if (vp.isDocked == false) {
            vp.setDock(false);
        }
    });
    widgetContainer.applyVisibility();

    if (state->viewerState->MeshPickingEnabled == false) {
        warningDisabledFeaturesLabel.setPixmap(warnPixmap.scaled(24, 24));
        warningDisabledFeaturesLabel.setToolTip(tr("Mesh selection is disabled, because it requires an OpenGL version ≥ 3.0.\nYour version: %1").arg(reinterpret_cast<const char*>(::glGetString(GL_VERSION))));
        statusBar()->addPermanentWidget(&warningDisabledFeaturesLabel);
        if (widgetContainer.preferencesWidget.treesTab.warnDisabledPickingCheck.isChecked()) {
            QMessageBox msgBox{QApplication::activeWindow()};
            msgBox.setCheckBox(new QCheckBox("Don’t show this message again.", &msgBox));
            QObject::connect(msgBox.checkBox(), &QCheckBox::clicked, [this](const bool checked) { widgetContainer.preferencesWidget.treesTab.warnDisabledPickingCheck.setChecked(!checked); });
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText(warningDisabledFeaturesLabel.toolTip());
            msgBox.exec();
        }
    }
}

void MainWindow::clearSettings() {
    QSettings settings;

    skeletonFileHistory.clear();

    for(int i = 0; i < FILE_DIALOG_HISTORY_MAX_ENTRIES; i++) {
        historyEntryActions[i]->setVisible(false);
    }

    for(auto a : settings.allKeys()) {
        settings.remove(a);
    }
}

void MainWindow::updateCoordinateBar(int x, int y, int z) {
    xField->setValue(x + 1);
    yField->setValue(y + 1);
    zField->setValue(z + 1);
}

void MainWindow::resizeEvent(QResizeEvent *) {
    if(state->viewerState->defaultVPSizeAndPos) {
        // don't resize viewports when user positioned and resized them manually
        resetViewports();
    } else {//ensure viewports fit the window
        forEachVPDo([](ViewportBase & vp) {
            vp.posAdapt();
            vp.sizeAdapt();
        });
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    QStringList files;
    for (auto && url : event->mimeData()->urls()) {
        files.append(url.toLocalFile());
    }
    QTimer::singleShot(0, [this, files](){
        openFileDispatch(files);
    });
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent * event) {
    if(event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        for (auto && url : urls) {
            qDebug() << url;//in case its no working
            if (url.isLocalFile()) {
                const auto fileName(url.toLocalFile());
                if (fileName.endsWith(".k.zip") || fileName.endsWith(".nml")) {
                    event->accept();
                }
            }
        }
    }
}

void MainWindow::resetViewports() {
    if (Session::singleton().guiMode == GUIMode::ProofReading) {
        viewportXY.get()->setDock(true);
        viewportXY.get()->show();
        viewportXZ->setHidden(true);
        viewportZY->setHidden(true);
        viewportArb->setHidden(true);
        viewport3D->setHidden(true);
    } else {
        forEachVPDo([](ViewportBase & vp) {
            vp.setDock(true);
            vp.setVisible(state->viewerState->enableArbVP || vp.viewportType != VIEWPORT_ARBITRARY);
        });
    }
    resizeToFitViewports(centralWidget()->width(), centralWidget()->height());
    state->viewer->setDefaultVPSizeAndPos(true);
}

void MainWindow::newTreeSlot() {
    if(state->skeletonState->synapseState == Synapse::State::Cleft) {
        Skeletonizer::singleton().continueSynapse(); //finish synaptic cleft
        state->viewer->window->toggleSynapseState(); //update statusbar
    }
    Skeletonizer::singleton().addTree();
}

void MainWindow::pushBranchNodeSlot() {
    if(state->skeletonState->activeNode) {
        Skeletonizer::singleton().pushBranchNode(*state->skeletonState->activeNode);
    }
}

void MainWindow::popBranchNodeSlot() {
    Skeletonizer::singleton().popBranchNodeAfterConfirmation(this);
}

void MainWindow::updateCommentShortcut(const int index, const QString & comment) {
    commentActions[index]->setText(comment);
    commentActions[index]->setDisabled(comment.isEmpty());
}

bool MainWindow::placeComment(const int index) {
    if (Session::singleton().annotationMode.testFlag(AnnotationMode::NodeEditing) && state->skeletonState->activeNode != nullptr) {
        CommentSetting comment = CommentSetting::comments[index];
        if (!comment.text.isEmpty()) {
            if(comment.appendComment) {
                auto activeNodeComment = state->skeletonState->activeNode->getComment();
                if(activeNodeComment.isEmpty() == false) {
                    activeNodeComment.append(' ');
                    comment.text.prepend(activeNodeComment);
                }
            }
            Skeletonizer::singleton().setComment(*state->skeletonState->activeNode, comment.text);
            return true;
        }
        return false;
    } else {
        return Segmentation::singleton().placeCommentForSelectedObject(CommentSetting::comments[index].text);
    }
}

void MainWindow::resizeToFitViewports(int width, int height) {
    width = (width - DEFAULT_VP_MARGIN);
    height = (height - DEFAULT_VP_MARGIN);
    int mindim = std::min(width, height);
    if (Session::singleton().guiMode == GUIMode::ProofReading) {
        viewportXY->setGeometry(DEFAULT_VP_MARGIN, DEFAULT_VP_MARGIN, mindim - DEFAULT_VP_MARGIN, mindim - DEFAULT_VP_MARGIN);
    } else {
        mindim /= 2;
        viewportXY->move(DEFAULT_VP_MARGIN, DEFAULT_VP_MARGIN);
        viewportXZ->move(DEFAULT_VP_MARGIN, DEFAULT_VP_MARGIN + mindim);
        viewportZY->move(DEFAULT_VP_MARGIN + mindim, DEFAULT_VP_MARGIN);
        viewportArb->move(DEFAULT_VP_MARGIN + 2 * mindim, DEFAULT_VP_MARGIN);
        viewport3D->move(DEFAULT_VP_MARGIN + mindim, DEFAULT_VP_MARGIN + mindim);

        forEachVPDo([&mindim](ViewportBase & vp) { vp.resize(mindim - DEFAULT_VP_MARGIN, mindim - DEFAULT_VP_MARGIN); });
    }
}

void MainWindow::pythonPropertiesSlot() {
    widgetContainer.pythonPropertyWidget.show();
}

void MainWindow::pythonFileSlot() {
    const QString pyFileName = state->viewer->suspend([this]{
        return QFileDialog::getOpenFileName(this, "Select python file", QDir::homePath(), "*.py");
    });
    state->scripting->runFile(pyFileName);
}

void MainWindow::refreshPluginMenu() {
    auto actions = pluginMenu->actions();
    for (auto action : actions) {
        pluginMenu->removeAction(action);
    }
    QObject::connect(pluginMenu->addAction("Open Plugins Directory"),
                     &QAction::triggered,
                     [](){QDesktopServices::openUrl(QUrl::fromLocalFile(state->scripting->getPluginDir()));});
    pluginMenu->addSeparator();

    QStringList pluginNames = state->scripting->getPluginNames().split(";");
    for (auto pluginName : pluginNames) {
        if (pluginName.isEmpty()) {
            continue;
        }
        auto pluginSubMenu = pluginMenu->addMenu(pluginName);
        QObject::connect(pluginSubMenu->addAction("Open"), &QAction::triggered,
                         [pluginName](){state->scripting->openPlugin(pluginName,false);});
        QObject::connect(pluginSubMenu->addAction("Close"), &QAction::triggered,
                         [pluginName](){state->scripting->closePlugin(pluginName,false);});
        QObject::connect(pluginSubMenu->addAction("Show"), &QAction::triggered,
                         [pluginName](){state->scripting->showPlugin(pluginName,false);});
        QObject::connect(pluginSubMenu->addAction("Hide"), &QAction::triggered,
                         [pluginName](){state->scripting->hidePlugin(pluginName,false);});
        QObject::connect(pluginSubMenu->addAction("Reload"), &QAction::triggered,
                         [pluginName](){state->scripting->reloadPlugin(pluginName,false);});
        QObject::connect(pluginSubMenu->addAction("Instantiate"), &QAction::triggered,
                         [pluginName](){state->scripting->instantiatePlugin(pluginName,false);});
        QObject::connect(pluginSubMenu->addAction("Remove instance"), &QAction::triggered,
                         [pluginName](){state->scripting->removePluginInstance(pluginName,false);});
        QObject::connect(pluginSubMenu->addAction("Import"), &QAction::triggered,
                         [pluginName](){state->scripting->importPlugin(pluginName,false);});
        QObject::connect(pluginSubMenu->addAction("Remove import"), &QAction::triggered,
                         [pluginName](){state->scripting->removePluginImport(pluginName,false);});
    }
}

void MainWindow::pythonInterpreterSlot() {
    widgetContainer.pythonInterpreterWidget.show();
    widgetContainer.pythonInterpreterWidget.activateWindow();
}

void MainWindow::pythonPluginMgrSlot() {
    auto showError = [this](const QString &errorStr) {
        QMessageBox errorBox(QMessageBox::Warning, "Python Plugin Manager: Error", errorStr, QMessageBox::Ok, this);
        errorBox.exec();
        return;
    };
    auto pluginDir = state->scripting->getPluginDir();
    auto pluginMgrFn = PLUGIN_MGR_NAME + ".py";
    auto pluginMgrPath = QString("%1/%2").arg(pluginDir).arg(pluginMgrFn);
    auto existed = QFile(pluginMgrPath).exists();
    if (!existed && !QFile::copy(QString(":/resources/plugins/%1").arg(pluginMgrFn), pluginMgrPath)) {
        return showError(QString("Cannot temporarily place default Plugin Manager in plugin directory:\n%1").arg(pluginMgrPath));
    }
    state->scripting->importPlugin(PLUGIN_MGR_NAME, false);
    if (!existed) {
        QFile pluginMgrFile(pluginMgrPath);
        if (!pluginMgrFile.setPermissions(QFile::WriteOther)) {
            return showError(QString("Cannot set write permissions for temporarily placed Plugin Manager in plugin directory:\n%1").arg(pluginMgrPath));
        }
        if (!pluginMgrFile.remove()) {
            return showError(QString("Cannot remove temporarily placed Plugin Manager from plugin directory:\n%1").arg(pluginMgrPath));
        }
    }
    state->scripting->openPlugin(PLUGIN_MGR_NAME, false);
}

void MainWindow::updateCompressionRatioDisplay() {
    compressionToggleAction->setText(tr("Toggle dataset compression: %1 ").arg(Dataset::current().compressionString()));
}

bool MainWindow::event(QEvent *event) {
    if (event->type() == QEvent::WindowActivate) {
        state->viewer->run();
    }
    return QMainWindow::event(event);
}
