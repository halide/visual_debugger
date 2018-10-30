#!/bin/bash

# v ## v ### SYSTEM INTROSPECTION ### v ## v #

function get_platform_name()
{
	local platform=
	case "$OSTYPE" in
		darwin*)  platform='MacOS' ;; 
		linux*)   platform='Linux' ;;
		msys*)    platform='Windows' ;;
		cygwin*)  platform='Windows' ;;
		bsd*)     ;;
		solaris*) ;;
		*)        ;;
	esac

	echo $platform
}

function windows_host()
{
	# will return "true" if this script is running on Windows
	# (for example, under MSYS2/MinGW or CygWin)
	[[ -n "$WINDIR" ]];
}

SCRIPT_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# ^ ## ^ ### SYSTEM INTROSPECTION ### ^ ## ^ #



# v ## v ### USER CONFIGURATION ### v ## v #

# pre-requisite tools:

GIT=git
CMAKE=cmake
PYTHON=$(which python)

# Windows interlude / overrides...

WINDOWS_MINGW_BUILD=0

if windows_host; then
	# Visual Studio version selection:
	# 14 -> VS 2015 ; 15 -> VS 2017
	VISUAL_STUDIO_VERSION=15
	WINDOWS_MINGW_BUILD=0
fi

LLVM_VERSION=70

LLVM_CHECKOUT_VERSION=origin/release_70
CLANG_CHECKOUT_VERSION=origin/release_70
HALIDE_CHECKOUT_VERSION=1d471591077939fda9d9cbb77f92c6322cf28206

BUILD_ROOT=${SCRIPT_PATH}/../../../build
WORKTREE_ROOT=${SCRIPT_PATH}/../../../build/source
THIRD_PARTY_ROOT=${SCRIPT_PATH}/../../../third-party

LLVM_CLONE_PATH=${THIRD_PARTY_ROOT}/llvm
CLANG_CLONE_PATH=${THIRD_PARTY_ROOT}/clang
HALIDE_CLONE_PATH=${THIRD_PARTY_ROOT}/Halide

LLVM_REPOSITORY_URL="https://github.com/llvm-mirror/llvm.git"
CLANG_REPOSITORY_URL="https://github.com/llvm-mirror/clang.git"
HALIDE_REPOSITORY_URL="https://github.com/halide/Halide.git"

# ^ ## ^ ### USER CONFIGURATION ### ^ ## ^ #



# v ## v ### FUNCTIONS ### v ## v #

function get_processor_count()
{
	# 'getconf' is possibly the most portable way to retrieve the number of
	# processor count from within a shell script:
	# (since 'grep -c ^processor /proc/cpuinfo' relies on procfs...)
	local num_processors=$(getconf _NPROCESSORS_ONLN)

	# make sure $num_processors is indeed an integer number:
	if [[ ! "$num_processors" -eq "$num_processors" ]] 2>/dev/null ; then
		num_processors=0
	fi

	# and ensure that it is a meaninful number:
	if [[ ! "$num_processors" > 0 ]] ; then
		num_processors=0
	fi

	echo $num_processors
}

function get_processor_count_for_build()
{
	local num_processors=$(get_processor_count)

	local num_processors_build=$((num_processors / 2))
	if [[ "$num_processors_build" = 0 ]] ; then
		num_processors_build=1
	fi

	echo $num_processors_build
}

function is_VisualStudio_build()
{
	if windows_host; then
		if [ $WINDOWS_MINGW_BUILD == 0 ]; then
			true
		else
			false
		fi
	else
		false
	fi
}

function check_tool()
{
	local NAME=$1
	local TOOL=$2

	echo
	echo [!!] checking for $NAME ...

	if [ -x "$TOOL" ]; then
		echo [OK] tool $NAME found.
	else
		if [ -x "$(command -v $TOOL)" ]; then
			echo [OK] tool $NAME found.
		else
			echo [ERROR] unable to locate $NAME ! >&2
			exit 1
		fi
	fi
}

