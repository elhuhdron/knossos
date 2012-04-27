/*
 *  This file is a part of KNOSSOS.
 *
 *  (C) Copyright 2007-2012
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

/*
 *      Code for whatever.
 *
 *      Explain it.
 *
 *      Functions explained in the header file.
 */

#define GLUT_DISABLE_ATEXIT_HACK

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <math.h>

#include <agar/core.h>
#include <agar/gui.h>

#include "knossos-global.h"
#include "renderer.h"

extern struct stateInfo *tempConfig;
extern struct stateInfo *state;

uint32_t drawGUI() {
    AG_Window *win;

    /* right place? TDitem */
    updateAGconfig();

    updateDisplayListsSkeleton(state);

    /* Check for openGL errors TDitem: move to better place */
    /*while(TRUE) {
        GLenum error;
        error = glGetError();
        if(error == GL_NO_ERROR) break;
        switch(error) {
            case GL_INVALID_ENUM:
                LOG("OpenGL error: GL_INVALID_ENUM");
                break;
            case GL_INVALID_VALUE:
                LOG("OpenGL error: GL_INVALID_VALUE");
                break;
            case GL_INVALID_OPERATION:
                LOG("OpenGL error: GL_INVALID_OPERATION");
                break;
            case GL_STACK_OVERFLOW:
                LOG("OpenGL error: GL_STACK_OVERFLOW");
                break;
            case GL_STACK_UNDERFLOW:
                LOG("OpenGL error: GL_STACK_UNDERFLOW");
                break;
            case GL_OUT_OF_MEMORY:
                LOG("OpenGL error: GL_OUT_OF_MEMORY");
                break;
            case GL_TABLE_TOO_LARGE:
                LOG("OpenGL error: GL_TABLE_TOO_LARGE");
                break;
        }
    }*/

    glViewport(0,
               0,
               state->viewerState->screenSizeX,
               state->viewerState->screenSizeY);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0, state->viewerState->screenSizeX,
        state->viewerState->screenSizeY, 0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* TEXTURE_2D has to be enabled for agar drawing - why?! */
    glEnable(GL_TEXTURE_2D);

    /* Render GUI elements */

    AG_LockVFS(&agDrivers);

    AG_BeginRendering(agDriverSw); //AGAR14
    AG_FOREACH_WINDOW(win, agDriverSw) {
        AG_ObjectLock(win);
        AG_WindowDraw(win);
        AG_ObjectUnlock(win);
    }

    glDisable(GL_TEXTURE_2D);

    if(!state->viewerState->splash) {
        AG_EndRendering(agDriverSw);
    }

    AG_UnlockVFS(&agDrivers);

    return TRUE;
}


/*
 * The following function draws the passed orthogonal VP.
*/
uint32_t renderOrthogonalVP(uint32_t currentVP, struct stateInfo *state)  {
    float dataPxX, dataPxY;

    if(!((state->viewerState->viewPorts[currentVP].type == VIEWPORT_XY)
            || (state->viewerState->viewPorts[currentVP].type == VIEWPORT_XZ)
            || (state->viewerState->viewPorts[currentVP].type == VIEWPORT_YZ))) {
        LOG("Wrong VP type given for renderOrthogonalVP() call.");
        return FALSE;
    }

    /* probably not needed TDitem
    glColor4f(0.5, 0.5, 0.5, 1.);
    glBegin(GL_QUADS);
        glVertex2d(0, 0);
        glVertex2d(state->viewerState->viewPorts[currentVP].edgeLength, 0);
        glVertex2d(state->viewerState->viewPorts[currentVP].edgeLength, state->viewerState->viewPorts[currentVP].edgeLength);
        glVertex2d(0, state->viewerState->viewPorts[currentVP].edgeLength);
    glEnd();
    */


    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    //glClear(GL_DEPTH_BUFFER_BIT); /* better place? TDitem */

    if(!state->viewerState->selectModeFlag) {
        if(state->viewerState->multisamplingOnOff) glEnable(GL_MULTISAMPLE);

        if(state->viewerState->lightOnOff) {
            /* Configure light. optimize that! TDitem */
            glEnable(GL_LIGHTING);
            GLfloat ambientLight[] = {0.5, 0.5, 0.5, 0.8};
            GLfloat diffuseLight[] = {1., 1., 1., 1.};
            GLfloat lightPos[] = {0., 0., 1., 1.};

            glLightfv(GL_LIGHT0,GL_AMBIENT,ambientLight);
            glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuseLight);
            glLightfv(GL_LIGHT0,GL_POSITION,lightPos);
            glEnable(GL_LIGHT0);

            GLfloat global_ambient[] = { 0.5f, 0.5f, 0.5f, 1.0f };
            glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);

            /* Enable materials with automatic color assignment */
            glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
            glEnable(GL_COLOR_MATERIAL);
        }
    }

    dataPxX = state->viewerState->viewPorts[currentVP].texture.displayedEdgeLengthX / state->viewerState->viewPorts[currentVP].texture.texUnitsPerDataPx * 0.5;
    dataPxY = state->viewerState->viewPorts[currentVP].texture.displayedEdgeLengthY / state->viewerState->viewPorts[currentVP].texture.texUnitsPerDataPx * 0.5;
    switch(state->viewerState->viewPorts[currentVP].type) {
        case VIEWPORT_XY:
            if(!state->viewerState->selectModeFlag) {
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
            }
            /* left, right, bottom, top, near, far clipping planes */
            glOrtho(-((float)state->boundary.x / 2.) + (float)state->viewerState->currentPosition.x - dataPxX,
                -((float)state->boundary.x / 2.) + (float)state->viewerState->currentPosition.x + dataPxX,
                -((float)state->boundary.y / 2.) + (float)state->viewerState->currentPosition.y - dataPxY,
                -((float)state->boundary.y / 2.) + (float)state->viewerState->currentPosition.y + dataPxY,
                ((float)state->boundary.z / 2.) - state->viewerState->depthCutOff - (float)state->viewerState->currentPosition.z,
                ((float)state->boundary.z / 2.) + state->viewerState->depthCutOff - (float)state->viewerState->currentPosition.z);

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            /*optimize that! TDitem */

            glTranslatef(-((float)state->boundary.x / 2.),
                        -((float)state->boundary.y / 2.),
                        -((float)state->boundary.z / 2.));

            glTranslatef((float)state->viewerState->currentPosition.x,
                        (float)state->viewerState->currentPosition.y,
                        (float)state->viewerState->currentPosition.z);

            glRotatef(180., 1.,0.,0.);

            glLoadName(3);

            glEnable(GL_TEXTURE_2D);
            glDisable(GL_DEPTH_TEST);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glColor4f(1., 1., 1., 1.);

            glBindTexture(GL_TEXTURE_2D, state->viewerState->viewPorts[currentVP].texture.texHandle);
            glBegin(GL_QUADS);
                glNormal3i(0,0,1);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLUx, state->viewerState->viewPorts[currentVP].texture.texLUy);
                glVertex3f(-dataPxX, -dataPxY, 0.);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRUx, state->viewerState->viewPorts[currentVP].texture.texRUy);
                glVertex3f(dataPxX, -dataPxY, 0.);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRLx, state->viewerState->viewPorts[currentVP].texture.texRLy);
                glVertex3f(dataPxX, dataPxY, 0.);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLLx, state->viewerState->viewPorts[currentVP].texture.texLLy);
                glVertex3f(-dataPxX, dataPxY, 0.);
            glEnd();
            glBindTexture (GL_TEXTURE_2D, 0);
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_DEPTH_TEST);


            glTranslatef(-(float)state->viewerState->currentPosition.x, -(float)state->viewerState->currentPosition.y, -(float)state->viewerState->currentPosition.z);
            glTranslatef(((float)state->boundary.x / 2.),((float)state->boundary.y / 2.),((float)state->boundary.z / 2.));

            if(state->skeletonState->displayListSkeletonSlicePlaneVP) glCallList(state->skeletonState->displayListSkeletonSlicePlaneVP);

            glTranslatef(-((float)state->boundary.x / 2.),-((float)state->boundary.y / 2.),-((float)state->boundary.z / 2.));
            glTranslatef((float)state->viewerState->currentPosition.x, (float)state->viewerState->currentPosition.y, (float)state->viewerState->currentPosition.z);
            glLoadName(3);

            glEnable(GL_TEXTURE_2D);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glColor4f(1., 1., 1., 0.6);

            glBindTexture(GL_TEXTURE_2D, state->viewerState->viewPorts[currentVP].texture.texHandle);
            glBegin(GL_QUADS);
                glNormal3i(0,0,1);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLUx, state->viewerState->viewPorts[currentVP].texture.texLUy);
                glVertex3f(-dataPxX, -dataPxY, 1.);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRUx, state->viewerState->viewPorts[currentVP].texture.texRUy);
                glVertex3f(dataPxX, -dataPxY, 1.);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRLx, state->viewerState->viewPorts[currentVP].texture.texRLy);
                glVertex3f(dataPxX, dataPxY, 1.);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLLx, state->viewerState->viewPorts[currentVP].texture.texLLy);
                glVertex3f(-dataPxX, dataPxY, 1.);
            glEnd();

            /* Draw the overlay textures */
            if(state->overlay) {
                if(state->viewerState->overlayVisible) {
                    glBindTexture(GL_TEXTURE_2D, state->viewerState->viewPorts[currentVP].texture.overlayHandle);
                    glBegin(GL_QUADS);
                        glNormal3i(0, 0, 1);
                        glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLUx,
                                     state->viewerState->viewPorts[currentVP].texture.texLUy);
                        glVertex3f(-dataPxX, -dataPxY, -0.1);
                        glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRUx,
                                     state->viewerState->viewPorts[currentVP].texture.texRUy);
                        glVertex3f(dataPxX, -dataPxY, -0.1);
                        glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRLx,
                                     state->viewerState->viewPorts[currentVP].texture.texRLy);
                        glVertex3f(dataPxX, dataPxY, -0.1);
                        glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLLx,
                                     state->viewerState->viewPorts[currentVP].texture.texLLy);
                        glVertex3f(-dataPxX, dataPxY, -0.1);
                    glEnd();
                }
            }

            glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_DEPTH_TEST);

            if(state->viewerState->drawVPCrosshairs) {
                glLineWidth(1.);
                glBegin(GL_LINES);
                    glColor4f(0., 1., 0., 0.3);
                    glVertex3f(-dataPxX, 0.5, -0.0001);
                    glVertex3f(dataPxX, 0.5, -0.0001);

                    glColor4f(0., 0., 1., 0.3);
                    glVertex3f(0.5, -dataPxY, -0.0001);
                    glVertex3f(0.5, dataPxY, -0.0001);
                glEnd();
            }

            break;

        case VIEWPORT_XZ:
            if(!state->viewerState->selectModeFlag) {
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
            }
            /* left, right, bottom, top, near, far clipping planes */
            glOrtho(-((float)state->boundary.x / 2.) + (float)state->viewerState->currentPosition.x - dataPxX,
                -((float)state->boundary.x / 2.) + (float)state->viewerState->currentPosition.x + dataPxX,
                -((float)state->boundary.y / 2.) + (float)state->viewerState->currentPosition.y - dataPxY,
                -((float)state->boundary.y / 2.) + (float)state->viewerState->currentPosition.y + dataPxY,
                ((float)state->boundary.z / 2.) - state->viewerState->depthCutOff - (float)state->viewerState->currentPosition.z,
                ((float)state->boundary.z / 2.) + state->viewerState->depthCutOff - (float)state->viewerState->currentPosition.z);

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

			/*optimize that! TDitem */

            glTranslatef(-((float)state->boundary.x / 2.),
						-((float)state->boundary.y / 2.),
						-((float)state->boundary.z / 2.));

            glTranslatef((float)state->viewerState->currentPosition.x,
						(float)state->viewerState->currentPosition.y,
						(float)state->viewerState->currentPosition.z);

            glRotatef(90., 1., 0., 0.);

            glLoadName(3);

			glEnable(GL_TEXTURE_2D);
            glDisable(GL_DEPTH_TEST);


            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glColor4f(1., 1., 1., 1.);

            glBindTexture(GL_TEXTURE_2D, state->viewerState->viewPorts[currentVP].texture.texHandle);
            glBegin(GL_QUADS);
                glNormal3i(0,1,0);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLUx, state->viewerState->viewPorts[currentVP].texture.texLUy);
                glVertex3f(-dataPxX, 0., -dataPxY);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRUx, state->viewerState->viewPorts[currentVP].texture.texRUy);
                glVertex3f(dataPxX, 0., -dataPxY);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRLx, state->viewerState->viewPorts[currentVP].texture.texRLy);
                glVertex3f(dataPxX, 0., dataPxY);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLLx, state->viewerState->viewPorts[currentVP].texture.texLLy);
                glVertex3f(-dataPxX, 0., dataPxY);
            glEnd();
            glBindTexture (GL_TEXTURE_2D, 0);
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_DEPTH_TEST);

            glTranslatef(-(float)state->viewerState->currentPosition.x, -(float)state->viewerState->currentPosition.y, -(float)state->viewerState->currentPosition.z);
            glTranslatef(((float)state->boundary.x / 2.),((float)state->boundary.y / 2.),((float)state->boundary.z / 2.));

            if(state->skeletonState->displayListSkeletonSlicePlaneVP) glCallList(state->skeletonState->displayListSkeletonSlicePlaneVP);

            glTranslatef(-((float)state->boundary.x / 2.),-((float)state->boundary.y / 2.),-((float)state->boundary.z / 2.));
            glTranslatef((float)state->viewerState->currentPosition.x, (float)state->viewerState->currentPosition.y, (float)state->viewerState->currentPosition.z);
            glLoadName(3);

            glEnable(GL_TEXTURE_2D);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glColor4f(1., 1., 1., 0.6);

            glBindTexture(GL_TEXTURE_2D, state->viewerState->viewPorts[currentVP].texture.texHandle);
            glBegin(GL_QUADS);
                glNormal3i(0,1,0);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLUx, state->viewerState->viewPorts[currentVP].texture.texLUy);
                glVertex3f(-dataPxX, 0., -dataPxY);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRUx, state->viewerState->viewPorts[currentVP].texture.texRUy);
                glVertex3f(dataPxX, -0., -dataPxY);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRLx, state->viewerState->viewPorts[currentVP].texture.texRLy);
                glVertex3f(dataPxX, -0., dataPxY);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLLx, state->viewerState->viewPorts[currentVP].texture.texLLy);
                glVertex3f(-dataPxX, -0., dataPxY);
            glEnd();

            /* Draw overlay */
            if(state->overlay) {
                if(state->viewerState->overlayVisible) {
                    glBindTexture(GL_TEXTURE_2D, state->viewerState->viewPorts[currentVP].texture.overlayHandle);
                    glBegin(GL_QUADS);
                        glNormal3i(0,1,0);
                        glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLUx,
                                     state->viewerState->viewPorts[currentVP].texture.texLUy);
                        glVertex3f(-dataPxX, 0.1, -dataPxY);
                        glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRUx,
                                     state->viewerState->viewPorts[currentVP].texture.texRUy);
                        glVertex3f(dataPxX, 0.1, -dataPxY);
                        glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRLx,
                                     state->viewerState->viewPorts[currentVP].texture.texRLy);
                        glVertex3f(dataPxX, 0.1, dataPxY);
                        glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLLx,
                                     state->viewerState->viewPorts[currentVP].texture.texLLy);
                        glVertex3f(-dataPxX, 0.1, dataPxY);
                    glEnd();
                }
            }
            glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_DEPTH_TEST);

            if(state->viewerState->drawVPCrosshairs) {
                glLineWidth(1.);
                glBegin(GL_LINES);
                    glColor4f(1., 0., 0., 0.3);
                    glVertex3f(-dataPxX, 0.0001, 0.5);
                    glVertex3f(dataPxX, 0.0001, 0.5);

                    glColor4f(0., 0., 1., 0.3);
                    glVertex3f(0.5, 0.0001, -dataPxY);
                    glVertex3f(0.5, 0.0001, dataPxY);
                glEnd();
            }

            break;
        case VIEWPORT_YZ:
            if(!state->viewerState->selectModeFlag) {
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
            }

            glOrtho(-((float)state->boundary.x / 2.) + (float)state->viewerState->currentPosition.x - dataPxY,
                -((float)state->boundary.x / 2.) + (float)state->viewerState->currentPosition.x + dataPxY,
                -((float)state->boundary.y / 2.) + (float)state->viewerState->currentPosition.y - dataPxX,
                -((float)state->boundary.y / 2.) + (float)state->viewerState->currentPosition.y + dataPxX,
                ((float)state->boundary.z / 2.) - state->viewerState->depthCutOff - (float)state->viewerState->currentPosition.z,
                ((float)state->boundary.z / 2.) + state->viewerState->depthCutOff - (float)state->viewerState->currentPosition.z);

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            glTranslatef(-((float)state->boundary.x / 2.),-((float)state->boundary.y / 2.),-((float)state->boundary.z / 2.));
            glTranslatef((float)state->viewerState->currentPosition.x, (float)state->viewerState->currentPosition.y, (float)state->viewerState->currentPosition.z);
            glRotatef(90., 0., 1., 0.);
            glScalef(1., -1., 1.);

            glLoadName(3);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_TEXTURE_2D);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glColor4f(1., 1., 1., 1.);

            glBindTexture(GL_TEXTURE_2D, state->viewerState->viewPorts[currentVP].texture.texHandle);
            glBegin(GL_QUADS);
                glNormal3i(1,0,0);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLUx, state->viewerState->viewPorts[currentVP].texture.texLUy);
                glVertex3f(0., -dataPxX, -dataPxY);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRUx, state->viewerState->viewPorts[currentVP].texture.texRUy);
                glVertex3f(0., dataPxX, -dataPxY);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRLx, state->viewerState->viewPorts[currentVP].texture.texRLy);
                glVertex3f(0., dataPxX, dataPxY);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLLx, state->viewerState->viewPorts[currentVP].texture.texLLy);
                glVertex3f(0., -dataPxX, dataPxY);
            glEnd();
            glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_DEPTH_TEST);

            glTranslatef(-(float)state->viewerState->currentPosition.x, -(float)state->viewerState->currentPosition.y, -(float)state->viewerState->currentPosition.z);
            glTranslatef((float)state->boundary.x / 2.,(float)state->boundary.y / 2.,(float)state->boundary.z / 2.);

            if(state->skeletonState->displayListSkeletonSlicePlaneVP) glCallList(state->skeletonState->displayListSkeletonSlicePlaneVP);

            glTranslatef(-((float)state->boundary.x / 2.),-((float)state->boundary.y / 2.),-((float)state->boundary.z / 2.));
            glTranslatef((float)state->viewerState->currentPosition.x, (float)state->viewerState->currentPosition.y, (float)state->viewerState->currentPosition.z);
            glLoadName(3);

            glEnable(GL_TEXTURE_2D);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glColor4f(1., 1., 1., 0.6);

            glBindTexture(GL_TEXTURE_2D, state->viewerState->viewPorts[currentVP].texture.texHandle);
            glBegin(GL_QUADS);
                glNormal3i(1,0,0);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLUx, state->viewerState->viewPorts[currentVP].texture.texLUy);
                glVertex3f(1., -dataPxX, -dataPxY);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRUx, state->viewerState->viewPorts[currentVP].texture.texRUy);
                glVertex3f(1., dataPxX, -dataPxY);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRLx, state->viewerState->viewPorts[currentVP].texture.texRLy);
                glVertex3f(1., dataPxX, dataPxY);
                glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLLx, state->viewerState->viewPorts[currentVP].texture.texLLy);
                glVertex3f(1., -dataPxX, dataPxY);
            glEnd();

            /* Draw overlay */
            if(state->overlay) {
                if(state->viewerState->overlayVisible) {
                    glBindTexture(GL_TEXTURE_2D, state->viewerState->viewPorts[currentVP].texture.overlayHandle);
                    glBegin(GL_QUADS);
                        glNormal3i(1,0,0);
                        glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLUx,
                                     state->viewerState->viewPorts[currentVP].texture.texLUy);
                        glVertex3f(-0.1, -dataPxX, -dataPxY);
                        glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRUx,
                                     state->viewerState->viewPorts[currentVP].texture.texRUy);
                    glVertex3f(-0.1, dataPxX, -dataPxY);
                    glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texRLx,
                                 state->viewerState->viewPorts[currentVP].texture.texRLy);
                    glVertex3f(-0.1, dataPxX, dataPxY);
                    glTexCoord2f(state->viewerState->viewPorts[currentVP].texture.texLLx,
                                 state->viewerState->viewPorts[currentVP].texture.texLLy);
                    glVertex3f(-0.1, -dataPxX, dataPxY);
                    glEnd();
                }
            }

            glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_DEPTH_TEST);

            if(state->viewerState->drawVPCrosshairs) {
                glLineWidth(1.);
                glBegin(GL_LINES);
                    glColor4f(1., 0., 0., 0.3);
                    glVertex3f(-0.0001, -dataPxX, 0.5);
                    glVertex3f(-0.0001, dataPxX, 0.5);

                    glColor4f(0., 1., 0., 0.3);
                    glVertex3f(-0.0001, 0.5, -dataPxX);
                    glVertex3f(-0.0001, 0.5, dataPxX);
                glEnd();
            }

            break;
    }

    glDisable(GL_BLEND);
    renderViewportBorders(currentVP);

    return TRUE;
}


