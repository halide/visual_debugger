add_executable(
	visdbg
	./main.cpp
	./imconfig.h
	./utils.h
)

# dependency: glfw
target_include_directories(
	visdbg PUBLIC
	../third-party/glfw/include
)
if (APPLE)
	target_sources(
		visdbg PUBLIC
		../third-party/amalgs/glfw/glfw-amalg.m
	)
	find_library(COCOA_LIBRARY Cocoa)
	find_library(IOKIT_LIBRARY IOKit)
	find_library(CORE_VIDEO_LIBRARY CoreVideo)
	target_link_libraries(
		visdbg
		${COCOA_LIBRARY}
		${IOKIT_LIBRARY}
		${CORE_VIDEO_LIBRARY}
	)
else()
	target_sources(
		visdbg PUBLIC
		../third-party/amalgs/glfw/glfw-amalg.c
	)
endif()

# dependency: imgui
target_include_directories(
	visdbg PUBLIC
	../third-party/imgui
	../third-party/imgui/examples
)
target_sources(
	visdbg PUBLIC
	../third-party/imgui/imgui.cpp
	../third-party/imgui/imgui_demo.cpp
	../third-party/imgui/imgui_draw.cpp
	../third-party/imgui/examples/imgui_impl_glfw.cpp
	../third-party/imgui/examples/imgui_impl_opengl2.cpp
)

# dependency: LLVM
if (APPLE)
	find_package(
		LLVM REQUIRED CONFIG
		HINTS "../build/llvm/make/MacOS/Release"
	)
elseif(WIN32)
	find_package(
		LLVM REQUIRED CONFIG
		HINTS "../build/llvm/msvc/Win64/Release"
	)
endif()

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs support core irreader)

target_link_libraries(visdbg ${llvm_libs})

# dependency: Halide
if (APPLE)
	find_library(
		HALIDE_LIBRARY 
		NAMES Halide libHalide
        HINTS "../build/halide/make/MacOS/Release/lib"
	)
	set( ../build/halide/make/MacOS/Release/include)
elseif(WIN32)
	find_library(
		HALIDE_LIBRARY 
		NAMES Halide libHalide
		HINTS "../build/halide/msvc/Win64/Release/lib/Release"
	)
	set(HALIDE_INCLUDE_DIR ../build/halide/msvc/Win64/Release/include)
	if (MSVC)
		set(CMAKE_MSVCIDE_RUN_PATH ../build/halide/msvc/Win64/Release/bin/Release)
	endif()
endif()

target_include_directories(
	visdbg PUBLIC
	${HALIDE_INCLUDE_DIR}
)

target_link_libraries(
	visdbg
	${HALIDE_LIBRARY}
)

# dependency: OpenGL
find_package(OpenGL REQUIRED)

target_include_directories(
	visdbg PUBLIC
	${OPENGL_INCLUDE_DIR}
)

target_link_libraries(
	visdbg
	${OPENGL_LIBRARIES}
)

# dependency: X11
if (LINUX)
	find_package(X11 REQUIRED)
	target_include_directories(
		visdbg PUBLIC
		${X11_INCLUDE_DIR}
	)
    target_link_libraries(
		visdbg
		${X11_LIBRARIES}
	)
endif()

# dependency: pthread
if (LINUX)
    target_link_libraries(
		visdbg
		pthread
		dl
	)
endif()

if (NOT MSVC)
	target_compile_options(visdbg PUBLIC -fno-rtti)
endif()
