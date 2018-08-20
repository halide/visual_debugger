# dependency: glfw
# -- GLFW_INCLUDE_DIR
# -- GLFW_SOURCE_AMALGAMATION
# -- GLFW_DEPENDENCIES
set(GLFW_INCLUDE_DIR
	${CMAKE_CURRENT_LIST_DIR}/glfw/include)
	
if (APPLE)
	set(GLFW_SOURCE_AMALGAMATION
		${CMAKE_CURRENT_LIST_DIR}/amalgs/glfw/glfw-amalg.m)
else()
	set(GLFW_SOURCE_AMALGAMATION
		${CMAKE_CURRENT_LIST_DIR}/amalgs/glfw/glfw-amalg.c)
endif()

set(GLFW_DEPENDENCIES)
if (APPLE)
	find_library(COCOA_LIBRARY Cocoa)
	find_library(IOKIT_LIBRARY IOKit)
	find_library(CORE_VIDEO_LIBRARY CoreVideo)
	set(GLFW_DEPENDENCIES
		${COCOA_LIBRARY}
		${IOKIT_LIBRARY}
		${CORE_VIDEO_LIBRARY}
	)
endif()


# dependency: imgui
set(IMGUI_INCLUDE_DIR
	${CMAKE_CURRENT_LIST_DIR}/imgui
	${CMAKE_CURRENT_LIST_DIR}/imgui/examples)
set(IMGUI_SOURCE_AMALGAMATION
	${CMAKE_CURRENT_LIST_DIR}/imgui/imgui.cpp
	${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_demo.cpp
	${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_draw.cpp
	${CMAKE_CURRENT_LIST_DIR}/imgui/examples/imgui_impl_glfw.cpp
	${CMAKE_CURRENT_LIST_DIR}/imgui/examples/imgui_impl_opengl2.cpp
)



# dependency: LLVM
if (APPLE)
	find_package(
		LLVM REQUIRED CONFIG
		HINTS "${CMAKE_CURRENT_LIST_DIR}/../build/llvm/make/MacOS/Release"
	)
elseif(LINUX)
	find_package(
		LLVM REQUIRED CONFIG
		HINTS "${CMAKE_CURRENT_LIST_DIR}/../build/llvm/make/Linux/Release"
	)
elseif(WIN32)
	if (MSVC)
		find_package(
			LLVM REQUIRED CONFIG
			HINTS "${CMAKE_CURRENT_LIST_DIR}/../build/llvm/msvc/Win64/Release"
		)
	else()
		find_package(
			LLVM REQUIRED CONFIG
			HINTS "${CMAKE_CURRENT_LIST_DIR}/../build/llvm/mingw-make/Win64/Release"
		)
	endif()
endif()



# dependency: Halide
# -- HALIDE_INCLUDE_DIR
# -- HALIDE_LIBRARY
if (APPLE)
	set(HALIDE_INCLUDE_DIR
		${CMAKE_CURRENT_LIST_DIR}/../build/halide/make/MacOS/Release/include)
	find_library(
		HALIDE_LIBRARY 
		NAMES Halide libHalide
        HINTS "${CMAKE_CURRENT_LIST_DIR}/../build/halide/make/MacOS/Release/lib"
	)
elseif(LINUX)
	set(HALIDE_INCLUDE_DIR
		${CMAKE_CURRENT_LIST_DIR}/../build/halide/make/Linux/Release/include)
	find_library(
		HALIDE_LIBRARY 
		NAMES Halide libHalide
        HINTS "${CMAKE_CURRENT_LIST_DIR}/../build/halide/make/Linux/Release/lib"
	)
elseif(WIN32)
	if (MSVC)
		set(HALIDE_INCLUDE_DIR
			${CMAKE_CURRENT_LIST_DIR}/../build/halide/msvc/Win64/Release/include)
		find_library(
			HALIDE_LIBRARY 
			NAMES Halide libHalide
			HINTS "${CMAKE_CURRENT_LIST_DIR}/../build/halide/msvc/Win64/Release/lib/Release"
		)
	else()
		set(HALIDE_INCLUDE_DIR
			${CMAKE_CURRENT_LIST_DIR}/../build/halide/mingw-make/Win64/Release/include)
		find_library(
			HALIDE_LIBRARY 
			NAMES Halide libHalide
			HINTS "${CMAKE_CURRENT_LIST_DIR}/../build/halide/mingw-make/Win64/Release/lib"
		)
	endif()
endif()



# dependency: OpenGL
find_package(OpenGL REQUIRED)



# dependency: X11
if (LINUX)
	find_package(X11 REQUIRED)
endif()