/* This function draws the borders of a given VP */
static uint32_t renderViewportBorders(uint32_t currentVP)  {
    /*
    char *description;
    char *c;

    setOGLforVP(currentVP, state);


    description = malloc(512);
    memset(description, '\0', 512);

    Draw move button in the upper left corner
    glColor4f(0.5, 0.5, 0.5, 0.7);
    glBegin(GL_QUADS);
    glVertex3i(0, 0, 0);
    glVertex3i(10, 0, 0);
    glVertex3i(10, 10, 0);
    glVertex3i(0, 10, 0);
    glEnd();
    //Draw resize button in the lower right corner
    glBegin(GL_QUADS);
    glVertex3i(state->viewerState->viewPorts[currentVP].edgeLength - 10, state->viewerState->viewPorts[currentVP].edgeLength - 10, 0);
    glVertex3i(state->viewerState->viewPorts[currentVP].edgeLength, state->viewerState->viewPorts[currentVP].edgeLength - 10, 0);
    glVertex3i(state->viewerState->viewPorts[currentVP].edgeLength, state->viewerState->viewPorts[currentVP].edgeLength, 0);
    glVertex3i(state->viewerState->viewPorts[currentVP].edgeLength - 10, state->viewerState->viewPorts[currentVP].edgeLength, 0);
    glEnd();
    //glTranslatef(0., 0., -0.5);
    //Draw button borders
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor4f(0., 0., 0., 1.);
    glBegin(GL_QUADS);
    glVertex3i(0, 0, 0);
    glVertex3i(10, 0, 0);
    glVertex3i(10, 10, 0);
    glVertex3i(0, 10, 0);
    glEnd();
    glBegin(GL_QUADS);
    glVertex3i(state->viewerState->viewPorts[currentVP].edgeLength - 10, state->viewerState->viewPorts[currentVP].edgeLength - 10, 0);
    glVertex3i(state->viewerState->viewPorts[currentVP].edgeLength, state->viewerState->viewPorts[currentVP].edgeLength - 10, 0);
    glVertex3i(state->viewerState->viewPorts[currentVP].edgeLength, state->viewerState->viewPorts[currentVP].edgeLength, 0);
    glVertex3i(state->viewerState->viewPorts[currentVP].edgeLength - 10, state->viewerState->viewPorts[currentVP].edgeLength, 0);
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
*/

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    /* define coordinate system for our viewport: left right bottom top near far */
    glOrtho(0, state->viewerState->viewPorts[currentVP].edgeLength,
            state->viewerState->viewPorts[currentVP].edgeLength, 0, 25, -25);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    switch(state->viewerState->viewPorts[currentVP].type) {
        case VIEWPORT_XY:
            glColor4f(0.7, 0., 0., 1.);
            break;
        case VIEWPORT_XZ:
            glColor4f(0., 0.7, 0., 1.);
            break;
        case VIEWPORT_YZ:
            glColor4f(0., 0., 0.7, 1.);
            break;
        case VIEWPORT_SKELETON:
            glColor4f(0., 0., 0., 1.);
            break;
    }
    glLineWidth(3.);
    glBegin(GL_LINES);
        glVertex3d(2, 1, 0);
        glVertex3d(state->viewerState->viewPorts[currentVP].edgeLength - 1, 1, 0);
        glVertex3d(state->viewerState->viewPorts[currentVP].edgeLength - 1, 1, 0);
        glVertex3d(state->viewerState->viewPorts[currentVP].edgeLength - 1, state->viewerState->viewPorts[currentVP].edgeLength - 1, 0);
        glVertex3d(state->viewerState->viewPorts[currentVP].edgeLength - 1, state->viewerState->viewPorts[currentVP].edgeLength - 1, 0);
        glVertex3d(2, state->viewerState->viewPorts[currentVP].edgeLength - 2, 0);
        glVertex3d(2, state->viewerState->viewPorts[currentVP].edgeLength - 2, 0);
        glVertex3d(2, 1, 0);
    glEnd();

    if(state->viewerState->viewPorts[currentVP].type == state->viewerState->highlightVp) {
        // Draw an orange border to highlight the viewport.

        glColor4f(1., 0.3, 0., 1.);
        glBegin(GL_LINES);
            glVertex3d(5, 4, 0);
            glVertex3d(state->viewerState->viewPorts[currentVP].edgeLength - 4, 4, 0);
            glVertex3d(state->viewerState->viewPorts[currentVP].edgeLength - 4, 4, 0);
            glVertex3d(state->viewerState->viewPorts[currentVP].edgeLength - 4, state->viewerState->viewPorts[currentVP].edgeLength - 4, 0);
            glVertex3d(state->viewerState->viewPorts[currentVP].edgeLength - 4, state->viewerState->viewPorts[currentVP].edgeLength - 4, 0);
            glVertex3d(5, state->viewerState->viewPorts[currentVP].edgeLength - 5, 0);
            glVertex3d(5, state->viewerState->viewPorts[currentVP].edgeLength - 5, 0);
            glVertex3d(5, 4, 0);
        glEnd();
    }

    glLineWidth(1.);

    /*
    //Draw VP description
    //set the drawing area in the window to our actually processed view port.
    glViewport(state->viewerState->viewPorts[currentVP].lowerLeftCorner.x - 5,
               state->viewerState->viewPorts[currentVP].lowerLeftCorner.y,
               state->viewerState->viewPorts[currentVP].edgeLength,
               state->viewerState->viewPorts[currentVP].edgeLength + 5);
    //select the projection matrix
    glMatrixMode(GL_PROJECTION);
    //reset it
    glLoadIdentity();

    //This is necessary to draw the text to the "outside" of the current VP
    //define coordinate system for our viewport: left right bottom top near far
    //coordinate values
    glOrtho(0, state->viewerState->viewPorts[currentVP].edgeLength + 5,
            state->viewerState->viewPorts[currentVP].edgeLength + 5, 0, 25, -25);
    //select the modelview matrix for modification
    glMatrixMode(GL_MODELVIEW);
    //reset it
    glLoadIdentity();

    glColor4f(0., 0., 0., 1.);

    switch(state->viewerState->viewPorts[currentVP].type) {
    case VIEWPORT_XY:
        glRasterPos2i(9, 0);
        sprintf(description, "Viewport XY     x length: %3.3f[um]    y length: %3.3f[um]", state->viewerState->viewPorts[currentVP].displayedlengthInNmX / 1000., state->viewerState->viewPorts[currentVP].displayedlengthInNmY / 1000.);
        for (c=description; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_10, *c);
        }
        break;
    case VIEWPORT_XZ:
        glRasterPos2i(9, 0);
        sprintf(description, "Viewport XZ     x length: %3.3f[um]    y length: %3.3f[um]", state->viewerState->viewPorts[currentVP].displayedlengthInNmX / 1000., state->viewerState->viewPorts[currentVP].displayedlengthInNmY / 1000.);
        for (c=description; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_10, *c);
        }
        break;
    case VIEWPORT_YZ:
        glRasterPos2i(9, 0);
        sprintf(description, "Viewport YZ     x length: %3.3f[um]    y length: %3.3f[um]", state->viewerState->viewPorts[currentVP].displayedlengthInNmX / 1000., state->viewerState->viewPorts[currentVP].displayedlengthInNmY / 1000.);
        for (c=description; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_10, *c);
        }
        break;

    case VIEWPORT_SKELETON:
        glRasterPos2i(9, 0);
        sprintf(description, "Viewport Skeleton     #trees: %d    #nodes: %d    #segments: %d", state->skeletonState->treeElements, state->skeletonState->totalNodeElements, state->skeletonState->totalSegmentElements);
        for (c=description; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_10, *c);
        }
        break;
    }

    free(description);
    */
    return TRUE;
}

