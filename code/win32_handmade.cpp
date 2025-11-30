/*
    TODO(casey): THIS IS NOT A FINAL PLATFORM LAYER!!!

    - Saved game locations
    -Getting a handle to our own exe file
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

*/
// TODO(casey): Implement sine ourselves
#include <math.h>
#include <stdint.h>

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

////////////////////////////////
// Defines
#define internal        static 
#define local_persist   static
#define global_variable static

#define Pi32            3.14159265359f

#include "handmade.cpp"

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
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable IDirectSoundBuffer    *GlobalSecondaryBuffer;

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
DEBUGPlatformFreeFileMemory(void *Memory)
{
    if (Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}
internal debug_read_file_result
DEBUGPlatformReadEntireFile(char *Filename)
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
                    DEBUGPlatformFreeFileMemory(Result.Contents);
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
internal b32
DEBUGPlatformWriteEntireFile(char *Filename, u32 MemorySize, void *Memory)  // blocking write, not for final game
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
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}
internal void 
Win32ProcessKeyboardMessage(game_button_state *NewState, b32 IsDown)
{
    Assert(NewState->EndedDown != IsDown); // make sure two states are different
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
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
    StretchDIBits(DeviceContext,
                  0,
                  0,
                  WindowWidth,
                  WindowHeight, // destination rectangle (window)
                  0,
                  0,
                  Buffer->Width,
                  Buffer->Height, // source rectangle (bitmap buffer)
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
Win32ProcessPendingMessages(game_controller_input *KeyboardController)
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

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR     CommandLine,
        int       ShowCode)
{
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    s64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;
    Win32LoadXInput();
    WNDCLASSA WindowClass = {};
    // TODO(casey): Check if HREDRAW/VREDRAW/OWNDC still matter
    WindowClass.style       = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance   = Instance;
    // WindowClass.hIcon
    WindowClass.lpszClassName = "HandmadeHeroClass";
    if (RegisterClassA(&WindowClass)) {
        HWND Window = CreateWindowExA(0,
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
            // Note(casey): Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone.
            HDC DeviceContext    = GetDC(Window);
            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000; //40kHz
            SoundOutput.BytesPerSample = sizeof(s16) * 2; // 16-bit samples playing in 2 channels (stereo)
            SoundOutput.SecondaryBufferSize = 2 * SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample; // Buffer size of 2 seconds
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;

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
            u64 TotalStorageSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;

            GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, (size_t)TotalStorageSize,
                                                        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            GameMemory.TransientStorage = ((u8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

            if (Samples && 
                GameMemory.PermanentStorage &&
                GameMemory.TransientStorage)
            {

                game_input Input[2] = {};
                game_input* OldInput = &Input[0];
                game_input* NewInput = &Input[1];

                LARGE_INTEGER LastCounter;
                QueryPerformanceCounter(&LastCounter);  // This isn't at the beginning of the loop because if getting back tot he top of the loop took some time (maybe the process switched or something else happened) we'd miss that time. This approach guarantees to never miss that time, because we have only one single place where we check the clock, and then measure the time on our last run ont he same spot.
                u64 LastCycleCount = __rdtsc();

                GlobalRunning = true;
                while (GlobalRunning) // frame loop 
                { 
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

                    Win32ProcessPendingMessages(NewKeyboardController);

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
                    XINPUT_VIBRATION Vibration;
                    Vibration.wLeftMotorSpeed  = 60000;
                    Vibration.wRightMotorSpeed = 60000;
                    XInputSetState(0, &Vibration);

                    DWORD ByteToLock;
                    DWORD TargetCursor;
                    DWORD BytesToWrite;
                    DWORD PlayCursor;
                    DWORD WriteCursor;
                    b32 SoundIsValid = false;
                    // TODO(casey): Tighten up sound logic so that we know where we should be
                    // writing to and can anticipate the time spent in the game updates.
                    if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                    {
                       
                        ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample)
                                             % SoundOutput.SecondaryBufferSize);
                        TargetCursor = ((PlayCursor +
                                                 (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample))
                                             % SoundOutput.SecondaryBufferSize);
                        
                        if(ByteToLock > TargetCursor)
                        {
                            BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                            BytesToWrite += TargetCursor;
                        }
                        else
                        {
                            BytesToWrite = TargetCursor - ByteToLock;
                        }
                        
                        SoundIsValid = true;
                    }

                    
                    game_sound_output_buffer SoundBuffer = {};
                    SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                    SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                    SoundBuffer.Samples = Samples;

                    game_offscreen_buffer Buffer = {};
                    Buffer.Memory = GlobalBackBuffer.Memory;
                    Buffer.Width = GlobalBackBuffer.Width;
                    Buffer.Height = GlobalBackBuffer.Height;
                    Buffer.Pitch = GlobalBackBuffer.Pitch;

                    GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);

                    // NOTE(casey): DirectSound output test
                    if(SoundIsValid)
                    {
                        Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                    } 


                    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                    Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);


                    LARGE_INTEGER EndCounter;
                    QueryPerformanceCounter(&EndCounter);
                    u64 EndCycleCount = __rdtsc();

                    // TODO(casey): Display the value here
                    s64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
                    s64 CyclesElapsed = EndCycleCount - LastCycleCount;
                    f64 MSPerFrame = 1000.0f*(f64)CounterElapsed / (f64)PerfCountFrequency;
                    f64 FPS = (f64)PerfCountFrequency / (f64)CounterElapsed;
                    f64 MegaCyclesPerFrame = (f64)CyclesElapsed / (1000.0 * 1000.0);

                    #if 0
                    char Buffer[256];
                    sprintf(Buffer, "%.02fms/f, %.02ff/s, %.02fMc/f \n", MSPerFrame, FPS, MegaCyclesPerFrame);
                    OutputDebugStringA(Buffer);
                    #endif

                    LastCounter = EndCounter;
                    LastCycleCount = EndCycleCount;

                    game_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;
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
