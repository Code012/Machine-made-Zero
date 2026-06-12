/*
    TODO(casey): THIS IS NOT A FINAL PLATFORM LAYER!!!

    - Saved game locations
    - Getting a handle to our own exe file
    - Asset loading path
    - Multithreading (launch a thread)
    - Raw Input ( support for multiple keyboards)
    - Sleep/timeBeginPeriod
    - ClipCursor() (for multi-monitor support)
    - Fullscreen support
    - QueryCancelAutoplay
    - WM_SETCURSOR (control cursor visibility)
    - WM_ACTIVATEAPP (for when we are not the active applcations)
    - Blit speed improvements (BitBlt)
    - Hardware acceleration (OpenGL or Direct3D or both??)
    - GetKeyboard Layout (for french keyboards, international WASD support)
    


    Replay System

    A replay consists of:

        1) A full snapshot of game memory.
        2) A stream of recorded game_input structures.

    The memory snapshot is stored in a memory-mapped file
    (ReplayBuffer->MemoryBlock).

    The input stream is stored separately in:

        loop_edit_X_input.hmi

    Recording:
        Snapshot current memory.
        Record inputs every frame.

    Playback:
        Restore snapshot.
        Feed recorded inputs back into the game.

    Because the game starts from the same memory state and
    receives the same inputs, execution should be deterministic.

*/
#include "handmade.h"

#include <windows.h>
#include <stdio.h>
#include <Xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

#define KeyMessageWasDownBit (1 << 30)
#define KeyMessageIsDownBit  (1 << 31)

////////////////////////////////
// Globals
global_variable bool                   GlobalRunning;
global_variable b32                    GlobalPause;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable IDirectSoundBuffer    *GlobalSecondaryBuffer;
global_variable s64 GlobalPerfCountFrequency;

// NOTE(me):
// 1. Defined a function prototype
// 2. Defined a function type from the prototype, so it can be used as a pointer
// 3. Created a stub function to use in case we fail to load the actual functions
// 4. Set up a permanent reference for the function of that type in a global variable. By default it points to the stub function
// 5. Defined the same name as the original function for that variable
// NOTE(casey): XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(casey): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_
// NOTE(yakvi): DirectSoundCreate
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS8, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);


internal void 
CatStrings(size_t SourceACount, char *SourceA,
           size_t SourceBCount, char *SourceB,
           size_t DestCount, char *Dest)
{
    for (int Index = 0;
         Index < SourceACount;
         ++Index)
    {
        *Dest++ = *SourceA++;
    }

    for (int Index = 0;
         Index < SourceBCount;
         ++Index)
    {
        *Dest++ = *SourceB++;
    }
    *Dest++ = 0;
}

internal int 
StringLength(char *String)
{
    int Count = 0;
    while (*String++)
    {
        ++Count;
    }
    return Count;
}

internal void 
Win32GetEXEFilename(win32_state *State)
{
    DWORD SizeOfFilename = GetModuleFileName(0, State->EXEFilename, sizeof(State->EXEFilename));
    State->OnePastLastEXEFilenameSlash = State->EXEFilename;
    for (char *Scan = State->EXEFilename;
         *Scan;
          ++Scan)
    {
        if (*Scan == '\\')
        {
            State->OnePastLastEXEFilenameSlash = Scan + 1;
        }
    }
}

internal void 
Win32BuildEXEPathFilename(win32_state *State, char *Filename, 
                            int DestCount, char *Dest)
{
    CatStrings(State->OnePastLastEXEFilenameSlash - State->EXEFilename, State->EXEFilename,
                StringLength(Filename), Filename, DestCount, Dest);
}


DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if (Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}
DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_read_file_result Result = {};

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if (GetFileSizeEx(FileHandle, &FileSize))
        {
            // FileSize.QuadPart is the 64-bit value of the size.
            u32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE); // fine for debug, not for prod
            if (Result.Contents)
            {
                DWORD BytesRead;    // note it only reads 32 bit values (4GB), if we need to read files larger, multiple reads will be needed
                if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
                    (FileSize32 == BytesRead))
                {
                    // NOTE(casey): File read successfully
                    Result.ContentsSize = BytesRead;
                }
                else
                {
                    // Error: Read failed
                    DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
                    Result.Contents = nullptr;
                    // TODO(casey): Logging
                }
            }
            else
            {
                // Error: Memory allocatino failed
                // TODO(casey): Logging
            }
        }
        else
        {
            // Error: File size evaluation failed
            // TODO(casey): Logging
        }
        CloseHandle(FileHandle);
    }
    else
    {
        // Error: handle creation failed
        // TODO(casey): Logging
    }

    return Result;
}
DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)  // blocking write, not for final game
{
    b32 Result = false;
    
    HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if(WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
        {
            // NOTE(casey): File written successfully
            Result = (BytesWritten == MemorySize);
        }
        else
        {
            // Error: Write failed
            // TODO(casey): Logging
        }
        CloseHandle(FileHandle);
    }
    else
    {
        // Error: Handle creation failed
        // TODO(casey): Logging
    }
    
    return(Result);
}
////////////////////////////////
// Helpers

internal FILETIME
Win32GetLastWriteTime(char *Filename)
{
    FILETIME LastWriteTime = {};

    WIN32_FILE_ATTRIBUTE_DATA Data;
    if (GetFileAttributesExA(Filename, GetFileExInfoStandard, &Data))
    {
        LastWriteTime = Data.ftLastWriteTime;
    }

    return LastWriteTime;
}

