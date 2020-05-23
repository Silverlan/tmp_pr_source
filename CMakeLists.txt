cmake_minimum_required(VERSION 3.12)

include("../CMakeShared.txt")

set(PROJ_NAME pr_source)

project(${PROJ_NAME} CXX)

# set(PRECOMPILED_HEADER "stdafx")

link_external_library(util_archive)
link_external_library(fbxsdk)
link_external_library(util_smdmodel)
link_external_library(niftools)
link_external_library(util_dmx)
link_external_library(util_source2)
link_external_library(util_bsp)
link_external_library(util_vmf)
link_external_library(util_fgd)
link_external_library(util_image)
link_external_library(RectangleBinPack)
if(CONFIG_ENABLE_VTF_SUPPORT)
	link_external_library(VTFLib)
endif()

add_external_library(fbxsdk)
add_external_library(lua)
add_external_library(lz4) # Required for source2 library

add_include_dir(niftools)
add_include_dir(glm)
add_include_dir(bullet)
add_include_dir(zlib)

if(CONFIG_ENABLE_VTF_SUPPORT)
	list(APPEND DEFINITIONS ENABLE_VMT_SUPPORT)
endif()

add_def(SMDMDL_STATIC)

link_generic_module_libraries()
register_pragma_module(${PROJ_NAME})

set_target_properties(pr_source PROPERTIES OUTPUT_NAME "pr_mount_external")

add_subdirectory(external_libs/util_archive)

set(PROJECT_STATIC_RUNTIME OFF CACHE BOOL OFF FORCE)
set(Boost_INCLUDE_DIRS ${DEPENDENCY_BOOST_INCLUDE} CACHE PATH "Path to include directory." FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL OFF FORCE)
add_subdirectory(third_party/libbsa)
set(BUILD_SHARED_LIBS ON CACHE BOOL ".." FORCE)
set_target_properties(bsa PROPERTIES FOLDER third_party/libbsa)
set_target_properties(tests PROPERTIES FOLDER third_party/libbsa)
# set_target_properties(zlib PROPERTIES FOLDER third_party/libbsa)
target_include_directories(bsa PUBLIC ${ZLIB_INCLUDE_DIR})
target_include_directories(bsa PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/third_party/zlib)

if(WIN32)
	include_external_msproject(ba2lib ${CMAKE_CURRENT_LIST_DIR}/third_party/ba2lib/BA2Lib/BA2Lib.vcxproj)
	set_target_properties(ba2lib PROPERTIES FOLDER third_party/libbsa)
endif()

add_dependencies(pr_source util_archive bsa ba2lib)
