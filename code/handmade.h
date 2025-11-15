/*  date = November 15th 2025 07:17 PM */ 

#if !defined(HANDMADE_H)

struct game_offscreen_buffer
{
    void      *Memory;
    int        Width;
    int        Height;
    int        Pitch;
};

// TODO(casey): Services that the platform layer provides to the game.

// NOTE(casey): Services that the game provides to the platform layer
void GameUpdateAndRender(game_offscreen_buffer *Buffer, int XOffset, int YOffset);

#define HANDMADE_H
#endif