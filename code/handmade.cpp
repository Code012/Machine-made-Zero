/*  date = November 15th 2025 07:21 PM */

#include "handmade.h"
#include "handmade_intrinsics.h"

internal void
RenderWeirdGradient(game_offscreen_buffer* Buffer, int XOffset, int YOffset)
{

    u8* Row = (u8*)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; ++Y) {
        u32* Pixel = (u32*)Row;
        for (int X = 0; X < Buffer->Width; ++X) {
            u8 Red   = 0;
            u8 Green = (u8)(Y + YOffset);
            u8 Blue  = (u8)(X + XOffset);

            *Pixel++ = Red << 16 | Green << 8 | Blue; // << 0
        }
        Row += Buffer->Pitch;
    }
}

internal void
DrawRectangle(game_offscreen_buffer* Buffer, 
                f32 RealMinX, f32 RealMinY, f32 RealMaxX, f32 RealMaxY,
                f32 R, f32 G, f32 B)    // R, G, B: percentage 0-1
{
    // Round up float Min/Max positions to an integer
    // Draw our rectangle from Min(x, y) up to and excluding Max(x, y)

    s32 MinX = RoundReal32ToUInt32(RealMinX); 
    s32 MinY = RoundReal32ToUInt32(RealMinY); 
    s32 MaxX = RoundReal32ToUInt32(RealMaxX); 
    s32 MaxY = RoundReal32ToUInt32(RealMaxY);
    // clipping to screen
    if (MinX < 0)
    {
        MinX = 0;
    }

    if (MinY < 0)
    {
        MinY = 0;
    }

    if (MaxX > Buffer->Width)
    {
        MaxX = Buffer->Width;
    }

    if (MaxY > Buffer->Height)
    {
        MaxY = Buffer->Height;
    }
    u32 Color = (u32)((RoundReal32ToInt32(R * 255.0f) << 16) |
                      (RoundReal32ToInt32(G * 255.0f) << 8)  |
                      (RoundReal32ToInt32(B * 255.0f) << 0));
    /*
We want to position ourselves at (MinX, MinY) before we enter the loop as follows:

- Since we want to offset one byte at a time for correct positioning, we define the row pointer as an 8-bit value
- We start at the start of the buffer memory.
- We then move “horizontally” by MinX amount of pixels, multiplied by the size of our pixels. We have saved this size as BytesPerPixel.
- Finally, we move “vertically” by MinY pixels, multiplied by the stride or Pitch of our row.
- We then advance our row by pitch until we fill the rectangle we need.
    */
    u8 *Row = ((u8 *)Buffer->Memory +
                MinX * Buffer->BytesPerPixel + 
                MinY * Buffer->Pitch);
    for (int Y = MinY;  // row
             Y < MaxY;
           ++Y)
    {
        u32 *Pixel = (u32 *)Row;
        for (int X = MinX;  // col
                 X < MaxX;
               ++X)
        {
            *Pixel++ = Color;
        }
        Row += Buffer->Pitch;
    }
}

internal void
GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz)
{
	s16 ToneVolume = 3000;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

	s16 *SampleOut = SoundBuffer->Samples;
    for (int SampleIndex = 0;
         SampleIndex < SoundBuffer->SampleCount;
         ++SampleIndex)
    {
#if 0
        f32 SineValue = sinf(GameState->tSine);
        s16 SampleValue = (s16)(SineValue * ToneVolume);
#else
        s16 SampleValue = 0;
#endif
        
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
#if 0
        GameState->tSine += (2.0f * Pi32 * 1.0f) / (f32)WavePeriod;
        if (GameState->tSine > 2.0f * Pi32)
        {
            GameState->tSine -= 2.0f * Pi32;
        }
#endif
    }
}

inline tile_map *
GetTileMap(world *World, s32 TileMapX, s32 TileMapY)
{
    tile_map *TileMap = 0;

    if ((TileMapX >= 0) && (TileMapX < World->TileMapCountX) &&
        (TileMapY >= 0) && (TileMapY < World->TileMapCountY))
    {
        TileMap = &World->TileMaps[TileMapY * World->TileMapCountX + TileMapX];
    }

    return (TileMap);
}

