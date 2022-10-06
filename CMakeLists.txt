cmake_minimum_required(VERSION 3.12)

include("../CMakeShared.txt")

set(PROJ_NAME pr_source)

project(${PROJ_NAME} CXX)

# set(PRECOMPILED_HEADER "stdafx")

option(CONFIG_ENABLE_VTF_SUPPORT "Enable VTF support?" ON)




set(DEPENDENCY_UTIL_ARCHIVE_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/external_libs/util_archive/include CACHE PATH "Path to include directory.")
search_lib_recursive(DEPENDENCY_UTIL_ARCHIVE_LIBRARY external_libs/util_archive/${BINARY_PRAGMA_DIR} util_archive)
search_lib_recursive(DEPENDENCY_UTIL_FGD_LIBRARY external_libs/util_fgd/${BINARY_PRAGMA_DIR} util_fgd STATIC)

link_external_library(util_archive)
#link_external_library(fbxsdk)
link_external_library(util_smdmodel)
link_external_library(niftools)
link_external_library(util_dmx)
link_external_library(util_source2)
link_external_library(util_bsp)
link_external_library(util_vmf)
link_external_library(util_fgd)
link_external_library(util_image)
link_external_library(util_mmd)
link_external_library(util_udm)
link_external_library(panima)
#link_external_library(RectangleBinPack)
link_external_library(liblzma)
link_external_library(util_zip)
link_external_library(libzip)
link_external_library(7zpp)
if(CONFIG_ENABLE_VTF_SUPPORT)
	link_external_library(VTFLib)
endif()

#add_external_library(fbxsdk)
add_external_library(lua)
add_external_library(lz4) # Required for source2 library

add_include_dir(glm)
add_include_dir(bullet)
add_include_dir(zlib)
add_include_dir(zlib_build)
add_include_dir(vtflib_build)

if(CONFIG_ENABLE_VTF_SUPPORT)
	list(APPEND DEFINITIONS ENABLE_VMT_SUPPORT)
endif()

add_def(SMDMDL_STATIC)
add_def(UIMG_ENABLE_NVTT)

link_generic_module_libraries()
register_pragma_module(${PROJ_NAME})

set_target_properties(pr_source PROPERTIES OUTPUT_NAME "pr_mount_external")

add_subdirectory(external_libs/util_archive)
add_subdirectory(external_libs/util_fgd)

set(PROJECT_STATIC_RUNTIME OFF CACHE BOOL OFF FORCE)
#set(Boost_INCLUDE_DIRS ${DEPENDENCY_BOOST_INCLUDE} CACHE PATH "Path to include directory." FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL OFF FORCE)
add_subdirectory(third_party/libbsa)
set(BUILD_SHARED_LIBS ON CACHE BOOL ".." FORCE)
set_target_properties(bsa PROPERTIES FOLDER third_party/libbsa)
set_target_properties(tests PROPERTIES FOLDER third_party/libbsa)
# set_target_properties(zlib PROPERTIES FOLDER third_party/libbsa)
target_include_directories(bsa PUBLIC ${ZLIB_INCLUDE_DIR})
target_include_directories(bsa PUBLIC ${DEPENDENCY_ZLIB_BUILD_INCLUDE})
target_include_directories(bsa PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/third_party/zlib)

add_subdirectory(third_party/qhull)
set_target_properties(libqhull PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(qconvex PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(qdelaunay PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(qhalf PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(qhull PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(qhull_p PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(qhull_r PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(qhullcpp PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(qhullp PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(qhullstatic PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(qhullstatic_r PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(qvoronoi PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(rbox PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(testqset PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(testqset_r PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(user_eg PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(user_eg2 PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(user_eg3 PROPERTIES FOLDER modules/mount_external/qhull)
set_target_properties(user_egp PROPERTIES FOLDER modules/mount_external/qhull)

add_subdirectory(third_party/niflib)
set_target_properties(bslightingshaderproperty_test PROPERTIES FOLDER modules/mount_external/niflib)
set_target_properties(missing_link_stack_test PROPERTIES FOLDER modules/mount_external/niflib)
set_target_properties(niflib PROPERTIES FOLDER modules/mount_external/niflib)
set_target_properties(niflib_static PROPERTIES FOLDER modules/mount_external/niflib)
set_target_properties(ninode_test PROPERTIES FOLDER modules/mount_external/niflib)
set_target_properties(numuvsets_test PROPERTIES FOLDER modules/mount_external/niflib)
set_target_properties(skin_test PROPERTIES FOLDER modules/mount_external/niflib)
set_target_properties(skinpart_test PROPERTIES FOLDER modules/mount_external/niflib)
set_target_properties(trishape_test PROPERTIES FOLDER modules/mount_external/niflib)
set_target_properties(write_test PROPERTIES FOLDER modules/mount_external/niflib)
target_include_directories(niflib_static PRIVATE ${CMAKE_CURRENT_LIST_DIR}/third_party/niflib)

set_target_properties(util_fgd PROPERTIES FOLDER modules/mount_external)
set_target_properties(util_archive PROPERTIES FOLDER modules/mount_external)
set_target_properties(pr_source PROPERTIES FOLDER modules/mount_external)
# set_target_properties(util_vmf PROPERTIES FOLDER modules/mount_external/util_vmf)

# set(DEPENDENCY_UTIL_FGD_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/external_libs/util_fgd/include CACHE PATH "Path to include directory.")
# search_lib_recursive_ext(DEPENDENCY_UTIL_FGD_LIBRARY util_fgd STATIC)

# if(WIN32)
# 	include_external_msproject(ba2lib ${CMAKE_CURRENT_LIST_DIR}/third_party/ba2lib/BA2Lib/BA2Lib.vcxproj)
# 	set_target_properties(ba2lib PROPERTIES FOLDER third_party/libbsa)
# endif()

# add_dependencies(pr_source util_archive bsa ba2lib)
add_dependencies(pr_source util_vmf util_fgd util_archive)

set_property(GLOBAL PROPERTY PRAGMA_MODULE_SKIP_TARGET_PROPERTY_FOLDER 1)
