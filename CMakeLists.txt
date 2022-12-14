cmake_minimum_required(VERSION 3.12)

include("./CMakeShared.txt")

function(search_lib_recursive_full VAR_NAME FILE_PATH)
	if(WIN32)
		set(FILE_PATH_WITH_EXT "${FILE_PATH}.lib")
	else()
		set(FILE_EXTENSION ".so")
		set(extra_macro_args ${ARGN})

		list(LENGTH extra_macro_args num_extra_args)
		if(${num_extra_args} GREATER 0)
			list(GET extra_macro_args 0 optional_arg)
			if(${optional_arg} STREQUAL "STATIC")
				set(FILE_EXTENSION ".a")
			endif()
		endif()

		set(FILE_PATH_WITH_EXT "${FILE_PATH}${FILE_EXTENSION}")
	endif()
	if(EXISTS ${FILE_PATH_WITH_EXT})
		get_filename_component(FILE_PATH_WITH_EXT "${FILE_PATH_WITH_EXT}" REALPATH)
	endif()

	if(UNIX)
		# Find actual library file name
		get_filename_component(FILE_PATH_DIRECTORY "${FILE_PATH_WITH_EXT}" DIRECTORY)
		get_filename_component(FILE_PATH_NAME "${FILE_PATH_WITH_EXT}" NAME)
		execute_process(
			COMMAND find ./ -name "${FILE_PATH_NAME}*"
			WORKING_DIRECTORY "${FILE_PATH_DIRECTORY}"
			OUTPUT_VARIABLE FOUND_RESULT
		)
		if(NOT ${FOUND_RESULT} STREQUAL "")
			get_filename_component(FOUND_RESULT "${FOUND_RESULT}" NAME)
			string(STRIP ${FOUND_RESULT} FOUND_RESULT)
                        #message("FIND: ${FILE_PATH_DIRECTORY}/${FILE_PATH_NAME}* -> ${FOUND_RESULT}")
			set(FILE_PATH_WITH_EXT "${FILE_PATH_DIRECTORY}/${FOUND_RESULT}")
		endif()
		#
	endif()

	set(${VAR_NAME} ${FILE_PATH_WITH_EXT} CACHE PATH "Path to library." FORCE)
endfunction(search_lib_recursive_full)

function(search_lib_recursive VAR_NAME FILE_PATH LIB_NAME)
	set(ARG_STATIC_LIBRARY FALSE)
	set(ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR})
	set(extra_macro_args ${ARGN})

	list(LENGTH extra_macro_args num_extra_args)
	if(${num_extra_args} GREATER 0)
		list(GET extra_macro_args 0 optional_arg)
		if(${optional_arg} STREQUAL "STATIC")
			set(ARG_STATIC_LIBRARY "STATIC")
		endif()
		if(${num_extra_args} GREATER 1)
			list(GET extra_macro_args 1 optional_arg)
			set(ROOT_DIR ${optional_arg})
		endif()
	endif()

	if(UNIX)
		set(LIB_NAME lib${LIB_NAME})
	endif()
	search_lib_recursive_full(${VAR_NAME} ${ROOT_DIR}/${FILE_PATH}/${LIB_NAME} ${ARG_STATIC_LIBRARY})
endfunction(search_lib_recursive)

set(PROJ_NAME pr_source)

project(${PROJ_NAME} CXX)

# set(PRECOMPILED_HEADER "stdafx")

option(CONFIG_ENABLE_VTF_SUPPORT "Enable VTF support?" ON)




set(DEPENDENCY_UTIL_ARCHIVE_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/external_libs/util_archive/include CACHE PATH "Path to include directory.")
set(DEPENDENCY_UTIL_FGD_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/external_libs/util_fgd/include CACHE PATH "Path to include directory.")
set(DEPENDENCY_UTIL_DMX_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/external_libs/util_dmx/include CACHE PATH "Path to include directory.")
set(DEPENDENCY_UTIL_MMD_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/external_libs/util_mmd/include CACHE PATH "Path to include directory.")
set(DEPENDENCY_QHULL_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/third_party/qhull/src/libqhull CACHE PATH "Path to include directory.")
search_lib_recursive(DEPENDENCY_UTIL_ARCHIVE_LIBRARY external_libs/util_archive/${BINARY_PRAGMA_DIR} util_archive)
search_lib_recursive(DEPENDENCY_UTIL_FGD_LIBRARY external_libs/util_fgd/${BINARY_PRAGMA_DIR} util_fgd STATIC)
search_lib_recursive(DEPENDENCY_UTIL_MMD_LIBRARY external_libs/util_mmd/${BINARY_PRAGMA_DIR} util_mmd STATIC)
search_lib_recursive(DEPENDENCY_UTIL_DMX_LIBRARY external_libs/util_dmx/${BINARY_PRAGMA_DIR} util_dmx STATIC)

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
link_external_library(RectangleBinPack)
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
add_include_dir(qhull)
add_include_dir(zlib_build)
add_include_dir(vtflib_build)

if(CONFIG_ENABLE_VTF_SUPPORT)
	list(APPEND DEFINITIONS ENABLE_VMT_SUPPORT)
endif()

add_def(SMDMDL_STATIC)
add_def(UIMG_ENABLE_NVTT)

link_external_library(sharedutils)
link_external_library(mathutil)
link_external_library(vfilesystem)
link_external_library(shared)
link_external_library(luasystem)
link_external_library(LuaJIT)
link_external_library(luabind)
link_external_library(materialsystem)
link_external_library(datasystem)
link_external_library(ishared)
link_external_library(util_udm)

register_pragma_module(${PROJ_NAME})

set_target_properties(pr_source PROPERTIES OUTPUT_NAME "pr_mount_external")

add_subdirectory(external_libs/util_archive)
add_subdirectory(external_libs/util_fgd)
add_subdirectory(external_libs/util_dmx)
add_subdirectory(external_libs/util_mmd)

set(PROJECT_STATIC_RUNTIME OFF CACHE BOOL OFF FORCE)
#set(Boost_INCLUDE_DIRS ${DEPENDENCY_BOOST_INCLUDE} CACHE PATH "Path to include directory." FORCE)

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

set_target_properties(util_fgd PROPERTIES FOLDER modules/mount_external)
set_target_properties(util_archive PROPERTIES FOLDER modules/mount_external)
set_target_properties(pr_source PROPERTIES FOLDER modules/mount_external)
# set_target_properties(util_vmf PROPERTIES FOLDER modules/mount_external/util_vmf)

# set(DEPENDENCY_UTIL_FGD_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/external_libs/util_fgd/include CACHE PATH "Path to include directory.")
# search_lib_recursive_ext(DEPENDENCY_UTIL_FGD_LIBRARY util_fgd STATIC)

add_dependencies(pr_source util_archive util_fgd lzma util_dmx util_mmd)

set_property(GLOBAL PROPERTY PRAGMA_MODULE_SKIP_TARGET_PROPERTY_FOLDER 1)