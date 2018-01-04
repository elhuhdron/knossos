/*
 *  (C) Copyright 2007-2016
 *  Max-Planck-Gesellschaft zur Foerderung der Wissenschaften e.V.
 *
 *  This file is a part of KNOSSOS.
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

#ifndef DATASET_H
#define DATASET_H

#include "coordinate.h"

#include <QString>
#include <QUrl>

struct Dataset {
    enum class API {
        Heidelbrain, WebKnossos, GoogleBrainmaps, OpenConnectome
    };
    enum class CubeType {
        RAW_UNCOMPRESSED, RAW_JPG, RAW_J2K, RAW_JP2_6, SEGMENTATION_UNCOMPRESSED, SEGMENTATION_SZ_ZIP
    };

    static Dataset dummyDataset();
    static Dataset parseGoogleJson(const QString & json_raw);
    static Dataset parseOpenConnectomeJson(const QUrl & infoUrl, const QString & json_raw);
    static Dataset parseWebKnossosJson(const QString & json_raw);
    static Dataset fromLegacyConf(const QUrl & url, QString config);
    void checkMagnifications();
    void applyToState() const;

    static QUrl apiSwitch(const API api, const QUrl & baseUrl, const Coordinate globalCoord, const int scale, const int cubeedgelength, const CubeType type);
    static bool isOverlay(const CubeType type);

    //rutuja
    bool fexists(const char *filename);

    API api;
    Coordinate boundary{0,0,0};
    floatCoordinate scale{0,0,0};

    //rutuja
    Coordinate superChunk{0,0,0};
    Coordinate cube_offset;

    int magnification{0};
    int lowestAvailableMag{0};
    int highestAvailableMag{0};
    int cubeEdgeLength{128};
    int compressionRatio{0};
    bool remote{false};
    bool overlay{false};
    QString experimentname{};
    QUrl url;
    //rutuja
    //mesh file name
    QString hdf5;
    // static id to enable loading of multi-level segmented labels
    QString seg_static_label;
    //static id to enable loading of multiple raw datasets
    QString rw_static_label;
    //Mode of operation
    int mode;

    // watkinspv - make supervoxels in superchunks unique across entire dataset, only used in mode 1
    static const uint16_t SC_BITS = 12;  // modify this to control bits encoding superchunk (3d)
    static const uint16_t SCL_BITS = 3;  // modify this to control bits superchunk level
    static const uint16_t SVOX_ID_BITS = 64 - SCL_BITS - 3*SC_BITS;    // remainder of bits used for superchunk id
    static const uint16_t SCL_SHIFT = SVOX_ID_BITS + 3*SC_BITS;
    // xxx - this does not link on osx, says SC_SHIFT is undefined
    //static constexpr const uint16_t SC_SHIFT[3] = {SVOX_ID_BITS + 2*SC_BITS, SVOX_ID_BITS + SC_BITS, SVOX_ID_BITS};
    static const uint16_t SC_SHIFT[3];
    // https://stackoverflow.com/questions/12416639/how-to-create-mask-with-least-significat-bits-set-to-1-in-c
    static const uint64_t SC_ID_MSK = ((uint64_t)1  << SVOX_ID_BITS)-1;
    static const uint64_t SC_MSK = ((uint64_t)1  << SC_BITS)-1;
    static const uint64_t SCL_MSK = ((uint64_t)1  << SCL_BITS)-1;
    // riddle me this: python > perl <==> C > C++
    static inline uint64_t create_prefix(CoordOfCube coord, int seglevel) {
        return (((uint64_t) (coord.x & SC_MSK)) << SC_SHIFT[0]) |
               (((uint64_t) (coord.y & SC_MSK)) << SC_SHIFT[1]) |
               (((uint64_t) (coord.z & SC_MSK)) << SC_SHIFT[2]) |
               (((uint64_t) (seglevel & SCL_MSK)) << SCL_SHIFT);
    }
    //static inline uint64_t create_prefix(Coordinate coord) {
    //    return (((uint64_t) (coord.x & SC_MSK)) << SC_SHIFT[0]) |
    //           (((uint64_t) (coord.y & SC_MSK)) << SC_SHIFT[1]) |
    //           (((uint64_t) (coord.z & SC_MSK)) << SC_SHIFT[2]);
    //}
    static inline void retrieve_prefix(uint64_t prefixed_id, CoordOfCube &coord, int &seglevel) {
        coord.x = (prefixed_id >> SC_SHIFT[0]) & SC_MSK;
        coord.y = (prefixed_id >> SC_SHIFT[1]) & SC_MSK;
        coord.z = (prefixed_id >> SC_SHIFT[2]) & SC_MSK;
        seglevel = (prefixed_id >> SCL_SHIFT) & SCL_MSK;
    }

};

#endif//DATASET_H