function git_checkout()
{
	local MODULE=$1
	local CLONE_PATH=$2
	local CHECKOUT_VERSION=$3
	local REPOSITORY_URL=$4

	local WORKTREE_DIR=$WORKTREE_ROOT/$MODULE

	echo
	echo [!!] checking out $CHECKOUT_VERSION from $CLONE_PATH into $WORKTREE_DIR

	# only clone if the checkout path does not exist
	if [ -d $CLONE_PATH ]; then
		echo [OK] git checkout path $CLONE_PATH exists.
	else
		echo [!!] cloning $REPOSITORY_URL into $CLONE_PATH...
		mkdir -p $CLONE_PATH
		pushd $CLONE_PATH
			"$GIT" clone $REPOSITORY_URL .
		popd
	fi

	# checkout the desired version in a new worktree
	pushd $CLONE_PATH
		"$GIT" fetch
		"$GIT" worktree add -f "$WORKTREE_DIR" $CHECKOUT_VERSION
	popd

	pushd "$WORKTREE_DIR"
		# https://stackoverflow.com/a/1783426
		# use basename to remove the "remote" from the branch
		local LOCAL_BRANCH=$(basename "$CHECKOUT_VERSION")
		"$GIT" checkout -b "$LOCAL_BRANCH" "$CHECKOUT_VERSION"
		"$GIT" pull
	popd
}

function make_link()
{
	# convert from relative path to absolute path
	local SOURCE=$(cd "$1"; pwd)

	# since the destination directory might not exist, we need a placeholder
	# in order to use the 'cd'/'pwd' hack to obtain the absolute path:
	mkdir -p "$2"
	local DESTINATION=$(cd "$2"; pwd)
	rmdir $DESTINATION
	
	echo
	echo [!!] linking $SOURCE to $DESTINATION ...

	# the source must be a valid directory
	if [ -d $SOURCE ]; then
		echo [OK] link source $SOURCE exists.
	else
		echo [ERROR] link source $SOURCE does not exist! >&2
		exit 2
	fi

	# the destination might already exist...
	if [ -d $DESTINATION ]; then
		echo [WARN] link destination $DESTINATION already exists...
	else
		# create the link (mklink junction on Windows)
		if windows_host; then
			SOURCE=$(cygpath -w "${SOURCE}")
			DESTINATION=$(cygpath -w "${DESTINATION}")
			cmd <<< "mklink /J \"${DESTINATION%/}\" \"${SOURCE%/}\""
		else
			ln -s $SOURCE $DESTINATION
		fi
	fi
}

function select_cmake_generator()
{
	# Default: Makefile generator
	CMAKE_GENERATOR="Unix Makefiles"
	
	# MacOS/Linux/MinGW: use Makefile
	if ! is_VisualStudio_build; then
		return
	fi

	# Visual Studio build: it's complicated...
	local PLATFORM=$1
	local CONFIGURATION=$2

	CMAKE_GENERATOR="Visual Studio $VISUAL_STUDIO_VERSION"
	if [ "$PLATFORM" == "Win64" ]; then
	CMAKE_GENERATOR+=" $PLATFORM"
	fi

	case "$VISUAL_STUDIO_VERSION" in
		14)	CMAKE_GENERATOR_EXTRA="-Tv140"
			;;
		15)	CMAKE_GENERATOR_EXTRA="-Tv141"
			;;
		*)	echo [ERROR] Unknown Visual Studio Version $VISUAL_STUDIO_VERSION.
			exit 10
			;;
	esac

	VISUAL_STUDIO_TOOLCHAIN="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build"

	if [ "$PLATFORM" == "Win64" ]; then
		CMAKE_GENERATOR_EXTRA+=",host=x64"
		VISUAL_STUDIO_TOOLCHAIN+="\vcvars64.bat"
		VISUAL_STUDIO_PLATFORM=x64
	else
		VISUAL_STUDIO_TOOLCHAIN+="\vcvars32.bat"
		VISUAL_STUDIO_PLATFORM=x86
	fi
	
	VISUAL_STUDIO_CONFIGURATION=$CONFIGURATION
}

