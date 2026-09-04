set(PROJECT_NAME Test)
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SRC 
${CMAKE_CURRENT_SOURCE_DIR}/*.h 
${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(${PROJECT_NAME} ${SRC})


include_directories(${OpenSceneGraph_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/osgDB.lib
)
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "ThirdPart/OSGDB")

