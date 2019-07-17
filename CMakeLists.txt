cmake_minimum_required(VERSION 3.10)
project(fme_CityJSON)

set(CMAKE_CXX_STANDARD 11)

add_library(fmecityjson SHARED
        ${CMAKE_SOURCE_DIR}/fmecityjson/fmecityjsonentrypoints.cpp
        ${CMAKE_SOURCE_DIR}/fmecityjson/fmecityjsongeometryvisitor.cpp
        ${CMAKE_SOURCE_DIR}/fmecityjson/fmecityjsongeometryvisitor.h
        ${CMAKE_SOURCE_DIR}/fmecityjson/fmecityjsonpriv.h
        ${CMAKE_SOURCE_DIR}/fmecityjson/fmecityjsonreader.cpp
        ${CMAKE_SOURCE_DIR}/fmecityjson/fmecityjsonreader.h
        ${CMAKE_SOURCE_DIR}/fmecityjson/fmecityjsonwriter.cpp
        ${CMAKE_SOURCE_DIR}/fmecityjson/fmecityjsonwriter.h)

# Set an environment variable FME_DEV_HOME to be the path to the directory where FME is installed
include_directories(${CMAKE_SOURCE_DIR}/fmecityjson
        ${CMAKE_SOURCE_DIR}/includes
        ${FME_DEV_HOME}/pluginbuilder/cpp
        ${FME_DEV_HOME}/fmeobjects/cpp)

set_target_properties(fmecityjson PROPERTIES PREFIX "")

install(TARGETS fmecityjson DESTINATION ${FME_DEV_HOME}/plugins
        PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE)
install(FILES fmecityjson.db DESTINATION ${FME_DEV_HOME}/formatsinfo)
install(FILES fmecityjson.fmf DESTINATION ${FME_DEV_HOME}/metafile)