function call_cmake()
{
	# skip cmake if solution/makefile already exists

	if is_VisualStudio_build; then
		# must call cmake indirectly here since it is impossible to pull in the
		# Visual Studio command shell environment into the MSYS2 shell...
		export CMAKE="$CMAKE"
		export VSTOOLCHAIN="$VISUAL_STUDIO_TOOLCHAIN"
		export VSPLATFORM="$VISUAL_STUDIO_PLATFORM"
		export VSCONFIGURATION="$VISUAL_STUDIO_CONFIGURATION"
		export VSSOLUTION="$VISUAL_STUDIO_SOLUTION"
		${SCRIPT_PATH}/windows/cmake.bat "$@"
		return
	fi

	local jN=$(get_processor_count_for_build)

	if ! [ -f Makefile ]; then
		$CMAKE "$@"
		"${CMAKE}" --build . --config ${CONFIGURATION} -- -j${jN}
		return
	fi

	make -j${jN}
}

function make_llvm()
{
	local SOURCE_PATH=$1
	local PLATFORM=$2
	local CONFIGURATION=$3

	echo
	echo [!!] perparing to build LLVM
	pwd
	
	local LLVM_BUILD_32_BITS=
	if [ "$PLATFORM" == "Win32" ]; then
		LLVM_BUILD_32_BITS=ON
	else
		LLVM_BUILD_32_BITS=OFF
	fi

	if is_VisualStudio_build; then
		VISUAL_STUDIO_SOLUTION=LLVM.sln
	fi

	call_cmake \
		-DLLVM_ENABLE_TERMINFO=OFF \
		-DLLVM_TARGETS_TO_BUILD="all" \
		-DLLVM_ENABLE_ASSERTIONS=ON \
		-DPYTHON_EXECUTABLE="$PYTHON" \
		-DCMAKE_BUILD_TYPE="$CONFIGURATION" \
		-DLLVM_BUILD_32_BITS=$LLVM_BUILD_32_BITS \
		-G "$CMAKE_GENERATOR" \
		$CMAKE_GENERATOR_EXTRA \
		"$SOURCE_PATH"
}

function make_halide()
{
	local SOURCE_PATH=$1
	local PLATFORM=$2
	local CONFIGURATION=$3

	echo
	echo [!!] perparing to build Halide
	pwd

	local LLVM_BUILD_DIR=
	LLVM_BUILD_DIR+=$BUILD_ROOT/llvm
	LLVM_BUILD_DIR+=/$BUILD_SYSTEM
	LLVM_BUILD_DIR+=/$PLATFORM
	LLVM_BUILD_DIR+=/$CONFIGURATION

	local LLVM_CMAKECONFIG_DIR=${LLVM_BUILD_DIR}/lib/cmake/llvm

	# can only proceed if LLVM has been built for this Platform/Configuration
	if [ -d $LLVM_CMAKECONFIG_DIR ]; then
		echo [OK] LLVM cmake config $LLVM_CMAKECONFIG_DIR exists.
	else
		echo [ERROR] Must first build LLVM for this Platform/Configuration... >&2
		exit 3
	fi

	LLVM_CONFIG=
	if is_VisualStudio_build; then
		VISUAL_STUDIO_SOLUTION=Halide.sln
	fi

	# on MacOS, the whole cmake+make cycle is bogus...
	# we're better off just calling the default Makefile that comes with Halide
	# but that requires setting up some environment variables...
	if ! is_VisualStudio_build; then
		export LLVM_CONFIG="$LLVM_BUILD_DIR/bin/llvm-config"
		export CLANG="$LLVM_BUILD_DIR/bin/clang"
		local MAKEFILE="${SOURCE_PATH}/Makefile"
		local jN=$(get_processor_count_for_build)
		make -f "$MAKEFILE" -j${jN}
		#make -f "$MAKEFILE" -j${jN} build_apps
		#make -f "$MAKEFILE" -j${jN} build_docs
		#make -f "$MAKEFILE" -j${jN} build_tests
		#make -f "$MAKEFILE" -j${jN} test_tutorial
		#make -f "$MAKEFILE" -j${jN} build_utils
		return
	fi

	SHARED_LIB_TOGGLE=ON
	if ! is_VisualStudio_build; then
		# static libHalide.a on MacOS and MinGW builds...
		echo "[!!] forcing static library (libHalide.a) build on MacOS."
		SHARED_LIB_TOGGLE=OFF
	fi

	call_cmake \
		-DLLVM_DIR="$LLVM_CMAKECONFIG_DIR" \
		-DLLVM_VERSION=$LLVM_VERSION \
		-DHALIDE_SHARED_LIBRARY="$SHARED_LIB_TOGGLE" \
		-DWITH_TUTORIALS=OFF \
		-DWITH_APPS=OFF \
		-DWITH_DOCS=OFF \
		-DWITH_TESTS=OFF \
		-DWITH_UTILS=OFF \
		-DCMAKE_BUILD_TYPE="$CONFIGURATION" \
		-G "$CMAKE_GENERATOR" \
		$CMAKE_GENERATOR_EXTRA \
		"$SOURCE_PATH"
}

