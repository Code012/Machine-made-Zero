#include <windows.h>
#include <Xinput.h>
#include <dsound.h>

#include <cstdint>
#include <stdint.h>

// unsigned integers
typedef uint8_t  u8;  // 1-byte long unsigned integer
typedef uint16_t u16; // 2-byte long unsigned integer
typedef uint32_t u32; // 4-byte long unsigned integer
typedef uint64_t u64; // 8-byte long unsigned integer
// signed integers
typedef int8_t  s8;  // 1-byte long signed integer
typedef int16_t s16; // 2-byte long signed integer
typedef int32_t s32; // 4-byte long signed integer
typedef int64_t s64; // 8-byte long signed integer
typedef s32     b32; // bool

#define internal \
    static // if static applied to a function, the function is marked as
           // "internal" to that specific file, and no one from outside may
           // access it (actually, the locality may extend to more thatn one
           // file, i.e. to the entire translation unit of the program. We'll
           // speak about the translation units much, much later down the line
#define local_persist \
    static // stays around similar to a 'global_variable', but it's locally
           // scoped. also never use static in the final code, bad for thread
           // safety and other advanced things. local_persist keyword made so it
           // can be stripped away from the final code, however handy tool for
           // development, because they allow to quicjly introduce something to
           // the code without having to worry about passing things around, and
           // this is exactly what we are doing right now in WM_PAINT: making
           // sure our PatBlt does exactly what we want to it to do.
#define global_variable \
    static // once declared, is available in the entirety of the file it's in.
           // Any function can access it, read its value and modify it
#define KeyMessageWasDownBit (1 << 30)
#define KeyMessageIsDownBit (1 << 31)

struct win32_offscreen_buffer
{
    // Note(joey): Pixels are always 32-bite wide,
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

global_variable bool                   GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;

// NOTE(me):
// 1. Defined a function prototype
// 2. Defined a function type from the prototype, so it can be used as a pointer
// 3. Created a stub function to use in case we fail to load the actual functions
// 4. Set up a permanent reference for the function of that type in a global variable. By default it points to the stub function
// 5. Defined the same name as the original function for that variable
// NOTE(joey): XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(joey): XInputSetState
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
        // TODO(joey): Diagnostic
    }
}
internal void 
Win32InitDSound(HWND Window, s32 SamplesPerSecond, s32 BufferSize)
{
    // NOTE(joey): Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if (DSoundLibrary)
    {
        // NOTE(joey): Create a DirectSound object
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


            // NOTE(joey): set cooperative level
            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                // NOTE(joey): "Create" a primary buffer
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                IDirectSoundBuffer *PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                   if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) 
                    {
                        // NOTE(joey): We have finally set hte format of the primary buffer!
                        OutputDebugStringA("Primary buffer format was set. \n");

                    }

                }

            }
        
            // NOTE(joey): "Create" a secondary buffer
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;

            IDirectSoundBuffer *SecondaryBuffer;
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &SecondaryBuffer, 0)))
            {
                // NOTE(joey): All good, secondary buffer works as intended
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

internal void
RenderWeirdGradient(win32_offscreen_buffer* Buffer, int XOffset, int YOffset)
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

// serves to resize or initialise if it doesn't exist a Device Independent
// Bitmap which is the name Windwos gies to the bitmaps it can display with GDI
internal void
Win32ResizeDIBSection(win32_offscreen_buffer* Buffer, int Width, int Height)
{
    // TODO(joey): Bulletproof this.
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

    // Note(joey): When the biHeight field is negative, this is the clue
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
    // TODO(joey): Aspect ratio correction
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

// Event handler, called when something happens to the window
LRESULT CALLBACK
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    switch (Message) {

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
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
    Win32LoadXInput();
    WNDCLASSA WindowClass = {};
    // TODO(joey): Check if HREDRAW/VREDRAW/OWNDC still matter
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
            // Note(joey): Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone.
            HDC DeviceContext    = GetDC(Window);
            int YOffset          = 0;
            int XOffset          = 0;
            int SamplesPerSecond = 48000; //40kHz
            int BytesPerSample = sizeof(s16) * 2; // 16-bit samples playing in 2 channels (stereo)
            int SecondaryBufferSize = 2 * SamplesPerSecond * BytesPerSample; // Buffer size of 2 seconds
            Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
            Win32InitDSound(Window, SamplesPerSecond, SecondaryBufferSize);

            GlobalRunning = true;
            while (GlobalRunning) {
                MSG Message;
                while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
                    if (Message.message == WM_QUIT) {
                        GlobalRunning = false;
                    }
                    TranslateMessage(&Message); // process message
                    DispatchMessageA(&Message); // send to callback
                }

                // TODO(joey): Should we poll this more frequently?
                for (DWORD ControllerIndex = 0;
                     ControllerIndex < XUSER_MAX_COUNT;
                     ++ControllerIndex) {
                    XINPUT_STATE ControllerState = {};
                    // ZeroMemory(&ControllerState, sizeof(XINPUT_STATE));

                    if (XInputGetState(ControllerIndex, &ControllerState) ==
                        ERROR_SUCCESS) {
                        // Note(joey): the controller is plugged in
                        //  TODO(joey): See if ControllerState.dwPacketNumber
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

                    } else {

                        // Note(joey): the controller is not available, can show
                        // user that it is
                    }
                }
                XINPUT_VIBRATION Vibration;
                Vibration.wLeftMotorSpeed  = 60000;
                Vibration.wRightMotorSpeed = 60000;
                XInputSetState(0, &Vibration);

                RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);
                ++XOffset;

                Win32DisplayBufferInWindow(
                  &GlobalBackBuffer, DeviceContext, 1280, 720);
            }
        } else {
            // Window creation failed!
            // TODO(joey): Logging
        }
    } else {
        // Window Class Registration failed
        // TODO(joey): Logging
    }
    return (0);
}
