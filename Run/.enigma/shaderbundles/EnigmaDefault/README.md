# Enigma ShaderBundle - EnigmaDefault

The Enigma engine defaults to ShaderBundle, which provides a reference implementation of basic rendering functions.

## Quick Start

### 1. Run the configuration script

**Windows:**

```batch
cd tools
setup.bat
```

**Linux/Mac:**

```bash
cd tools
chmod +x setup.sh
./setup.sh
```

The script automatically creates an `@engine` symbolic link pointing to the engine core shader directory.

### 2. Reference engine code in Shader

```hlsl
// Reference the engine core library
#include "../@engine/core/core.hlsl"

// Reference engine Uniform definition
#include "../@engine/include/fog_uniforms.hlsl"
#include "../@engine/include/camera_uniforms.hlsl"
#include "../@engine/include/celestial_uniforms.hlsl"

// Reference engine tool library
#include "../@engine/lib/math.hlsl"
#include "../@engine/lib/fog.hlsl"
```

## Directory structure

```
EnigmaDefault/
├── shaders/
│   ├── bundle.json # Bundle metadata
│   ├── fallback_rule.json # Fallback rule configuration
│   ├── program/ # Shader program (VS/PS)
│   │   ├── composite.ps.hlsl
│   │   ├── gbuffers_basic.ps.hlsl
│   │   └── ...
│   ├── lib/ # User-defined library
│   ├── include/ # User-defined header file
│   └── @engine/ # [Symbolic link] Engine shader (created by setup)
│
├── tools/
│   ├── setup.bat # Windows configuration script
│   └── setup.sh # Linux/Mac configuration script
│
├── .gitignore
└── README.md
```

## Engine shader structure (@engine)

```
@engine/
├── core/
│   ├── core.hlsl # Core definition (must be quoted)
│   └── Common.hlsl # Common utility function
│
├── include/
│   ├── fog_uniforms.hlsl # Fog effect Uniform
│   ├── camera_uniforms.hlsl # Camera Uniform
│   ├── celestial_uniforms.hlsl # Celestial Uniform
│   └── ...
│
└── lib/
    ├── math.hlsl # Math utility function
    ├── fog.hlsl # Fog effect calculation function
    └──...
```

## Development Guide

### Shader entry point

All shaders use `main` as entry point (Iris compatible):

```hlsl
// Vertex Shader
VSOutput main(VSInput input)
{
    // ...
}

// Pixel Shader
PSOutput main(PSInput input)
{
    // ...
}
```

### Fallback mechanism

When a shader program does not exist, the engine will fall back according to the rules defined in `fallback_rule.json`:

```json
{
  "gbuffers_terrain_cutout": [
    "gbuffers_terrain",
    "gbuffers_textured"
  ],
  "gbuffers_water": [
    "gbuffers_terrain",
    "gbuffers_textured"
  ]
}
```

### Debugging Tips

1. **View preprocessed code**: Enable the `-E` parameter in the compilation options
2. **Enable debug information**: Set `enableDebugInfo = true`
3. **Check Symlink**: Make sure the `@engine` link is valid

## FAQ

### Q: setup.bat prompts insufficient permissions

**Solution**:

1. Enable Windows Developer Mode: Settings → Update & Security → Developer Options
2. Or run the script as administrator

### Q: IDE displays include path error

**Solution**:

1. Make sure you have run `setup.bat/setup.sh`
2. Restart the IDE or refresh the project

### Q: @engine link is invalid

**Solution**:

1. Delete the `shaders/@engine` directory
2. Rerun `setup.bat/setup.sh`

## Version information

- **Bundle Version**: 1.0.0
- **Engine Compatibility**: Enigma Engine 1.x
- **Shader Model**: 6.6

## License

MIT License - Free to modify and distribute