setlocal
cd /d "%~dp0"
call toolchain.bat
msbuild Marionette.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m /nologo

set ZIP="%~dp0Externals\7za.exe"
set DIST_DIR="_dist\Marionette"
set BIN_DIR="%DIST_DIR%\bin"
mkdir %DIST_DIR%
rem mkdir %BIN_DIR%
copy _out\x64_Release\Marionette.exe %DIST_DIR%
cd "_dist"
%ZIP% a Marionette.zip Marionette
exit /B 0
