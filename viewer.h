#ifndef VIEWER_H
#define VIEWER_H

/*
 *  This file is a part of KNOSSOS.
 *
 *  (C) Copyright 2007-2013
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
 */

/*
 * For further information, visit http://www.knossostool.org or contact
 *     Joergen.Kornfeld@mpimf-heidelberg.mpg.de or
 *     Fabian.Svara@mpimf-heidelberg.mpg.de
 */
#include "functions.h"
#include "mesh.h"
#include "remote.h"
#include "slicer/gpucuber.h"
#include "widgets/mainwindow.h"
#include "widgets/navigationwidget.h"
#include "widgets/viewport.h"

#include <QObject>
#include <QDebug>
#include <QCursor>
#include <QTimer>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QQuaternion>

#define SLOW 1000
#define FAST 10

#define MAX_COLORVAL 255.

enum class SkeletonDisplay {
    OnlySelected = 0x1,
    ShowIn3DVP = 0x2,
    ShowInOrthoVPs = 0x4
};

enum class IdDisplay {
    None = 0x0,
    ActiveNode = 0x1,
    AllNodes = 0x2 | ActiveNode
};

enum class RotationCenter {
    DatasetCenter,
    ActiveNode,
    CurrentPosition
};

struct ViewerState {
    ViewerState() {
        state->viewerState = this;
    }

    int highlightVp{VIEWPORT_UNDEFINED};
    int vpKeyDirection[3]{1,1,1};

    int texEdgeLength = 512;
    // don't jump between mags on zooming
    bool datasetMagLock;
    // Current position of the user crosshair.
    //   Given in pixel coordinates of the current local dataset (whatever magnification
    //   is currently loaded.)
    Coordinate currentPosition;

    //Keyboard repeat rate
    uint stepsPerSec{40};
    int multisamplingOnOff;
    int lightOnOff;

    // Draw the colored lines that highlight the orthogonal VP intersections with each other.
    // flag to indicate if user has pulled/is pulling a selection square in a viewport, which should be displayed
    std::pair<int, Qt::KeyboardModifiers> nodeSelectSquareData{-1, Qt::NoModifier};
    std::pair<Coordinate, Coordinate> nodeSelectionSquare;

    bool selectModeFlag{false};

    uint dropFrames{1};
    uint walkFrames{10};

    float voxelDimX;
    float voxelDimY;
    float voxelXYRatio;
    float voxelDimZ;
    //ZY can't be different to XZ because of the intrinsic properties of the SBF-SEM.
    float voxelXYtoZRatio;

    // Advanced Tracing Modes Stuff
    navigationMode autoTracingMode{navigationMode::recenter};
    int autoTracingDelay{50};
    int autoTracingSteps{10};

    float cumDistRenderThres{7.f};

    bool defaultVPSizeAndPos{true};
    //there’s a problem (intel drivers on linux only?)
    //rendering knossos at full speed and displaying a file dialog
    //so we reduce the rendering speed during display of said dialog
    uint renderInterval{FAST};

    //In pennmode right and left click are switched
    bool penmode = false;

    float movementAreaFactor{80.f};

    // dataset & segmentation rendering options
    int luminanceRangeDelta{255};
    int luminanceBias{0};
    bool showOverlay{true};
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> datasetColortable;//user LUT
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> datasetAdjustmentTable;//final LUT used during slicing
    bool datasetColortableOn{false};
    bool datasetAdjustmentOn{false};
    // skeleton rendering options
    float depthCutOff{5.f};
    QFlags<SkeletonDisplay> skeletonDisplay{SkeletonDisplay::ShowIn3DVP, SkeletonDisplay::ShowInOrthoVPs};
    QFlags<IdDisplay> idDisplay{IdDisplay::None};
    int highlightActiveTree{true};
    float segRadiusToNodeRadius{.5f};
    int overrideNodeRadiusBool{false};
    float overrideNodeRadiusVal{1.f};
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> nodeColors;
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> treeColors;
    QString highlightedNodePropertyByRadius{""};
    double nodePropertyRadiusScale{1};
    QString highlightedNodePropertyByColor{""};
    double nodePropertyColorMapMin{0};
    double nodePropertyColorMapMax{0};
    // viewport rendering options
    float FOVmin{0.5};
    float FOVmax{1};
    bool drawVPCrosshairs{true};
    RotationCenter rotationCenter{RotationCenter::ActiveNode};
    int showIntersections{false};
    bool showScalebar{false};
    bool showXYplane{true};
    bool showXZplane{true};
    bool showZYplane{true};
    bool showArbplane{true};
    // temporary vertex buffers that are available for rendering, get cleared
    // every frame */
    mesh lineVertBuffer; /* ONLY for lines */
    mesh pointVertBuffer; /* ONLY for points */
};

