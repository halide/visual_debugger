if (NOT CMAKING_WHOLE_SOLUTION)
	cmake_minimum_required(VERSION 3.0)
	set(CMAKE_CXX_STANDARD 11)
	project(visdbg)
	# locate and configure third-party dependencies:
	include(../third-party/CMakeLists.txt)
endif()



add_executable(
	visdbg

	../data/force_working_directory.cpp
	
	./imgui_main.cpp
	./main.cpp
	./system.cpp
	./treedump.cpp

	./HalideIdentity.h
	./HalideImageIO.h
	./imconfig.h
	./io-broadcast.hpp
	./system.hpp
	./utils.h

	../third-party/imguifilesystem/dirent_portable.h
	../third-party/imguifilesystem/imguifilesystem.h		
	../third-party/imguifilesystem/imguifilesystem.cpp
)



# configure third-party dependencies:

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
