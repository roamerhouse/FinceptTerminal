@echo off
set "VS_PATH=D:\Tools\Microsoft Visual Studio\18\Community"
set "VCVARS=%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat"

if not exist "%VCVARS%" (
    echo [ERROR] Could not find vcvarsall.bat at %VCVARS%
    exit /b 1
)

echo [INFO] Initializing MSVC Environment...
call "%VCVARS%" x64

echo [INFO] Configuring CMake...
cd fincept-qt
cmake --preset win-release -DCMAKE_MAKE_PROGRAM="D:\Tools\Python311\Scripts\ninja.exe" -DOPENSSL_ROOT_DIR="C:\Program Files\OpenSSL-Win64"

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake Configuration Failed
    exit /b %ERRORLEVEL%
)

echo [INFO] Building...
cmake --build --preset win-release

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build Failed
    exit /b %ERRORLEVEL%
)

echo [INFO] Deploying Qt Dependencies...
set "WINDEPLOYQT=D:\Tools\Qt683\6.8.3\msvc2022_64\bin\windeployqt.exe"
"%WINDEPLOYQT%" --release --no-compiler-runtime "fincept-qt\build\win-release\FinceptTerminal.exe"

echo [INFO] Build and Deployment Successful!
