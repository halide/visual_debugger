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



cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 11)

# Solution / Workspace:
set (CMAKING_WHOLE_SOLUTION true)
project(halide-visdbg)

# locate and configure third-party dependencies:
include(../../third-party/CMakeLists.txt)



# have the amalgamated demo also in the solution
add_subdirectory(
	# source directory
	../
	# binary/build directory
	./amalgd
)



add_executable(
	demo

	../main.cpp

	../data/force_working_directory.cpp

	../../src/imgui_main.cpp
	../../src/system.cpp
	../../src/treedump.cpp
	../../src/debug-api.cpp
	../../src/halide-image-io.cpp

	../../src/imconfig.h
	../../src/io-broadcast.hpp
	../../src/system.hpp
	../../src/utils.h
	../../src/halide-image-io.h

	../../include/debug-api.h
)



# configure third-party dependencies:

target_include_directories(
	demo PUBLIC
	${GLFW_INCLUDE_DIR}
	${IMGUI_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR}
	${HALIDE_INCLUDE_DIR}
	${LLVM_INCLUDE_DIRS}
)

target_sources(
	demo PUBLIC
	${GLFW_SOURCE_AMALGAMATION}
	${IMGUI_SOURCE_AMALGAMATION}
	${IMGUI_FILESYSTEN_SOURCE_AMALGAMATION}
)

llvm_map_components_to_libnames(llvm_libs support core irreader)

target_link_libraries(
	demo
	${GLFW_DEPENDENCIES}
	${OPENGL_LIBRARIES}
	${HALIDE_LIBRARY}
	${llvm_libs}
)

target_compile_definitions(
	demo PUBLIC
	${LLVM_DEFINITIONS}
)



# dependency: X11
if (LINUX)
	target_include_directories(
		demo PUBLIC
		${X11_INCLUDE_DIR}
	)
    target_link_libraries(
		demo
		${X11_LIBRARIES}
	)
endif()

# dependency: pthread
if (LINUX)
    target_link_libraries(
		demo
		pthread
		dl
	)
endif()


if (NOT MSVC)
	target_compile_options(demo PUBLIC -fno-rtti)
endif()
