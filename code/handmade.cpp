/*  date = November 15th 2025 07:21 PM */

#include "handmade.h"

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

inline s32
RoundReal32ToInt32(f32 Number)
{
    s32 Result = (s32)(Number + 0.5f);
    return (Result);
}
inline u32
RoundReal32ToUInt32(f32 Number)
{
    u32 Result = (u32)(Number + 0.5f);
    return (Result);
}
inline s32
TruncateReal32ToInt32(f32 Number)
{
    s32 Result = (s32)Number;
    return (Result);
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
        TileMap = &World->TileMaps[TileMapY * World->TileMapCountX + TileMapY];
    }

    return (TileMap);
}

inline u32 
GetTileValueUnchecked(tile_map *TileMap, s32 TileX, s32 TileY)
{  
    u32 TileMapValue = TileMap->Tiles[TileY * TileMap->CountX + TileX]; // RowIndex * Width of row + Column Index
    return (TileMapValue);
}
internal b32
IsTileMapPointEmpty(tile_map *TileMap, f32 TestX, f32 TestY)
{
    // (player position relative to tilemap origin) / tile width or height to get tile index
    s32 TileX = TruncateReal32ToInt32((TestX - TileMap->UpperLeftX) / TileMap->TileWidth);  
    s32 TileY = TruncateReal32ToInt32((TestY - TileMap->UpperLeftY) / TileMap->TileHeight);

    b32 Empty = false;
    if ((TileX >= 0) && (TileX < TileMap->CountX) &&   // player within bounds
        (TileY >= 0) && (TileY < TileMap->CountY))
    {
        u32 TileMapValue = GetTileValueUnchecked(TileMap, TileX, TileY);
        Empty = (TileMapValue == 0);  // player not on wall
    }

    return (Empty);
}
internal b32 
IsWorldPointEmpty(world *World, s32 TileMapX, s32 TileMapY, f32 TestX, f32 TestY)
{
    b32 Empty = false;

    tile_map *TileMap = GetTileMap(World, TileMapX, TileMapY);
    if (TileMap)
    {
        Empty = IsTileMapPointEmpty(TileMap, TestX, TestY);
    }

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
    TileMaps[0][0].CountX = TILE_MAP_COUNT_X;
    TileMaps[0][0].CountY = TILE_MAP_COUNT_Y;

    TileMaps[0][0].UpperLeftX = -30;
    TileMaps[0][0].UpperLeftY = 0;
    TileMaps[0][0].TileWidth  = 60;
    TileMaps[0][0].TileHeight = 60;
    
    TileMaps[0][0].Tiles = (u32 *)Tiles00;

    TileMaps[0][1] = TileMaps[0][0];
    TileMaps[0][1].Tiles = (u32 *)Tiles01;

    TileMaps[1][0] = TileMaps[0][0];
    TileMaps[1][0].Tiles = (u32 *)Tiles10;

    TileMaps[1][1] = TileMaps[0][0];
    TileMaps[1][1].Tiles = (u32 *)Tiles11;

    tile_map *TileMap = &TileMaps[0][0];
    world World;
    World.TileMapCountX = 2;
    World.TileMapCountY = 2;
    World.TileMaps = (tile_map *)TileMaps;

    f32 PlayerWidth  = 0.75 * TileMap->TileWidth;
    f32 PlayerHeight = TileMap->TileHeight;

    game_state *GameState = (game_state *)Memory->PermanentStorage;

    if (!Memory->IsInitialised)
    {
        GameState->PlayerX = 150;
        GameState->PlayerY = 150;
        // TODO(casey): This may be more appropriate to do in the platform layer
        Memory->IsInitialised = true;
    }

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
            
            dPlayerX *= 64.0f;
            dPlayerY *= 64.0f;

            f32 NewPlayerX = GameState->PlayerX + Input->dtForFrame * dPlayerX;
            f32 NewPlayerY = GameState->PlayerY + Input->dtForFrame * dPlayerY;
            

            if (IsTileMapPointEmpty(TileMap, NewPlayerX, NewPlayerY) &&
                IsTileMapPointEmpty(TileMap, NewPlayerX - 0.5f * PlayerWidth, NewPlayerY) &&
                IsTileMapPointEmpty(TileMap, NewPlayerX + 0.5f * PlayerWidth, NewPlayerY))
            {
                GameState->PlayerX = NewPlayerX;
                GameState->PlayerY = NewPlayerY;
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
            u32 TileID = GetTileValueUnchecked(TileMap, Column, Row);
            f32 Gray = 0.5f;
            if (TileID == 1)
            {
                Gray = 1.0f;
            }

            f32 MinX = TileMap->UpperLeftX + ((f32)Column) * TileMap->TileWidth;
            f32 MinY = TileMap->UpperLeftY + ((f32)Row) * TileMap->TileHeight;
            f32 MaxX = MinX + TileMap->TileWidth;
            f32 MaxY = MinY + TileMap->TileHeight;

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