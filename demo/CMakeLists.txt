if (NOT CMAKING_WHOLE_SOLUTION)
	cmake_minimum_required(VERSION 3.0)
	set(CMAKE_CXX_STANDARD 11)
	project(visdbg-demo-amalg)
	# locate and configure third-party dependencies:
	include(../third-party/CMakeLists.txt)
endif()



# check for dependencies

if (NOT LLVM_INCLUDE_DIRS)
	message( FATAL_ERROR "LLVM include directories have not been specified (LLVM_INCLUDE_DIRS)" )
endif()
if (NOT HALIDE_INCLUDE_DIR)
	message( FATAL_ERROR "Halide include directories have not been specified (HALIDE_INCLUDE_DIR)" )
endif()
if (NOT GLFW_INCLUDE_DIR)
	message( FATAL_ERROR "GLFW include directories have not been specified (GLFW_INCLUDE_DIR)" )
endif()
if (NOT IMGUI_INCLUDE_DIR)
	message( FATAL_ERROR "Dear ImGui include directories have not been specified (IMGUI_INCLUDE_DIR)" )
endif()
if (NOT MSVC AND NOT OPENGL_INCLUDE_DIR)
	message( FATAL_ERROR "OpenGL include directories have not been specified (OPENGL_INCLUDE_DIR)" )
endif()
if (LINUX AND NOT X11_INCLUDE_DIR)
	message( FATAL_ERROR "X11 include directories have not been specified (X11_INCLUDE_DIR)" )
endif()

if (NOT LLVM_DEFINITIONS)
	message( FATAL_ERROR "LLVM definitions have not been specified (LLVM_DEFINITIONS)" )
endif()
if (NOT HALIDE_LIBRARY)
	message( FATAL_ERROR "Halide library has not been specified (HALIDE_LIBRARY)" )
endif()
if (APPLE AND NOT GLFW_DEPENDENCIES)
	message( FATAL_ERROR "GLFW MacOS dependencies have not been specified (GLFW_DEPENDENCIES)" )
endif()
if (NOT OPENGL_LIBRARIES)
	message( FATAL_ERROR "OpenGL libraries have not been specified (OPENGL_LIBRARIES)" )
endif()
if (LINUX AND NOT X11_LIBRARIES)
	message( FATAL_ERROR "X11 libraries have not been specified (X11_LIBRARIES)" )
endif()



add_executable(
	demo-amalg

	./main.cpp

	./data/force_working_directory.cpp

	../src/amalg/amalg.cpp

	../include/debug-api.h
)



# configure third-party dependencies:

target_include_directories(
	demo-amalg PUBLIC
	${GLFW_INCLUDE_DIR}
	${IMGUI_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR}
	${HALIDE_INCLUDE_DIR}
	${LLVM_INCLUDE_DIRS}
)

if (APPLE)
	target_sources(
		demo-amalg PUBLIC
		../src/amalg/amalg.m
	)
else()
	target_sources(
		demo-amalg PUBLIC
		../src/amalg/amalg.c
	)
endif()

llvm_map_components_to_libnames(llvm_libs support core irreader)

target_link_libraries(
	demo-amalg
	${GLFW_DEPENDENCIES}
	${OPENGL_LIBRARIES}
	${HALIDE_LIBRARY}
	${llvm_libs}
)

target_compile_definitions(
	demo-amalg PUBLIC
	${LLVM_DEFINITIONS}
)



# dependency: X11
if (LINUX)
	target_include_directories(
		demo-amalg PUBLIC
		${X11_INCLUDE_DIR}
	)
    target_link_libraries(
		demo-amalg
		${X11_LIBRARIES}
	)
endif()

# dependency: pthread
if (LINUX)
    target_link_libraries(
		demo-amalg
		pthread
		dl
	)
endif()


if (NOT MSVC)
	target_compile_options(demo-amalg PUBLIC -fno-rtti)
endif()