/**
 *
 *  This file contains functions that are called by the managing,
 *  all openGL rendering operations and
 *  all skeletonization operations commanded directly by the user over the GUI. The files gui.c, renderer.c and
 *  skeletonizer.c contain functions mainly used by the corresponding "subsystems". viewer.c contains the main
 *  event loop and routines that handle (extract slices, pack into openGL textures,...) the data coming
 *  from the loader thread.
 */
class Skeletonizer;
class ViewportBase;
class Viewer : public QObject {
    const bool evilHack = [this](){
        state->viewer = this;// make state->viewer available to widgets
        return true;
    }();
    Q_OBJECT
private:
    QElapsedTimer baseTime;
    float alphaCache;
    QQuaternion rotation;
    floatCoordinate moveCache; //Cache for Movements smaller than pixel coordinate

    ViewerState viewerState;
    void initViewer();
    void rewire();

    void vpGenerateTexture(ViewportArb & vp);

    bool dcSliceExtract(char *datacube, Coordinate cubePosInAbsPx, char *slice, size_t dcOffset, ViewportOrtho & vp, bool useCustomLUT);
    bool dcSliceExtract(char *datacube, floatCoordinate *currentPxInDc_float, char *slice, int s, int *t, ViewportArb &vp, bool useCustomLUT);

    void ocSliceExtract(char *datacube, Coordinate cubePosInAbsPx, char *slice, size_t dcOffset, ViewportOrtho & vp);

    bool calcLeftUpperTexAbsPx();

    Remote remote;
public:
    Viewer();
    Skeletonizer *skeletonizer;
    MainWindow mainWindow;
    MainWindow *window = &mainWindow;

    std::list<TextureLayer> layers;
    int gpucubeedge = 64;
    bool gpuRendering = false;

    ViewportOrtho *viewportXY, *viewportXZ, *viewportZY;
    ViewportArb *viewportArb;
    void zoom(const float factor);
    void zoomReset();
    QTimer timer;

    void arbCubes(ViewportArb & vp);
    void resizeTexEdgeLength(const int cubeEdge, const int superCubeEdge);
    void loadNodeLUT(const QString & path);
    void loadTreeLUT(const QString & path = ":/resources/color_palette/default.json");
    color4F getNodeColor(const nodeListElement & node) const;
signals:
    void coordinateChangedSignal(const Coordinate & pos);
    void zoomChanged();
    void movementAreaFactorChangedSignal();
    void magnificationLockChanged(bool);
public slots:
    void updateCurrentPosition();
    bool updateDatasetMag(const uint mag = 0);
    void setPosition(const floatCoordinate & pos, UserMoveType userMoveType = USERMOVE_NEUTRAL, const Coordinate & viewportNormal = {0, 0, 0});
    void setPositionWithRecentering(const Coordinate &pos);
    void setPositionWithRecenteringAndRotation(const Coordinate &pos, const ViewportType vpType);
    void userMoveVoxels(const Coordinate &step, UserMoveType userMoveType, const Coordinate & viewportNormal);
    void userMove(const floatCoordinate & floatStep, UserMoveType userMoveType = USERMOVE_NEUTRAL, const Coordinate & viewportNormal = {0, 0, 0});
    void userMoveRound(UserMoveType userMoveType = USERMOVE_NEUTRAL, const Coordinate & viewportNormal = {0, 0, 0});
    void userMoveClear();
    bool recalcTextureOffsets();
    bool calcDisplayedEdgeLength();
    void applyTextureFilterSetting(const GLint texFiltering);
    void run();
    void loader_notify();
    void defaultDatasetLUT();
    void loadDatasetLUT(const QString & path);
    void datasetColorAdjustmentsChanged();
    bool vpGenerateTexture(ViewportOrtho & vp);
    void setRotation(const QQuaternion & quaternion);
    void resetRotation();
    void calculateMissingOrthoGPUCubes(TextureLayer & layer);
    void dc_reslice_notify_visible();
    void dc_reslice_notify_all(const Coordinate coord);
    void oc_reslice_notify_visible();
    void oc_reslice_notify_all(const Coordinate coord);
    void setMovementAreaFactor(float alpha);
    uint highestMag();
    uint lowestMag();
    float highestScreenPxXPerDataPx(const bool ofCurrentMag = true);
    float lowestScreenPxXPerDataPx(const bool ofCurrentMag = true);
    uint calcMag(const float screenPxXPerDataPx);
    void setMagnificationLock(const bool locked);
};

#endif // VIEWER_H