/*
 *
 * This function is not currently used.
 *
 *

static uint32_t overlayOrthogonalVpPixel(uint32_t currentVP, Coordinate position, color4F color, struct stateInfo *state)  {
    uint32_t i;
    Byte drawFlag;
    float xOffset, yOffset;

    drawFlag = FALSE;
    xOffset = yOffset = 0.;

    switch(state->viewerState->viewPorts[currentVP].type) {
    case VIEWPORT_XY:
        //Pixel in current slice plane?
        if(position.z == state->viewerState->currentPosition.z) {
            //Pixel in currently displayed pane?
            if((position.x - state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.x >= 0)
                    && (position.x < (state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.x + (state->viewerState->viewPorts[currentVP].texture.displayedEdgeLengthX / state->viewerState->viewPorts[currentVP].texture.texUnitsPerDataPx)))
                    && (position.y - state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.y >= 0)
                    && (position.y < (state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.y + (state->viewerState->viewPorts[currentVP].texture.displayedEdgeLengthY / state->viewerState->viewPorts[currentVP].texture.texUnitsPerDataPx)))
              ) {

                drawFlag = TRUE;
                //Determine position of the pixel of interest
                xOffset = ((float)(position.x - state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.x)) * state->viewerState->viewPorts[currentVP].screenPxXPerDataPx;
                yOffset = ((float)(position.y - state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.y)) * state->viewerState->viewPorts[currentVP].screenPxYPerDataPx;
            }
        }
        break;
    case VIEWPORT_XZ:
        //Pixel in current slice plane?
        if(position.y == state->viewerState->currentPosition.y) {
            //Pixel in currently displayed pane?
            if((position.x - state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.x >= 0)
                    && (position.x < (state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.x + (state->viewerState->viewPorts[currentVP].texture.displayedEdgeLengthX / state->viewerState->viewPorts[currentVP].texture.texUnitsPerDataPx)))
                    && (position.z - state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.z >= 0)
                    && (position.z < (state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.z + (state->viewerState->viewPorts[currentVP].texture.displayedEdgeLengthY / state->viewerState->viewPorts[currentVP].texture.texUnitsPerDataPx)))
              ) {

                drawFlag = TRUE;
                //Determine position of the pixel of interest
                xOffset = ((float)(position.x - state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.x)) * state->viewerState->viewPorts[currentVP].screenPxXPerDataPx;
                yOffset = ((float)(position.z - state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.z)) * state->viewerState->viewPorts[currentVP].screenPxYPerDataPx;
            }
        }
        break;
    case VIEWPORT_YZ:
        //Pixel in current slice plane?
        if(position.x == state->viewerState->currentPosition.x) {
            //Pixel in currently displayed pane?
            if((position.z - state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.z >= 0)
                    && (position.z < (state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.z + (state->viewerState->viewPorts[currentVP].texture.displayedEdgeLengthY / state->viewerState->viewPorts[currentVP].texture.texUnitsPerDataPx)))
                    && (position.y - state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.y >= 0)
                    && (position.y < (state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.y + (state->viewerState->viewPorts[currentVP].texture.displayedEdgeLengthX / state->viewerState->viewPorts[currentVP].texture.texUnitsPerDataPx)))
              ) {

                drawFlag = TRUE;
                //Determine position of the pixel of interest
                xOffset = ((float)(position.z - state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.z)) * state->viewerState->viewPorts[currentVP].screenPxXPerDataPx;
                yOffset = ((float)(position.y - state->viewerState->viewPorts[currentVP].leftUpperDataPxOnScreen.y)) * state->viewerState->viewPorts[currentVP].screenPxYPerDataPx;
            }
        }
        break;
    }

    if(drawFlag == TRUE) {

        setOGLforVP(currentVP, state);

        //Additional OGL settings
        glDisable(GL_TEXTURE_2D);

        glColor4f(color.r, color.g, color.b, color.a);

        glBegin(GL_QUADS);
        //draw points clockwise!!
        glVertex3f(xOffset, yOffset, 0);
        glVertex3f(xOffset + state->viewerState->viewPorts[currentVP].screenPxXPerDataPx, yOffset, 0);
        glVertex3f(xOffset + state->viewerState->viewPorts[currentVP].screenPxXPerDataPx, yOffset + state->viewerState->viewPorts[currentVP].screenPxYPerDataPx, 0);
        glVertex3f(xOffset, yOffset + state->viewerState->viewPorts[currentVP].screenPxYPerDataPx, 0);
        glEnd();

        glColor4f(1., 1., 1., 1.);
    }

    return TRUE;
}
*/

/*
static uint32_t setOGLforVP(uint32_t currentVP, struct stateInfo *state) {
     * The following code configures openGL to draw into the current VP
    //set the drawing area in the window to our actually processed view port.
    glViewport(state->viewerState->viewPorts[currentVP].lowerLeftCorner.x,
               state->viewerState->viewPorts[currentVP].lowerLeftCorner.y,
               state->viewerState->viewPorts[currentVP].edgeLength,
               state->viewerState->viewPorts[currentVP].edgeLength);
    //select the projection matrix
    glMatrixMode(GL_PROJECTION);
    //reset it
    glLoadIdentity();
    //define coordinate system for our viewport: left right bottom top near far
    //coordinate values
    glOrtho(0, state->viewerState->viewPorts[currentVP].edgeLength,
            state->viewerState->viewPorts[currentVP].edgeLength, 0, 25, -25);
    //select the modelview matrix for modification
    glMatrixMode(GL_MODELVIEW);
    //reset it
    glLoadIdentity();
    //glBlendFunc(GL_ONE_MINUS_DST_ALPHA,GL_DST_ALPHA);

    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    return TRUE;
}
*/

//Flag 0 if called for slice plane VP; Flag 1 if called for skeleton VP
static GLuint renderActiveTreeSkeleton(struct stateInfo *state, Byte callFlag) {
    struct treeListElement *currentTree;
    struct nodeListElement *currentNode;
    struct segmentListElement *currentSegment;

    char *textBuffer;
    textBuffer = malloc(32);
    memset(textBuffer, '\0', 32);

    GLuint tempList;
    tempList = glGenLists(1);
    glNewList(tempList, GL_COMPILE);
    if(callFlag) glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    //Save current matrix stack (modelview!!)
    glPushMatrix();

    //Rendering of objects has to start always at the origin of our data pixel coordinate system.
    //Thus, we have to translate there.
    glTranslatef(-(float)state->boundary.x / 2. + 0.5,-(float)state->boundary.y / 2. + 0.5,-(float)state->boundary.z / 2. + 0.5);


    if(state->skeletonState->activeTree) {
        currentTree = state->skeletonState->activeTree;

        currentNode = currentTree->firstNode;
        while(currentNode) {
            //Set color
            if(currentNode->isBranchNode)
                glColor4f(0., 0., 1., 1.);
            else {
                if(state->skeletonState->highlightActiveTree) glColor4f(1., 0., 0., 1.);
                else glColor4f(currentTree->color.r, currentTree->color.g, currentTree->color.b, currentTree->color.a);
            }

            //The first 50 entries of the openGL namespace are reserved for static objects (like slice plane quads...)
            glLoadName(currentNode->nodeID + 50);
            if(state->skeletonState->overrideNodeRadiusBool) {
                renderSphere(ORIGINAL_MAG_COORDINATES,
                             &(currentNode->position),
                             state->skeletonState->overrideNodeRadiusVal);
            }
            else {
                renderSphere(ORIGINAL_MAG_COORDINATES,
                             &(currentNode->position),
                             currentNode->radius);
            }

            if(state->skeletonState->highlightActiveTree) glColor4f(1., 0., 0., 1.);
            else glColor4f(currentTree->color.r, currentTree->color.g, currentTree->color.b, currentTree->color.a);

            currentSegment = currentNode->firstSegment;
            while(currentSegment) {
                //2 indicates a backward connection, which should not be rendered.
                if(currentSegment->flag == 2) {
                    currentSegment = currentSegment->next;
                    continue;
                }
                glLoadName(3);
                if(state->skeletonState->overrideNodeRadiusBool)
                    renderCylinder(ORIGINAL_MAG_COORDINATES,
                                   &(currentSegment->source->position),
                                   state->skeletonState->overrideNodeRadiusVal
								   * state->skeletonState->segRadiusToNodeRadius,
                                   &(currentSegment->target->position),
                                   state->skeletonState->overrideNodeRadiusVal
								   * state->skeletonState->segRadiusToNodeRadius);
                else
                    renderCylinder(ORIGINAL_MAG_COORDINATES,
                                   &(currentSegment->source->position),
                                   currentSegment->source->radius
								   * state->skeletonState->segRadiusToNodeRadius,
                                   &(currentSegment->target->position),
                                   currentSegment->target->radius
								   * state->skeletonState->segRadiusToNodeRadius);
                    //Gets true, if called for slice plane VP
                    if(!callFlag) {
                        if(state->skeletonState->showIntersections)
                            renderSegPlaneIntersection(currentSegment, state);
                    }

                currentSegment = currentSegment->next;
            }
            //Render the node description only when option is set.
            if(state->skeletonState->showNodeIDs) {
                glColor4f(0., 0., 0., 1.);
                memset(textBuffer, '\0', 32);
                sprintf(textBuffer, "%d", currentNode->nodeID);
			    renderText(ORIGINAL_MAG_COORDINATES,
					&(currentNode->position),
					textBuffer);
            }
            currentNode = currentNode->next;
        }
        //Highlight active node
        if(state->skeletonState->activeNode) {
            if(state->skeletonState->activeNode->correspondingTree == currentTree) {
                if(state->skeletonState->activeNode->isBranchNode)
                    glColor4f(0., 0., 1., 0.2);
                else
                    glColor4f(1.0, 0., 0., 0.2);
                glEnable(GL_BLEND);
                glLoadName(state->skeletonState->activeNode->nodeID + 50);
                if(state->skeletonState->overrideNodeRadiusBool)
                    renderSphere(ORIGINAL_MAG_COORDINATES,
                                 &(state->skeletonState->activeNode->position),
                                 state->skeletonState->overrideNodeRadiusVal * 1.5);
                else
                    renderSphere(ORIGINAL_MAG_COORDINATES,
                                 &(state->skeletonState->activeNode->position),
                                 state->skeletonState->activeNode->radius * 1.5);
                glDisable(GL_BLEND);
                //Description of active node is always rendered, ignoring state->skeletonState->showNodeIDs
                glColor4f(0., 0., 0., 1.);
                memset(textBuffer, '\0', 32);
                sprintf(textBuffer, "%d", state->skeletonState->activeNode->nodeID);
                renderText(ORIGINAL_MAG_COORDINATES,
                           &(state->skeletonState->activeNode->position),
                           textBuffer);
            }
        }
    }
    //Restore modelview matrix
    glPopMatrix();
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    //Stop display list recording
    glEndList();
    free(textBuffer);
    return tempList;
}

