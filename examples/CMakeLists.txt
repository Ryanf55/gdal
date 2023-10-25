cmake_minimum_required(VERSION 3.9..3.27)
project(examples)

find_package(GDAL CONFIG REQUIRED)

add_executable(raster_api_tutorial raster_api_tutorial.cpp)
target_link_libraries(raster_api_tutorial PRIVATE GDAL::GDAL)
