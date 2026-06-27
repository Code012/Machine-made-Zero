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

struct canonical_position
{
    s32 TileMapX; 
    s32 TileMapY; 

    s32 TileX;
    s32 TileY;

    // NOTE(casey): This is tile-relative X and Y
    // TODO(casey): These are still in pixels.... :/
    f32 X; 
    f32 Y;
};

// TODO(casey): Is this ever necessary?
struct raw_position
{
    s32 TileMapX;
    s32 TileMapY;

    // NOTE(casey): Tile-map relative X and Y
    f32 X;
    f32 Y;
};

struct tile_map
{
    u32 *Tiles;
}; 
struct world
{
    s32 CountX; // number of columns
    s32 CountY; // number of rows

    f32 UpperLeftX;
    f32 UpperLeftY;
    f32 TileWidth;
    f32 TileHeight;

    // TODO(casey): Beginner's sparseness
    s32 TileMapCountX;
    s32 TileMapCountY;

    tile_map *TileMaps;
};   

struct game_state
{
    // TODO(casey): Player state should be canonical position now?
    s32 PlayerTileMapX; // which tilemap player is in
    s32 PlayerTileMapY;

    f32 PlayerX;    // where in tilemap
    f32 PlayerY;
};

#define HANDMADE_H
#endif