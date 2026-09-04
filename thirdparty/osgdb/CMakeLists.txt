# CMakeList.txt: DigitalViewer 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
# 添加UTF-8编码支持
if(MSVC)
  add_compile_options(/utf-8)
endif()

# 开始配置项目
cmake_minimum_required(VERSION 3.10.0)

project ("OSGDB")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED true)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  #set_property(TARGET DigitalViewer PROPERTY CXX_STANDARD 20)
endif()

SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

set(OpenSceneGraph_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(3RDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdPart)

# TODO: 如有需要，请添加测试并安装目标。

add_subdirectory("src/osg")
add_subdirectory("src/osgDB")
#add_subdirectory("test")


