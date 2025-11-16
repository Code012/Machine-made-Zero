/*  date = November 15th 2025 07:17 PM */ 

#if !defined(HANDMADE_H)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

struct game_offscreen_buffer
{
    void      *Memory;
    int        Width;
    int        Height;
    int        Pitch;
};

struct game_sound_output_buffer // sound is tricky to understand, i gave up understanding it
{
	int SamplesPerSecond;
	int SampleCount;
	s16* Samples;
};

// stores the information on each individual button
struct game_button_state
{
    s32 HalfTransitionCount;
    b32 EndedDown;
};

// stores all the information on each controller that we care about
struct game_controller_input
{
    b32 IsAnalog;

    f32 StartX;
    f32 StartY;

    f32 MinX;
    f32 MinY;

    f32 MaxX;
    f32 MaxY;

    f32 EndX;
    f32 EndY;

    union 
    {
        game_button_state Buttons[6];
        
        struct 
        {
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
        };    
    };

};

// stores multiple controllers
struct game_input
{
    game_controller_input Controllers[4];
};

// TODO(casey): Services that the platform layer provides to the game.

// NOTE(casey): Services that the game provides to the platform layer
internal void GameUpdateAndRender(game_input *Input, game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer);

#define HANDMADE_H
#endif