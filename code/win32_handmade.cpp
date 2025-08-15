#include <cstdint>
#include <stdint.h>
#include <windows.h>

// unsigned integers
typedef uint8_t u8;   // 1-byte long unsigned integer
typedef uint16_t u16; // 2-byte long unsigned integer
typedef uint32_t u32; // 4-byte long unsigned integer
typedef uint64_t u64; // 8-byte long unsigned integer
// signed integers
typedef int8_t s8;   // 1-byte long signed integer
typedef int16_t s16; // 2-byte long signed integer
typedef int32_t s32; // 4-byte long signed integer
typedef int64_t s64; // 8-byte long signed integer

#define internal                                                               \
    static // if static applied to a function, the function is marked as
           // "internal" to that specific file, and no one from outside may
           // access it (actually, the locality may extend to more thatn one
           // file, i.e. to the entire translation unit of the program. We'll
           // speak about the translation units much, much later down the line
#define local_persist                                                          \
    static // stays around similar to a 'global_variable', but it's locally
           // scoped. also never use static in the final code, bad for thread
           // safety and other advanced things. local_persist keyword made so it
           // can be stripped away from the final code, however handy tool for
           // development, because they allow to quicjly introduce something to
           // the code without having to worry about passing things around, and
           // this is exactly what we are doing right now in WM_PAINT: making
           // sure our PatBlt does exactly what we want to it to do.
#define global_variable                                                        \
    static // once declared, is available in the entirety of the file it's in.
           // Any function can access it, read its value and modify it

struct win32_offscreen_buffer
{
    // Note(joey): Pixels are always 32-bite wide,
    // Memory Order  0x BB GG RR xx
    // Little Endian 0x xx RR GG BB

    BITMAPINFO Info;
    void* Memory;
    int Width;
    int Height;
    int Pitch;
};
struct win32_window_dimension
{
    int Width;
    int Height;
};

global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
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
            u8 Red = 0;
            u8 Green = (u8)(Y + YOffset);
            u8 Blue = (u8)(X + XOffset);

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

    Buffer->Width = Width;
    Buffer->Height = Height;
    int BytesPerPixel = 4; // 3 for rgb, 1 to ensure alignment
    Buffer->Pitch = Buffer->Width * BytesPerPixel;

    // Note(joey): When the biHeight field is negative, this is the clue
    // to Windows to treat this bitmap as top-down, not bottom-up, meaning
    // that the first bytes of the image are the color for the top left
    // pixel in the bitmap, not the bottom left!
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight =
      -Buffer->Height; // negative value: top-down pitch/stride
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize =
      BytesPerPixel * (Buffer->Width * Buffer->Height);
    Buffer->Memory = VirtualAlloc(
      0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer* Buffer,
                           HDC DeviceContext,
                           int WindowWidth,
                           int WindowHeight)
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
            PAINTSTRUCT Paint = {};
            HDC DeviceContext = BeginPaint(Window, &Paint);
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
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
        LPSTR CommandLine,
        int ShowCode)
{
    WNDCLASSA WindowClass = {};
    // TODO(joey): Check if HREDRAW/VREDRAW/OWNDC still matter
    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
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
            HDC DeviceContext = GetDC(Window);
            int dir = 1;
            int YOffset = 0;
            int XOffset = 0;
            int OscillateCounter = 300;

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
                RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);
                if (OscillateCounter == 0) {
                    OscillateCounter = 300;
                    dir *= -1;
                }
                --OscillateCounter;
                ++XOffset;
                YOffset += dir;

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
