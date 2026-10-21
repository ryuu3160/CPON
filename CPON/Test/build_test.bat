@echo off
rem Builds and runs test_binary.cpp (x64) in Debug (/MDd, iterator debugging) and Release (/MD).
rem Usage: build_test.bat [output_dir]   (default: %TEMP%\cpon_test)
setlocal
set "OUT=%~1"
if "%OUT%"=="" set "OUT=%TEMP%\cpon_test"
if not exist "%OUT%\dbg" mkdir "%OUT%\dbg"
if not exist "%OUT%\rel" mkdir "%OUT%\rel"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "SRC=%~dp0..\src"

echo === Debug x64 /MDd ===
cl /nologo /std:c++20 /EHsc /permissive- /W3 /MDd /D_DEBUG /Od /I"%SRC%" /Fo"%OUT%\dbg\\" /Fe:"%OUT%\test_dbg.exe" "%~dp0test_binary.cpp" "%SRC%\cpon.cpp" "%SRC%\cpon_object.cpp" "%SRC%\cpon_binary_helper.cpp" || exit /b 1
echo === Release x64 /MD ===
cl /nologo /std:c++20 /EHsc /permissive- /W3 /MD /DNDEBUG /O2 /I"%SRC%" /Fo"%OUT%\rel\\" /Fe:"%OUT%\test_rel.exe" "%~dp0test_binary.cpp" "%SRC%\cpon.cpp" "%SRC%\cpon_object.cpp" "%SRC%\cpon_binary_helper.cpp" || exit /b 1

pushd "%OUT%"
echo === run Debug ===
"%OUT%\test_dbg.exe" || (popd & exit /b 1)
echo === run Release ===
"%OUT%\test_rel.exe" || (popd & exit /b 1)
popd
echo ALL TESTS PASSED
