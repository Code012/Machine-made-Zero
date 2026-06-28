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
    /* TODO(casey):

        Take the tile map x and y
        and the tile x and y

        and pack them into sinle 32-bit values for x and y
        where there is some low bits for the tile index
        and the the high bits are the tile "page"
    */

    s32 TileMapX; 
    s32 TileMapY; 

    s32 TileX;
    s32 TileY;

    /*  TODO(casey):

        Convert these to math=friendly, resolution independent representation of
        world units relative to a tile.
        
    */
    f32 TileRelX; 
    f32 TileRelY;
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
    f32 TileSideInMeters;
    s32 TileSideInPixels;

    s32 CountX; // number of columns
    s32 CountY; // number of rows

    f32 UpperLeftX;
    f32 UpperLeftY;

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