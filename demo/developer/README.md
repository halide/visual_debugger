# Demo (for developers)

Use the convenience [scripts/bootstrap.sh](scripts/bootstrap.sh) script to configure and build LLVM and Halide from scratch at a predefined location. Once that's done, run `cmake` here at this location to have the visual debugger developer demo built.

## Side-effects of the bootstrap script

The bootstrap script will clone LLVM, clang and Halide to `visual_debugger/third-party` and checkout specific worktrees to `visual_debugger/build/source`. The script will then build LLVM (with clang) and Halide into `visual_debugger/build`. The cmake file contained in this folder will look for LLVM and Halide at these predetermined build locations in order to build the demo.

## Settings

The user might need to tweak the following settings in `scripts/bootstrap.sh`:

- location of the necessary tools:
```
GIT=git
CMAKE=cmake
PYTHON=$(which python)
```

- Windows build settings:
```
if windows_host; then
	# Visual Studio version selection:
	# 14 -> VS 2015 ; 15 -> VS 2017
	VISUAL_STUDIO_VERSION=15
	WINDOWS_MINGW_BUILD=0
fi
```
> NOTE: if `WINDOWS_MINGW_BUILD` is set, the build will proceed with MinGW make (`VISUAL_STUDIO_VERSION` will be ignored).

- LLVM/Halide branch selection:
```
LLVM_VERSION=60
LLVM_CHECKOUT_VERSION=origin/release_60
CLANG_CHECKOUT_VERSION=origin/release_60
HALIDE_CHECKOUT_VERSION=cc65b9961cb4f5f9ad76c04b0082698557af6681
```

## Advanced settings

By default, the script will build both LLVM and Halide only in Release mode, but it is possible to override it by editing:
```
# overrides
#platforms='Win32 Win64'
#configurations='Debug Release'
```
