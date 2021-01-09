@echo off

cd External

IF NOT EXIST "7za.exe" (
    echo "downloading 7za.exe..."
    powershell.exe -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "[System.Net.ServicePointManager]::SecurityProtocol=[System.Net.SecurityProtocolType]::Tls12; wget https://github.com/i-saint/MouseReplayer/releases/download/20210109/7za.exe -OutFile 7za.exe"
)

echo "downloading external libararies..."
powershell.exe -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "[System.Net.ServicePointManager]::SecurityProtocol=[System.Net.SecurityProtocolType]::Tls12; wget https://github.com/i-saint/MouseReplayer/releases/download/20210109/Externals.7z -OutFile Externals.7z"
7za.exe x -aos Externals.7z

cd ..
