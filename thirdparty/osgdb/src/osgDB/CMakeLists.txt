SET(LIB_NAME osgDB)

if(MSVC)
  add_compile_options(/utf-8)
endif()

SET(HEADER_PATH ${OpenSceneGraph_SOURCE_DIR}/include/${LIB_NAME})

file(GLOB TARGET_H ${HEADER_PATH}/*.h 
     readerwriters/osg/*.h
     readerwriters/jpeg/*.h
)
file(GLOB TARGET_SRC *.cpp 
     serializers/osg/*.cpp
     readerwriters/osg/*.cpp
     readerwriters/jpeg/*.cpp
     ${OPENSCENEGRAPH_VERSIONINFO_RC}
)

set(SRC ${TARGET_H} ${TARGET_SRC})

add_library(${LIB_NAME} SHARED ${SRC})
target_include_directories(${LIB_NAME} PUBLIC ${OpenSceneGraph_SOURCE_DIR}/include)

if(${CMAKE_SYSTEM_NAME} STREQUAL Windows)
    target_link_libraries(${LIB_NAME}
            PRIVATE
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/osg.lib
    )
elseif(${CMAKE_SYSTEM_NAME} STREQUAL Linux)
    target_link_libraries(${LIB_NAME}
            PRIVATE
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/osg.a
    )
elseif(${CMAKE_SYSTEM_NAME} STREQUAL Android)
    target_link_libraries(${LIB_NAME}
            PRIVATE
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libosg.a
    )
elseif(${CMAKE_SYSTEM_NAME} STREQUAL Emscripten)

else()

endif()

 target_compile_definitions(${LIB_NAME} PRIVATE "-DOSGDB_LIBRARY")
 set_property(TARGET ${LIB_NAME} PROPERTY FOLDER "ThirdPart/OSGDB")

