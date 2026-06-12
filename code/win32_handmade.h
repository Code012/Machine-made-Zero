/*  date = November 16th 2025 04:44 PM */ 

#if !defined(WIN32_HANDMADE_H)

struct win32_offscreen_buffer
{
    // Note(casey): Pixels are always 32-bite wide,
    // Memory Order  0x BB GG RR xx
    // Little Endian 0x xx RR GG BB

    BITMAPINFO Info;
    void*      Memory;
    int        Width;
    int        Height;
    int        Pitch;
    int        BytesPerPixel;
};
struct win32_window_dimension
{
    int Width;
    int Height;
};
struct win32_sound_output
{
    int SamplesPerSecond;
    int BytesPerSample;
    DWORD SecondaryBufferSize;
    u32 RunningSampleIndex;
    DWORD SafetyBytes;
    // TODO(casey): Math gets simpler if we add a "bytes per second" field
};

struct win32_debug_time_marker
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation; 
    DWORD OutputByteCount;
    DWORD ExpectedFlipPlayCursor;

    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
};


struct win32_game_code
{
    HMODULE GameCodeDLL;
    FILETIME DLLLastWriteTime;

    // Note(sb): Function pointers to exported functions from handmade.dll
    // IMPORTANT(casey): Either of the callbacks can be 0!
    // You must check before calling
    game_update_and_render *UpdateAndRender;
    game_get_sound_samples *GetSoundSamples;

    b32 isValid;
};

struct win32_recorded_input
{
    int InputCount;
    game_input *InputStream;
};

#define WIN32_STATE_FILENAME_COUNT MAX_PATH
struct win32_replay_buffer
{
    HANDLE FileHandle;
    HANDLE MemoryMap;
    char Filename[WIN32_STATE_FILENAME_COUNT];
    
    // Pointer to a memory-mapped file that stores a complete
    // snapshot of the game's memory (permanent + transient).
    //
    // During recording:
    //     GameMemory -> MemoryBlock
    //
    // During playback:
    //     MemoryBlock -> GameMemory
    //
    // This is NOT the recorded input stream. The input stream
    // is stored separately in loop_edit_X_input.hmi.
    void *MemoryBlock;                      // data for file
} ;

struct win32_state
{
    u64 TotalSize;
    void *GameMemoryBlock;
    win32_replay_buffer ReplayBuffers[4];

    HANDLE RecordingHandle;
    int InputRecordingIndex;

    HANDLE PlaybackHandle;
    int InputPlayingIndex;

    char EXEFilename[WIN32_STATE_FILENAME_COUNT];
    char *OnePastLastEXEFilenameSlash;
};

#define WIN32_HANDMADE_H
#endif