internal win32_game_code 
Win32LoadGameCode(char *SourceDLLName, char *TempDLLName)
{
    win32_game_code Result = {};

    // TODO(casey): Need to get the proper path here!
    // TODO(casey): Automatic determination of when updates are necessary.


    Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);

    // Sleep(30);
    CopyFile(SourceDLLName, TempDLLName, FALSE);
    Result.GameCodeDLL = LoadLibraryA(TempDLLName); 
    if (Result.GameCodeDLL)
    {
        Result.UpdateAndRender = (game_update_and_render *)GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
        Result.GetSoundSamples = (game_get_sound_samples *)GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");

        Result.isValid = (Result.UpdateAndRender && Result.GetSoundSamples);
    }

    if (!Result.isValid)
    {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
    }

    return (Result);
}

internal void
Win32UnloadGameCode(win32_game_code *GameCode)
{
    if (GameCode->GameCodeDLL) 
    {
        FreeLibrary(GameCode->GameCodeDLL);
        GameCode->GameCodeDLL = 0;
    }

    GameCode->isValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
}

internal void
Win32LoadXInput()
{
    // NOTE(me): Loads Xinput dll, and degrades version until one is founc that matches system. If none is found, we just point to our stub function to avoid crashes
    HMODULE XInputLibrary = LoadLibraryA("Xinput1_4.dll");
    if (!XInputLibrary) {
        XInputLibrary = LoadLibraryA("Xinput1_3.dll");
    }
    if (!XInputLibrary) {
        XInputLibrary = LoadLibraryA("Xinput9_1_0.dll");
    }

    if (XInputLibrary) {
        XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
        if (!XInputGetState) {
            XInputGetState = XInputGetStateStub;
        }
        XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
        if (!XInputSetState) {
            XInputSetState = XInputSetStateStub;
        }
    } else {
        // We still don't have any XInputLibrary
        XInputGetState = XInputGetStateStub;
        XInputSetState = XInputSetStateStub;
        // TODO(casey): Diagnostic
    }
}

internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                game_button_state *OldState, DWORD ButtonBit,
                                game_button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;   // only 1 or 0, becuase polling api. we get entire snapshot of controller state once per frame
}
internal void 
Win32ProcessKeyboardMessage(game_button_state *NewState, b32 IsDown)
{
    if (NewState->EndedDown != IsDown)          // only update state on key change (down->up, up->down)
    {
        NewState->EndedDown = IsDown;
        ++NewState->HalfTransitionCount;        // increment because keyboard input is event-based, key can be pressed many times in one frame
    }
}
internal f32 
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
    f32 Result = 0;
    
    if (Value < -DeadZoneThreshold)
    {
        Result = (f32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
    }
    else if(Value > DeadZoneThreshold)
    {
        Result = (f32)((Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));
    }

    return (Result);
}

internal void 
Win32InitDSound(HWND Window, s32 SamplesPerSecond, s32 BufferSize)
{
    // NOTE(casey): Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if (DSoundLibrary)
    {
        // NOTE(casey): Create a DirectSound object
        direct_sound_create *DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        IDirectSound *DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8; // 4 bytes per frame under current settings left and right 2 bytes (stereo, 16bit)
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign; // bytes/sec


            // NOTE(casey): set cooperative level
            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                // NOTE(casey): "Create" a primary buffer
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                IDirectSoundBuffer *PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                   if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) 
                    {
                        // NOTE(casey): We have finally set hte format of the primary buffer!
                        OutputDebugStringA("Primary buffer format was set. \n");

                    }

                }

            }
        
            // NOTE(casey): "Create" a secondary buffer
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;

            
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
            {
                // NOTE(casey): All good, secondary buffer works as intended
                OutputDebugStringA("Secondary buffer create Successfully.\n");

            }


        }
    }
}


internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width  = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return (Result);
}



