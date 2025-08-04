#include <windows.h>

LRESULT CALLBACK MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    switch (Message)
    {
        // When the user changes the size of the window
    case WM_SIZE: {
        OutputDebugStringA("WM_SIZE\n");
    }
    break;

        // When the user deletes our window
    case WM_DESTROY: {
        OutputDebugStringA("WM_Destroy\n");
    }
    break;

        // When the user clicks on the little X in the top-right corner
    case WM_CLOSE: {
        OutputDebugStringA("WM_CLOSE\n");
    }
    break;

        // When the user clicked on the window and it became active
    case WM_ACTIVATEAPP: {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
    }
    break;

    case WM_PAINT: {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);
        int X = Paint.rcPaint.left;
        int Y = Paint.rcPaint.top;
        int Width = Paint.rcPaint.right - Paint.rcPaint.left;
        int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
        static DWORD Operation = WHITENESS;
        if (Operation == WHITENESS)
        {
            Operation = BLACKNESS;
        }
        else
        {
            Operation = WHITENESS;
        }
        PatBlt(DeviceContext, X, Y, Width, Height, Operation);
        EndPaint(Window, &Paint);
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
    WindowClass.lpfnWndProc = MainWindowCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon
    WindowClass.lpszClassName = "HandmadeHeroClass";
    if (RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(0, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);
        if (Window)
        {

            /*MSG Message;
            while (GetMessage(&Message, 0, 0, 0))
            {

            }*/
            for (;;)
            {
                MSG Message;
                BOOL MessageResult = GetMessage(&Message, 0, 0, 0);
                if (MessageResult > 0)
                {
                    // 0 is the WM_QUIT message, -1 is invalid window handle
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
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
