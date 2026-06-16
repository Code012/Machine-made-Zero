/*  date = November 15th 2025 07:17 PM */ 

#if !defined(HANDMADE_H)

/*
NOTE(casey):

HANDMADE_INTERNAL:
    0 - Build for public release
    1 - Build for developer only

HANDMADE_SLOW:
    0 - No slow code allowed!
    1 - Slow code welcome.
*/
#include "handmade_platform.h"


struct tile_map
{
    s32 CountX; // number of columns
    s32 CountY; // number of rows

    f32 UpperLeftX;
    f32 UpperLeftY;
    f32 TileWidth;
    f32 TileHeight;

    u32 *Tiles;
}; 
struct world
{
    // TODO(casey): Beginner's sparseness
    s32 TileMapCountX;
    s32 TileMapCountY;

    tile_map *TileMaps;
};   

struct game_state
{
    f32 PlayerX;
    f32 PlayerY;
};

#define HANDMADE_H
#endif