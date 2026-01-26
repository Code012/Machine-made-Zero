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

internal void
GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz)
{
	local_persist f32 tSine;
	s16 ToneVolume = 3000;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

	s16 *SampleOut = SoundBuffer->Samples;
    for (int SampleIndex = 0;
         SampleIndex < SoundBuffer->SampleCount;
         ++SampleIndex)
    {
        f32 SineValue = sinf(GameState->tSine);
        s16 SampleValue = (s16)(SineValue * ToneVolume);
        
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
        
        GameState->tSine += (2.0f * Pi32 * 1.0f) / (f32)WavePeriod;
        if (GameState->tSine > 2.0f * Pi32)
        {
            GameState->tSine -= 2.0f * Pi32;
        }
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
        debug_read_file_result FileData = Memory->DEBUGPlatformReadEntireFile(__FILE__); 
        if (FileData.Contents)
        {
            Memory->DEBUGPlatformWriteEntireFile("test.out", FileData.ContentsSize, FileData.Contents);
            Memory->DEBUGPlatformFreeFileMemory(FileData.Contents);
            FileData.Contents = nullptr;
        }

        // GameState->XOffset = 0; 
        // GameState->YOffset = 0;
        GameState->ToneHz = 512;
        GameState->tSine = 0.0f;

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
            GameState->XOffset += (int)(4.0f * Controller->StickAverageX);
            GameState->ToneHz = 256 + (int)(128.0f * (Controller->StickAverageY));
        }
        else
        {
            // NOTE(casey): Use digital movement tuning
            if (Controller->MoveLeft.EndedDown)
            {
                GameState->XOffset -= 1;
            }

            if (Controller->MoveRight.EndedDown)
            {
                GameState->XOffset += 1;
            }

            if (Controller->MoveUp.EndedDown)
            {
                GameState->ToneHz += 10;
            }

            if (Controller->MoveDown.EndedDown)
            {
                GameState->ToneHz -= 10;
            }
        }

        if (Controller->ActionDown.EndedDown)
        {
            GameState->YOffset += 1;
        }
    }
	RenderWeirdGradient(Buffer, GameState->XOffset, GameState->YOffset);
}
#if defined __cplusplus
extern "C"
#endif
GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, GameState->ToneHz);
}