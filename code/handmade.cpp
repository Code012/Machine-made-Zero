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

internal s32
RoundReal32ToInt32(f32 Number)
{
    s32 Result = (s32)(Number + 0.5f);
    return (Result);
}
internal void
DrawRectangle(game_offscreen_buffer* Buffer, 
                f32 RealMinX, f32 RealMinY, f32 RealMaxX, f32 RealMaxY,
                u32 Color)
{
    // Round up float Min/Max positions to an integer
    // Draw our rectangle from Min(x, y) up to and excluding Max(x, y)

    s32 MinX = RoundReal32ToInt32(RealMinX); 
    s32 MinY = RoundReal32ToInt32(RealMinY); 
    s32 MaxX = RoundReal32ToInt32(RealMaxX); 
    s32 MaxY = RoundReal32ToInt32(RealMaxX);
    // clipping
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

#if defined __cplusplus
extern "C"
#endif
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons))); // so we dont forget to update Buttons length when adding or removing new buttons
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state *GameState = (game_state *)Memory->PermanentStorage;

    if (!Memory->IsInitialised)
    {
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
        }

    }

    DrawRectangle(Buffer, 0.0f, 0.0f, (f32)Buffer->Width, (f32)Buffer->Height, 0x00FF00FF);
    DrawRectangle(Buffer, 10.0f, 10.0f, 30.0f, 30.0f, 0x0000FFFF);
}
#if defined __cplusplus
extern "C"
#endif
GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, 400);
}