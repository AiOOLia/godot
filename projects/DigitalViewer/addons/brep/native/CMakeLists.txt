cmake_minimum_required(VERSION 3.20)
project(DigitalViewerBRep LANGUAGES CXX)
if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The supplied BREP SDK requires MSVC x64")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set(BREP_SDK_PATH "" CACHE PATH "Existing NDS SDK repository (libraries are never rebuilt)")
set(GODOT_CPP_PATH "" CACHE PATH "Godot 4.1 single precision godot-cpp with prebuilt /MD release library")
set(BREP_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../bin" CACHE PATH "Extension output")
foreach(input "${BREP_SDK_PATH}/lib/BRepCore.lib" "${BREP_SDK_PATH}/lib/BRepToTopoDS.lib" "${GODOT_CPP_PATH}/lib/godot-cpp.windows.release.64.lib")
    if(NOT EXISTS "${input}")
        message(FATAL_ERROR "Missing precompiled dependency: ${input}")
    endif()
endforeach()
add_library(DigitalViewerBRep SHARED brep_document.cpp)
target_compile_definitions(DigitalViewerBRep PRIVATE TYPED_METHOD_BIND NOMINMAX WIN32_LEAN_AND_MEAN _ITERATOR_DEBUG_LEVEL=0)
target_compile_options(DigitalViewerBRep PRIVATE /MD /EHsc /utf-8 /W3)
target_include_directories(DigitalViewerBRep PRIVATE sdk/include
    "${BREP_SDK_PATH}/ThirdPart/OCCT/windows/include/opencascade"
    "${GODOT_CPP_PATH}/include" "${GODOT_CPP_PATH}/gen/include" "${GODOT_CPP_PATH}/gdextension")
target_link_libraries(DigitalViewerBRep PRIVATE
    "${GODOT_CPP_PATH}/lib/godot-cpp.windows.release.64.lib"
    "${BREP_SDK_PATH}/lib/BRepCore.lib" "${BREP_SDK_PATH}/lib/BRepToTopoDS.lib"
    "${BREP_SDK_PATH}/ThirdPart/OCCT/windows/lib/OCCT_Lite.lib"
    "${BREP_SDK_PATH}/ThirdPart/OCCT/windows/lib/OCCT_Tools.lib")
set_target_properties(DigitalViewerBRep PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BREP_OUTPUT_PATH}")
add_executable(BRepFixture EXCLUDE_FROM_ALL make_fixture.cpp)
target_include_directories(BRepFixture PRIVATE sdk/include)
target_compile_definitions(BRepFixture PRIVATE NOMINMAX _ITERATOR_DEBUG_LEVEL=0)
target_compile_options(BRepFixture PRIVATE /MD /EHsc /utf-8)
target_link_libraries(BRepFixture PRIVATE "${BREP_SDK_PATH}/lib/BRepCore.lib")
set_target_properties(BRepFixture PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/fixture")