// serves to resize or initialise if it doesn't exist a Device Independent
// Bitmap which is the name Windwos gies to the bitmaps it can display with GDI
internal void
Win32ResizeDIBSection(win32_offscreen_buffer* Buffer, int Width, int Height)
{
    // TODO(casey): Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.
    if (Buffer->Memory) {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
        // Optionally, you can check if the result of VirtualFree is not zero.
        // Print out an error message if it is.
    }

    Buffer->Width     = Width;
    Buffer->Height    = Height;
    WORD BytesPerPixel = 4; // 3 for rgb, 1 to ensure alignment
    Buffer->BytesPerPixel = BytesPerPixel;
    Buffer->Pitch     = Buffer->Width * BytesPerPixel;

    // Note(casey): When the biHeight field is negative, this is the clue
    // to Windows to treat this bitmap as top-down, not bottom-up, meaning
    // that the first bytes of the image are the color for the top left
    // pixel in the bitmap, not the bottom left!
    Buffer->Info.bmiHeader.biSize  = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight =
      -Buffer->Height; // negative value: top-down pitch/stride
    Buffer->Info.bmiHeader.biPlanes      = 1;
    Buffer->Info.bmiHeader.biBitCount    = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = BytesPerPixel * (Buffer->Width * Buffer->Height);
    Buffer->Memory       = VirtualAlloc(
      0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer* Buffer,
                           HDC                     DeviceContext,
                           int                     WindowWidth,
                           int                     WindowHeight)
{
    // TODO(casey): Aspect ratio correction
    // NOTE(casey): For prototyping purposes, we're going to always blit
    // 1-1 pixels to make sure we don't introduce artifacts with 
    // stretching while we are learning to code the renderer!
    StretchDIBits(DeviceContext,
                  0, 0, Buffer->Width, Buffer->Height,      // destination rectangle (window)
                  0, 0, Buffer->Width, Buffer->Height,  // source rectangle (bitmap buffer)
                  Buffer->Memory,
                  &Buffer->Info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

internal void
Win32ClearBuffer(win32_sound_output *SoundOutput)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
                                             &Region1, &Region1Size,
                                             &Region2, &Region2Size,
                                             0)))
    {
        u8 *DestSample = (u8 *)Region1;
        for (DWORD ByteIndex = 0;
             ByteIndex < Region1Size;
             ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        DestSample = (u8 *)Region2;
        for (DWORD ByteIndex = 0;
             ByteIndex < Region2Size;
             ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite,
                     game_sound_output_buffer *SourceBuffer)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                                             &Region1, &Region1Size,
                                             &Region2, &Region2Size,
                                             0)))
    {
        // TODO(casey): assert that Region1Size/Region2Size are valid
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        s16 *SourceSample = SourceBuffer->Samples;
        s16 *DestSample = (s16 *)Region1;
        for (DWORD SampleIndex = 0;
             SampleIndex < Region1SampleCount;
             ++SampleIndex)
        {
            
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            
            ++SoundOutput->RunningSampleIndex;
        }
        
        DestSample = (s16 *)Region2;
        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        for (DWORD SampleIndex = 0;
             SampleIndex < Region2SampleCount;
             ++SampleIndex)
        {
            
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            
            ++SoundOutput->RunningSampleIndex;
        }
        
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32GetInputFileLocation(win32_state *State, b32 InputStream,
                          int SlotIndex, int DestCount, char *Dest)
{
    char Name[64];
    wsprintf(Name, "loop_edit_%d_%s.hmi", SlotIndex,
                InputStream ? "input": "state");            // is file for input stream or for game state
    Win32BuildEXEPathFilename(State, Name, DestCount, Dest);
}

internal win32_replay_buffer *
Win32GetReplayBuffer(win32_state *State, int unsigned Index)
{
    Assert(Index < ArrayCount(State->ReplayBuffers));
    win32_replay_buffer *Result = &State->ReplayBuffers[Index];

    return (Result);
}

internal void 
Win32BeginRecordingInput(win32_state *State, int InputRecordingIndex)
{
    // Begin recording a deterministic replay.
    //
    // We save:
    //   1) A snapshot of current game memory.
    //   2) All future frame inputs.
    //
    // The snapshot allows playback to restart from the exact
    // same state the recording began from.
    
    win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputRecordingIndex);
    if (ReplayBuffer->MemoryBlock)
    {
        State->InputRecordingIndex = InputRecordingIndex;
        // Open the input stream file.
        //
        // This file contains ONLY game_input structures,
        // one per frame.
        char Filename[WIN32_STATE_FILENAME_COUNT];
        Win32GetInputFileLocation(State, true, InputRecordingIndex, sizeof(Filename), Filename);
        State->RecordingHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

#if 0   
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = State->TotalSize;
        SetFilePointerEx(State->RecordingHandle, FilePosition, 0, FILE_BEGIN);
#endif
        // Save a complete snapshot of current game memory
        // into the replay slot.
        //
        // ReplayMemory <- GameMemory
        CopyMemory(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
    }
}

internal void 
Win32EndRecordingInput(win32_state *State)
{
    // Stop recording new inputs and close the replay file.
    //
    // The saved memory snapshot remains available in the
    // replay slot's memory-mapped state file.
    CloseHandle(State->RecordingHandle);
    State->InputRecordingIndex = 0;
}

internal void 
Win32RecordInput(win32_state *State, game_input *Input)
{
    // Append one frame of user input to the replay file.
    //
    // The file becomes:
    //
    // Frame 0 input
    // Frame 1 input
    // Frame 2 input
    // ...
    //
    // No game state is written here.
    DWORD BytesWritten;
    WriteFile(State->RecordingHandle, Input, sizeof(*Input), &BytesWritten, 0);
}

internal void 
Win32BeginInputPlayback(win32_state *State, int InputPlayingIndex)
{
    /// Begin replay playback.
    //
    // We:
    //   1) Start reading recorded inputs from disk.
    //   2) Restore the saved memory snapshot.
    //
    // This guarantees deterministic execution.

    win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputPlayingIndex);
    if (ReplayBuffer->MemoryBlock)
    {
        State->InputPlayingIndex = InputPlayingIndex;
        char Filename[WIN32_STATE_FILENAME_COUNT];
        Win32GetInputFileLocation(State, true, InputPlayingIndex, sizeof(Filename), Filename);
        State->PlaybackHandle = CreateFileA(Filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

#if 0
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = State->TotalSize;
        SetFilePointerEx(State->PlaybackHandle, FilePosition, 0, FILE_BEGIN);
#endif
        // Restore the exact memory state that existed
        // when recording began.
        //
        // GameMemory <- ReplayMemory
        CopyMemory(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
    }
}

internal void 
Win32EndInputPlayback(win32_state *State)
{
    // Close the file we were reading from
    CloseHandle(State->PlaybackHandle);
    State->InputPlayingIndex = 0;
}

internal void 
Win32PlaybackInput(win32_state *State, game_input *Input)
{
    // Read one frame's worth of recorded input.
    //
    // The game receives this input instead of live keyboard
    // or controller data.

    DWORD BytesRead = 0;
    if (ReadFile(State->PlaybackHandle, Input, sizeof(*Input), &BytesRead, 0))
    {
        if (BytesRead == 0)
        {
            // NOTE(casey): We've hit the end of the stream, go back to the beginning
            int PlayingIndex = State->InputPlayingIndex;
            Win32EndInputPlayback(State);
            Win32BeginInputPlayback(State, PlayingIndex);
            ReadFile(State->PlaybackHandle, Input, sizeof(*Input), &BytesRead, 0);
        }
    }
}

internal void
Win32ProcessPendingMessages(win32_state *State, game_controller_input *KeyboardController)
{
    MSG Message;
    while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) 
    {
        switch(Message.message)
        {
        case WM_QUIT:
            {
                GlobalRunning = false;
            } break;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            // Handle keyboard messaged here
            u32  VKCode  = (u32)Message.wParam;
            bool IsDown  = ((Message.lParam & KeyMessageWasDownBit) == 0);
            bool WasDown = ((Message.lParam & KeyMessageIsDownBit) != 0);
            if (IsDown != WasDown) {

                if (VKCode == 'W') 
                {
                    Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                } 
                else if (VKCode == 'A') 
                {
                    Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                } 
                else if (VKCode == 'S') 
                {
                    Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                } 
                else if (VKCode == 'D') 
                {
                    Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                } 
                else if (VKCode == 'Q') 
                {
                    Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                } 
                else if (VKCode == 'E') 
                {
                    Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                } 
                else if (VKCode == VK_UP) 
                {
                    Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                } 
                else if (VKCode == VK_DOWN) 
                {
                    Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                } 
                else if (VKCode == VK_LEFT) 
                {
                    Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                } 
                else if (VKCode == VK_RIGHT) 
                {
                    Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                } 
                else if (VKCode == VK_ESCAPE) 
                {
                    GlobalRunning = false;
                } 
                else if (VKCode == VK_SPACE) 
                {
                    Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                }
                else if (VKCode == VK_BACK)
                {
                    Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                }

                #if HANDMADE_INTERNAL
                else if (VKCode == 'P')
                {
                    if (IsDown)
                    {
                        GlobalPause = !GlobalPause;
                    }
                }
                else if (VKCode == 'L')
                {
                    if (IsDown)
                    {
                        if (State->InputRecordingIndex == 0)
                        {
                            Win32BeginRecordingInput(State, 1);
                        }
                        else
                        {
                            Win32EndRecordingInput(State);
                            Win32BeginInputPlayback(State, 1);
                        }
                    }
                }
                #endif

                b32 AltKeyWasDown = ((Message.lParam & (1 << 29)) != 0); 
                if ((VKCode == VK_F4) && AltKeyWasDown) 
                {
                    GlobalRunning = false;
                }
            }
        } break;


            // More messages will go here

        default:
            {
                TranslateMessage(&Message); // process message
                DispatchMessageA(&Message); // send to callback
            } break;
        }
    }
}

