# 3Depict_Isosurfaces

The idea is to implement the Dreamworks OpenVDB library into the 3Depict source.

The core steps for this should be:

1. Create own filter which is NOT compatible with the standard voxelization
2. In the filter the point cloud is written into an OpenVDB Grid
2. Write a stream comparable to the voxel stream which transports an OpenVDB grid object 
   that is modified in the filter mentioned above.
3. Write own Draw Isosurface Class in Drawables (gets called by viscontrol.ccp)
   -> here the volume to mesh conversion is done.
