@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

echo ╔════════════════════════════════════════════════════════════╗
echo ║         Enigma ShaderBundle Development Setup              ║
echo ╚════════════════════════════════════════════════════════════╝
echo.

:: ============================================================
:: Configuration (Developers can modify)
:: ============================================================
set "ENGINE_SHADER_RELATIVE_PATH=..\..\..\assets\engine\shaders"
set "SYMLINK_NAME=@engine"

:: ============================================================
:: Path Calculation
:: ============================================================
set "SCRIPT_DIR=%~dp0"
set "BUNDLE_ROOT=%SCRIPT_DIR%.."
set "SHADERS_DIR=%BUNDLE_ROOT%\shaders"
set "SYMLINK_PATH=%SHADERS_DIR%\%SYMLINK_NAME%"

:: Calculate Engine Shader absolute path
pushd "%SHADERS_DIR%"
cd /d "%ENGINE_SHADER_RELATIVE_PATH%" 2>nul
if !errorlevel! neq 0 (
    popd
    echo [ERROR] Cannot navigate to engine shader directory.
    echo         Please ensure this ShaderBundle is placed in:
    echo             Run/.enigma/shaderbundles/YourBundleName/
    echo.
    pause
    exit /b 1
)
set "ENGINE_SHADER_PATH=%CD%"
popd

:: ============================================================
:: Check Engine Shader Directory
:: ============================================================
if not exist "%ENGINE_SHADER_PATH%\core" (
    echo [ERROR] Engine shader directory structure invalid:
    echo         %ENGINE_SHADER_PATH%
    echo.
    echo Expected structure:
    echo     engine/shaders/core/
    echo     engine/shaders/include/
    echo     engine/shaders/lib/
    echo.
    pause
    exit /b 1
)

echo Engine Shader Path: %ENGINE_SHADER_PATH%
echo Symlink Target:     %SYMLINK_PATH%
echo.

:: ============================================================
:: Step 1: Create @engine Symlink
:: ============================================================
echo [1/3] Creating @engine symlink...

if exist "%SYMLINK_PATH%" (
    echo       @engine already exists, checking validity...

    :: Check if it's a valid junction/symlink
    dir "%SYMLINK_PATH%" >nul 2>&1
    if !errorlevel! equ 0 (
        echo       @engine is valid, skipping.
    ) else (
        echo       @engine is broken, recreating...
        rmdir "%SYMLINK_PATH%" 2>nul
        goto :CreateSymlink
    )
) else (
    :CreateSymlink
    :: Try Junction first (no admin required on Windows)
    mklink /J "%SYMLINK_PATH%" "%ENGINE_SHADER_PATH%" >nul 2>&1
    if !errorlevel! equ 0 (
        echo       Created @engine junction successfully.
    ) else (
        :: Fallback: Try directory symlink (requires admin or dev mode)
        mklink /D "%SYMLINK_PATH%" "%ENGINE_SHADER_PATH%" >nul 2>&1
        if !errorlevel! equ 0 (
            echo       Created @engine symlink successfully.
        ) else (
            echo.
            echo [WARN] Failed to create symlink automatically.
            echo        Please try one of the following:
            echo.
            echo        Option 1: Enable Windows Developer Mode
            echo                  Settings ^> Update ^& Security ^> For developers
            echo.
            echo        Option 2: Run this script as Administrator
            echo.
            echo        Option 3: Create manually with admin CMD:
            echo                  mklink /J "%SYMLINK_PATH%" "%ENGINE_SHADER_PATH%"
            echo.
        )
    )
)

:: ============================================================
:: Step 2: Validate Bundle Structure
:: ============================================================
echo [2/3] Validating bundle structure...

set "VALIDATION_OK=1"

:: Check bundle.json
if not exist "%SHADERS_DIR%\bundle.json" (
    echo       [WARN] bundle.json not found
    set "VALIDATION_OK=0"
)

:: Check program directory
if not exist "%SHADERS_DIR%\program" (
    echo       [WARN] program/ directory not found
    set "VALIDATION_OK=0"
)

if "%VALIDATION_OK%"=="1" (
    echo       Bundle structure OK.
) else (
    echo       Bundle structure has warnings, please check above.
)

:: ============================================================
:: Step 3: Future Extension Point
:: ============================================================
echo [3/3] Additional setup...

:: TODO: Generate IDE configuration files
:: TODO: HLSL syntax pre-validation
:: TODO: Download engine shader documentation

echo       No additional setup required.

:: ============================================================
:: Complete
:: ============================================================
echo.
echo ╔════════════════════════════════════════════════════════════╗
echo ║                    Setup Complete!                         ║
echo ╠════════════════════════════════════════════════════════════╣
echo ║                                                            ║
echo ║  You can now use in your shaders:                          ║
echo ║                                                            ║
echo ║    #include "../@engine/core/core.hlsl"                    ║
echo ║    #include "../@engine/include/fog_uniforms.hlsl"         ║
echo ║    #include "../@engine/include/camera_uniforms.hlsl"      ║
echo ║    #include "../@engine/lib/math.hlsl"                     ║
echo ║    #include "../@engine/lib/fog.hlsl"                      ║
echo ║                                                            ║
echo ╚════════════════════════════════════════════════════════════╝
echo.

:: ============================================================
:: Verify symlink works
:: ============================================================
if exist "%SYMLINK_PATH%\core\core.hlsl" (
    echo [OK] Symlink verification passed: @engine/core/core.hlsl accessible
) else (
    echo [WARN] Symlink verification failed: cannot access @engine/core/core.hlsl
)

echo.
pause
