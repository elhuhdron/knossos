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

#include "widgets/viewport.h"
#include <iostream>
#include "functions.h"
#include "gui_wrapper.h"
#include "coordinate.h"
#ifdef WITH_PYTHON_QT
#include "scriptengine/scripting.h"
#endif
#include "segmentation/cubeloader.h"
#include "segmentation/segmentation.h"
#include "segmentation/segmentationsplit.h"
#include "session.h"
#include "skeleton/skeletonizer.h"
#include "skeleton/tree.h"
#include "viewer.h"
#include "widgets/preferences/navigationtab.h"
#include "widgets/mainwindow.h"

#include <QApplication>
#include <QMessageBox>

#include <boost/optional.hpp>

#include <cstdlib>
#include <unordered_set>

void merging(const QMouseEvent *event, ViewportOrtho & vp) {
    //std::cout << "merging " << std::endl;
    auto & seg = Segmentation::singleton();
    const auto brushCenter = getCoordinateFromOrthogonalClick(event->x(), event->y(), vp);
    const auto subobjectIds = readVoxels(brushCenter, seg.brush.value());
    for (const auto subobjectPair : subobjectIds) {

        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            // mode for deleting subjobects (unmerge and delete subobject)
            //std::cout << "delete subobject " << std::endl;
            auto & segmentation = Segmentation::singleton();

            //const auto clickPos = getCoordinateFromOrthogonalClick(event->x(), event->y(), *this);
            //const auto subobjectId = readVoxel(clickPos);
            const auto subobjectId = subobjectPair.first;
            const auto clickPos = subobjectPair.second;

            //rutuja - to delete a subobject from object
            //if (subobjectId != 0 && event->modifiers().testFlag(Qt::ShiftModifier)) {// delete a subobject
            // watkinspv - flag checked above already, do nothing if suboject is not part of any current object
            if( subobjectId != 0 && segmentation.subobjectExists(subobjectId) ) {

                auto & subobject = segmentation.subobjectFromId(subobjectId, clickPos);
                auto objIndex = segmentation.largestObjectContainingSubobject(subobject);
                if (segmentation.isSelected(objIndex)) {// unselect if selected
                    //std::cout << "\tselected " << std::endl;

                    auto & object = segmentation.objects.at(objIndex);
                    // watkinspv - do not delete from non-active (meaning not selected in object list) objects
                    if( object.active ) {
                        segmentation.unselectObject(object);
                        segmentation.remObject(subobjectId,object);
                        segmentation.selectObject(object);

                        if(state->hdf5_found && state->mode == 1){
                            segmentation.cell_delete();
                            segmentation.delete_seg_lvl(subobjectId);
                        }

                        // watkinspv - delete object if removing last subobject
                        if( object.subobjects.size() == 0 ) {
                            segmentation.removeObject(object);
                        }
                    }
                // watkinspv - removed this in favor of doing nothing if object does not exist
                //} else {
                //    std::cout << "\tnot selected " << std::endl;
                //    // watkinspv - if the object was not selected, remove the object that was created
                //    segmentation.removeObject(segmentation.objects.back());
                }
            }
        } else if (seg.activeObjectsCount() == 1) {//rutuja
            //std::cout << "merge subobject " << std::endl;
            // mode for merging subjobects (creates a new object and then merges subobject into current object)
            const auto soid = subobjectPair.first;
            const auto pos = subobjectPair.second;
            auto & subobject = seg.subobjectFromId(soid, pos);
            const auto objectToMergeId = seg.smallestImmutableObjectContainingSubobject(subobject);

            //rutuja - get superchunkid
            state->viewer->setSuperChunk(pos);
            state->viewer->setSuperChunkCoordinate(state->viewer->superChunkId);
            //std::cout << state->viewer->superChunkId.x << " " << state->viewer->superChunkId.y << " "
            //           << state->viewer->superChunkId.z << std::endl;
            //std::cout << state->viewer->super_start_coord.x << " " << state->viewer->super_start_coord.y << " "
            //           << state->viewer->super_start_coord.z << std::endl;

            // START OLD unmerge code
            // if clicked object is currently selected, an unmerge is requested
            /*
            if (seg.isSelected(subobject)) {
                if (event->modifiers().testFlag(Qt::ShiftModifier)) {
                    if (event->modifiers().testFlag(Qt::ControlModifier)) {
                        seg.selectObjectFromSubObject(subobject, pos);
                        seg.unmergeSelectedObjects(pos);
                    } else {
                        if(seg.isSelected(objectToMergeId)) { // if no other object to unmerge, just unmerge subobject
                            seg.selectObjectFromSubObject(subobject, pos);
                        }
                        else {
                            seg.selectObject(objectToMergeId);
                        }
                        seg.unmergeSelectedObjects(pos);

                    }
                }
            } else { // object is not selected, so user wants to merge
            */
            // END OLD unmerge code

            // do a merge if object is not currently selected
            if (!seg.isSelected(subobject)) {
                if (!event->modifiers().testFlag(Qt::ShiftModifier)) {
                    if (event->modifiers().testFlag(Qt::ControlModifier)) {
                        seg.selectObjectFromSubObject(subobject, pos);
                    } else {
                        seg.selectObject(objectToMergeId);//select largest object
                    }
                    if (seg.activeObjectsCount() >= 2) {//rutuja
                        seg.mergeSelectedObjects();
                    }
                    //rutuja - mesh for merging task
                    if(state->hdf5_found && state->mode == 1)
                    {
                            auto objIndex = seg.largestObjectContainingSubobject(subobject);
                            std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>color = seg.get_active_color();
                            auto obj = seg.objects.at(objIndex);
                            supervoxel info;
                            info.seed = soid;
                            info.objid = obj.id;
                            info.color = color;
                            info.show = true;
                            seg.superChunkids.insert(std::make_pair(soid,state->viewer->super_start_coord));
                            seg.seg_level_list.insert(std::make_pair(soid,state->segmentation_level));
                            state->viewer->supervoxel_info.push_back(info);
                            state->viewer->hdf5_read(info);
                            //emit seg.beforemerge();
                            seg.setCurrentmergeid(soid);
                            emit seg.merge();
                    }
                    seg.touchObjects(soid);
                } else {
                    seg.removeObject(seg.objects.back());
                }

            }

        }

    } // for subobject pairs
}