//Flag 0 if called for slice plane VP; Flag 1 if called for skeleton VP
static GLuint renderSuperCubeSkeleton(struct stateInfo *state, Byte callFlag) {
    Coordinate currentPosDC, currentPosDCCounter;

    struct skeletonDC *currentSkeletonDC;
    struct skeletonDCnode *currentSkeletonDCnode;
    struct skeletonDCsegment *currentSkeletonDCsegment;
    struct skeletonDCsegment *firstRenderedSkeletonDCsegment;
    struct skeletonDCsegment *currentSkeletonDCsegmentSearch;

    Byte rendered = FALSE;

    firstRenderedSkeletonDCsegment = malloc(sizeof(struct skeletonDCsegment));
    memset(firstRenderedSkeletonDCsegment, '\0', sizeof(struct skeletonDCsegment));

    currentPosDC = Px2DcCoord(state->viewerState->currentPosition, state);

    char *textBuffer;
    textBuffer = malloc(32);
    memset(textBuffer, '\0', 32);

    GLuint tempList;
    tempList = glGenLists(1);
    glNewList(tempList, GL_COMPILE);

    if(callFlag) glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    //Save current matrix stack (modelview!!)
    glPushMatrix();

    //Rendering of objects has to start always at the origin of our data pixel coordinate system.
    //Thus, we have to translate there.
    glTranslatef(-(float)state->boundary.x / 2. + 0.5,-(float)state->boundary.y / 2. + 0.5,-(float)state->boundary.z / 2. + 0.5);


    //We take all skeletonDCs out of our current SC
    for(currentPosDCCounter.x = currentPosDC.x - state->viewerState->zoomCube; currentPosDCCounter.x <= currentPosDC.x + state->viewerState->zoomCube; currentPosDCCounter.x++) {
        for(currentPosDCCounter.y = currentPosDC.y - state->viewerState->zoomCube; currentPosDCCounter.y <= currentPosDC.y + state->viewerState->zoomCube; currentPosDCCounter.y++) {
            for(currentPosDCCounter.z = currentPosDC.z - state->viewerState->zoomCube; currentPosDCCounter.z <= currentPosDC.z + state->viewerState->zoomCube; currentPosDCCounter.z++) {
                currentSkeletonDC = (struct skeletonDC *)ht_get(state->skeletonState->skeletonDCs, currentPosDCCounter);

                //If there is a valid skeletonDC, there are nodes / segments (or both) in it.
                if(currentSkeletonDC != HT_FAILURE) {

                    //Go through all segments of the data cube
                    if(currentSkeletonDC->firstSkeletonDCsegment) {
                        currentSkeletonDCsegment = currentSkeletonDC->firstSkeletonDCsegment;

                        while(currentSkeletonDCsegment) {

                            //Check if this segment has already been rendered and skip it if true.
                            //No segment rendered yet.. this is the first one
                            if(firstRenderedSkeletonDCsegment->segment == NULL) {
                                //Set color
                                if((currentSkeletonDCsegment->segment->source->correspondingTree == state->skeletonState->activeTree)
                                    && (state->skeletonState->highlightActiveTree)) {
                                        glColor4f(1., 0., 0., 1.);
                                }
                                else
                                    glColor4f(currentSkeletonDCsegment->segment->source->correspondingTree->color.r,
                                              currentSkeletonDCsegment->segment->source->correspondingTree->color.g,
                                              currentSkeletonDCsegment->segment->source->correspondingTree->color.b,
                                              currentSkeletonDCsegment->segment->source->correspondingTree->color.a);
                                glLoadName(3);
                                if(state->skeletonState->overrideNodeRadiusBool)
                                    renderCylinder(ORIGINAL_MAG_COORDINATES,
                                                   &(currentSkeletonDCsegment->segment->source->position),
                                                   state->skeletonState->overrideNodeRadiusVal *
                                                    state->skeletonState->segRadiusToNodeRadius,
                                                   &currentSkeletonDCsegment->segment->target->position,
                                                   state->skeletonState->overrideNodeRadiusVal *
                                                    state->skeletonState->segRadiusToNodeRadius);
                                else
                                    renderCylinder(ORIGINAL_MAG_COORDINATES, &(currentSkeletonDCsegment->segment->source->position),
                                        currentSkeletonDCsegment->segment->source->radius * state->skeletonState->segRadiusToNodeRadius,
                                        &(currentSkeletonDCsegment->segment->target->position),
                                        currentSkeletonDCsegment->segment->target->radius * state->skeletonState->segRadiusToNodeRadius);

                                //Gets true, if called for slice plane VP
                                if(!callFlag) {
                                    if(state->skeletonState->showIntersections)
                                        renderSegPlaneIntersection(currentSkeletonDCsegment->segment, state);
                                }

                                //A bit hackish to use the same struct for it, I know...
                                firstRenderedSkeletonDCsegment->segment = (struct segmentListElement *)currentSkeletonDCsegment;
                            }
                            else {
                                rendered = FALSE;
                                currentSkeletonDCsegmentSearch = firstRenderedSkeletonDCsegment;
                                //Check all segments in the current SC if they were rendered...
                                /* this adds O(n^2) :( ) */
                                /*while(currentSkeletonDCsegmentSearch) {
                                    //Already rendered, skip this
                                    if(((struct skeletonDCsegment *)currentSkeletonDCsegmentSearch->segment) == currentSkeletonDCsegment) {
                                        //currentSkeletonDCsegmentSearch = currentSkeletonDCsegmentSearch->next;
                                        rendered = TRUE;
                                        break;
                                    }
                                    currentSkeletonDCsegmentSearch = currentSkeletonDCsegmentSearch->next;
                                }*/
                                //So render it and add a new element to the list of rendered segments
                                if(rendered == FALSE) {
                                    //Set color
                                    if((currentSkeletonDCsegment->segment->source->correspondingTree == state->skeletonState->activeTree)
                                        && (state->skeletonState->highlightActiveTree))
                                        glColor4f(1., 0., 0., 1.);
                                    else
                                        glColor4f(currentSkeletonDCsegment->segment->source->correspondingTree->color.r,
                                                  currentSkeletonDCsegment->segment->source->correspondingTree->color.g,
                                                  currentSkeletonDCsegment->segment->source->correspondingTree->color.b,
                                                  currentSkeletonDCsegment->segment->source->correspondingTree->color.a);
                                    glLoadName(3);
                                    if(state->skeletonState->overrideNodeRadiusBool)
                                        renderCylinder(ORIGINAL_MAG_COORDINATES, &(currentSkeletonDCsegment->segment->source->position),
                                            state->skeletonState->overrideNodeRadiusVal * state->skeletonState->segRadiusToNodeRadius,
                                            &(currentSkeletonDCsegment->segment->target->position),
                                            state->skeletonState->overrideNodeRadiusVal * state->skeletonState->segRadiusToNodeRadius);
                                    else
                                        renderCylinder(ORIGINAL_MAG_COORDINATES, &(currentSkeletonDCsegment->segment->source->position),
                                            currentSkeletonDCsegment->segment->source->radius * state->skeletonState->segRadiusToNodeRadius,
                                            &(currentSkeletonDCsegment->segment->target->position),
                                            currentSkeletonDCsegment->segment->target->radius * state->skeletonState->segRadiusToNodeRadius);

                                    //Gets true, if called for slice plane VP
                                    if(!callFlag) {
                                        if(state->skeletonState->showIntersections)
                                            renderSegPlaneIntersection(currentSkeletonDCsegment->segment, state);
                                    }

                                    currentSkeletonDCsegmentSearch = malloc(sizeof(struct skeletonDCsegment));
                                    memset(currentSkeletonDCsegmentSearch, '\0', sizeof(struct skeletonDCsegment));

                                    currentSkeletonDCsegmentSearch->next = firstRenderedSkeletonDCsegment;
                                    firstRenderedSkeletonDCsegment = currentSkeletonDCsegmentSearch;
                                }
                            }

                            currentSkeletonDCsegment = currentSkeletonDCsegment->next;
                        }
                    }

                    //Go through all nodes of the SC
                    if(currentSkeletonDC->firstSkeletonDCnode) {
                        currentSkeletonDCnode = currentSkeletonDC->firstSkeletonDCnode;

                        while(currentSkeletonDCnode) {
                            //Set color
                            if((currentSkeletonDCnode->node->correspondingTree == state->skeletonState->activeTree)
                                && (state->skeletonState->highlightActiveTree)) {
                                    glColor4f(1., 0., 0., 1.);
                            }
                            else
                                glColor4f(currentSkeletonDCnode->node->correspondingTree->color.r,
                                          currentSkeletonDCnode->node->correspondingTree->color.g,
                                          currentSkeletonDCnode->node->correspondingTree->color.b,
                                          currentSkeletonDCnode->node->correspondingTree->color.a);

                            if(currentSkeletonDCnode->node->isBranchNode)
                                glColor4f(0., 0., 1., 1.);

                            //The first 50 entries of the openGL namespace are reserved for static objects (like slice plane quads...)
                            glLoadName(currentSkeletonDCnode->node->nodeID + 50);
                            //renderSphere(&(currentSkeletonDCnode->node->position), currentSkeletonDCnode->node->radius);
                            if(state->skeletonState->overrideNodeRadiusBool)
                                renderSphere(ORIGINAL_MAG_COORDINATES, &(currentSkeletonDCnode->node->position), state->skeletonState->overrideNodeRadiusVal);
                            else
                                renderSphere(ORIGINAL_MAG_COORDINATES, &(currentSkeletonDCnode->node->position), currentSkeletonDCnode->node->radius);

                            //Check if this node is an active node and highlight if true
                            if(state->skeletonState->activeNode) {
                                if(currentSkeletonDCnode->node->nodeID == state->skeletonState->activeNode->nodeID) {
                                    if(currentSkeletonDCnode->node->isBranchNode)
                                        glColor4f(0., 0., 1.0, 0.2);
                                    else
                                        glColor4f(1.0, 0., 0., 0.2);

                                    glLoadName(currentSkeletonDCnode->node->nodeID + 50);
                                    glEnable(GL_BLEND);
                                    //renderSphere(&(currentSkeletonDCnode->node->position), currentSkeletonDCnode->node->radius * 1.5);
                                    if(state->skeletonState->overrideNodeRadiusBool)
                                        renderSphere(ORIGINAL_MAG_COORDINATES, &(currentSkeletonDCnode->node->position), state->skeletonState->overrideNodeRadiusVal);
                                    else
                                        renderSphere(ORIGINAL_MAG_COORDINATES, &(currentSkeletonDCnode->node->position), currentSkeletonDCnode->node->radius * 1.5);
                                    glDisable(GL_BLEND);
                                    //Description of active node is always rendered, ignoring state->skeletonState->showNodeIDs
                                    glColor4f(0., 0., 0., 1.);
                                    memset(textBuffer, '\0', 32);
                                    sprintf(textBuffer, "%d", state->skeletonState->activeNode->nodeID);
                                    renderText(ORIGINAL_MAG_COORDINATES, &(currentSkeletonDCnode->node->position), textBuffer);
                                }
                            }

                            //Render the node description only when option is set.
                            if(state->skeletonState->showNodeIDs) {
                                glColor4f(0., 0., 0., 1.);
                                memset(textBuffer, '\0', 32);
                                sprintf(textBuffer, "%d", currentSkeletonDCnode->node->nodeID);
                                renderText(ORIGINAL_MAG_COORDINATES, &(currentSkeletonDCnode->node->position), textBuffer);
                            }
                            currentSkeletonDCnode = currentSkeletonDCnode->next;
                        }
                    }
                }
            }
        }
    }

    //Now we have to clean up our list of rendered segments...
    while(firstRenderedSkeletonDCsegment) {
        currentSkeletonDCsegmentSearch = firstRenderedSkeletonDCsegment->next;
        free(firstRenderedSkeletonDCsegment);
        firstRenderedSkeletonDCsegment = currentSkeletonDCsegmentSearch;
    }

    //Restore modelview matrix
    glPopMatrix();
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    //Stop display list recording
    glEndList();

    free(textBuffer);

    return tempList;
}

