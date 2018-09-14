cmake_minimum_required(VERSION 3.0)

# Commands to create platform-specific build files for the project assuming the
# command is run from within a _build subdirectory:
#
# Xcode:
#     cmake .. -G Xcode
#
# Visual Studio 2017:
#     cmake .. -G "Visual Studio 15 2017 Win64"
#  or cmake .. -G "Visual Studio 15 2017"       -- for 32bit builds
#
# Makefile:
#     cmake .. -G "Unix Makefiles"
#

####################################[ setup ]###################################

#-----------------------------[ utility functions ]----------------------------#
# A utility function to append all the C++ header and source files within a
# directory to a list.  The first two parameters passed to this function are the
# list to append to and the path of the directory respectively.  
function(add_source_directory SOURCE_FILES DIRECTORY_NAME)
	if(${ARGC} LESS 2)
		message(FATAL_ERROR "add_source_directory must be provided at least two arguments (the list of source files to append to, the source directory)")
	endif()
	
	file(GLOB ADD_SOURCE_DIR_H   "${DIRECTORY_NAME}/*.h")
	file(GLOB ADD_SOURCE_DIR_HPP "${DIRECTORY_NAME}/*.hpp")
	file(GLOB ADD_SOURCE_DIR_C   "${DIRECTORY_NAME}/*.c")
	file(GLOB ADD_SOURCE_DIR_CPP "${DIRECTORY_NAME}/*.cpp")
	set(ADD_SOURCE_DIR_SRC
		${ADD_SOURCE_DIR_H}
		${ADD_SOURCE_DIR_HPP}
		${ADD_SOURCE_DIR_C}
		${ADD_SOURCE_DIR_CPP})
		
	if(${ARGC} GREATER 2)
		string(REPLACE ";" "\\" SOURCE_GROUP_NAME "${ARGN}")
		source_group(${SOURCE_GROUP_NAME} FILES ${ADD_SOURCE_DIR_SRC})
	endif()
	
	set(${SOURCE_FILES} ${${SOURCE_FILES}} ${ADD_SOURCE_DIR_SRC} PARENT_SCOPE)
endfunction()
#
#----------------------------[ user configuration ]----------------------------#
#
# include the fole for user-specific configurations, if it exists
# include("CMakeLocalConfig.txt" OPTIONAL)
set(CMAKE_CXX_STANDARD 11)
#
#----------------------------[ user configuration ]----------------------------#

##########################[ libraries and executables ]#########################

# Solution / Workspace:
set (CMAKING_WHOLE_SOLUTION true)
project(halide-visdbg)
# locate and configure third-party dependencies:
include(third-party/CMakeLists.txt)

# Projects:
add_subdirectory(./demo)
add_subdirectory(./demo/amalg)