void segmentation_brush_work(const QMouseEvent *event, ViewportOrtho & vp) {
    //std::cout << "brush work " << std::endl;
    const Coordinate coord = getCoordinateFromOrthogonalClick(event->x(), event->y(), vp);
    auto & seg = Segmentation::singleton();

    if (Session::singleton().annotationMode.testFlag(AnnotationMode::ObjectMerge)) {
        merging(event, vp);
    } else if (Session::singleton().annotationMode.testFlag(AnnotationMode::Mode_Paint)) {//paint and erase
        if (!seg.brush.isInverse() && seg.selectedObjectsCount() == 0) {
            seg.createAndSelectObject(coord);
        }
        if (seg.selectedObjectsCount() > 0) {
            uint64_t soid = seg.subobjectIdOfFirstSelectedObject(coord);
            writeVoxels(coord, soid, seg.brush.value());
        }
    }
}


void ViewportOrtho::handleMouseHover(const QMouseEvent *event) {
    auto coord = getCoordinateFromOrthogonalClick(event->x(), event->y(), *this);
    emit cursorPositionChanged(coord, viewportType);
    auto subObjectId = readVoxel(coord);
    Segmentation::singleton().hoverSubObject(subObjectId);
#ifdef WITH_PYTHON_QT
    EmitOnCtorDtor eocd(&SignalRelay::Signal_EventModel_handleMouseHover, state->signalRelay, coord, subObjectId, viewportType, event);
#endif
    if(Segmentation::singleton().hoverVersion && Segmentation::enabled) {
        Segmentation::singleton().mouseFocusedObjectId = Segmentation::singleton().tryLargestObjectContainingSubobject(subObjectId);
    }
    ViewportBase::handleMouseHover(event);
}

void startNodeSelection(const int x, const int y, const ViewportType vpType, const Qt::KeyboardModifiers modifiers) {
    state->viewerState->nodeSelectionSquare.first.x = x;
    state->viewerState->nodeSelectionSquare.first.y = y;

    // reset second point from a possible previous selection square.
    state->viewerState->nodeSelectionSquare.second = state->viewerState->nodeSelectionSquare.first;
    state->viewerState->nodeSelectSquareData = std::make_pair(vpType, modifiers);
}

void ViewportBase::handleLinkToggle(const QMouseEvent & event) {
    auto * activeNode = state->skeletonState->activeNode;
    auto clickedNode = pickNode(event.x(), event.y(), 10);
    if (clickedNode && activeNode != nullptr) {
        checkedToggleNodeLink(this, *activeNode, clickedNode.get());
    }
}

void ViewportBase::handleMouseButtonLeft(const QMouseEvent *event) {
    if (Session::singleton().annotationMode.testFlag(AnnotationMode::NodeEditing)) {
        const bool selection = event->modifiers().testFlag(Qt::ShiftModifier) || event->modifiers().testFlag(Qt::ControlModifier);
        if (selection) {
            startNodeSelection(event->pos().x(), event->pos().y(), viewportType, event->modifiers());
            return;
        }
        //Set Connection between Active Node and Clicked Node
        else if (QApplication::keyboardModifiers() == Qt::ALT) {
            handleLinkToggle(*event);
        }
    }
}

void ViewportBase::handleMouseButtonMiddle(const QMouseEvent *event) {
    if (event->modifiers().testFlag(Qt::ShiftModifier) && Session::singleton().annotationMode.testFlag(AnnotationMode::NodeEditing)) {
        handleLinkToggle(*event);
    }
}

void ViewportOrtho::handleMouseButtonMiddle(const QMouseEvent *event) {
    if (event->modifiers().testFlag(Qt::NoModifier) && Session::singleton().annotationMode.testFlag(AnnotationMode::NodeEditing)) {
        if (auto clickedNode = pickNode(event->x(), event->y(), 10)) {
            draggedNode = &clickedNode.get();
        }
    }
    ViewportBase::handleMouseButtonMiddle(event);
}

