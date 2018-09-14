if (NOT CMAKING_WHOLE_SOLUTION)
	cmake_minimum_required(VERSION 3.0)
	set(CMAKE_CXX_STANDARD 11)
	project(visdbg-demo-amalg)
	# locate and configure third-party dependencies:
	include(../third-party/CMakeLists.txt)
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
