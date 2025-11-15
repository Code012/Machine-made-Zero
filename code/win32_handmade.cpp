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


#define KeyMessageWasDownBit (1 << 30)
#define KeyMessageIsDownBit  (1 << 31)

////////////////////////////////
// Globals
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
    int SecondaryBufferSize;
    u32 RunningSampleIndex;
    int ToneHz;
    int ToneVolume;
    int WavePeriod;
    f32 tSine;
    int LatencySampleCount;
};

global_variable bool                   GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable IDirectSoundBuffer    *GlobalSecondaryBuffer;
global_variable win32_sound_output     SoundOutput;

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
    int BytesPerPixel = 4; // 3 for rgb, 1 to ensure alignment
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
        case WM_KEYDOWN: {
            // (me): Don't have an xbox controller and dont want to rewrite controller logic with another api (at the moment) for dualshock
            u32 VKCode = WParam;
            if (VKCode == 'A')
            {
                ++SoundOutput.ToneHz;
                SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
            }
            else if (VKCode == 'D')
            {
                --SoundOutput.ToneHz;
                SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
            }
        }
        case WM_KEYUP: {
            // Handle keyboard messaged here
            bool IsDown  = ((LParam & KeyMessageWasDownBit) == 0);
            bool WasDown = ((LParam & KeyMessageIsDownBit) != 0);
            u32  VKCode  = WParam;
            if (IsDown != WasDown) {

                if (VKCode == 'W') {
                } else if (VKCode == 'A') { 
                } else if (VKCode == 'S') {
                } else if (VKCode == 'D') {
                } else if (VKCode == 'Q') {
                } else if (VKCode == 'E') {
                } else if (VKCode == VK_UP) {
                } else if (VKCode == VK_DOWN) {
                } else if (VKCode == VK_LEFT) {
                } else if (VKCode == VK_RIGHT) {
                } else if (VKCode == VK_ESCAPE) {
                    OutputDebugString("ESCAPE: ");
                    if (IsDown) {
                        OutputDebugString("ISDOWN ");
                    }
                    if (WasDown) {

                        OutputDebugStringA("WAASDOWN ");
                    }
                    OutputDebugString("\n");

                } else if (VKCode == VK_SPACE) {
                }

                b32 AltKeyWasDown = (LParam & (1 << 29)); 
                if ((VKCode == VK_F4) && AltKeyWasDown) {
                    GlobalRunning = false;
                }
            }
        } break;

            // When the user deletes our window
        case WM_DESTROY: {
            GlobalRunning = false;
            OutputDebugStringA("WM_Destroy\n");
        } break;

            // When the user clicks on the little X in the top-right corner
        case WM_CLOSE: {
            GlobalRunning = false;
            PostQuitMessage(0);
            OutputDebugStringA("WM_CLOSE\n");
        } break;

            // When the user clicked on the window and it became active
        case WM_ACTIVATEAPP: {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_PAINT: {
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
        case WM_SIZE: {
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32ResizeDIBSection(
              &GlobalBackBuffer, Dimension.Width, Dimension.Height);
        } break;

        default: {
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
        if (Window) {
            // Note(casey): Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone.
            HDC DeviceContext    = GetDC(Window);
            int YOffset          = 0;
            int XOffset          = 0;
            // NOTE(casey): Sound test constants
            // win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000; //40kHz
            SoundOutput.BytesPerSample = sizeof(s16) * 2; // 16-bit samples playing in 2 channels (stereo)
            SoundOutput.SecondaryBufferSize = 2 * SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample; // Buffer size of 2 seconds
            SoundOutput.ToneHz = 256;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.ToneVolume = 3000;
            SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;

            // Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            s16 *Samples = (s16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            GlobalRunning = true;
            LARGE_INTEGER LastCounter;
            QueryPerformanceCounter(&LastCounter);  // This isn't at the beginning of the loop because if getting back tot he top of the loop took some time (maybe the process switched or something else happened) we'd miss that time. This approach guarantees to never miss that time, because we have only one single place where we check the clock, and then measure the time on our last run ont he same spot.
            u64 LastCycleCount = __rdtsc();
            while (GlobalRunning) {
                MSG Message;
                while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
                    if (Message.message == WM_QUIT) {
                        GlobalRunning = false;
                    }
                    TranslateMessage(&Message); // process message
                    DispatchMessageA(&Message); // send to callback
                }

                // TODO(casey): Should we poll this more frequently?
                for (DWORD ControllerIndex = 0;
                     ControllerIndex < XUSER_MAX_COUNT;
                     ++ControllerIndex) {
                    XINPUT_STATE ControllerState = {};
                    // ZeroMemory(&ControllerState, sizeof(XINPUT_STATE));

                    if (XInputGetState(ControllerIndex, &ControllerState) ==
                        ERROR_SUCCESS) {
                        // Note(casey): the controller is plugged in
                        //  TODO(casey): See if ControllerState.dwPacketNumber
                        //  increments too rapidly
                        XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

                        bool Up            = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool Down          = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool Left          = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool Right         = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        bool Start         = Pad->wButtons & XINPUT_GAMEPAD_START;
                        bool Back          = Pad->wButtons & XINPUT_GAMEPAD_BACK;
                        bool LeftShoulder  = Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                        bool RightShoulder = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
                        bool AButton       = Pad->wButtons & XINPUT_GAMEPAD_A;
                        bool BButton       = Pad->wButtons & XINPUT_GAMEPAD_B;
                        bool XButton       = Pad->wButtons & XINPUT_GAMEPAD_X;
                        bool YButton       = Pad->wButtons & XINPUT_GAMEPAD_Y;
                        s16  StickX        = Pad->sThumbLX;
                        s16  StickY        = Pad->sThumbLY;
                        XOffset += StickX >> 12;
                        YOffset += StickY >> 12;
                        SoundOutput.ToneHz = 512 + (int)(256.0f * ((f32)StickY / 30000.0f));
                        SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;

                    } else {

                        // Note(casey): the controller is not available, can show
                        // user that it is
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

                GameUpdateAndRender(&Buffer, XOffset, YOffset, &SoundBuffer, SoundOutput.ToneHz);
                ++XOffset;

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
            } 
        } else {
            // Window creation failed!
            // TODO(casey): Logging
        }
    } else {
        // Window Class Registration failed
        // TODO(casey): Logging
    }
    return (0);
}