inline u32 
GetTileValueUnchecked(world *World, tile_map *TileMap, s32 TileX, s32 TileY)
{  
    Assert(TileMap);
    Assert(((TileX >= 0) && (TileX < World->CountX) &&   // player within bounds
            (TileY >= 0) && (TileY < World->CountY)));

    u32 TileMapValue = TileMap->Tiles[TileY * World->CountX + TileX]; // RowIndex * Width of row + Column Index
    return (TileMapValue);
}
internal b32
IsTileMapPointEmpty(world *World, tile_map *TileMap, s32 TestTileX, s32 TestTileY)
{
    b32 Empty = false;

    if (TileMap)
    {
        if ((TestTileX >= 0) && (TestTileX < World->CountX) &&   // player within bounds
            (TestTileY >= 0) && (TestTileY < World->CountY))
        {
            u32 TileMapValue = GetTileValueUnchecked(World, TileMap, TestTileX, TestTileY);
            Empty = (TileMapValue == 0);  // player not on wall
        }
    }

    return (Empty);
}



inline canonical_position
GetCanonicalPosition(world *World, raw_position Pos)
{
    canonical_position Result;

    Result.TileMapX = Pos.TileMapX;
    Result.TileMapY = Pos.TileMapY;

    
    f32 X = Pos.X - World->UpperLeftX; // relative to tilemap origin
    f32 Y = Pos.Y - World->UpperLeftY;
    Result.TileX = FloorReal32ToInt32((X) / World->TileSideInPixels);  
    Result.TileY = FloorReal32ToInt32((Y) / World->TileSideInPixels);
    
    Result.TileRelX = X - Result.TileX*World->TileSideInPixels;   // relative to tile map
    Result.TileRelY = Y - Result.TileY*World->TileSideInPixels;

    Assert(Result.TileRelX >= 0);
    Assert(Result.TileRelY >= 0);
    Assert(Result.TileRelX < World->TileSideInPixels);
    Assert(Result.TileRelY < World->TileSideInPixels);

    // map player point onto other tilemaps if it goes out of bounds of current tilemap
    if (Result.TileX < 0) // going to left tilemap
    {
        Result.TileX = World->CountX + Result.TileX; // tilemap width - playerX (TestTileX is already negative here so we just add)
        --Result.TileMapX;
    }
    
    if (Result.TileY < 0)    // going to top tilemap
    {
        Result.TileY = World->CountY + Result.TileY;// tilemap height - playerY (TestTileY is already negative here so we just add)
        --Result.TileMapY;
    }

    if (Result.TileX >= World->CountX)   // going to right tilemap
    {
        Result.TileX = Result.TileX - World->CountX;  // playerX - tilemapwidth. CountX goes from 0 -> n-1 (0-indexed), so when it goes out of bounds, plyaerX is at n, so we just subtract by n, i.e. CountX.
        ++Result.TileMapX;
    }

    if (Result.TileY >= World->CountY)   // going to bottom tilemap
    {
        Result.TileY = Result.TileY - World->CountY;  // playerY - tilemap height. CountY goes from 0 -> n-1 (0-indexed), so when it goes out of bounds, plyaerY is at n, so we just subtract by n, i.e. CountY.
        ++Result.TileMapY;
    }

    return (Result);
}

internal b32
IsWorldPointEmpty(world *World, raw_position TestPos)
{
    b32 Empty = false;

    
    canonical_position CanPos = GetCanonicalPosition(World, TestPos);
    tile_map *TileMap = GetTileMap(World, CanPos.TileMapX, CanPos.TileMapY);
    Empty = IsTileMapPointEmpty(World, TileMap, CanPos.TileX, CanPos.TileY);

    return (Empty);
}

