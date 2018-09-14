@ECHO OFF

PUSHD .
CALL "%VSTOOLCHAIN%"
POPD

:: skip CMake if the solution already exists:
IF EXIST "%VSSOLUTION%" GOTO BUILD

:: https://cmake.org/cmake/help/v3.8/generator/Visual%20Studio%2015%202017.html
SET CMAKE_ARGS=%*
:: ECHO %CMAKE_ARGS%
"%CMAKE%" %CMAKE_ARGS%

:BUILD
:: NOTE(marcos): nerfing the number of cpus allowed during the build to prevent
::               throttling the system since LLVM and Halide are comprised of a
::               bazillion projects...
:: msbuild "%VSSOLUTION%" /property:Configuration=%VSCONFIGURATION% /property:Platform=%VSPLATFORM% /maxcpucount:2
:: devenv "%VSSOLUTION%" /Build "%VSCONFIGURATION%|%VSPLATFORM%"

ECHO .
ECHO "=====> !!! You must build '%VSSOLUTION%' manually in Visual Studio."
ECHO .
