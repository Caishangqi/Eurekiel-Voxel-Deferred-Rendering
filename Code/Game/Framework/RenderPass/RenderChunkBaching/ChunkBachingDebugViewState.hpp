#pragma once

class ChunkBachingDebugViewState
{
public:
    ChunkBachingDebugViewState()                                             = delete;
    ChunkBachingDebugViewState(const ChunkBachingDebugViewState&)            = delete;
    ChunkBachingDebugViewState& operator=(const ChunkBachingDebugViewState&) = delete;

    static inline bool  enableRegionWireframe = false;
    static inline float cullingMapHalfExtent  = 160.0f;
};
