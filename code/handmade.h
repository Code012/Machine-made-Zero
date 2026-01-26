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


// TODO(casey): Implement sine ourselves
#include <math.h>
#include <stdint.h>

////////////////////////////////
// Defines
#define internal        static 
#define local_persist   static
#define global_variable static

#define Pi32            3.14159265359f

////////////////////////////////
// Base Types

typedef uint8_t  u8; 
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8; 
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef s32     b32; 
typedef float   f32;
typedef double  f64;


#if HANDMADE_SLOW
# define Assert(Expression) if (!(Expression)) { *(int *)0 = 0; } 
#else
# define Assert(Expression)
#endif

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
inline u32
SafeTruncateUInt64(u64 Value)
{
    Assert(Value <= 0xFFFFFFFF);
    u32 Result = (u32)Value;
    return Result;
}

// TODO(casey): Services that the platform layer provides to the game.
#if HANDMADE_INTERNAL
struct debug_read_file_result
{
    u32 ContentsSize;
    void *Contents;
};
#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name (void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name (char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) b32 name (char *Filename, u32 MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif

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
    b32 IsConnected;
    b32 IsAnalog;

    f32 StickAverageX;
    f32 StickAverageY;

    union 
    {
        game_button_state Buttons[12];
        
        struct 
        {
            game_button_state MoveUp;
            game_button_state MoveDown;
            game_button_state MoveLeft;
            game_button_state MoveRight;

            game_button_state ActionUp;     // 
            game_button_state ActionDown;   // 
            game_button_state ActionLeft;   // 
            game_button_state ActionRight;  // 

            game_button_state LeftShoulder;
            game_button_state RightShoulder;
            game_button_state Back;
            game_button_state Start;
            // Note(casey): All buttons should be added above this line

            game_button_state Terminator;

        };    
    };

};

// stores multiple controllers
struct game_input
{
    game_controller_input Controllers[5];
};

inline game_controller_input *GetController(game_input *Input, int ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    game_controller_input *Result = &Input->Controllers[ControllerIndex];
    return (Result);
}

struct game_memory
{
    u64 PermanentStorageSize;
    void *PermanentStorage; // NOTE(casey): REQUIRED to be cleared to zero at startup

    u64 TransientStorageSize;
    void *TransientStorage;

    b32 IsInitialised;

    debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
    debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
    debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;

};


// NOTE(casey): Services that the game provides to the platform layer
#define GAME_UPDATE_AND_RENDER(name) void name(game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{
}
// NOTE(casey): At the moment, this has to be a very fast function, it cannot be
// more than a millisecond or so.
// TODO(casey): Reduce the pressure on this function's performance by measuring it
// or asking about it, etc.
#define GAME_GET_SOUND_SAMPLES(name) void name(game_memory *Memory, game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub)
{
}


struct game_state
{
    int ToneHz;
    int XOffset;
    int YOffset;

    f32 tSine;
};

#define HANDMADE_H
#endif