#if defined __cplusplus
extern "C"
#endif
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons))); // so we dont forget to update Buttons length when adding or removing new buttons
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    #define TILE_MAP_COUNT_X 17
    #define TILE_MAP_COUNT_Y 9
    // same as HD aspect ratio
    u32 Tiles00[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] =
    {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
        {1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0},
        {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1},
        {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
    };
    u32 Tiles01[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] =
    {
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
    };
    u32 Tiles10[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] =
    {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
    };

    u32 Tiles11[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] =
    {
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
    };

    tile_map TileMaps[2][2];
    
    TileMaps[0][0].Tiles = (u32 *)Tiles00;
    TileMaps[0][1].Tiles = (u32 *)Tiles10;
    TileMaps[1][0].Tiles = (u32 *)Tiles01;
    TileMaps[1][1].Tiles = (u32 *)Tiles11;

    world World;
    World.TileMapCountX = 2;
    World.TileMapCountY = 2;
    World.CountX = TILE_MAP_COUNT_X;
    World.CountY = TILE_MAP_COUNT_Y;

    //TODO(casey): Begin using tile side in meters
    World.TileSideInMeters = 1.4f;
    World.TileSideInPixels = 60;

    World.UpperLeftX = -(f32)World.TileSideInPixels/2;
    World.UpperLeftY = 0;

    f32 PlayerWidth  = 0.75 * World.TileSideInPixels;
    f32 PlayerHeight = (f32)World.TileSideInPixels;

    World.TileMaps = (tile_map *)TileMaps;

    game_state *GameState = (game_state *)Memory->PermanentStorage;

    if (!Memory->IsInitialised)
    {
        GameState->PlayerX = 150;
        GameState->PlayerY = 150;
        // TODO(casey): This may be more appropriate to do in the platform layer
        Memory->IsInitialised = true;
    }

    tile_map *TileMap = GetTileMap(&World, GameState->PlayerTileMapX, GameState->PlayerTileMapY);

    
    

    for (int ControllerIndex = 0; 
        ControllerIndex < ArrayCount(Input->Controllers); 
        ++ControllerIndex)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        if (Controller->IsAnalog)
        {
            // NOTE(casey): Use analog movement tuning
        }
        else
        {
            // NOTE(casey): Use digital movement tuning
            f32 dPlayerX = 0.0f;
            f32 dPlayerY = 0.0f;

            if (Controller->MoveRight.EndedDown)    { dPlayerX = 1.0f;  }
            if (Controller->MoveLeft.EndedDown)     { dPlayerX = -1.0f; }
            if (Controller->MoveDown.EndedDown)     { dPlayerY = 1.0f;  }
            if (Controller->MoveUp.EndedDown)       { dPlayerY = -1.0f; }
            
            dPlayerX *= 64.0f *2.0f;
            dPlayerY *= 64.0f *2.0f;

            f32 NewPlayerX = GameState->PlayerX + Input->dtForFrame * dPlayerX;
            f32 NewPlayerY = GameState->PlayerY + Input->dtForFrame * dPlayerY;
            
            raw_position PlayerPos = {
                GameState->PlayerTileMapX, GameState->PlayerTileMapY,
                NewPlayerX, NewPlayerY
            };
            raw_position PlayerLeft = PlayerPos;
            PlayerLeft.X -= 0.5*PlayerWidth;
            raw_position PlayerRight = PlayerPos;
            PlayerRight.X += 0.5*PlayerWidth;

            if (IsWorldPointEmpty(&World, PlayerPos)     &&
                IsWorldPointEmpty(&World, PlayerLeft)    &&
                IsWorldPointEmpty(&World, PlayerRight))
            {
                canonical_position CanPos = GetCanonicalPosition(&World, PlayerPos);
                GameState->PlayerTileMapX = CanPos.TileMapX;
                GameState->PlayerTileMapY = CanPos.TileMapY;

                GameState->PlayerX = World.UpperLeftX + World.TileSideInPixels*CanPos.TileX + CanPos.TileRelX;
                GameState->PlayerY = World.UpperLeftY + World.TileSideInPixels*CanPos.TileY + CanPos.TileRelY;
            }
        }

    }





    DrawRectangle(Buffer, 0.0f, 0.0f, (f32)Buffer->Width, (f32)Buffer->Height, 
                  1.0f, 0.0f, 0.1f);
    for (int Row = 0;
             Row < TILE_MAP_COUNT_Y;
             ++Row)
    {
        for (int Column = 0;
                 Column < TILE_MAP_COUNT_X;
                 ++Column)
        {
            u32 TileID = GetTileValueUnchecked(&World, TileMap, Column, Row);
            f32 Gray = 0.5f;
            if (TileID == 1)
            {
                Gray = 1.0f;
            }

            f32 MinX = World.UpperLeftX + ((f32)Column) * World.TileSideInPixels;
            f32 MinY = World.UpperLeftY + ((f32)Row) * World.TileSideInPixels;
            f32 MaxX = MinX + World.TileSideInPixels;
            f32 MaxY = MinY + World.TileSideInPixels;

            DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray);
        }
    }
    f32 PlayerR = 1.0f;
    f32 PlayerG = 1.0f;
    f32 PlayerB = 0.0f;
    
    // (PlayerX, PlayerY) is bottom-middle to point to the player's feet
    f32 PlayerLeft = GameState->PlayerX - (0.5f * PlayerWidth);
    f32 PlayerTop = GameState->PlayerY - PlayerHeight;

    DrawRectangle(Buffer, PlayerLeft, PlayerTop, 
                  PlayerLeft + PlayerWidth, 
                  PlayerTop + PlayerHeight, 
                  PlayerR, PlayerG, PlayerB);

}
#if defined __cplusplus
extern "C"
#endif
GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, 400);
}