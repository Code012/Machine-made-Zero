#include <windows.h>
#define internal                                                                                                       \
    static // if static applied to a function, the function is marked as "internal" to that specific file, and no one
           // from outside may access it (actually, the locality may extend to more thatn one file, i.e. to the entire
           // translation unit of the program. We'll speak about the translation units much, much later down the line
#define local_persist                                                                                                  \
    static // stays around similar to a 'global_variable', but it's locally scoped. also never use static in the final
           // code, bad for thread safety and other advanced things. local_persist keyword made so it can be stripped
           // away from the final code, however handy tool for development, because they allow to quicjly introduce
           // something to the code without having to worry about passing things around, and this is exactly what we are
           // doing right now in WM_PAINT: making sure our PatBlt does exactly what we want to it to do.
#define global_variable                                                                                                \
    static // once declared, is available in the entirety of the file it's in. Any function can access it, read its
           // value and modify it

global_variable bool Running;
global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable HBITMAP BitmapHandle;
global_variable HDC BitmapDeviceContext;

// serves to resize or initialise if it doesn't exist a Device Independent Bitmap which is the name Windwos gies to the
// bitmaps it can display with GDI
internal void Win32ResizeDIBSection(int Width, int Height)
{
    // TODO(joey): Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.

    if (BitmapHandle)
    {
        DeleteObject(BitmapHandle);
    }

    if (!BitmapDeviceContext)
    {
        // TODO(joey): Should we recreate these under certain special circumstances?
        BitmapDeviceContext = CreateCompatibleDC(0);
    }

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = Width;
    BitmapInfo.bmiHeader.biHeight = Height;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    HBITMAP BitmapHandle = CreateDIBSection(BitmapDeviceContext, &BitmapInfo, DIB_RGB_COLORS, &BitmapMemory, 0, 0);
}
internal void Win32UpdateWindow(HDC DeviceContext, int X, int Y, int Width, int Height)
{
    StretchDIBits(DeviceContext, X, Y, Width, Height, X, Y, Width, Height, BitmapMemory, &BitmapInfo, DIB_RGB_COLORS,
                  SRCCOPY);
}

// Event handler, called when something happens to the window
LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    switch (Message)
    {
    

        // When the user deletes our window
    case WM_DESTROY: {
        Running = false;
        OutputDebugStringA("WM_Destroy\n");
    }
    break;

        // When the user clicks on the little X in the top-right corner
    case WM_CLOSE: {
        Running = false;
        PostQuitMessage(0);
        OutputDebugStringA("WM_CLOSE\n");
    }
    break;

        // When the user clicked on the window and it became active
    case WM_ACTIVATEAPP: {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
    }
    break;

    case WM_PAINT: {
        PAINTSTRUCT Paint = {};
        HDC DeviceContext = BeginPaint(Window, &Paint);
        int X = Paint.rcPaint.left;
        int Y = Paint.rcPaint.top;
        int Width = Paint.rcPaint.right - Paint.rcPaint.left;
        int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
        Win32UpdateWindow(DeviceContext, X, Y, Width, Height);

        EndPaint(Window, &Paint);
    }
    break;

        // When the user changes the size of the window
    case WM_SIZE: {
        RECT ClientRect;
        GetClientRect(Window, &ClientRect);
        int Width = ClientRect.right - ClientRect.left;
        int Height = ClientRect.bottom - ClientRect.top;
        Win32ResizeDIBSection(Width, Height);
    }
    break;
            
    default: {
        // Do something in case of any other message
        Result = DefWindowProc(Window, Message, WParam, LParam);
    }
    break;
    }

    return (Result);
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    WNDCLASSA WindowClass = {};
    // TODO(joey): Check if HREDRAW/VREDRAW/OWNDC still matter
    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon
    WindowClass.lpszClassName = "HandmadeHeroClass";
    if (RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(0, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);
        if (Window)
        {

            Running = true;
            while (Running)
            {
                MSG Message;
                BOOL MessageResult = GetMessage(&Message, 0, 0, 0); // wait for a message
                if (MessageResult > 0)
                {
                    // 0 is the WM_QUIT message, -1 is invalid window handle
                    TranslateMessage(&Message); // process message
                    DispatchMessageA(&Message); // send to callback
                }
                else
                {
                    break; // break out of the loop
                }
            }
        }
        else
        {
            // Window creation failed!
            // TODO(joey): Logging
        }
    }
    else
    {
        // Window Class Registration failed
        // TODO(joey): Logging
    }
    return (0);
}
