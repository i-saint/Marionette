setlocal
cd /d "%~dp0"
call toolchain.bat
msbuild MouseReplayer.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m /nologo
set DIST_DIR="_dist\MouseReplayer"
set BIN_DIR="%DIST_DIR%\bin"
mkdir "%DIST_DIR%"
mkdir "%BIN_DIR%"
copy _out\x64_Release\MouseReplayer.exe "%DIST_DIR%"
copy MouseReplayer\Externals\lib\tbb.dll "%BIN_DIR%"
copy MouseReplayer\Externals\lib\opencv_core451.dll "%BIN_DIR%"
copy MouseReplayer\Externals\lib\opencv_imgcodecs451.dll "%BIN_DIR%"
copy MouseReplayer\Externals\lib\opencv_imgproc451.dll "%BIN_DIR%"
exit /B 0