static GLuint renderWholeSkeleton(struct stateInfo *state) {
    struct treeListElement *currentTree;
    struct nodeListElement *currentNode;
    struct segmentListElement *currentSegment;

    char *textBuffer;
    textBuffer = malloc(32);
    memset(textBuffer, '\0', 32);

    GLuint tempList;
    tempList = glGenLists(1);
    glNewList(tempList, GL_COMPILE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    //Save current matrix stack (modelview!!)
    glPushMatrix();

    //Rendering of objects has to start always at the origin of our data pixel coordinate system.
    //Thus, we have to translate there.
    glTranslatef(-(float)state->boundary.x / 2. + 0.5,-(float)state->boundary.y / 2. + 0.5,-(float)state->boundary.z / 2. + 0.5);

    //We iterate over the whole tree structure here.
    currentTree = state->skeletonState->firstTree;

    while(currentTree) {
        currentNode = currentTree->firstNode;
        while(currentNode) {
            //Set color
            if((currentTree->treeID == state->skeletonState->activeTree->treeID)
                && (state->skeletonState->highlightActiveTree)) {
                    glColor4f(1., 0., 0., 1.);
            }
            else
                glColor4f(currentTree->color.r,
                          currentTree->color.g,
                          currentTree->color.b,
                          currentTree->color.a);

            if(currentNode->isBranchNode)
                glColor4f(0., 0., 1., 1.);

            //The first 50 entries of the openGL namespace are reserved for static objects (like slice plane quads...)
            glLoadName(currentNode->nodeID + 50);
            if(state->skeletonState->overrideNodeRadiusBool)
                renderSphere(ORIGINAL_MAG_COORDINATES, &(currentNode->position), state->skeletonState->overrideNodeRadiusVal);
            else
                renderSphere(ORIGINAL_MAG_COORDINATES, &(currentNode->position), currentNode->radius);

            if((currentTree->treeID == state->skeletonState->activeTree->treeID)
                && (state->skeletonState->highlightActiveTree)) {
                    glColor4f(1., 0., 0., 1.);
            }
            else
                glColor4f(currentTree->color.r,
                          currentTree->color.g,
                          currentTree->color.b,
                          currentTree->color.a);
            currentSegment = currentNode->firstSegment;
            while(currentSegment) {
                //2 indicates a backward connection, which should not be rendered.
                if(currentSegment->flag == 2) {
                    currentSegment = currentSegment->next;
                    continue;
                }
                glLoadName(3);
                if(state->skeletonState->overrideNodeRadiusBool)
                    renderCylinder(ORIGINAL_MAG_COORDINATES, &(currentSegment->source->position),
                        state->skeletonState->overrideNodeRadiusVal * state->skeletonState->segRadiusToNodeRadius,
                        &(currentSegment->target->position),
                        state->skeletonState->overrideNodeRadiusVal * state->skeletonState->segRadiusToNodeRadius);
                else
                    renderCylinder(ORIGINAL_MAG_COORDINATES, &(currentSegment->source->position),
                        currentSegment->source->radius * state->skeletonState->segRadiusToNodeRadius,
                        &(currentSegment->target->position),
                        currentSegment->target->radius * state->skeletonState->segRadiusToNodeRadius);

                currentSegment = currentSegment->next;

            }
            //Render the node description only when option is set.
            if(state->skeletonState->showNodeIDs) {
                glColor4f(0., 0., 0., 1.);
                memset(textBuffer, '\0', 32);
                sprintf(textBuffer, "%d", currentNode->nodeID);
                renderText(ORIGINAL_MAG_COORDINATES, &(currentNode->position), textBuffer);
            }

            currentNode = currentNode->next;
        }

        currentTree = currentTree->next;
    }

    //Highlight active node
    if(state->skeletonState->activeNode) {
        if(state->skeletonState->activeNode->isBranchNode)
            glColor4f(0., 0., 1., 0.2);
        else
            glColor4f(1.0, 0., 0., 0.2);

        glLoadName(state->skeletonState->activeNode->nodeID + 50);
        glEnable(GL_BLEND);
        if(state->skeletonState->overrideNodeRadiusBool)
            renderSphere(ORIGINAL_MAG_COORDINATES, &(state->skeletonState->activeNode->position), state->skeletonState->overrideNodeRadiusVal * 1.5);
        else
            renderSphere(ORIGINAL_MAG_COORDINATES, &(state->skeletonState->activeNode->position), state->skeletonState->activeNode->radius * 1.5);
        glDisable(GL_BLEND);
        //Description of active node is always rendered, ignoring state->skeletonState->showNodeIDs
        glColor4f(0., 0., 0., 1.);
        memset(textBuffer, '\0', 32);
        sprintf(textBuffer, "%d", state->skeletonState->activeNode->nodeID);
        renderText(ORIGINAL_MAG_COORDINATES, &(state->skeletonState->activeNode->position), textBuffer);
        /*if(state->skeletonState->activeNode->comment) {
            glPushMatrix();
            glTranslated(10,10, 10);
            renderText(ORIGINAL_MAG_COORDINATES, &(state->skeletonState->activeNode->position), state->skeletonState->activeNode->comment);
            glPopMatrix();
        }*/

    }
    //Restore modelview matrix
    glPopMatrix();
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    //Stop display list recording
    glEndList();

    free(textBuffer);

    return tempList;
}

uint32_t updateDisplayListsSkeleton(struct stateInfo *state) {

    if(state->skeletonState->skeletonChanged) {
        state->skeletonState->skeletonChanged = FALSE;
        state->viewerState->superCubeChanged = FALSE;
        state->skeletonState->skeletonSliceVPchanged = FALSE;

        /* clean up the old display lists */
        if(state->skeletonState->displayListSkeletonSkeletonizerVP) {
            glDeleteLists(state->skeletonState->displayListSkeletonSkeletonizerVP, 1);
            state->skeletonState->displayListSkeletonSkeletonizerVP = 0;
        }
        if(state->skeletonState->displayListSkeletonSlicePlaneVP) {
            glDeleteLists(state->skeletonState->displayListSkeletonSlicePlaneVP, 1);
            state->skeletonState->displayListSkeletonSlicePlaneVP = 0;
        }

        /* create new display lists that are up-to-date */
        if(state->skeletonState->displayMode & DSP_SKEL_VP_WHOLE) {
            state->skeletonState->displayListSkeletonSkeletonizerVP =
                renderWholeSkeleton(state);
            if(!(state->skeletonState->displayMode & DSP_SLICE_VP_HIDE))
                state->skeletonState->displayListSkeletonSlicePlaneVP =
                    renderSuperCubeSkeleton(state, 0);

        }

        if(state->skeletonState->displayMode & DSP_SKEL_VP_HIDE) {
            if(!(state->skeletonState->displayMode & DSP_SLICE_VP_HIDE))
                state->skeletonState->displayListSkeletonSlicePlaneVP =
                    renderSuperCubeSkeleton(state, 0);
        }

        if(state->skeletonState->displayMode & DSP_SKEL_VP_CURRENTCUBE) {
            state->skeletonState->displayListSkeletonSkeletonizerVP =
                renderSuperCubeSkeleton(state, 1);
            if(!(state->skeletonState->displayMode & DSP_SLICE_VP_HIDE)) {
                if(state->skeletonState->showIntersections)
                    state->skeletonState->displayListSkeletonSlicePlaneVP =
                        renderSuperCubeSkeleton(state, 0);
                else state->skeletonState->displayListSkeletonSlicePlaneVP =
                    state->skeletonState->displayListSkeletonSkeletonizerVP;
            }
        }
        /* TDitem active tree should be limited to current cube in this case */
        if(state->skeletonState->displayMode & DSP_ACTIVETREE) {

            state->skeletonState->displayListSkeletonSkeletonizerVP =
                renderActiveTreeSkeleton(state, 1);
            if(!(state->skeletonState->displayMode & DSP_SLICE_VP_HIDE)) {
                if(state->skeletonState->showIntersections)
                    state->skeletonState->displayListSkeletonSlicePlaneVP =
                        renderActiveTreeSkeleton(state, 0);
                else state->skeletonState->displayListSkeletonSlicePlaneVP =
                    state->skeletonState->displayListSkeletonSkeletonizerVP;
            }
        }
    }

    if(state->viewerState->superCubeChanged) {
        state->viewerState->superCubeChanged = FALSE;

        if(state->skeletonState->displayMode & DSP_SKEL_VP_CURRENTCUBE) {
            glDeleteLists(state->skeletonState->displayListSkeletonSkeletonizerVP, 1);
            state->skeletonState->displayListSkeletonSkeletonizerVP = 0;

            state->skeletonState->displayListSkeletonSkeletonizerVP =
                renderSuperCubeSkeleton(state, 1);
        }

        if(!(state->skeletonState->displayMode & DSP_SLICE_VP_HIDE)) {
            if(!(state->skeletonState->displayMode & DSP_ACTIVETREE)) {
                state->skeletonState->displayListSkeletonSlicePlaneVP =
                    renderSuperCubeSkeleton(state, 0);
            }
        }
    }

    if(state->skeletonState->skeletonSliceVPchanged) {
        state->skeletonState->skeletonSliceVPchanged = FALSE;
        glDeleteLists(state->skeletonState->displayListSkeletonSlicePlaneVP, 1);
        state->skeletonState->displayListSkeletonSlicePlaneVP = 0;

        if(!(state->skeletonState->displayMode & DSP_SLICE_VP_HIDE)) {
            if(state->skeletonState->displayMode & DSP_ACTIVETREE) {
                state->skeletonState->displayListSkeletonSlicePlaneVP =
                    renderActiveTreeSkeleton(state, 0);
            }
            else {
                state->skeletonState->displayListSkeletonSlicePlaneVP =
                    renderSuperCubeSkeleton(state, 0);
            }
        }
    }

    return TRUE;
}



uint32_t renderSkeletonVP(uint32_t currentVP, struct stateInfo *state) {
    char *textBuffer;
    char *c;
    uint32_t i;

    GLUquadricObj *gluCylObj = NULL;

    /* Used for calculation of slice pane length inside the 3d view */
    float dataPxX, dataPxY;

    textBuffer = malloc(32);
    memset(textBuffer, '\0', 32);

    glClear(GL_DEPTH_BUFFER_BIT); /* better place? TDitem */

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    /* left, right, bottom, top, near, far clipping planes; substitute arbitrary vals to something more sensible. TDitem */
//LOG("%f, %f, %f", state->skeletonState->translateX, state->skeletonState->translateY, state->skeletonState->zoomLevel);
    glOrtho(state->skeletonState->volBoundary * state->skeletonState->zoomLevel + state->skeletonState->translateX,
        state->skeletonState->volBoundary - (state->skeletonState->volBoundary * state->skeletonState->zoomLevel) + state->skeletonState->translateX,
        state->skeletonState->volBoundary - (state->skeletonState->volBoundary * state->skeletonState->zoomLevel) + state->skeletonState->translateY,
        state->skeletonState->volBoundary * state->skeletonState->zoomLevel + state->skeletonState->translateY, -1000, 10 *state->skeletonState->volBoundary);

    if(state->viewerState->lightOnOff) {
        /* Configure light */
        glEnable(GL_LIGHTING);
        GLfloat ambientLight[] = {0.5, 0.5, 0.5, 0.8};
        GLfloat diffuseLight[] = {1., 1., 1., 1.};
        GLfloat lightPos[] = {0., 0., 1., 1.};

        glLightfv(GL_LIGHT0,GL_AMBIENT,ambientLight);
        glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuseLight);
        glLightfv(GL_LIGHT0,GL_POSITION,lightPos);
        glEnable(GL_LIGHT0);

        GLfloat global_ambient[] = { 0.5f, 0.5f, 0.5f, 1.0f };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);

        /* Enable materials with automatic color tracking */
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        glEnable(GL_COLOR_MATERIAL);
    }
    if(state->viewerState->multisamplingOnOff) glEnable(GL_MULTISAMPLE);

    /*
     * Now we set up the view on the skeleton and draw some very basic VP stuff like the gray background
    */
    state->skeletonState->viewChanged = TRUE;
    if(state->skeletonState->viewChanged) {
        state->skeletonState->viewChanged = FALSE;
        if(state->skeletonState->displayListView) glDeleteLists(state->skeletonState->displayListView, 1);
        state->skeletonState->displayListView = glGenLists(1);
        /* COMPILE_AND_EXECUTE because we grab the rotation matrix inside! */
        glNewList(state->skeletonState->displayListView, GL_COMPILE_AND_EXECUTE);

        glEnable(GL_DEPTH_TEST);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        /*
         * Now we draw the  background of our skeleton VP
         */

        glPushMatrix();
        glTranslatef(0., 0., -10. * ((float)state->skeletonState->volBoundary - 2.));

        glShadeModel(GL_SMOOTH);
        glDisable(GL_TEXTURE_2D);

        glLoadName(1);
        glColor4f(0.9, 0.9, 0.9, 1.);
        /* The * 10 should prevent, that the user translates into space with gray background - dirty solution. TDitem */
        glBegin(GL_QUADS);
            glVertex3i(-state->skeletonState->volBoundary * 10, -state->skeletonState->volBoundary * 10, 0);
            glVertex3i(state->skeletonState->volBoundary  * 10, -state->skeletonState->volBoundary * 10, 0);
            glVertex3i(state->skeletonState->volBoundary  * 10, state->skeletonState->volBoundary  * 10, 0);
            glVertex3i(-state->skeletonState->volBoundary * 10, state->skeletonState->volBoundary  * 10, 0);
        glEnd();

        glPopMatrix();

        /* load model view matrix that stores rotation state! */
        glLoadMatrixf(state->skeletonState->skeletonVpModelView);


        /* perform user defined coordinate system rotations. use single matrix multiplication as opt.! TDitem */
        if((state->skeletonState->rotateX)
            || (state->skeletonState->rotateY)
            || (state->skeletonState->rotateZ)) {


            if((state->skeletonState->rotateAroundActiveNode) && (state->skeletonState->activeNode)) {
                glTranslatef(-((float)state->boundary.x / 2.),-((float)state->boundary.y / 2),-((float)state->boundary.z / 2.));
                glTranslatef((float)state->skeletonState->activeNode->position.x / (float)state->magnification,
                             (float)state->skeletonState->activeNode->position.y / (float)state->magnification,
                             (float)state->skeletonState->activeNode->position.z / (float)state->magnification);
                glScalef(1., 1., state->viewerState->voxelXYtoZRatio);
                glRotatef((float)state->skeletonState->rotateX, 1., 0., 0.);
                glRotatef((float)state->skeletonState->rotateY, 0., 1., 0.);
                glRotatef((float)state->skeletonState->rotateZ, 0., 0., 1.);
                glScalef(1., 1., 1./state->viewerState->voxelXYtoZRatio);
                glTranslatef(-(float)state->skeletonState->activeNode->position.x / (float)state->magnification,
                             -(float)state->skeletonState->activeNode->position.y / (float)state->magnification,
                             -(float)state->skeletonState->activeNode->position.z / (float)state->magnification);
                glTranslatef(((float)state->boundary.x / 2.),((float)state->boundary.y / 2.),((float)state->boundary.z / 2.));
            }
            /* rotate around dataset center if no active node is selected */
            else {
                glScalef(1., 1., state->viewerState->voxelXYtoZRatio);
                glRotatef((float)state->skeletonState->rotateX, 1., 0., 0.);
                glRotatef((float)state->skeletonState->rotateY, 0., 1., 0.);
                glRotatef((float)state->skeletonState->rotateZ, 0., 0., 1.);
                glScalef(1., 1., 1./state->viewerState->voxelXYtoZRatio);
            }

            /* save the modified basic model view matrix */

            glGetFloatv(GL_MODELVIEW_MATRIX, state->skeletonState->skeletonVpModelView);

            /* reset the relative rotation angles because rotation has been performed. */
            state->skeletonState->rotateX = 0;
            state->skeletonState->rotateY = 0;
            state->skeletonState->rotateZ = 0;
        }

        switch(state->skeletonState->definedSkeletonVpView) {
            case 0:
                break;
            case 1:
                /* XY viewport like view */
                state->skeletonState->definedSkeletonVpView = 0;

                glLoadIdentity();
                glTranslatef((float)state->skeletonState->volBoundary / 2.,
                             (float)state->skeletonState->volBoundary / 2.,
                             (float)state->skeletonState->volBoundary / -2.);
//glScalef(1., 1., 1./state->viewerState->voxelXYtoZRatio);
                glGetFloatv(GL_MODELVIEW_MATRIX, state->skeletonState->skeletonVpModelView);

                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();

                state->skeletonState->translateX = ((float)state->boundary.x / -2.) + (float)state->viewerState->currentPosition.x;
                state->skeletonState->translateY = ((float)state->boundary.y / -2.) + (float)state->viewerState->currentPosition.y;

                glOrtho(state->skeletonState->volBoundary * state->skeletonState->zoomLevel + state->skeletonState->translateX,
                        state->skeletonState->volBoundary - (state->skeletonState->volBoundary * state->skeletonState->zoomLevel) + state->skeletonState->translateX,
                        state->skeletonState->volBoundary - (state->skeletonState->volBoundary * state->skeletonState->zoomLevel) + state->skeletonState->translateY,
                        state->skeletonState->volBoundary * state->skeletonState->zoomLevel + state->skeletonState->translateY,
                        -500,
                        10 * state->skeletonState->volBoundary);

                break;

            case 2:
                /* XZ viewport like view*/
                state->skeletonState->definedSkeletonVpView = 0;

                glLoadIdentity();

                glTranslatef((float)state->skeletonState->volBoundary / 2.,
                             (float)state->skeletonState->volBoundary / 2.,
                             (float)state->skeletonState->volBoundary / -2.);

                glRotatef(90, 0., 1., 0.);
                glScalef(1., 1., 1./state->viewerState->voxelXYtoZRatio);
                glGetFloatv(GL_MODELVIEW_MATRIX, state->skeletonState->skeletonVpModelView);

                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();

                state->skeletonState->translateX = ((float)state->boundary.z / -2.) + (float)state->viewerState->currentPosition.z;
                state->skeletonState->translateY = ((float)state->boundary.y / -2.) + (float)state->viewerState->currentPosition.y;

                glOrtho(state->skeletonState->volBoundary * state->skeletonState->zoomLevel + state->skeletonState->translateX,
                        state->skeletonState->volBoundary - (state->skeletonState->volBoundary * state->skeletonState->zoomLevel) + state->skeletonState->translateX,
                        state->skeletonState->volBoundary - (state->skeletonState->volBoundary * state->skeletonState->zoomLevel) + state->skeletonState->translateY,
                        state->skeletonState->volBoundary * state->skeletonState->zoomLevel + state->skeletonState->translateY,
                        -500,
                        10 * state->skeletonState->volBoundary);

                break;

            case 3:
                /* YZ viewport like view */
                state->skeletonState->definedSkeletonVpView = 0;
                glLoadIdentity();
                glTranslatef((float)state->skeletonState->volBoundary / 2.,
                             (float)state->skeletonState->volBoundary / 2.,
                             (float)state->skeletonState->volBoundary / -2.);
                glRotatef(270, 1., 0., 0.);
                glScalef(1., 1., 1./state->viewerState->voxelXYtoZRatio);

                glGetFloatv(GL_MODELVIEW_MATRIX, state->skeletonState->skeletonVpModelView);

                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();

                state->skeletonState->translateX = ((float)state->boundary.x / -2.) + (float)state->viewerState->currentPosition.x;
                state->skeletonState->translateY = ((float)state->boundary.z / -2.) + (float)state->viewerState->currentPosition.z;

                glOrtho(state->skeletonState->volBoundary * state->skeletonState->zoomLevel + state->skeletonState->translateX,
                        state->skeletonState->volBoundary - (state->skeletonState->volBoundary * state->skeletonState->zoomLevel) + state->skeletonState->translateX,
                        state->skeletonState->volBoundary - (state->skeletonState->volBoundary * state->skeletonState->zoomLevel) + state->skeletonState->translateY,
                        state->skeletonState->volBoundary * state->skeletonState->zoomLevel + state->skeletonState->translateY,
                        -500,
                        10 * state->skeletonState->volBoundary);

                break;

            case 4:
                /* flip view */
                state->skeletonState->definedSkeletonVpView = 0;

                /* */
                /*LOG("%f, %f, %f", state->skeletonState->translateX, state->skeletonState->translateY, state->skeletonState->zoomLevel);*/
                glScalef(-1.,-1.,1.);

                glGetFloatv(GL_MODELVIEW_MATRIX, state->skeletonState->skeletonVpModelView);

                break;

            case 5:
                /* Resetting */
                state->skeletonState->definedSkeletonVpView = 0;
                state->skeletonState->translateX = 0;
                state->skeletonState->translateY = 0;
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
                glTranslatef((float)state->skeletonState->volBoundary / 2.,
                             (float)state->skeletonState->volBoundary / 2.,
                             (float)state->skeletonState->volBoundary / -2.);
                glScalef(-1., 1., 1.);
                glRotatef(235., 1., 0., 0.);
                glRotatef(210., 0., 0., 1.);
                glGetFloatv(GL_MODELVIEW_MATRIX, state->skeletonState->skeletonVpModelView);
                state->skeletonState->zoomLevel = SKELZOOMMIN;
                break;
        }

        /*
         * Draw the slice planes for orientation inside the data stack
         */

        glPushMatrix();

        /* single operation! TDitem */
        glTranslatef(-((float)state->boundary.x / 2.),-((float)state->boundary.y / 2.),-((float)state->boundary.z / 2.));
        glTranslatef(0.5,0.5,0.5);
        glTranslatef((float)state->viewerState->currentPosition.x, (float)state->viewerState->currentPosition.y, (float)state->viewerState->currentPosition.z);

        glEnable(GL_TEXTURE_2D);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColor4f(1., 1., 1., 1.);

        for(i = 0; i < state->viewerState->numberViewPorts; i++) {
            dataPxX = state->viewerState->viewPorts[i].texture.displayedEdgeLengthX
                / state->viewerState->viewPorts[i].texture.texUnitsPerDataPx * 0.5;
            dataPxY = state->viewerState->viewPorts[i].texture.displayedEdgeLengthY
                / state->viewerState->viewPorts[i].texture.texUnitsPerDataPx * 0.5;
            switch(state->viewerState->viewPorts[i].type) {
            case VIEWPORT_XY:
                if(!state->skeletonState->showXYplane) break;

                glBindTexture(GL_TEXTURE_2D, state->viewerState->viewPorts[i].texture.texHandle);
                glLoadName(VIEWPORT_XY);
                glBegin(GL_QUADS);
                    glNormal3i(0,0,1);
                    glTexCoord2f(state->viewerState->viewPorts[i].texture.texLUx, state->viewerState->viewPorts[i].texture.texLUy);
                    glVertex3f(-dataPxX, -dataPxY, 0.);
                    glTexCoord2f(state->viewerState->viewPorts[i].texture.texRUx, state->viewerState->viewPorts[i].texture.texRUy);
                    glVertex3f(dataPxX, -dataPxY, 0.);
                    glTexCoord2f(state->viewerState->viewPorts[i].texture.texRLx, state->viewerState->viewPorts[i].texture.texRLy);
                    glVertex3f(dataPxX, dataPxY, 0.);
                    glTexCoord2f(state->viewerState->viewPorts[i].texture.texLLx, state->viewerState->viewPorts[i].texture.texLLy);
                    glVertex3f(-dataPxX, dataPxY, 0.);
                glEnd();
                glBindTexture (GL_TEXTURE_2D, 0);
                break;
            case VIEWPORT_XZ:
                if(!state->skeletonState->showXZplane) break;
                glBindTexture(GL_TEXTURE_2D, state->viewerState->viewPorts[i].texture.texHandle);
                glLoadName(VIEWPORT_XZ);
                glBegin(GL_QUADS);
                    glNormal3i(0,1,0);
                    glTexCoord2f(state->viewerState->viewPorts[i].texture.texLUx, state->viewerState->viewPorts[i].texture.texLUy);
                    glVertex3f(-dataPxX, 0., -dataPxY);
                    glTexCoord2f(state->viewerState->viewPorts[i].texture.texRUx, state->viewerState->viewPorts[i].texture.texRUy);
                    glVertex3f(dataPxX, 0., -dataPxY);
                    glTexCoord2f(state->viewerState->viewPorts[i].texture.texRLx, state->viewerState->viewPorts[i].texture.texRLy);
                    glVertex3f(dataPxX, 0., dataPxY);
                    glTexCoord2f(state->viewerState->viewPorts[i].texture.texLLx, state->viewerState->viewPorts[i].texture.texLLy);
                    glVertex3f(-dataPxX, 0., dataPxY);
                glEnd();
                glBindTexture (GL_TEXTURE_2D, 0);
                break;
            case VIEWPORT_YZ:
                if(!state->skeletonState->showYZplane) break;
                glBindTexture(GL_TEXTURE_2D, state->viewerState->viewPorts[i].texture.texHandle);
                glLoadName(VIEWPORT_YZ);
                glBegin(GL_QUADS);
                    glNormal3i(1,0,0);
                    glTexCoord2f(state->viewerState->viewPorts[i].texture.texLUx, state->viewerState->viewPorts[i].texture.texLUy);
                    glVertex3f(0., -dataPxX, -dataPxY);
                    glTexCoord2f(state->viewerState->viewPorts[i].texture.texRUx, state->viewerState->viewPorts[i].texture.texRUy);
                    glVertex3f(0., dataPxX, -dataPxY);
                    glTexCoord2f(state->viewerState->viewPorts[i].texture.texRLx, state->viewerState->viewPorts[i].texture.texRLy);
                    glVertex3f(0., dataPxX, dataPxY);
                    glTexCoord2f(state->viewerState->viewPorts[i].texture.texLLx, state->viewerState->viewPorts[i].texture.texLLy);
                    glVertex3f(0., -dataPxX, dataPxY);
                glEnd();
                glBindTexture (GL_TEXTURE_2D, 0);
                break;
            }

        }

        glDisable(GL_TEXTURE_2D);
        glLoadName(3);
        for(i = 0; i < state->viewerState->numberViewPorts; i++) {
            dataPxX = state->viewerState->viewPorts[i].texture.displayedEdgeLengthX / state->viewerState->viewPorts[i].texture.texUnitsPerDataPx * 0.5;
            dataPxY = state->viewerState->viewPorts[i].texture.displayedEdgeLengthY / state->viewerState->viewPorts[i].texture.texUnitsPerDataPx * 0.5;
            switch(state->viewerState->viewPorts[i].type) {
            case VIEWPORT_XY:
                glColor4f(0.7, 0., 0., 1.);
                glBegin(GL_LINE_LOOP);
                    glVertex3f(-dataPxX, -dataPxY, 0.);
                    glVertex3f(dataPxX, -dataPxY, 0.);
                    glVertex3f(dataPxX, dataPxY, 0.);
                    glVertex3f(-dataPxX, dataPxY, 0.);
                glEnd();

                glColor4f(0., 0., 0., 1.);
                glPushMatrix();
                glTranslatef(-dataPxX, 0., 0.);
                glRotatef(90., 0., 1., 0.);
                gluCylObj = gluNewQuadric();
                gluQuadricNormals(gluCylObj, GLU_SMOOTH);
                gluQuadricOrientation(gluCylObj, GLU_OUTSIDE);
                gluCylinder(gluCylObj, 0.4, 0.4, dataPxX * 2, 5, 5);
                gluDeleteQuadric(gluCylObj);
                glPopMatrix();

                glPushMatrix();
                glTranslatef(0., dataPxY, 0.);
                glRotatef(90., 1., 0., 0.);
                gluCylObj = gluNewQuadric();
                gluQuadricNormals(gluCylObj, GLU_SMOOTH);
                gluQuadricOrientation(gluCylObj, GLU_OUTSIDE);
                gluCylinder(gluCylObj, 0.4, 0.4, dataPxY * 2, 5, 5);
                gluDeleteQuadric(gluCylObj);
                glPopMatrix();

                break;
            case VIEWPORT_XZ:
                glColor4f(0., 0.7, 0., 1.);
                glBegin(GL_LINE_LOOP);
                    glVertex3f(-dataPxX, 0., -dataPxY);
                    glVertex3f(dataPxX, 0., -dataPxY);
                    glVertex3f(dataPxX, 0., dataPxY);
                    glVertex3f(-dataPxX, 0., dataPxY);
                glEnd();

                glColor4f(0., 0., 0., 1.);
                glPushMatrix();
                glTranslatef(0., 0., -dataPxY);
                gluCylObj = gluNewQuadric();
                gluQuadricNormals(gluCylObj, GLU_SMOOTH);
                gluQuadricOrientation(gluCylObj, GLU_OUTSIDE);
                gluCylinder(gluCylObj, 0.4, 0.4, dataPxY * 2, 5, 5);
                gluDeleteQuadric(gluCylObj);
                glPopMatrix();

                break;
            case VIEWPORT_YZ:
                glColor4f(0., 0., 0.7, 1.);
                glBegin(GL_LINE_LOOP);
                    glVertex3f(0., -dataPxX, -dataPxY);
                    glVertex3f(0., dataPxX, -dataPxY);
                    glVertex3f(0., dataPxX, dataPxY);
                    glVertex3f(0., -dataPxX, dataPxY);
                glEnd();
                break;
            }
        }

        glPopMatrix();
        glEndList();
    }
    else {
        glCallList(state->skeletonState->displayListView);
    }

    /*
     * Now we draw the skeleton structure (Changes of it are adressed inside updateSkeletonDisplayList())
    */

    if(state->skeletonState->displayListSkeletonSkeletonizerVP)
        glCallList(state->skeletonState->displayListSkeletonSkeletonizerVP);

    /*
     * Now we draw the dataset corresponding stuff (volume box of right size, axis descriptions...)
    */

    if(state->skeletonState->datasetChanged) {

        state->skeletonState->datasetChanged = FALSE;
        if(state->skeletonState->displayListDataset) glDeleteLists(state->skeletonState->displayListDataset, 1);
        state->skeletonState->displayListDataset = glGenLists(1);
        glNewList(state->skeletonState->displayListDataset, GL_COMPILE);
        glEnable(GL_BLEND);
        /*
         * Now we draw the data volume box. use display list for that...very static TDitem
         */

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLoadName(3);
        glColor4f(0., 0., 0., 0.1);
        glBegin(GL_QUADS);
            glNormal3i(0,0,1);
            glVertex3i(-(state->boundary.x / 2), -(state->boundary.y / 2), -(state->boundary.z / 2));
            glVertex3i(state->boundary.x / 2, -(state->boundary.y / 2), -(state->boundary.z / 2));

            glVertex3i(state->boundary.x / 2, (state->boundary.y / 2), -(state->boundary.z / 2));
            glVertex3i(-(state->boundary.x / 2), (state->boundary.y / 2), -(state->boundary.z / 2));

            glNormal3i(0,0,1);
            glVertex3i(-(state->boundary.x / 2), -(state->boundary.y / 2), (state->boundary.z / 2));
            glVertex3i(state->boundary.x / 2, -(state->boundary.y / 2), (state->boundary.z / 2));

            glVertex3i(state->boundary.x / 2, (state->boundary.y / 2), (state->boundary.z / 2));
            glVertex3i(-(state->boundary.x / 2), (state->boundary.y / 2), (state->boundary.z / 2));

            glNormal3i(0,1,0);
            glVertex3i(-(state->boundary.x / 2), -(state->boundary.y / 2), -(state->boundary.z / 2));
            glVertex3i(-(state->boundary.x / 2), -(state->boundary.y / 2), (state->boundary.z / 2));

            glVertex3i(state->boundary.x / 2, -(state->boundary.y / 2), (state->boundary.z / 2));
            glVertex3i(state->boundary.x / 2, -(state->boundary.y / 2), -(state->boundary.z / 2));

            glNormal3i(0,1,0);
            glVertex3i(-(state->boundary.x / 2), (state->boundary.y / 2), -(state->boundary.z / 2));
            glVertex3i(-(state->boundary.x / 2), (state->boundary.y / 2), (state->boundary.z / 2));

            glVertex3i(state->boundary.x / 2, (state->boundary.y / 2), (state->boundary.z / 2));
            glVertex3i(state->boundary.x / 2, (state->boundary.y / 2), -(state->boundary.z / 2));

            glNormal3i(1,0,0);
            glVertex3i(-(state->boundary.x / 2), -(state->boundary.y / 2), -(state->boundary.z / 2));
            glVertex3i(-(state->boundary.x / 2), -(state->boundary.y / 2), (state->boundary.z / 2));

            glVertex3i(-(state->boundary.x / 2), (state->boundary.y / 2), (state->boundary.z / 2));
            glVertex3i(-(state->boundary.x / 2), (state->boundary.y / 2), -(state->boundary.z / 2));

            glNormal3i(1,0,0);
            glVertex3i(state->boundary.x / 2, -(state->boundary.y / 2), -(state->boundary.z / 2));
            glVertex3i(state->boundary.x / 2, -(state->boundary.y / 2), (state->boundary.z / 2));

            glVertex3i(state->boundary.x / 2, (state->boundary.y / 2), (state->boundary.z / 2));
            glVertex3i(state->boundary.x / 2, (state->boundary.y / 2), -(state->boundary.z / 2));
        glEnd();

        glColor4f(0., 0., 0., 1.);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glPushMatrix();
        glTranslatef(-(state->boundary.x / 2),-(state->boundary.y / 2),-(state->boundary.z / 2));
        gluCylObj = gluNewQuadric();
        gluQuadricNormals(gluCylObj, GLU_SMOOTH);
        gluQuadricOrientation(gluCylObj, GLU_OUTSIDE);
        gluCylinder(gluCylObj, 3., 3. , state->boundary.z, 5, 5);
        gluDeleteQuadric(gluCylObj);

        glPushMatrix();
        glTranslatef(0.,0., state->boundary.z);
        gluCylObj = gluNewQuadric();
        gluQuadricNormals(gluCylObj, GLU_SMOOTH);
        gluQuadricOrientation(gluCylObj, GLU_OUTSIDE);
        gluCylinder(gluCylObj, 15., 0. , 50., 15, 15);
        gluDeleteQuadric(gluCylObj);
        glPopMatrix();

        gluCylObj = gluNewQuadric();
        gluQuadricNormals(gluCylObj, GLU_SMOOTH);
        gluQuadricOrientation(gluCylObj, GLU_OUTSIDE);
        glRotatef(90., 0., 1., 0.);
        gluCylinder(gluCylObj, 3., 3. , state->boundary.x, 5, 5);
        gluDeleteQuadric(gluCylObj);

        glPushMatrix();
        glTranslatef(0.,0., state->boundary.x);
        gluCylObj = gluNewQuadric();
        gluQuadricNormals(gluCylObj, GLU_SMOOTH);
        gluQuadricOrientation(gluCylObj, GLU_OUTSIDE);
        gluCylinder(gluCylObj, 15., 0. , 50., 15, 15);
        gluDeleteQuadric(gluCylObj);
        glPopMatrix();

        gluCylObj = gluNewQuadric();
        gluQuadricNormals(gluCylObj, GLU_SMOOTH);
        gluQuadricOrientation(gluCylObj, GLU_OUTSIDE);
        glRotatef(-90., 1., 0., 0.);
        gluCylinder(gluCylObj, 3., 3. , state->boundary.y, 5, 5);
        gluDeleteQuadric(gluCylObj);

        glPushMatrix();
        glTranslatef(0.,0., state->boundary.y);
        gluCylObj = gluNewQuadric();
        gluQuadricNormals(gluCylObj, GLU_SMOOTH);
        gluQuadricOrientation(gluCylObj, GLU_OUTSIDE);
        gluCylinder(gluCylObj, 15., 0. , 50., 15, 15);
        gluDeleteQuadric(gluCylObj);
        glPopMatrix();

        glPopMatrix();

        /*
         * Draw axis description
         */

        glColor4f(0., 0., 0., 1.);
        memset(textBuffer, '\0', 32);
        glRasterPos3f((float)-(state->boundary.x) / 2. - 50., (float)-(state->boundary.y) / 2. - 50., (float)-(state->boundary.z) / 2. - 50.);
        sprintf(textBuffer, "1, 1, 1");
        for (c=textBuffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_10, *c);
        }
        memset(textBuffer, '\0', 32);
        glRasterPos3f((float)(state->boundary.x) / 2. - 50., -(state->boundary.y / 2) - 50., -(state->boundary.z / 2)- 50.);
        sprintf(textBuffer, "%d, 1, 1", state->boundary.x + 1);
        for (c=textBuffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_10, *c);
        }

        memset(textBuffer, '\0', 32);
        glRasterPos3f(-(state->boundary.x / 2)- 50., (float)(state->boundary.y) / 2. - 50., -(state->boundary.z / 2)- 50.);
        sprintf(textBuffer, "1, %d, 1", state->boundary.y + 1);
        for (c=textBuffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_10, *c);
        }
        memset(textBuffer, '\0', 32);
        glRasterPos3f(-(state->boundary.x / 2)- 50., -(state->boundary.y / 2)- 50., (float)(state->boundary.z) / 2. - 50.);
        sprintf(textBuffer, "1, 1, %d", state->boundary.z + 1);
        for (c=textBuffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_10, *c);
        }
        glEnable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glEndList();
        glCallList(state->skeletonState->displayListDataset);

    }
    else {
        glCallList(state->skeletonState->displayListDataset);
    }

    /*
     * Reset previously changed OGL parameters
     */

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    //glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glDisable(GL_MULTISAMPLE);
    glLoadIdentity();

    free(textBuffer);

    renderViewportBorders(currentVP);

    return TRUE;
}


