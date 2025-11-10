#pragma once
#include <vector>

#include "Engine/Graphic/Integration/RendererSubsystem.hpp"


/// Whether or not enable cosmic circle (developer)
#define COSMIC


namespace enigma::resource
{
    class ResourceSubsystem;
}

struct Vertex_PCU;
struct Rgba8;
struct Vec2;
class Camera;
class App;
class RandomNumberGenerator;
class IRenderer;
class InputSystem;
class AudioSubsystem;
class Game;


extern RandomNumberGenerator* g_rng;
extern App*                   g_theApp;
extern IRenderer*             g_theRenderer;
extern InputSystem*           g_theInput;
extern AudioSubsystem*        g_theAudio;
extern Game*                  g_theGame;

#pragma region WINDOW
#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720
#pragma endregion WINDOW

#pragma region RENDER
#define ENABLE_DEBUG    true
#define ENABLE_GPU_VALIDATION true
#pragma endregion RENDER

constexpr float WORLD_SIZE_X   = 200.f;
constexpr float WORLD_SIZE_Y   = 100.f;
constexpr float WORLD_CENTER_X = WORLD_SIZE_X / 2.f;
constexpr float WORLD_CENTER_Y = WORLD_SIZE_Y / 2.f;
// Math
constexpr float PI = 3.14159265359f;
// Entity Data
constexpr int MAX_ENTITY_PER_TYPE = 64;

/// Grid
constexpr int GRID_SIZE      = 50; // Half
constexpr int GRID_UNIT_SIZE = 5;
/// 

void DebugDrawRing(const Vec2& center, float radius, float thickness, const Rgba8& color);

void DebugDrawLine(const Vec2& start, const Vec2& end, float thickness, const Rgba8& color);

void AddVertsForCube3D(std::vector<Vertex_PCU>& verts, const Rgba8& color);
void AddVertsForCube3D(std::vector<Vertex_PCU>& verts, const Rgba8& colorX, const Rgba8& colorNX, const Rgba8& colorY, const Rgba8& colorNY, const Rgba8& colorZ, const Rgba8& colorNZ);
