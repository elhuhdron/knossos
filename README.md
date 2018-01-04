# KNOSSOS

## Background
KNOSSOS is a software tool for the visualization and annotation of 3D image data and was developed for the rapid reconstruction of neural morphology and connectivity.

## Technical
By dynamically loading only data from the surround of the current view point, seamless navigation is not limited to datasets that fit into the available RAM but works with much larger dataset stored in a special format on disk. Currently, KNOSSOS is limited to 8-bit data. In addition to viewing and navigating, KNOSSOS allows efficient manual neurite annotation ('skeletonization').

## Use
KNOSSOS is being used mostly for reconstructing cell morphologies from 3D electron microscopic data generated by Serial Block-Face Electron Microscopy (SBEM), with an occasional application to 2-photon and confocal optical microscopy data.

# Modifications from Knossos Base

## Features for Large-Scale Manual Agglomeration
This fork supports real time mesh display during supervoxel merging, only displaying supervoxels as they are clicked and highlighting membrane areas in red. The old mode is still available as an option in knossos.conf. Additionally supports "superchunked" labels, meaning that labels are only unique within certain sized volumes and are made unique across an entire EM volume by automatically tagging each label with its xyz superchunk location. Meshes are stored as hdf5 files, one per superchunk. Additionally multiple segmentation levels can be loaded as a method of dealing with a low percentage of remaining mergers at an "optimal" segmentation level. Finally different raw data can be loaded (for example as a method of displaying the supervoxels overlaid with the probability output from voxel classification, i.e. convnets).

## Packaging
This fork is built without PythonQt support, removing a difficult-to-port dependency. Builds are not static, but Releases provided with dynamic libraries required to run.