/*
 * Some useful helper functions for geometry
*/

float scalarProduct(floatCoordinate *v1, floatCoordinate *v2) {
    return ((v1->x * v2->x) + (v1->y * v2->y) + (v1->z * v2->z));
}


floatCoordinate *crossProduct(floatCoordinate *v1, floatCoordinate *v2) {
    floatCoordinate *result = NULL;
    result = malloc(sizeof(floatCoordinate));

    //Orthogonal result vector
    result->x = v1->y * v2->z - v1->z * v2->y;
    result->y = v1->z * v2->x - v1->x * v2->z;
    result->z = v1->x * v2->y - v1->y * v2->x;

    return result;
}

//Result in rad
float vectorAngle(floatCoordinate *v1, floatCoordinate *v2) {
    return ((float)acos((double)(scalarProduct(v1, v2)) / (euclidicNorm(v1)*euclidicNorm(v2))));
}

float euclidicNorm(floatCoordinate *v) {
    return ((float)sqrt((double)scalarProduct(v, v)));
}

uint32_t normalizeVector(floatCoordinate *v) {
    float norm = euclidicNorm(v);

    v->x /= norm;
    v->y /= norm;
    v->z /= norm;

    return TRUE;
}

float radToDeg(float rad) {
    return ((180. * rad) / PI);
}

