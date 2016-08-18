# 3Depict_Isosurfaces

The idea is to implement the Dreamworks OpenVDB library into the 3Depict source.
The module is called via an openvdb_includes.h header that provides all openvdb
files needed.

Dependencies: libopenvdb3.1 via apt and libopenvdb-dev via apt

The core steps for this should be:

1. Create own filter which is NOT compatible with the standard voxelization
2. In the filter the point cloud is written into an OpenVDB Grid
2. Write a stream comparable to the voxel stream which transports an OpenVDB grid object 
   that is modified in the filter mentioned above.
3. Write own Draw Isosurface Class in Drawables (gets called by viscontrol.ccp)
   -> here the volume to mesh conversion is done.
4. In the last step a centroidal voronoi tesselation runs over the mesh 
   the version from here is to be used http://research.microsoft.com/en-us/um/people/yangliu/ LPCVT