void ViewportOrtho::handleMouseButtonRight(const QMouseEvent *event) {
    if (!mouseEventAtValidDatasetPosition(event)) { //don’t place nodes outside movement area
        return;
    }

    const auto & annotationMode = Session::singleton().annotationMode;
    if (annotationMode.testFlag(AnnotationMode::Brush)) {
        Segmentation::singleton().brush.setInverse(event->modifiers().testFlag(Qt::ShiftModifier));
        segmentation_brush_work(event, *this);
        return;
    }

    Coordinate clickedCoordinate = getCoordinateFromOrthogonalClick(event->x(), event->y(), *this);
    const quint64 subobjectId = readVoxel(clickedCoordinate);
    const bool background = subobjectId == Segmentation::singleton().getBackgroundId();
    if (annotationMode.testFlag(AnnotationMode::Mode_MergeTracing) && background && !event->modifiers().testFlag(Qt::ControlModifier)) {
        return;
    }

    nodeListElement * oldNode = state->skeletonState->activeNode;
    boost::optional<nodeListElement &> newNode;

    if (annotationMode.testFlag(AnnotationMode::LinkedNodes)) {
        if (oldNode == nullptr || state->skeletonState->activeTree->nodes.empty()) {
            //no node to link with or empty tree
            newNode = Skeletonizer::singleton().UI_addSkeletonNode(clickedCoordinate, viewportType);
        } else if (event->modifiers().testFlag(Qt::ControlModifier)) {
            if (Session::singleton().annotationMode.testFlag(AnnotationMode::Mode_MergeTracing)) {
                const auto splitNode = Skeletonizer::singleton().UI_addSkeletonNode(clickedCoordinate, viewportType);
                if (splitNode) {
                    const auto comment = background ? "ecs" : "split";
                    Skeletonizer::singleton().setSubobject(splitNode.get(), subobjectId);
                    Skeletonizer::singleton().setComment(splitNode.get(), comment);
                    Skeletonizer::singleton().setActiveNode(oldNode);
                }
            } else if (auto stumpNode = Skeletonizer::singleton().addSkeletonNodeAndLinkWithActive(clickedCoordinate, viewportType, false)) {
                //Add a "stump", a branch node to which we don't automatically move.
                Skeletonizer::singleton().pushBranchNode(stumpNode.get());
                Skeletonizer::singleton().setActiveNode(oldNode);
            }
        } else {
            const auto lastPos = state->skeletonState->activeNode->position;
            newNode = Skeletonizer::singleton().addSkeletonNodeAndLinkWithActive(clickedCoordinate, viewportType, true);
            if (!newNode) {
                return;
            }
            const auto movement = clickedCoordinate - lastPos;
            //Highlight the viewport with the biggest movement component
            if ((std::abs(movement.x) >= std::abs(movement.y)) && (std::abs(movement.x) >= std::abs(movement.z))) {
                state->viewerState->highlightVp = VIEWPORT_ZY;
            } else if ((std::abs(movement.y) >= std::abs(movement.x)) && (std::abs(movement.y) >= std::abs(movement.z))) {
                state->viewerState->highlightVp = VIEWPORT_XZ;
            } else {
                state->viewerState->highlightVp = VIEWPORT_XY;
            }
            //Determine the directions for the f and d keys based on the signs of the movement components along the three dimensions
            state->viewerState->tracingDirection = movement;

            //Additional move of specified steps along tracing direction
            if (state->viewerState->autoTracingMode == Recentering::AheadOfNode) {
                floatCoordinate walking{movement};
                const auto factor = state->viewerState->autoTracingSteps / walking.length();
                clickedCoordinate += Coordinate(std::lround(movement.x * factor), std::lround(movement.y * factor), std::lround(movement.z * factor));
            }

            clickedCoordinate = clickedCoordinate.capped({0, 0, 0}, state->boundary);// Do not allow clicks outside the dataset

            if(state->skeletonState->synapseState == Synapse::State::PostSynapse) {
                //The synapse workflow has been interrupted
                //Reset the synapse
                auto & tempSynapse = state->skeletonState->temporarySynapse;
                if (tempSynapse.getPreSynapse()) { tempSynapse.getPreSynapse()->isSynapticNode = false; }
                Skeletonizer::singleton().delTree(tempSynapse.getCleft()->treeID);
                tempSynapse = Synapse(); //reset temporary class
                state->skeletonState->synapseState = Synapse::State::PreSynapse;
                state->viewer->window->setSynapseState(SynapseState::Off); //reset statusbar entry
            }
        }
    } else { // unlinked
        newNode = Skeletonizer::singleton().UI_addSkeletonNode(clickedCoordinate,viewportType);
    }

    if (newNode) {
        if (Session::singleton().annotationMode.testFlag(AnnotationMode::Mode_MergeTracing)) {
            Skeletonizer::singleton().setSubobjectSelectAndMergeWithPrevious(newNode.get(), subobjectId, oldNode);
        }
        // Move to the new node position
        if (state->viewerState->autoTracingMode != Recentering::Off) {
            if (viewportType == VIEWPORT_ARBITRARY) {
                state->viewer->setPositionWithRecenteringAndRotation(clickedCoordinate, viewportType);
            } else {
                state->viewer->setPositionWithRecentering(clickedCoordinate);
            }
        }
        auto & mainWin = *state->viewer->window;
        if (mainWin.segmentState == SegmentState::Off_Once) {
            mainWin.setSegmentState(SegmentState::On);
        }
        if(state->skeletonState->synapseState == Synapse::State::PostSynapse && state->skeletonState->activeTree->nodes.size() == 1) {
            auto & tempSynapse = state->skeletonState->temporarySynapse;
            tempSynapse.setPostSynapse(*state->skeletonState->activeNode);
            if (tempSynapse.getCleft() && tempSynapse.getPreSynapse() && tempSynapse.getPostSynapse()) {
                Skeletonizer::singleton().addFinishedSynapse(*tempSynapse.getCleft(), *tempSynapse.getPreSynapse(), *tempSynapse.getPostSynapse()); //move finished synapse to our synapse vector
            }
            state->skeletonState->synapseState = Synapse::State::PreSynapse;
            tempSynapse = Synapse(); //reset temporary class
            state->viewer->window->toggleSynapseState(); //update statusbar
        }
    }
    ViewportBase::handleMouseButtonRight(event);
}