function build()
{
	local BUILD_PATH=$1
	local SOURCE_PATH=$2
	local PLATFORM=$3
	local CONFIGURATION=$4

	pushd "$BUILD_PATH"
		BUILD_PATH+=/$BUILD_SYSTEM
		BUILD_PATH+=/$PLATFORM
		BUILD_PATH+=/$CONFIGURATION
		mkdir -p "$BUILD_PATH"
		pushd "$BUILD_PATH"
			# fifth argument of this build() routine is the "make_xyz" routine:
			$5 $SOURCE_PATH $PLATFORM $CONFIGURATION
		popd
	popd
}

function build_all()
{
	local platforms=$(get_platform_name)
	if windows_host; then
		platforms='Win64'
	fi

	configurations='Release'
	
	# overrides
	#platforms='Win32 Win64'
	#configurations='Debug Release'

	for platform in $platforms
	do
		for configuration in $configurations
		do
			echo
			echo "[!!] preparing build : $platform | $configuration"
			echo
			# select the appropriate cmake generator for the host platform and configuration:
			select_cmake_generator $platform $configuration
			build ${BUILD_ROOT}/llvm   ${WORKTREE_ROOT}/llvm   $platform $configuration make_llvm
			build ${BUILD_ROOT}/halide ${WORKTREE_ROOT}/halide $platform $configuration make_halide
		done
	done
}

# ^ ## ^ ### FUNCTIONS ### ^ ## ^ #



# v ## v ### THE MEAT OF THE SCRIPT ### v ## v #

# a.k.a. ECHO OFF
set +x;

# check system information
if [[ -z $(get_platform_name) ]]; then
	echo [ERROR] Unknown or unsupported platform: ${OSTYPE} ! >&2
	exit
fi
if [ $(get_processor_count) = 0 ] ; then
	echo [ERROR] Unable to retrieve processor count ! >&2
	exit
fi

# make sure the necessary tools are available
check_tool git    "$GIT"
check_tool cmake  "$CMAKE"
check_tool python "$PYTHON"

if ! is_VisualStudio_build; then
	check_tool make make
fi

# build root
if is_VisualStudio_build; then
	BUILD_SYSTEM=msvc
else
	BUILD_SYSTEM=make
	if windows_host; then
		BUILD_SYSTEM=mingw-make
	fi
fi

# clone and checkout LLVM and clang:
git_checkout llvm  $LLVM_CLONE_PATH   $LLVM_CHECKOUT_VERSION  $LLVM_REPOSITORY_URL
git_checkout clang $CLANG_CLONE_PATH $CLANG_CHECKOUT_VERSION $CLANG_REPOSITORY_URL

# create a symbolic link to clang within llvm/tools:
make_link ${WORKTREE_ROOT}/clang ${WORKTREE_ROOT}/llvm/tools/clang

# clone and checkout Halide:
git_checkout halide $HALIDE_CLONE_PATH $HALIDE_CHECKOUT_VERSION $HALIDE_REPOSITORY_URL

build_all

# ^ ## ^ ### THE MEAT OF THE SCRIPT ### ^ ## ^ #