////////////////////////////////
// Windows

// Event handler, called when something happens to the window
LRESULT CALLBACK
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) 
{

    // LParam is signed, WParam isn't. (pointers)
    // WParam represents the VKCode
    // LParam is extra info about how the it was pressed depending on the VKCode
    LRESULT Result = 0;
    switch (Message) {

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: 
            {
                Assert(!"Keyboard input came in through a non-dispatch message!!!");
            } break;

            // When the user deletes our window
        case WM_DESTROY: 
            {
            GlobalRunning = false;
            OutputDebugStringA("WM_Destroy\n");
            } break;

            // When the user clicks on the little X in the top-right corner
        case WM_CLOSE: 
            {
            GlobalRunning = false;
            PostQuitMessage(0);
            OutputDebugStringA("WM_CLOSE\n");
            } break;

            // When the user clicked on the window and it became active
        case WM_ACTIVATEAPP: 
            {
                #if 0
                if (WParam == TRUE) // WParam is TRUE if the window is currently focused and FALSE if it's not
                {
                    SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
                }
                else
                {
                    SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 64, LWA_ALPHA);
                }
                #endif
                OutputDebugStringA("WM_ACTIVATEAPP\n");
            } break;

        case WM_PAINT: 
            {
            PAINTSTRUCT            Paint         = {};
            HDC                    DeviceContext = BeginPaint(Window, &Paint);
            win32_window_dimension Dimension     = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(&GlobalBackBuffer,
                                       DeviceContext,
                                       Dimension.Width,
                                       Dimension.Height);
            EndPaint(Window, &Paint);
            } break;

            // When the user changes the size of the window
        case WM_SIZE: 
            {
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32ResizeDIBSection(
              &GlobalBackBuffer, Dimension.Width, Dimension.Height);
            } break;

        default: 
            {
            // Do something in case of any other message
            Result = DefWindowProc(Window, Message, WParam, LParam);
            } break;
    }

    return (Result);
}

inline LARGE_INTEGER
Win32GetWallClock()
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return (Result);
}

inline f32 
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    f32 Result = (f32)(End.QuadPart - Start.QuadPart) / (f32)GlobalPerfCountFrequency;
    return (Result);
}

internal void
Win32DebugDrawVertical(win32_offscreen_buffer *Backbuffer,
                        int X, int Top, int Bottom, u32 Colour)
{
    if (Top <= 0)
    {
        Top = 0;
    }
    
    if (Bottom > Backbuffer->Height)
    {
        Bottom = Backbuffer->Height;
    }
    
    if ((X >= 0) && (X < Backbuffer->Width))
    {
        u8 *Pixel = (u8 *)Backbuffer->Memory + 
                    (Top * Backbuffer->Pitch) + 
                    (X * Backbuffer->BytesPerPixel);

        for (int Y = Top;
            Y < Bottom;
            ++Y)
        {
            *(u32 *)Pixel = Colour;
            Pixel += Backbuffer->Pitch;
        }
    }
}