floatCoordinate ViewportOrtho::handleMovement(const QPoint & pos) {
    const QPointF posDelta(xrel(pos.x()), yrel(pos.y()));
    const QPointF arbitraryMouseSlide = {-posDelta.x() / screenPxXPerDataPx, -posDelta.y() / screenPxYPerDataPx};
    const auto move = v1 * arbitraryMouseSlide.x() - v2 * arbitraryMouseSlide.y();
    return move;
}

void ViewportBase::handleMouseMotionLeftHold(const QMouseEvent *event) {
    // pull selection square
    if (state->viewerState->nodeSelectSquareData.first != -1) {
        state->viewerState->nodeSelectionSquare.second.x = event->pos().x();
        state->viewerState->nodeSelectionSquare.second.y = event->pos().y();
    }
}

void Viewport3D::handleMouseMotionLeftHold(const QMouseEvent *event) {
    if (event->modifiers() == Qt::NoModifier) {
        if (Segmentation::singleton().volume_render_toggle) {
            auto & seg = Segmentation::singleton();
            seg.volume_mouse_move_x -= xrel(event->x());
            seg.volume_mouse_move_y -= yrel(event->y());
        } else {
            state->skeletonState->translateX += -xrel(event->x()) / screenPxXPerDataPx;
            state->skeletonState->translateY += -yrel(event->y()) / screenPxXPerDataPx;
        }
    }
    ViewportBase::handleMouseMotionLeftHold(event);
}

void ViewportOrtho::handleMouseMotionLeftHold(const QMouseEvent *event) {
    if (event->modifiers() == Qt::NoModifier) {
        state->viewer->userMove(handleMovement(event->pos()), USERMOVE_HORIZONTAL, n);
    }
    ViewportBase::handleMouseMotionLeftHold(event);
}

void Viewport3D::handleMouseMotionRightHold(const QMouseEvent *event) {
    if (event->modifiers() == Qt::NoModifier && state->skeletonState->rotationcounter == 0) {
        state->skeletonState->rotdx += xrel(event->x());
        state->skeletonState->rotdy += yrel(event->y());
    }
    ViewportBase::handleMouseMotionRightHold(event);
}


void ViewportOrtho::handleMouseMotionRightHold(const QMouseEvent *event) {
    if (Session::singleton().annotationMode.testFlag(AnnotationMode::Brush)) {
        const bool notOrigin = event->pos() != mouseDown;//don’t do redundant work
        if (notOrigin) {
            segmentation_brush_work(event, *this);
        }
    }
    ViewportBase::handleMouseMotionRightHold(event);
}

void ViewportOrtho::handleMouseMotionMiddleHold(const QMouseEvent *event) {
    if (Session::singleton().annotationMode.testFlag(AnnotationMode::NodeEditing) && draggedNode != nullptr) {
        const auto moveAccurate = handleMovement(event->pos());
        arbNodeDragCache += moveAccurate;//accumulate subpixel movements
        Coordinate moveTrunc = arbNodeDragCache;//truncate
        arbNodeDragCache -= moveTrunc;//only keep remaining fraction
        const auto newPos = draggedNode->position - moveTrunc;
        Skeletonizer::singleton().editNode(0, draggedNode, 0., newPos, state->magnification, 0);
    }
    ViewportBase::handleMouseMotionMiddleHold(event);
}