float degToRad(float deg) {
    return ((deg / 180.) * PI);
}

int32_t roundFloat(float number) {
    // No freaky shit, JK! :-D
    if(number >= 0)
        return (int32_t)(number + 0.5);
    else
        return (int32_t)(number - 0.5);
}

int32_t sgn(float number) {
    if(number>0.) return 1;
    else if(number==0.) return 0;
    else return -1;
}

/* We use the openGL selection buffer for picking. You have to pass openGL window coords... not window manager windows coords!! */
uint32_t retrieveVisibleObjectBeneathSquare(uint32_t currentVP,
    uint32_t x, uint32_t y, uint32_t width,
    struct stateInfo *state) {
    uint32_t i;

    /* 8192 is really arbitrary. It should be a value dependent on the
    number of nodes / segments */
 	GLuint selectionBuffer[8192] = {0};
 	GLint hits, openGLviewport[4];
    GLuint names, *ptr, minZ, *ptrName;
    ptrName = NULL;

    glViewport(state->viewerState->viewPorts[currentVP].upperLeftCorner.x,
        state->viewerState->screenSizeY
        - state->viewerState->viewPorts[currentVP].upperLeftCorner.y
        - state->viewerState->viewPorts[currentVP].edgeLength,
        state->viewerState->viewPorts[currentVP].edgeLength,
        state->viewerState->viewPorts[currentVP].edgeLength);

    glGetIntegerv(GL_VIEWPORT, openGLviewport);

    glSelectBuffer(8192, selectionBuffer);

    state->viewerState->selectModeFlag = TRUE;

    glRenderMode(GL_SELECT);

    glInitNames();
    glPushName(0);

	glMatrixMode(GL_PROJECTION);

    glLoadIdentity();

    gluPickMatrix(x, y, (float)width, (float)width, openGLviewport);

    if(state->viewerState->viewPorts[currentVP].type == VIEWPORT_SKELETON) {
            glOrtho(state->skeletonState->volBoundary
                * state->skeletonState->zoomLevel
                + state->skeletonState->translateX,
                state->skeletonState->volBoundary
                - (state->skeletonState->volBoundary
                * state->skeletonState->zoomLevel)
                + state->skeletonState->translateX,
                state->skeletonState->volBoundary
                - (state->skeletonState->volBoundary
                * state->skeletonState->zoomLevel)
                + state->skeletonState->translateY,
                state->skeletonState->volBoundary
                * state->skeletonState->zoomLevel
                + state->skeletonState->translateY,
                -10000, 10 * state->skeletonState->volBoundary);
            glCallList(state->skeletonState->displayListView);
            glCallList(state->skeletonState->displayListSkeletonSkeletonizerVP);
            glCallList(state->skeletonState->displayListDataset); //TDitem fix that display list !!

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDisable(GL_BLEND);
            glDisable(GL_LIGHTING);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_MULTISAMPLE);
    }
    else {
            //glEnable(GL_DEPTH_TEST);
            //glCallList(state->viewerState->viewPorts[currentVP].displayList);
            glDisable(GL_DEPTH_TEST);
            renderOrthogonalVP(currentVP, state);
    }


    hits = glRenderMode(GL_RENDER);
    glLoadIdentity();

    ptr = (GLuint *)selectionBuffer;

    minZ = 0xffffffff;


    for(i = 0; i < hits; i++) {
        names = *ptr;

        ptr++;
        if((*ptr < minZ) && (*(ptr + 2) >= 50)) {
            minZ = *ptr;
            ptrName = ptr + 2;
        }
        ptr += names + 2;
    }

    state->viewerState->selectModeFlag = FALSE;
    if(ptrName) return *ptrName - 50;
    else return FALSE;
}