inline void
Win32DrawSoundBufferMarker(win32_offscreen_buffer *Backbuffer,
    win32_sound_output *SoundOutput, f32 C, int PadX, int Top, int Bottom,
    DWORD Value, u32 Color)
{
    f32 XReal = C * (f32)Value;
    int X = PadX + (int)XReal;
    Win32DebugDrawVertical(Backbuffer, X, Top, Bottom, Color);
}

#if 0
internal void
Win32DebugSyncDisplay(win32_offscreen_buffer *Backbuffer,
                        int MarkerCount, win32_debug_time_marker *Markers,
                        int CurrentMarkerIndex,
                        win32_sound_output *SoundOutput, f32 TargetSecondsPerFrame)
{
    int PadX = 16;
    int PadY = 16;

    int LineHeight = 64;

    f32 C = (f32)(Backbuffer->Width - 2 * PadX) / (f32)SoundOutput->SecondaryBufferSize;
    for (int MarkerIndex = 0;
            MarkerIndex < MarkerCount;
            ++MarkerIndex)
    {
        win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
        Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputByteCount < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);

        DWORD PlayColor = 0xFFFFFFFF;           // White
        DWORD WriteColor = 0xFFFF0000;          // Red
        DWORD ExpectedFlipColor = 0xFFFFFF00;   // Yellow
        DWORD PlayWindowColor = 0xFFFF00FF;     // Magenta

        int Top = PadY;
        int Bottom = PadY + LineHeight;

        if (MarkerIndex == CurrentMarkerIndex) 
        {
            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, 
                                           ThisMarker->OutputPlayCursor, PlayColor);
            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, 
                                       ThisMarker->OutputWriteCursor, WriteColor);
            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, 
                                       ThisMarker->OutputLocation, PlayColor);
            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom,       
                                       ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);
            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, PadY, Bottom, 
                               ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
        }

        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, 
                           ThisMarker->FlipPlayCursor + (480 * SoundOutput->BytesPerSample), 
                           PlayWindowColor);
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);

        // DWORD ThisPlayCursor = Markers[PlayCursorIndex];
        // Assert(ThisPlayCursor < SoundOutput->SecondaryBufferSize);
        // f32 XReal = C * (f32)ThisPlayCursor;    // C is scale factor to map from sound buffer to display buffer
        // int X = PadX + (int) XReal;
        // Win32DebugDrawVertical(Backbuffer, X, Top, Bottom, 0xFFFFFFFF);
    }
}
#endif

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR     CommandLine,
        int       ShowCode)
{
    win32_state Win32State = {};
    Win32GetEXEFilename(&Win32State);

    char SourceGameCodeDLLFullPath[WIN32_STATE_FILENAME_COUNT];
    Win32BuildEXEPathFilename(&Win32State, "handmade.dll", 
                                sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);

    char TempGameCodeDLLFullPath[WIN32_STATE_FILENAME_COUNT];
    Win32BuildEXEPathFilename(&Win32State, "handmade_temp.dll",
                                sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);

    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;
    // NOTE(casey): Set the Windows scheduler granularity to 1ms
    // so that our Sleep() can be more granular
    UINT DesiredSchedulerMS = 1;
    b32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);
    Win32LoadXInput();
    WNDCLASSA WindowClass = {};
    // TODO(casey): Check if HREDRAW/VREDRAW/OWNDC still matter
    WindowClass.style       = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance   = Instance;
    // WindowClass.hIcon
    WindowClass.lpszClassName = "HandmadeHeroClass";

    
    if (RegisterClassA(&WindowClass)) 
    {
        HWND Window = CreateWindowExA(0, // WS_EX_TOPMOST | WS_EX_LAYERED,
                                      WindowClass.lpszClassName,
                                      "Handmade Hero",
                                      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      0,
                                      0,
                                      Instance,
                                      0);
        if (Window) 
        {
            int MonitorRefreshHz = 60;
            HDC RefreshDC = GetDC(Window);
            int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
            ReleaseDC(Window, RefreshDC);
            if (Win32RefreshRate > 1)
            {
                MonitorRefreshHz = Win32RefreshRate;
            }
            f32 GameUpdateHz (MonitorRefreshHz / 2.0f); // game refresh rate can be a fraction of monitor's refresh rate and still sync properly
            f32 TargetSecondsPerFrame = 1.0f / GameUpdateHz; // for frame timing
            // NOTE(casey): Sound test
            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000; //40kHz
            SoundOutput.BytesPerSample = sizeof(s16) * 2; // 16-bit samples playing in 2 channels (stereo)
            SoundOutput.SecondaryBufferSize = 2 * SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample; // Buffer size of 2 seconds
            SoundOutput.RunningSampleIndex = 0;
            // TODO(casey): Actually compute this variance and see
            // what the lowest reasonable value is
            SoundOutput.SafetyBytes = (int)(((f32)SoundOutput.SamplesPerSecond * (f32)SoundOutput.BytesPerSample)
                        / GameUpdateHz) / 3.0f;

            // Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            s16 *Samples = (s16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            #if HANDMADE_INTERNAL
                LPVOID BaseAddress = (LPVOID)Terabytes(2); // fixed address so easy to find and debug
            #else
                LPVOID BaseAddress = 0;
            #endif

            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes(1);
            GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
            GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
            GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

            // TODO(casey): Handle various memory footprints

            // TODO(casey): Use MEM_LARGE_PAGES and call adjust token
            // privileges when not on Windows XP?
            Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize,
                                                        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
            GameMemory.TransientStorage = ((u8 *)GameMemory.PermanentStorage + 
                                            GameMemory.PermanentStorageSize);
            for (int ReplayIndex = 0;
                ReplayIndex < ArrayCount(Win32State.ReplayBuffers);
                ++ReplayIndex)
            {
                win32_replay_buffer *ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];
                
                // TODO(casey): Recording systme still seems to take too long
                // on record start - find out what Windows is doing and if
                // we can speed up / defer some of that processsing

                // NOTE(sb): Build the filename for this replay slot's state file.
                //
                // Example:
                //     loop_edit_0_state.hmi
                //     loop_edit_1_state.hmi
                //
                // This file will hold a complete snapshot of game memory.
                Win32GetInputFileLocation(&Win32State, false, ReplayIndex,
                                            sizeof(ReplayBuffer->Filename), ReplayBuffer->Filename);
                // memory mapped file for storing snapshot of game memory
                ReplayBuffer->FileHandle = CreateFileA(ReplayBuffer->Filename, GENERIC_READ | GENERIC_WRITE,
                                                        0, 0, CREATE_ALWAYS, 0, 0);
                LARGE_INTEGER MaxSize;
                MaxSize.QuadPart = Win32State.TotalSize;
                ReplayBuffer->MemoryMap = CreateFileMapping(ReplayBuffer->FileHandle, 0, PAGE_READWRITE,
                                                            MaxSize.HighPart, MaxSize.LowPart, 0);
                ReplayBuffer->MemoryBlock = MapViewOfFile(ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS,
                                                            0, 0, Win32State.TotalSize);
                if (ReplayBuffer->MemoryBlock)
                {
                    // all good
                }
                else
                {
                    // TODO(casey): Diagnostic
                }
            }

            if (Samples && 
                GameMemory.PermanentStorage &&
                GameMemory.TransientStorage)
            {

                game_input Input[2] = {};
                game_input* OldInput = &Input[0];
                game_input* NewInput = &Input[1];
                win32_debug_time_marker DebugTimeMarkers[30] = {};
                int DebugTimeMarkersIndex = 0;
                b32 SoundIsValid = false;

                // This isn't at the beginning of the loop because if getting back tot he top of the loop took some time (maybe the process switched or something else happened) we'd miss that time. This approach guarantees to never miss that time, because we have only one single place where we check the clock, and then measure the time on our last run ont he same spot.
                LARGE_INTEGER LastCounter = Win32GetWallClock();        // frame timings
                LARGE_INTEGER FlipWallClock = Win32GetWallClock();      // time since last visual presentation
                u64 LastCycleCount = __rdtsc();
                
                win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, 
                                                         TempGameCodeDLLFullPath);
                u32 LoadCounter = 0;

                GlobalRunning = true;
                while (GlobalRunning) // frame loop 
                { 
                    FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
                    if ( CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0)
                    {
                        Win32UnloadGameCode(&Game);
                        Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, 
                                                 TempGameCodeDLLFullPath);
                        LoadCounter = 0;
                    }

                    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
                    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
                    *NewKeyboardController = {};
                    NewKeyboardController->IsConnected = true;      // keyboard always connected
                    for (int ButtonIndex = 0;
                         ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
                         ++ButtonIndex)
                    {
                        // key should be down until user releases key, only want to reset halftransitioncount of each button state
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }

                    Win32ProcessPendingMessages(&Win32State, NewKeyboardController);
                    if (!GlobalPause)
                    {
                        POINT MouseP;
                        GetCursorPos(&MouseP);
                        ScreenToClient(Window, &MouseP);    // Note(sb): Don't want to use this in a shipped game; for instance, if you move the mouse to anohter monitor in a multi-monitor setup, the mouse coords will be wrong. But since we don't plane to use this code in the actual game, we can ignore it.
                        NewInput->MouseX = MouseP.x;
                        NewInput->MouseY = MouseP.y;
                        NewInput->MouseZ = 0;   // TODO(casey): Support mousewheel?
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0], GetKeyState(VK_LBUTTON) & (1 << 15));    // value returned stored in high bit of short type (16th bit)
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1], GetKeyState(VK_MBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2], GetKeyState(VK_RBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));


                        if (NewKeyboardController->MoveUp.EndedDown)
                        {
                            NewKeyboardController->StickAverageY = 1.0f;
                        }
                        if (NewKeyboardController->MoveDown.EndedDown)
                        {
                            NewKeyboardController->StickAverageY = -1.0f;
                        }
                        if (NewKeyboardController->MoveLeft.EndedDown)
                        {
                            NewKeyboardController->StickAverageX = -1.0f;
                        }
                        if (NewKeyboardController->MoveRight.EndedDown)
                        {
                            NewKeyboardController->StickAverageX = 1.0f;
                        }

                        // TODO(casey): Need to not poll disconneted controllers to avoid xinput frame rate hit on older libraries
                        // TODO(casey): Should we poll this more frequently?
                        DWORD MaxControllerCount = XUSER_MAX_COUNT;
                        if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
                        {
                            MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
                        }

                        for (DWORD ControllerIndex = 0;
                             ControllerIndex < MaxControllerCount;
                             ++ControllerIndex) 
                        {
                            DWORD OurControllerIndex = ControllerIndex + 1;
                            game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
                            game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

                            XINPUT_STATE ControllerState = {};

                            if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                            {
                                // Note(casey): This controller is plugged in
                                NewController->IsConnected = true;
                                NewController->IsAnalog = OldController->IsAnalog;
                                // TODO(casey): See if ControllerState.dwPacketNumber increments too rapidly
                                XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

                                // TODO(casey): This is a square deadzone, check XInput to verify that the deadzone is "round" and show how to do round deadzone processing
                                NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

                                if ((NewController->StickAverageX != 0.0f) ||
                                    (NewController->StickAverageY != 0.0f))
                                {
                                    NewController->IsAnalog = true;
                                }

                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                                {
                                    NewController->StickAverageY = 1.0f;
                                    NewController->IsAnalog = false;
                                }
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                                {
                                    NewController->StickAverageY = -1.0f;
                                    NewController->IsAnalog = false;
                                }
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                                {
                                    NewController->StickAverageX = -1.0f;
                                    NewController->IsAnalog = false;
                                }
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                                {
                                    NewController->StickAverageX = 1.0f;
                                    NewController->IsAnalog = false;
                                }


                                f32 Threshold = 0.5f;

                                Win32ProcessXInputDigitalButton((NewController->StickAverageX > Threshold) ? 1 : 0,
                                                                &OldController->MoveRight, 1,
                                                                &NewController->MoveRight);
                                Win32ProcessXInputDigitalButton((NewController->StickAverageX < -Threshold) ? 1 : 0,
                                                                &OldController->MoveLeft, 1,
                                                                &NewController->MoveLeft);
                                Win32ProcessXInputDigitalButton((NewController->StickAverageY > Threshold) ? 1 : 0,
                                                                &OldController->MoveUp, 1,
                                                                &NewController->MoveUp);
                                Win32ProcessXInputDigitalButton((NewController->StickAverageY < -Threshold) ? 1 : 0,
                                                                &OldController->MoveDown, 1,
                                                                &NewController->MoveDown);


                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionDown, XINPUT_GAMEPAD_A,
                                                                &NewController->ActionDown);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionRight,XINPUT_GAMEPAD_B,
                                                                &NewController->ActionRight);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionLeft,XINPUT_GAMEPAD_X,
                                                                &NewController->ActionLeft);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionUp,XINPUT_GAMEPAD_Y,
                                                                &NewController->ActionUp);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->LeftShoulder,XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                                &NewController->LeftShoulder);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->RightShoulder,XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                                &NewController->RightShoulder);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->Start,XINPUT_GAMEPAD_START,
                                                                &NewController->Start);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->Back,XINPUT_GAMEPAD_BACK,
                                                                &NewController->Back);

                                // bool Start         = Pad->wButtons & XINPUT_GAMEPAD_START;
                                // bool Back          = Pad->wButtons & XINPUT_GAMEPAD_BACK;
                            } 
                            else 
                            {

                                // Note(casey): This controller is not available
                                NewController->IsConnected = false;
                            }
                        }
                        // XINPUT_VIBRATION Vibration;
                        // Vibration.wLeftMotorSpeed  = 60000;
                        // Vibration.wRightMotorSpeed = 60000;
                        // XInputSetState(0, &Vibration);
                        thread_context Thread = {};
                        game_offscreen_buffer Buffer = {};
                        Buffer.Memory = GlobalBackBuffer.Memory;
                        Buffer.Width = GlobalBackBuffer.Width;
                        Buffer.Height = GlobalBackBuffer.Height;
                        Buffer.Pitch = GlobalBackBuffer.Pitch;
                        Buffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;
                        if (Win32State.InputRecordingIndex)
                        {
                            Win32RecordInput(&Win32State, NewInput);
                        }
                        if (Win32State.InputPlayingIndex)
                        {
                            Win32PlaybackInput(&Win32State, NewInput);
                        }

                        if (Game.UpdateAndRender)
                        {
                            Game.UpdateAndRender(&Thread, &GameMemory, NewInput, &Buffer);
                        }

                        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                        f32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);    // how far are we into the current visual frame

                        DWORD PlayCursor;
                        DWORD WriteCursor;
                        if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                        {
                            /* NOTE(casey):
                               Here is how sound output computation works.
                             
                               We define a safety value that is the number 
                               of samples we think our game update loop 
                               may vary by (let's say up to 2ms). 
                             
                               When we wake up to write audio, we will look
                               and see what the play cursor position is and we
                               will forecast ahead where we think the
                               play cursor will be on the next frame boundary.
                             
                               We will then look to see if the write cursor is
                               before that by at least our safety value. If it is, the
                               target fill position is that frame boundary
                               plus one frame. This gives us perfect audio
                               sync in the case of a card that has low enough
                               latency.
                             
                               If the write cursor is _after_ that safety 
                               margin, then we assume we can never sync the
                               audio perfectly, so we will write one frame's
                               worth of audio plus the safety margin's worth
                               of guard samples.
                            */

                            if (!SoundIsValid)
                            {
                                SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                                SoundIsValid = true;
                            }

                            // ByteToLock: point from where we should start writing
                            // TargetCursor: where we're writing until (will move depending on hardware's latency. Low latency path we can write up from the frame flip so we offset TargetCursor until the next frame's boundary so that audio will always go out with the image. High latency path, then we won't wait until the framw flip, and the Targetcursor can be whatever our WriteCursor would be after one frame)
                            // BytesToWrite: difference between ByteToLock and TargetCursor ( considering circular buffer wrap )
                            DWORD ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample)
                                                    % SoundOutput.SecondaryBufferSize);

                            DWORD ExpectedSoundBytesPerFrame = (DWORD)((f32)SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample)
                                                                / GameUpdateHz;
                            f32 SecondsLeftUntilFlip = TargetSecondsPerFrame - FromBeginToAudioSeconds;
                            DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) * (f32)ExpectedSoundBytesPerFrame);
                            DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;

                            DWORD SafeWriteCursor = WriteCursor;
                            if (SafeWriteCursor < PlayCursor)
                            {
                                SafeWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            Assert(SafeWriteCursor >= PlayCursor);
                            SafeWriteCursor += SoundOutput.SafetyBytes;
                            b32 AudioCardIsLowLatency = SafeWriteCursor < ExpectedFrameBoundaryByte;

                            DWORD TargetCursor = 0;
                            if (AudioCardIsLowLatency)
                            {
                                TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;
                            }
                            else
                            {
                                TargetCursor = WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes;
                            }
                            TargetCursor = TargetCursor % SoundOutput.SecondaryBufferSize;
                               
                            DWORD BytesToWrite = 0;
                            if(ByteToLock > TargetCursor)
                            {
                               BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                               BytesToWrite += TargetCursor;
                            }
                            else
                            {
                               BytesToWrite = TargetCursor - ByteToLock;
                            }

                            game_sound_output_buffer SoundBuffer = {};
                            SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                            SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                            SoundBuffer.Samples = Samples;

                            if (Game.GetSoundSamples)
                            {
                                Game.GetSoundSamples(&Thread, &GameMemory, &SoundBuffer);
                            }
                            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);

                            #if HANDMADE_INTERNAL
                            // NOTE(casey): This is debug code
                            win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkersIndex];
                            Marker->OutputPlayCursor = PlayCursor;
                            Marker->OutputWriteCursor = WriteCursor;
                            Marker->OutputLocation = ByteToLock;
                            Marker->OutputByteCount = BytesToWrite;
                            Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;
                            #if 0
                            DWORD UnwrappedWriteCursor = WriteCursor;
                            if (UnwrappedWriteCursor < PlayCursor)
                            {
                                UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;    // when the write cursor wraps back to the fron (behind hte play cursor), we can just offset it by the length of the buffer to get its unwrapped position
                            }
                            DWORD AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
                            f32 AudioLatencySeconds = ((f32)AudioLatencyBytes / (f32)SoundOutput.BytesPerSample) /
                                                        (f32)SoundOutput.SamplesPerSecond;

                            char TextBuffer[256];
                            sprintf_s(TextBuffer, sizeof(TextBuffer), "BTL:%u TC:%u BTW:%u - PC:%u WC:%u DELTA:%u (%.2fs)\n",
                                        ByteToLock, TargetCursor, BytesToWrite,
                                        PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
                            OutputDebugStringA(TextBuffer);
                            #endif
                            #endif


                        }
                        else
                        {
                            // GetCurrentPosition didn't succeed
                            SoundIsValid = false;
                        }
                        
                        // debug frame timings
                        LARGE_INTEGER WorkCounter = Win32GetWallClock();
                        f32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);
                        f32 SecondsElapsedForFrame = WorkSecondsElapsed;
                        if (SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            // sleep until frame time ends, then display frame after (displaybufferinwindow)
                            if (SleepIsGranular)
                            {
                                DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));

                                if (SleepMS > 0)
                                {
                                    Sleep(SleepMS);
                                }
                            }

                            f32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                            if(TestSecondsElapsedForFrame < TargetSecondsPerFrame)
                            {
                                // TODO(casey): LOG MISSED SLEEP HERE
                            }

                            // spinning last fre microseconds
                            while (SecondsElapsedForFrame < TargetSecondsPerFrame)
                            {
                                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                            }
                            
                        }
                        else
                        {
                            // TODO(casey): MISSED FRAME RATE!
                            // TODO(casey): Logging
                        }

                        LARGE_INTEGER EndCounter = Win32GetWallClock();
                        f32 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
                        LastCounter = EndCounter;

                        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                        HDC DeviceContext = GetDC(Window);
                        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
                        ReleaseDC(Window, DeviceContext);
                        FlipWallClock = Win32GetWallClock();

                        