void ViewportBase::handleMouseReleaseLeft(const QMouseEvent *event) {
    if (Session::singleton().annotationMode.testFlag(AnnotationMode::NodeEditing)) {
        QSet<nodeListElement*> selectedNodes;
        int diffX = std::abs(state->viewerState->nodeSelectionSquare.first.x - event->pos().x());
        int diffY = std::abs(state->viewerState->nodeSelectionSquare.first.y - event->pos().y());
        if ((diffX < 5 && diffY < 5) || (event->pos() - mouseDown).manhattanLength() < 5) { // interpreted as click instead of drag
            // mouse released on same spot on which it was pressed down: single node selection
            auto selectedNode = pickNode(event->pos().x(), event->pos().y(), 10);
            if (selectedNode) {
                selectedNodes = {&selectedNode.get()};
            }
        } else if (state->viewerState->nodeSelectSquareData.first != -1) {
            selectedNodes = nodeSelection(event->pos().x(), event->pos().y());
        }
        if (state->viewerState->nodeSelectSquareData.first != -1 || !selectedNodes.empty()) {//only select no nodes if we drew a selection rectangle
            if (state->viewerState->nodeSelectSquareData.second == Qt::ControlModifier) {
                Skeletonizer::singleton().toggleNodeSelection(selectedNodes);
            } else {
                Skeletonizer::singleton().selectNodes(selectedNodes);
            }
        }
        state->viewerState->nodeSelectSquareData = std::make_pair(-1, Qt::NoModifier);//disable node selection square
    }
}

void Viewport3D::handleMouseReleaseLeft(const QMouseEvent *event) {
    if (mouseDown == event->pos()) {// mouse click
        meshLastClickInformation = pickMesh(event->pos());
        meshLastClickCurrentlyVisited = false;
        if (meshLastClickInformation) {
            Skeletonizer::singleton().setActiveTreeByID(meshLastClickInformation.get().treeId);
        }
    }
    ViewportBase::handleMouseReleaseLeft(event);
}

void ViewportOrtho::handleMouseReleaseLeft(const QMouseEvent *event) {
    auto & segmentation = Segmentation::singleton();

    if (Session::singleton().annotationMode.testFlag(AnnotationMode::ObjectSelection) && mouseEventAtValidDatasetPosition(event)) { // in task mode the object should not be switched
        if (event->pos() == mouseDown) {// mouse click
            const auto clickPos = getCoordinateFromOrthogonalClick(event->x(), event->y(), *this);
            const auto subobjectId = readVoxel(clickPos);


            if (subobjectId != segmentation.getBackgroundId() && segmentation.createandselect) {// don’t select the unsegmented area as object
                auto & subobject = segmentation.subobjectFromId(subobjectId, clickPos);
                auto objIndex = segmentation.largestObjectContainingSubobject(subobject);
                segmentation.createandselect = false;
                state->viewer->setSuperChunk(clickPos);
                state->viewer->setSuperChunkCoordinate(state->viewer->superChunkId);
                //std::cout << state->viewer->super_start_coord.x << std::endl;
                //if (!event->modifiers().testFlag(Qt::ControlModifier)) { // watkinspv - what was this for?

                // watkinspv - modified change_colors to only change current and previous selection
                const auto lastActiveid = (segmentation.activeIndices.size() > 0 ?
                                           segmentation.objects.at(segmentation.activeIndices.back()).id : 0);

                segmentation.clearActiveSelection();//rutuja
                segmentation.selectObject(objIndex);
                auto object = segmentation.objects.at(objIndex);
                std::tuple<uint8_t,uint8_t,uint8_t,uint8_t> color = segmentation.get_active_color();

                if(state->hdf5_found && state->mode == 1){

                    supervoxel info;
                    info.seed = subobjectId;
                    info.objid = object.id;
                    info.color = color;
                    info.show = true;
                    state->viewer->supervoxel_info.push_back(info);
                    segmentation.superChunkids.insert(std::make_pair(subobjectId,state->viewer->super_start_coord));
                    segmentation.seg_level_list.insert(std::make_pair(subobjectId,state->segmentation_level));
                    state->viewer->hdf5_read(info);

                    // to color all the selected supervoxels by their respective colors once they are not the current active selection
                    //segmentation.change_colors(object.id);
                    // watkinspv - modified change_colors to only change current and previous selection
                    segmentation.change_colors(lastActiveid, 0);

                    // add subobjects to segmentation tab window - seg lvl and cube positions
                    //emit segmentation.beforemerge();
                    segmentation.setCurrentmergeid(subobjectId);
                    emit segmentation.appendmerge();
                }

                //}
                //rutuja - removed this from original code
                /* } else if (segmentation.isSelected(objIndex)) {// unselect if selected
                    segmentation.unselectObject(objIndex);
                } else { // select largest object
                    segmentation.selectObject(objIndex);
                }*/
                if (segmentation.isSelected(subobject)) {//touch other objects containing this subobject
                    segmentation.touchObjects(subobjectId);
                } else {
                    segmentation.untouchObjects();
                }
            }
            /* watkinspv - moved this to work with shift-right click instead
            //rutuja - to delete a subobject from object
            if (subobjectId != 0 && event->modifiers().testFlag(Qt::ShiftModifier)) {// delete a subobject

                auto & subobject = segmentation.subobjectFromId(subobjectId, clickPos);
                auto objIndex = segmentation.largestObjectContainingSubobject(subobject);
                if (segmentation.isSelected(objIndex)) {// unselect if selected

                    auto & object = segmentation.objects.at(objIndex);
                    segmentation.unselectObject(object);
                    segmentation.remObject(subobjectId,object);
                    segmentation.selectObject(object);

                    if(state->hdf5_found && state->mode == 1){

                        segmentation.cell_delete();
                        segmentation.delete_seg_lvl(subobjectId);


                    }
                }

            }
            */
        }
    }
    state->viewer->userMoveClear();//finish dataset drag

    ViewportBase::handleMouseReleaseLeft(event);
}

