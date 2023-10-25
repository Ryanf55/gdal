# Examples

The following directory has examples. 

## Raster API tutorial

First, download a dataset: 
https://www.usgs.gov/faqs/where-can-i-get-global-elevation-data

For example, in earth explorer, you can download SRTM data for Boulder, CO into a file:
`n40_w106_3arc_v2.dt1`


On Linux, build GDAL:
```
# Build gdal
cd /path/to/gdal
cmake -S . -B build
cmake --build build --parallel `nproc`
cmake --install build --prefix install
```


Next, build the example which links to the installed version of gdal, and run it on the input file.
```
cd examples
cmake -S . -B build -DCMAKE_PREFIX_PATH=../install
cmake --build build
./build/raster_api_tutorial ~/Downloads/n40_w106_3arc_v2.dt1
```