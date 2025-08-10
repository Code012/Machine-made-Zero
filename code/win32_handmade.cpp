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

global_variable bool Running;
global_variable BITMAPINFO BitmapInfo;
global_variable void* BitmapMemory;
global_variable int BitmapWidth;
global_variable int BitmapHeight;
global_variable int BytesPerPixel;

internal void
RenderWeirdGradient(int XOffset, int YOffset)
{

    int Pitch = BitmapWidth * BytesPerPixel;
    u8* Row = (u8*)BitmapMemory;
    for (int Y = 0; Y < BitmapHeight; ++Y) {
        u32 * Pixel = (u32 *)Row;
        for (int X = 0; X < BitmapWidth; ++X) {
            u8 Red = 0;
            u8 Green = (u8)(Y + YOffset);
            u8 Blue = (u8)(X + XOffset);

            *Pixel++ = Red << 16 | Green << 8 | Blue; // << 0
        }
        Row += Pitch;
    }
}

// serves to resize or initialise if it doesn't exist a Device Independent
// Bitmap which is the name Windwos gies to the bitmaps it can display with GDI
internal void
Win32ResizeDIBSection(int Width, int Height)
{
    // TODO(joey): Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.
    if (BitmapMemory) {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
        // Optionally, you can check if the result of VirtualFree is not zero.
        // Print out an error message if it is.
    }

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = BitmapWidth;
    BitmapInfo.bmiHeader.biHeight =
      -BitmapHeight; // negative value: top-down pitch/stride
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    BytesPerPixel = 4; // 3 for rgb, 1 to ensure alignment
    BitmapWidth = Width;
    BitmapHeight = Height;
    int BitmapMemorySize = BytesPerPixel * (Width * Height);
    BitmapMemory = VirtualAlloc(
      0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

}
internal void
Win32UpdateWindow(HDC DeviceContext, RECT* ClientRect)
{
    int WindowWidth = ClientRect->right - ClientRect->left;
    int WindowHeight = ClientRect->bottom - ClientRect->top;
    StretchDIBits(DeviceContext,
                  0,
                  0,
                  WindowWidth,
                  WindowHeight, // destination rectangle (window)
                  0,
                  0,
                  BitmapWidth,
                  BitmapHeight, // source rectangle (bitmap buffer)
                  BitmapMemory,
                  &BitmapInfo,
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
            Running = false;
            OutputDebugStringA("WM_Destroy\n");
        } break;

            // When the user clicks on the little X in the top-right corner
        case WM_CLOSE: {
            Running = false;
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
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            Win32UpdateWindow(DeviceContext, &ClientRect);
            EndPaint(Window, &Paint);
        } break;

            // When the user changes the size of the window
        case WM_SIZE: {
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            int Width = ClientRect.right - ClientRect.left;
            int Height = ClientRect.bottom - ClientRect.top;
            Win32ResizeDIBSection(Width, Height);
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
            int dir = 1;
            int YOffset = 0;
            int XOffset = 0;
            int OscillateCounter = 300;

            Running = true;
            while (Running) {
                MSG Message;
                while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
                    if (Message.message == WM_QUIT) {
                        Running = false;
                    }
                    TranslateMessage(&Message); // process message
                    DispatchMessageA(&Message); // send to callback
                }
                RenderWeirdGradient(XOffset, YOffset);
                if (OscillateCounter == 0) {
                    OscillateCounter = 300;
                    dir *= -1;
                }
                --OscillateCounter;
                ++XOffset;
                YOffset += dir;

                HDC DeviceContext = GetDC(Window);
                RECT ClientRect;
                GetClientRect(Window, &ClientRect);

                Win32UpdateWindow(DeviceContext, &ClientRect);

                ReleaseDC(Window, DeviceContext);
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