void ViewportOrtho::handleMouseReleaseRight(const QMouseEvent *event) {
    if (Session::singleton().annotationMode.testFlag(AnnotationMode::Brush)) {
        if (event->pos() != mouseDown) {//merge took already place on mouse down
            segmentation_brush_work(event, *this);
        }
    }
    ViewportBase::handleMouseReleaseRight(event);

}

void ViewportOrtho::handleMouseReleaseMiddle(const QMouseEvent *event) {
    if (mouseEventAtValidDatasetPosition(event)) {
        Coordinate clickedCoordinate = getCoordinateFromOrthogonalClick(event->x(), event->y(), *this);
#ifdef WITH_PYTHON_QT
        EmitOnCtorDtor eocd(&SignalRelay::Signal_EventModel_handleMouseReleaseMiddle, state->signalRelay, clickedCoordinate, viewportType, event);
#endif
        auto & seg = Segmentation::singleton();
        if (Session::singleton().annotationMode.testFlag(AnnotationMode::Mode_Paint) && seg.selectedObjectsCount() == 1) {
            auto brush_copy = seg.brush.value();
            uint64_t soid = brush_copy.inverse ? seg.getBackgroundId() : seg.subobjectIdOfFirstSelectedObject(clickedCoordinate);
            brush_copy.shape = brush_t::shape_t::angular;
            brush_copy.radius = displayedlengthInNmX / 2;//set brush to fill visible area
            subobjectBucketFill(clickedCoordinate, state->viewerState->currentPosition, soid, brush_copy);
        }
    }
    //finish node drag
    arbNodeDragCache = {};
    draggedNode = nullptr;

    ViewportBase::handleMouseReleaseMiddle(event);
}

void ViewportBase::handleWheelEvent(const QWheelEvent *event) {
    if (QApplication::activeWindow() != nullptr) {//only if active widget belongs to application
        activateWindow();//steal keyboard focus
    }
    setFocus();//get keyboard focus for this widget for viewport specific shortcuts

    const int directionSign = event->delta() > 0 ? -1 : 1;
    auto& seg = Segmentation::singleton();

    if (Session::singleton().annotationMode.testFlag(AnnotationMode::NodeEditing)
            && event->modifiers() == Qt::SHIFT
            && state->skeletonState->activeNode != nullptr)
    {//change node radius
        float radius = state->skeletonState->activeNode->radius + directionSign * 0.2 * state->skeletonState->activeNode->radius;
        Skeletonizer::singleton().editNode(0, state->skeletonState->activeNode, radius, state->skeletonState->activeNode->position, state->magnification, state->skeletonState->activeNode->synapse_check);;
    } else if (Session::singleton().annotationMode.testFlag(AnnotationMode::Brush) && event->modifiers() == Qt::SHIFT) {
        auto curRadius = seg.brush.getRadius();
        seg.brush.setRadius(std::max(curRadius + (int)((event->delta() / 120) *
                                                  // brush radius delta factor (float), as a function of current radius
                                                  std::pow(curRadius + 1, 0.5)
                                                  ), 1));
    }
}

void Viewport3D::handleWheelEvent(const QWheelEvent *event) {
    if (event->modifiers() == Qt::NoModifier) {
        if(Segmentation::singleton().volume_render_toggle) {
            auto& seg = Segmentation::singleton();
            seg.volume_mouse_zoom *= (event->delta() > 0) ? 1.1f : 0.9f;
        } else {
            const QPointF mouseRel{(event->x() - 0.5 * width()), (event->y() - 0.5 * height())};
            const auto oldZoom = zoomFactor;
            if (event->delta() > 0) {
                zoomIn();
            } else {
                zoomOut();
            }
            const auto oldFactor = state->skeletonState->volBoundary / oldZoom;
            const auto newFactor = state->skeletonState->volBoundary / zoomFactor;
            state->skeletonState->translateX += mouseRel.x() * (oldFactor - newFactor) / width();
            state->skeletonState->translateY += mouseRel.y() * (oldFactor - newFactor) / height();
        }
    }
    ViewportBase::handleWheelEvent(event);
}

void ViewportOrtho::handleWheelEvent(const QWheelEvent *event) {
    if (event->modifiers() == Qt::CTRL) { // Orthogonal VP or outside VP
        if(event->delta() > 0) {
            zoomIn();
        }
        else {
            zoomOut();
        }
    } else if (event->modifiers() == Qt::NoModifier) {
        const float directionSign = event->delta() > 0 ? -1 : 1;
        const auto multiplier = directionSign * state->viewerState->dropFrames * state->magnification;
        state->viewer->userMove(n * multiplier, USERMOVE_DRILL, n);
    }
    ViewportBase::handleWheelEvent(event);
}

