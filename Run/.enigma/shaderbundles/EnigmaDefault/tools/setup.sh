#!/bin/bash

# ============================================================
# Enigma ShaderBundle Development Setup Script
# ============================================================

echo "╔════════════════════════════════════════════════════════════╗"
echo "║         Enigma ShaderBundle Development Setup              ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo

# ============================================================
# Configuration (Developers can modify)
# ============================================================
ENGINE_SHADER_RELATIVE_PATH="../../../assets/engine/shaders"
SYMLINK_NAME="@engine"

# ============================================================
# Path Calculation
# ============================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUNDLE_ROOT="$(dirname "$SCRIPT_DIR")"
SHADERS_DIR="$BUNDLE_ROOT/shaders"
SYMLINK_PATH="$SHADERS_DIR/$SYMLINK_NAME"

# Calculate Engine Shader absolute path
if [ -d "$SHADERS_DIR" ]; then
    ENGINE_SHADER_PATH="$(cd "$SHADERS_DIR" && cd "$ENGINE_SHADER_RELATIVE_PATH" 2>/dev/null && pwd)"
else
    echo "[ERROR] Shaders directory not found: $SHADERS_DIR"
    exit 1
fi

# ============================================================
# Check Engine Shader Directory
# ============================================================
if [ -z "$ENGINE_SHADER_PATH" ] || [ ! -d "$ENGINE_SHADER_PATH" ]; then
    echo "[ERROR] Engine shader directory not found."
    echo "        Please ensure this ShaderBundle is placed in:"
    echo "            Run/.enigma/shaderbundles/YourBundleName/"
    echo
    exit 1
fi

if [ ! -d "$ENGINE_SHADER_PATH/core" ]; then
    echo "[ERROR] Engine shader directory structure invalid:"
    echo "        $ENGINE_SHADER_PATH"
    echo
    echo "Expected structure:"
    echo "    engine/shaders/core/"
    echo "    engine/shaders/include/"
    echo "    engine/shaders/lib/"
    echo
    exit 1
fi

echo "Engine Shader Path: $ENGINE_SHADER_PATH"
echo "Symlink Target:     $SYMLINK_PATH"
echo

# ============================================================
# Step 1: Create @engine Symlink
# ============================================================
echo "[1/3] Creating @engine symlink..."

if [ -e "$SYMLINK_PATH" ] || [ -L "$SYMLINK_PATH" ]; then
    # Check if existing symlink is valid
    if [ -d "$SYMLINK_PATH" ]; then
        echo "      @engine already exists and is valid, skipping."
    else
        echo "      @engine exists but is broken, recreating..."
        rm -f "$SYMLINK_PATH"
        ln -s "$ENGINE_SHADER_PATH" "$SYMLINK_PATH"
        if [ $? -eq 0 ]; then
            echo "      Recreated @engine symlink successfully."
        else
            echo "[ERROR] Failed to recreate symlink."
            exit 1
        fi
    fi
else
    ln -s "$ENGINE_SHADER_PATH" "$SYMLINK_PATH"
    if [ $? -eq 0 ]; then
        echo "      Created @engine symlink successfully."
    else
        echo "[ERROR] Failed to create symlink."
        echo "        Please check directory permissions."
        exit 1
    fi
fi

# ============================================================
# Step 2: Validate Bundle Structure
# ============================================================
echo "[2/3] Validating bundle structure..."

VALIDATION_OK=1

# Check bundle.json
if [ ! -f "$SHADERS_DIR/bundle.json" ]; then
    echo "      [WARN] bundle.json not found"
    VALIDATION_OK=0
fi

# Check program directory
if [ ! -d "$SHADERS_DIR/program" ]; then
    echo "      [WARN] program/ directory not found"
    VALIDATION_OK=0
fi

if [ "$VALIDATION_OK" -eq 1 ]; then
    echo "      Bundle structure OK."
else
    echo "      Bundle structure has warnings, please check above."
fi

# ============================================================
# Step 3: Future Extension Point
# ============================================================
echo "[3/3] Additional setup..."

# TODO: Generate IDE configuration files
# TODO: HLSL syntax pre-validation
# TODO: Download engine shader documentation

echo "      No additional setup required."

# ============================================================
# Complete
# ============================================================
echo
echo "╔════════════════════════════════════════════════════════════╗"
echo "║                    Setup Complete!                         ║"
echo "╠════════════════════════════════════════════════════════════╣"
echo "║                                                            ║"
echo "║  You can now use in your shaders:                          ║"
echo "║                                                            ║"
echo "║    #include \"../@engine/core/core.hlsl\"                    ║"
echo "║    #include \"../@engine/include/fog_uniforms.hlsl\"         ║"
echo "║    #include \"../@engine/include/camera_uniforms.hlsl\"      ║"
echo "║    #include \"../@engine/lib/math.hlsl\"                     ║"
echo "║    #include \"../@engine/lib/fog.hlsl\"                      ║"
echo "║                                                            ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo

# ============================================================
# Verify symlink works
# ============================================================
if [ -f "$SYMLINK_PATH/core/core.hlsl" ]; then
    echo "[OK] Symlink verification passed: @engine/core/core.hlsl accessible"
else
    echo "[WARN] Symlink verification failed: cannot access @engine/core/core.hlsl"
fi

echo
