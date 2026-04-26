<p align="center"><img src="https://github.com/user-attachments/assets/12663581-fb81-4364-96d6-36e57d6cfd4f" alt="Logo" width="300"></p>

<h1 align="center">Enigma DirectX 12 Voxel Renderer</h1>
<h4 align="center">Stylized shader bundles in a real-time destructible voxel world.</h4>

<p align="center">
<img alt="Render API" src="https://img.shields.io/badge/Render%20API-DirectX%2012-242629">
<img alt="Shader Model" src="https://img.shields.io/badge/Shader%20Model-6.6-4c6fff">
<img alt="C++" src="https://img.shields.io/badge/C%2B%2B-17-cherry">
<img alt="Platform" src="https://img.shields.io/badge/Platform-Windows-0078d4">
</p>

## Overview

Enigma DirectX 12 Voxel Renderer is a thesis renderer that explores an Iris-inspired deferred rendering pipeline on top of DirectX 12 and Shader Model 6.6. The project focuses on making voxel
rendering programmable through external Shader Bundles, so lighting, atmosphere, shadows, reflections, volumetric effects, and post-processing can be authored outside the C++ frame logic.

This repository contains the sample application, world/render-pass orchestration, shader bundles, paper assets, and profiling artifacts. The shared engine library is expected as a sibling `Engine`
checkout.

## Highlights

- DirectX 12 renderer with Shader Model 6.6 bindless resource access.
- Deferred, Iris-inspired frame pipeline: shadow, sky, terrain, deferred lighting, translucent terrain, cloud, composite, and final passes.
- ShaderBundle system for loadable shader packs, fallback rules, lifecycle events, and comment directives such as `DRAWBUFFERS`, `RENDERTARGETS`, `BLEND`, `DEPTHTEST`, and `FORMAT`.
- DXC-based HLSL compilation with include graph validation and deterministic shader program construction.
- Voxel terrain renderer with region batching, copy-ready GPU buffers, and async chunk mesh build.
- EnigmaDefault stylized bundle with atmosphere, soft shadows, volumetric clouds/light, SSR, underwater distortion, bloom, SSAO, sun glare, and depth of field.

## Repository Layout

| Path                                       | Purpose                                                                             |
|--------------------------------------------|-------------------------------------------------------------------------------------|
| `Code/Game/`                               | Application layer, world logic, render-pass scheduling, and ImGui inspection tools. |
| `Run/.enigma/`                             | Runtime configuration, engine assets, and active ShaderBundle data.                 |
| `Run/.enigma/shaderbundles/EnigmaDefault/` | Main stylized ShaderBundle used to validate the pipeline ceiling.                   |
| `Paper/Technical Design Document/`         | Technical design document and supporting thesis material.                           |
| `captures/`                                | RenderDoc captures used for GPU validation and performance analysis.                |
| `../Engine/Code/`                          | Shared engine library referenced by the Visual Studio solution.                     |

## Requirements

- Windows 10/11.
- Visual Studio 2022 with the v143 C++ toolset.
- Windows 10 SDK.
- DirectX 12 capable GPU and driver with Shader Model 6.6 support.
- A sibling `Engine` checkout next to this repository:

```text
SD/
  Engine/
  Thesis/
```

## Build And Run

Open `FGSPDX12DRECPS.sln` in Visual Studio 2022, select `Debug|x64` or `Release|x64`, then build `FGSPDX12DRECPS`.

Command-line build:

```powershell
msbuild "FGSPDX12DRECPS.sln" /p:Configuration=Debug /p:Platform=x64
```

The post-build step copies the executable and runtime resources into `Run/`. Launch from that directory so relative `.enigma` paths resolve correctly:

```powershell
cd "Run"
.\FGSPDX12DRECPS_Debug_x64.exe
```

## Shader Bundles

Shader Bundles are loadable shader packages inspired by Minecraft Iris shader packs. A bundle can provide HLSL programs, custom textures, render-target directives, material metadata, and fallback
rules. The engine keeps render-pass scheduling and GPU resource ownership in C++, while the active bundle defines the visual behavior of each pass.

The active bundle is configured through:

```text
Run/.enigma/config/engine/shaderbundle.yml
```

Current bundles include:

- `EnigmaDefault`: the primary stylized bundle for atmosphere, volumetrics, reflections, underwater effects, and post-processing.
- `GuildhallVector`: an additional runtime bundle used for experimentation.

## Documentation

The concise architecture reference is in the technical design document:

[Paper/Technical Design Document/Technical Design Document.docx](Paper/Technical%20Design%20Document/Technical%20Design%20Document.docx)

This project is a research renderer, not a packaged engine SDK. APIs, shader directives, and runtime configuration may change as the thesis implementation evolves.