void ViewportBase::handleKeyPress(const QKeyEvent *event) {
    const auto ctrl = event->modifiers().testFlag(Qt::ControlModifier);
    const auto alt = event->modifiers().testFlag(Qt::AltModifier);
    const auto shift = event->modifiers().testFlag(Qt::ShiftModifier);
    if (event->key() == Qt::Key_F11) {
        fullscreenAction.trigger();
    } else if (ctrl && shift && event->key() == Qt::Key_C) {
        if(state->skeletonState->activeNode && state->skeletonState->activeNode->isSynapticNode) {
            state->skeletonState->activeNode->correspondingSynapse->toggleDirection();
        }
    } else if (shift) {
        if (!event->isAutoRepeat() && state->viewerState->keyRepeat) {// if ctrl was pressed initially don’t apply it again
            state->viewerState->repeatDirection *= 10;// increase movement speed
        }
        Segmentation::singleton().brush.setInverse(true);// enable erase mode on shift down
    } else if(event->key() == Qt::Key_K || event->key() == Qt::Key_L) {
        const float angle = ctrl ? -1: 1;
        switch(event->key()) {
        case Qt::Key_K:
            state->viewer->addRotation(QQuaternion::fromAxisAndAngle(state->viewer->viewportArb->n, angle));
            break;
        case Qt::Key_L:
            state->viewer->addRotation(QQuaternion::fromAxisAndAngle(state->viewer->viewportArb->v2, angle));
            break;
        }
    } else if (ctrl && event->key() == Qt::Key_0) {
        state->viewer->zoomReset();
    } else if(event->key() == Qt::Key_Plus && ctrl) {
        zoomIn();
    } else if(event->key() == Qt::Key_Minus && ctrl) {
        zoomOut();
    } else if(event->key() == Qt::Key_V) {
        if(ctrl) {
            emit pasteCoordinateSignal();
        }
    } else if(event->key() == Qt::Key_Space) {
        state->viewerState->showOnlyRawData = true;
        state->viewer->mainWindow.forEachVPDo([] (ViewportBase & vp) {
            vp.showHideButtons(false);
        });
        state->viewer->oc_reslice_notify_visible();
    } else if(event->key() == Qt::Key_Delete) {
        if(ctrl) {
            if(state->skeletonState->activeTree) {
               // Skeletonizer::singleton().delTree(state->skeletonState->activeTree->treeID);
            }
        } else if(state->skeletonState->selectedNodes.size() > 0) {
            bool deleteNodes = true;
            if(state->skeletonState->selectedNodes.size() != 1) {
                QMessageBox prompt;
                prompt.setWindowFlags(Qt::WindowStaysOnTopHint);
                prompt.setIcon(QMessageBox::Question);
                prompt.setWindowTitle("Cofirmation required");
                prompt.setText("Delete selected nodes?");
                QPushButton *confirmButton = prompt.addButton("Yes", QMessageBox::ActionRole);
                prompt.addButton("No", QMessageBox::ActionRole);
                prompt.exec();
                deleteNodes = prompt.clickedButton() == confirmButton;
            }
            if(deleteNodes) {
                Skeletonizer::singleton().deleteSelectedNodes();
            }
        }
    } else if(event->key() == Qt::Key_Escape) {
        if(state->skeletonState->selectedNodes.size() == 1
           && state->skeletonState->activeNode->selected) {
            // active node must always be selected if nothing else is selected.
            return;
        }
        if(state->skeletonState->selectedNodes.empty() == false) {
            QMessageBox prompt;
            prompt.setWindowFlags(Qt::WindowStaysOnTopHint);
            prompt.setIcon(QMessageBox::Question);
            prompt.setWindowTitle("Cofirmation required");
            prompt.setText("Unselect current node selection?");
            QPushButton *confirmButton = prompt.addButton("Yes", QMessageBox::ActionRole);
            prompt.addButton("No", QMessageBox::ActionRole);
            prompt.exec();
            if(prompt.clickedButton() == confirmButton) {
                Skeletonizer::singleton().setActiveNode(state->skeletonState->activeNode);
            }
        }
    } else if(event->key() == Qt::Key_F4) {
        if(alt) {
            QApplication::closeAllWindows();
        }
    }
}