#if HANDMADE_INTERNAL
                        DWORD FlipPlayCursor = 0;
                        DWORD FlipWriteCursor = 0;
                        if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&FlipPlayCursor, &FlipWriteCursor)))
                        {

                            Assert(DebugTimeMarkersIndex < ArrayCount(DebugTimeMarkers));
                            win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkersIndex];
                            Marker->FlipPlayCursor = FlipPlayCursor;
                            Marker->FlipWriteCursor = FlipWriteCursor;
                        }
#endif


                        game_input *Temp = NewInput;
                        NewInput = OldInput;
                        OldInput = Temp;


                        u64 EndCycleCount = __rdtsc();
                        s64 CyclesElapsed = EndCycleCount - LastCycleCount;
                        LastCycleCount = EndCycleCount;

                        #if 0
                        // debug timing output
                        f32 FPS = 0.0f; // To be fixed later
                        f32 MegaCyclesPerFrame = (f32)CyclesElapsed / (1000.0f * 1000.0f);

                        char FPSBuffer[256];
                        sprintf_s(FPSBuffer, sizeof(FPSBuffer), "%.02fms/f, %.02ff/s, %.02fMc/f\n", MSPerFrame, FPS, MegaCyclesPerFrame);
                        OutputDebugStringA(FPSBuffer);
                        #endif

#if HANDMADE_INTERNAL
                        ++DebugTimeMarkersIndex;
                        if (DebugTimeMarkersIndex == ArrayCount(DebugTimeMarkers))
                        {
                            DebugTimeMarkersIndex = 0;
                        }
#endif

                    } // if (!GlobalPause)
                    
                } // while (GlobalRunning) 
            } // if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
            else
            {
                // Memory allocation failed
                // TODO(casey): Logging
            }
        } // if (Window) 
        else 
        {
            // Window creation failed!
            // TODO(casey): Logging
        }
    } // (RegisterClassA(&WindowClass)) 
    else 
    {
        // Window Class Registration failed
        // TODO(casey): Logging
    }
    return (0);
} // WinMain