static uint32_t renderCylinder(int32_t coordinateMag,
                               Coordinate *base,
                               float baseRadius,
                               Coordinate *top,
                               float topRadius) {
    float currentAngle = 0.;
    int32_t transFactor = 1;
    Coordinate transBase, transTop;
    floatCoordinate segDirection, tempVec, *tempVec2;
    GLUquadricObj *gluCylObj = NULL;

    if(coordinateMag == ORIGINAL_MAG_COORDINATES) {
        transFactor = state->magnification;
    }

    transBase.x = base->x / transFactor;
    transBase.y = base->y / transFactor;
    transBase.z = base->z / transFactor;
    transTop.x = top->x / transFactor;
    transTop.y = top->y / transFactor;
    transTop.z = top->z / transFactor;
    topRadius = topRadius / transFactor;
    baseRadius = baseRadius / transFactor;

    if(!(state->skeletonState->displayMode & DSP_LINES_POINTS)) {
        glPushMatrix();
        gluCylObj = gluNewQuadric();
        gluQuadricNormals(gluCylObj, GLU_SMOOTH);
        gluQuadricOrientation(gluCylObj, GLU_OUTSIDE);

        glTranslatef((float)transBase.x, (float)transBase.y, (float)transBase.z);

        //Some calculations for the correct direction of the cylinder.
        tempVec.x = 0.;
        tempVec.y = 0.;
        tempVec.z = 1.;
        segDirection.x = (float)(transTop.x - transBase.x);
        segDirection.y = (float)(transTop.y - transBase.y);
        segDirection.z = (float)(transTop.z - transBase.z);

        //temVec2 defines the rotation axis
        tempVec2 = crossProduct(&tempVec, &segDirection);
        currentAngle = radToDeg(vectorAngle(&tempVec, &segDirection));

        //some gl implementations have problems with the params occuring for
        //segs in straight directions. we need a fix here.
        glRotatef(currentAngle, tempVec2->x, tempVec2->y, tempVec2->z);

        free(tempVec2);

        gluCylinder(gluCylObj, baseRadius, topRadius, euclidicNorm(&segDirection), 4, 1);
        gluDeleteQuadric(gluCylObj);
        glPopMatrix();
    }
    else {
        glBegin(GL_LINES);
            glVertex3f((float)transBase.x, (float)transBase.y, (float)transBase.z);
            glVertex3f((float)transTop.x, (float)transTop.y, (float)transTop.z);
        glEnd();
    }

    return TRUE;
}


static uint32_t renderSphere(int32_t coordinateMag, Coordinate *pos, float radius) {
    GLUquadricObj *gluSphereObj = NULL;
    Coordinate transPos;
    int32_t transFactor = 1;

    if(coordinateMag == ORIGINAL_MAG_COORDINATES)
        transFactor = state->magnification;

    transPos.x = pos->x / transFactor;
    transPos.y = pos->y / transFactor;
    transPos.z = pos->z / transFactor;

    radius = radius / (float)transFactor;

    /* Render point instead of sphere if user has chosen mode */
    if(!(state->skeletonState->displayMode & DSP_LINES_POINTS)) {
        glPushMatrix();
        glTranslatef((float)transPos.x, (float)transPos.y, (float)transPos.z);
        gluSphereObj = gluNewQuadric();
        gluQuadricNormals(gluSphereObj, GLU_SMOOTH);
        gluQuadricOrientation(gluSphereObj, GLU_OUTSIDE);

        gluSphere(gluSphereObj, radius, 5, 5);

        gluDeleteQuadric(gluSphereObj);
        glPopMatrix();
    }
    else {
        glPointSize(radius*3.);
        glBegin(GL_POINTS);
            glVertex3f((float)transPos.x, (float)transPos.y, (float)transPos.z);
        glEnd();
        glPointSize(1.);
    }

    return TRUE;
}

static uint32_t renderText(int32_t coordinateMag,
                           Coordinate *pos,
                           char *string) {

    char *c;
    int32_t transFactor = 1;
    Coordinate transPos;

    if(coordinateMag == ORIGINAL_MAG_COORDINATES)
        transFactor = state->magnification;

    transPos.x = pos->x / transFactor;
    transPos.y = pos->y / transFactor;
    transPos.z = pos->z / transFactor;

    glDisable(GL_DEPTH_TEST);
    glRasterPos3d(transPos.x, transPos.y, transPos.z);
    for (c = string; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_10, *c);
    }
    glEnable(GL_DEPTH_TEST);

    return TRUE;
}

static uint32_t renderSegPlaneIntersection(struct segmentListElement *segment, struct stateInfo *state) {

    if(!state->skeletonState->showIntersections) return TRUE;
    if(state->skeletonState->displayMode & DSP_LINES_POINTS) return TRUE;

    float p[2][3], a, currentAngle, length, radius, distSourceInter, sSr_local, sTr_local;
    int32_t i, distToCurrPos;
    floatCoordinate *tempVec2, tempVec, tempVec3, segDir, intPoint, sTp_local, sSp_local;
    GLUquadricObj *gluCylObj = NULL;

    sSp_local.x = segment->source->position.x / state->magnification;
    sSp_local.y = segment->source->position.y / state->magnification;
    sSp_local.z = segment->source->position.z / state->magnification;

    sTp_local.x = segment->target->position.x / state->magnification;
    sTp_local.y = segment->target->position.y / state->magnification;
    sTp_local.z = segment->target->position.z / state->magnification;

    sSr_local = segment->source->radius / state->magnification;
    sTr_local = segment->target->radius / state->magnification;

    //n contains the normal vectors of the 3 orthogonal planes
    float n[3][3] = {{1.,0.,0.},
                    {0.,1.,0.},
                    {0.,0.,1.}};

    distToCurrPos = state->viewerState->zoomCube + 1 * state->cubeEdgeLength;

    //Check if there is an intersection between the given segment and one
    //of the slice planes.
    p[0][0] = (float)(sSp_local.x - state->viewerState->currentPosition.x);
    p[0][1] = (float)(sSp_local.y - state->viewerState->currentPosition.y);
    p[0][2] = (float)(sSp_local.z - state->viewerState->currentPosition.z);

    p[1][0] = (float)(sTp_local.x - state->viewerState->currentPosition.x);
    p[1][1] = (float)(sTp_local.y - state->viewerState->currentPosition.y);
    p[1][2] = (float)(sTp_local.z - state->viewerState->currentPosition.z);


    //i represents the current orthogonal plane
    for(i = 0; i<=2; i++) {
        //There is an intersection and the segment doesn't lie in the plane
        if(sgn(p[0][i])*sgn(p[1][i]) == -1) {
            //Calculate intersection point
            segDir.x = (float)(sTp_local.x - sSp_local.x);
            segDir.y = (float)(sTp_local.y - sSp_local.y);
            segDir.z = (float)(sTp_local.z - sSp_local.z);

            //a is the scaling factor for the straight line equation: g:=segDir*a+v0
            a = (n[i][0] * ((float)(state->viewerState->currentPosition.x - sSp_local.x))
                    + n[i][1] * ((float)(state->viewerState->currentPosition.y - sSp_local.y))
                    + n[i][2] * ((float)(state->viewerState->currentPosition.z - sSp_local.z)))
                / (segDir.x*n[i][0] + segDir.y*n[i][1] + segDir.z*n[i][2]);

            tempVec3.x = segDir.x * a;
            tempVec3.y = segDir.y * a;
            tempVec3.z = segDir.z * a;

            intPoint.x = tempVec3.x + (float)sSp_local.x;
            intPoint.y = tempVec3.y + (float)sSp_local.y;
            intPoint.z = tempVec3.z + (float)sSp_local.z;

            //Check wether the intersection point lies outside the current zoom cube
            if(abs((int32_t)intPoint.x - state->viewerState->currentPosition.x) <= distToCurrPos
                && abs((int32_t)intPoint.y - state->viewerState->currentPosition.y) <= distToCurrPos
                && abs((int32_t)intPoint.z - state->viewerState->currentPosition.z) <= distToCurrPos) {

                //Render a cylinder to highlight the intersection
                glPushMatrix();
                gluCylObj = gluNewQuadric();
                gluQuadricNormals(gluCylObj, GLU_SMOOTH);
                gluQuadricOrientation(gluCylObj, GLU_OUTSIDE);

                length = euclidicNorm(&segDir);
                distSourceInter = euclidicNorm(&tempVec3);

                if(sSr_local < sTr_local)
                    radius = sTr_local + sSr_local * (1. - distSourceInter / length);
                else if(sSr_local == sTr_local)
                    radius = sSr_local;
                else
                    radius = sSr_local - (sSr_local - sTr_local) * distSourceInter / length;

                segDir.x /= length;
                segDir.y /= length;
                segDir.z /= length;

                glTranslatef(intPoint.x - 0.75 * segDir.x, intPoint.y - 0.75 * segDir.y, intPoint.z - 0.75 * segDir.z);
                //glTranslatef(intPoint.x, intPoint.y, intPoint.z);

                //Some calculations for the correct direction of the cylinder.
                tempVec.x = 0.;
                tempVec.y = 0.;
                tempVec.z = 1.;

                //temVec2 defines the rotation axis
                tempVec2 = crossProduct(&tempVec, &segDir);
                currentAngle = radToDeg(vectorAngle(&tempVec, &segDir));
                glRotatef(currentAngle, tempVec2->x, tempVec2->y, tempVec2->z);
                free(tempVec2);

                glColor4f(0.,0.,0.,1.);

                if(state->skeletonState->overrideNodeRadiusBool)
                    gluCylinder(gluCylObj,
                        state->skeletonState->overrideNodeRadiusVal * state->skeletonState->segRadiusToNodeRadius*1.2,
                        state->skeletonState->overrideNodeRadiusVal * state->skeletonState->segRadiusToNodeRadius*1.2,
                        1.5, 4, 1);

                else gluCylinder(gluCylObj,
                        radius * state->skeletonState->segRadiusToNodeRadius*1.2,
                        radius * state->skeletonState->segRadiusToNodeRadius*1.2,
                        1.5, 4, 1);

                gluDeleteQuadric(gluCylObj);
                glPopMatrix();
            }

        }
    }

    return TRUE;
}

uint32_t initRenderer() {

    /* initialize the textures used to display the SBFSEM data TDitem: return val check*/
    initializeTextures();
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    /* Initialize the basic model view matrix for the skeleton VP
    Perform basic coordinate system rotations */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef((float)state->skeletonState->volBoundary / 2.,
        (float)state->skeletonState->volBoundary / 2.,
        -((float)state->skeletonState->volBoundary / 2.));

    glScalef(-1., 1., 1.);
    //);
    //LOG("state->viewerState->voxelXYtoZRatio = %f", state->viewerState->voxelXYtoZRatio);
    glRotatef(235., 1., 0., 0.);
    glRotatef(210., 0., 0., 1.);
 //glScalef(1., 1., 1./state->viewerState->voxelXYtoZRatio);
    /* save the matrix for further use... */
    glGetFloatv(GL_MODELVIEW_MATRIX, state->skeletonState->skeletonVpModelView);

    glLoadIdentity();

    return TRUE;
}

uint32_t splashScreen() {
    SDL_Surface *splashImg;

    splashImg = SDL_LoadBMP("splash");
    GLenum textureFormat;

    state->viewerState->splash = TRUE;
    uint32_t i, width, height, scaleFac = 1;
    float midX, midY;

    if(splashImg == NULL) {
        LOG("Could not display splash screen.");
        return FALSE;
    }

    width = splashImg->w / scaleFac;
    height = splashImg->h / scaleFac;
    /* Set the color mode */
    textureFormat = GL_RGB;

    glGenTextures(1, &state->viewerState->splashTexture);
    glBindTexture(GL_TEXTURE_2D, state->viewerState->splashTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, splashImg->format->BytesPerPixel, splashImg->w,
    splashImg->h, 0, textureFormat, GL_UNSIGNED_BYTE, splashImg->pixels);

    midX = (float)state->viewerState->screenSizeX / 2.;
    midY = (float)state->viewerState->screenSizeY / 2.;

    for(i = 0; i < 100; i++) {
        SDL_Delay(10);
        glBindTexture(GL_TEXTURE_2D, 0);

        drawGUI(state);

        glDisable(GL_DEPTH_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_BLEND);

        glViewport(0,
                   0,
                   state->viewerState->screenSizeX,
                   state->viewerState->screenSizeY);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        glOrtho(0, state->viewerState->screenSizeX,
                state->viewerState->screenSizeY, 0, 25, -25);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, state->viewerState->splashTexture);

        glColor4f(1., 1., 1., 80./((float)i)-1.);

        glBegin(GL_QUADS);
            glNormal3i(0,0,1);
            glTexCoord2f(0., 0.);
            glVertex3f(midX-(float)width/2., midY-(float)height/2., 0.);
            glTexCoord2f(1., 0.);
            glVertex3f(midX+(float)width/2., midY-(float)height/2., 0.);
            glTexCoord2f(1., 1.);
            glVertex3f(midX+(float)width/2., midY+(float)height/2., 0.);
            glTexCoord2f(0., 1.);
            glVertex3f(midX-(float)width/2., midY+(float)height/2., 0.);
        glEnd();

        glDisable(GL_BLEND);
        //glDisable(GL_TEXTURE_2D);
        AG_EndRendering(agDriverSw); //AGAR14
        AG_UnlockVFS(agView);
    }
    glEnable(GL_TEXTURE_2D);
    state->viewerState->splash = FALSE;
    return TRUE;
}