void ViewportOrtho::handleKeyPress(const QKeyEvent *event) {
    //events
    //↓          #   #   #   #   #   #   #   # ↑  ↓          #  #  #…
    //^ os delay ^       ^---^ os key repeat

    //intended behavior:
    //↓          # # # # # # # # # # # # # # # ↑  ↓          # # # #…
    //^ os delay ^       ^-^ knossos specified interval

    //after a ›#‹ event state->viewerKeyRepeat instructs the viewer to check in each frame if a move should be performed
    const bool keyD = event->key() == Qt::Key_D;
    const bool keyF = event->key() == Qt::Key_F;
    const bool keyLeft = event->key() == Qt::Key_Left;
    const bool keyRight = event->key() == Qt::Key_Right;
    const bool keyDown = event->key() == Qt::Key_Down;
    const bool keyUp = event->key() == Qt::Key_Up;
    const bool keyE = event->key() == Qt::Key_E;
    if (!event->isAutoRepeat()) {
        const int shiftMultiplier = event->modifiers().testFlag(Qt::ShiftModifier) ? 10 : 1;
        const auto direction = (n * -1).dot(state->viewerState->tracingDirection) >= 0 ? 1 : -1;// reverse n into the frame
        const float directionSign = (keyLeft || keyUp) ? -1 : (keyRight || keyDown) ? 1 : direction * (keyD || keyE ? -1 : 1);
        if (keyLeft || keyRight || keyDown || keyUp || keyD || keyF) {
            const auto vector = (keyLeft || keyRight) ? v1 : (keyUp || keyDown) ? (v2 * -1) : (n * -1); // transform v2 and n from 1. to 8. octant
            state->viewerState->repeatDirection = vector * directionSign * shiftMultiplier * state->viewerState->dropFrames * state->magnification;
            state->viewer->userMove(state->viewerState->repeatDirection, USERMOVE_HORIZONTAL, n);
        } else if(event->key() == Qt::Key_R || keyE) {
            state->viewer->setPositionWithRecentering(state->viewerState->currentPosition - n * directionSign * shiftMultiplier * state->viewerState->walkFrames * state->magnification);
        }
    } else if (keyD || keyF || keyLeft || keyRight || keyDown || keyUp) {
        // movement key pressed
        state->viewerState->keyRepeat = true;
    }
    ViewportBase::handleKeyPress(event);
}

void ViewportBase::handleKeyRelease(const QKeyEvent *event) {
    if(event->key() == Qt::Key_Space) {
        state->viewerState->showOnlyRawData = false;
        state->viewer->mainWindow.forEachVPDo([] (ViewportBase & vp) {
            vp.showHideButtons(state->viewerState->showVpDecorations);
        });
        state->viewer->oc_reslice_notify_visible();
    }
}

void Viewport3D::handleKeyPress(const QKeyEvent *event) {
    if (event->key() == Qt::Key_W && !event->isAutoRepeat()) {// real key press
        wiggletimer.start();
        wiggleButton.setChecked(true);
    }
    ViewportBase::handleKeyPress(event);
}

void Viewport3D::resetWiggle() {
    wiggletimer.stop();
    state->skeletonState->rotdx -= wiggle;
    state->skeletonState->rotdy -= wiggle;
    wiggleDirection = true;
    wiggle = 0;
    wiggleButton.setChecked(false);
}

void Viewport3D::handleKeyRelease(const QKeyEvent *event) {
    if (event->key() == Qt::Key_W && !event->isAutoRepeat()) {// real key release
        resetWiggle();
    }
    ViewportBase::handleKeyRelease(event);
}

void Viewport3D::focusOutEvent(QFocusEvent * event) {
    resetWiggle();
    QWidget::focusOutEvent(event);
}

Coordinate getCoordinateFromOrthogonalClick(const int x_dist, const int y_dist, ViewportOrtho & vp) {
    const auto leftUpper = floatCoordinate{state->viewerState->currentPosition} - (vp.v1 * vp.edgeLength / vp.screenPxXPerDataPx - vp.v2 * vp.edgeLength / vp.screenPxYPerDataPx) * 0.5;
    return floatCoordinate{leftUpper + (x_dist / vp.screenPxXPerDataPx) * vp.v1 - (y_dist / vp.screenPxYPerDataPx) * vp.v2};
}

bool ViewportOrtho::mouseEventAtValidDatasetPosition(const QMouseEvent *event) {
    if(event->x() < 0 || event->x() > (int)edgeLength ||
       event->y() < 0 || event->y() > (int)edgeLength) {
            return false;
    }

    Coordinate pos = getCoordinateFromOrthogonalClick(event->x(), event->y(), *this);
    const auto min = Session::singleton().movementAreaMin;
    const auto max = Session::singleton().movementAreaMax;
    //check if coordinates are in range
    if((pos.x < min.x) || (pos.x > max.x)
        ||(pos.y < min.y) || (pos.y > max.y)
        ||(pos.z < min.z) || (pos.z > max.z)) {
        return false;
    }
    return true;
}

QSet<nodeListElement*> ViewportBase::nodeSelection(int x, int y) {
    // node selection square
    state->viewerState->nodeSelectionSquare.second.x = x;
    state->viewerState->nodeSelectionSquare.second.y = y;
    Coordinate first = state->viewerState->nodeSelectionSquare.first;
    Coordinate second = state->viewerState->nodeSelectionSquare.second;
    // create square
    int minX, maxX, minY, maxY;
    minX = std::min(first.x, second.x);
    maxX = std::max(first.x, second.x);
    minY = std::min(first.y, second.y);
    maxY = std::max(first.y, second.y);
    const auto width = std::abs(maxX - minX);
    const auto height = std::abs(maxY - minY);
    const auto centerX = minX + width / 2;
    const auto centerY = minY + height / 2;
    return pickNodes(centerX, centerY, width, height);
}

Coordinate ViewportOrtho::getMouseCoordinate() {
    return getCoordinateFromOrthogonalClick(prevMouseMove.x(), prevMouseMove.y(), *this);
}
