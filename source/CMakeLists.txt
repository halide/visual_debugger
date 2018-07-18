add_executable(
	visdbg
	./main.cpp
	./imconfig.h
	./utils.h
	../data/force_working_directory.cpp
)



# locate and configure third-party dependencies:
include(../third-party/CMakeLists.txt)

target_include_directories(
	visdbg PUBLIC
	${GLFW_INCLUDE_DIR}
	${IMGUI_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR}
	${HALIDE_INCLUDE_DIR}
	${LLVM_INCLUDE_DIRS}
)

target_sources(
	visdbg PUBLIC
	${GLFW_SOURCE_AMALGAMATION}
	${IMGUI_SOURCE_AMALGAMATION}
)

llvm_map_components_to_libnames(llvm_libs support core irreader)

target_link_libraries(
	visdbg
	${GLFW_DEPENDENCIES}
	${OPENGL_LIBRARIES}
	${HALIDE_LIBRARY}
	${llvm_libs}
)

target_compile_definitions(
	visdbg PUBLIC
	${LLVM_DEFINITIONS}
)



# dependency: X11
if (LINUX)
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
