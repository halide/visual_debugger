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

project(VisDebugger)

####################################[ setup ]###################################

#-----------------------------[ utility functions ]----------------------------#
#
if(UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()
#
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


#add_source_directory(SIMPLE_GUI_SOURCE_FILES "./")
add_executable(
	${PROJECT_NAME}
	./main.cpp
)

# dependency: glfw
target_include_directories(
	${PROJECT_NAME} PUBLIC
	../third-party/glfw/include
)
if (APPLE)
	#INCLUDE_DIRECTORIES("/usr/local/include")
	#LINK_DIRECTORIES("/usr/local/lib")
	target_sources(
		${PROJECT_NAME} PUBLIC
		../third-party/amalgs/glfw/glfw-amalg.m
	)
	find_library(COCOA_LIBRARY Cocoa)
	find_library(IOKIT_LIBRARY IOKit)
	find_library(CORE_VIDEO_LIBRARY CoreVideo)
	target_link_libraries(
		${PROJECT_NAME}
		${COCOA_LIBRARY}
		${IOKIT_LIBRARY}
		${CORE_VIDEO_LIBRARY}
	)
else()
	target_sources(
		${PROJECT_NAME} PUBLIC
		../third-party/amalgs/glfw/glfw-amalg.c
	)
endif()

# dependency: imgui
target_include_directories(
	${PROJECT_NAME} PUBLIC
	../third-party/imgui
	../third-party/imgui/examples
)
target_sources(
	${PROJECT_NAME} PUBLIC
	../third-party/imgui/imgui.cpp
	../third-party/imgui/imgui_demo.cpp
	../third-party/imgui/imgui_draw.cpp
	../third-party/imgui/examples/imgui_impl_glfw.cpp
	../third-party/imgui/examples/imgui_impl_opengl2.cpp
)

# dependency: LLVM
find_package(LLVM REQUIRED CONFIG
	HINTS "../build/llvm/make/MacOS/Release")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs support core irreader)

target_link_libraries(${PROJECT_NAME} ${llvm_libs})

# dependency: Halide
find_library(HALIDE_LIBRARY 
	NAMES Halide libHalide
        HINTS "../build/halide/make/MacOS/Release/lib")

target_include_directories(
	${PROJECT_NAME} PUBLIC
	../build/halide/make/MacOS/Release/include
)

target_link_libraries(
	${PROJECT_NAME}
	${HALIDE_LIBRARY}
)

# dependency: OpenGL
find_package(OpenGL REQUIRED)

target_include_directories(
	${PROJECT_NAME} PUBLIC
	${OPENGL_INCLUDE_DIR}
)

target_link_libraries(
	${PROJECT_NAME}
	${OPENGL_LIBRARIES}
)

# dependency: X11
if (LINUX)
	find_package(X11 REQUIRED)
	target_include_directories(
		${PROJECT_NAME} PUBLIC
		${X11_INCLUDE_DIR}
	)
    target_link_libraries(
		${PROJECT_NAME}
		${X11_LIBRARIES}
	)
endif()

# dependency: pthread
if (LINUX)
    target_link_libraries(
		${PROJECT_NAME}
		pthread
		dl
	)
endif()

target_compile_options(${PROJECT_NAME} PUBLIC -fno-rtti)
