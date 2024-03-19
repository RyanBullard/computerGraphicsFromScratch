#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

static bool quit = false;

struct {
    int width;
    int height;
    uint32_t *pixels;
} frame = { 0 };

static BITMAPINFO bmi;
static HBITMAP frameBitmap = 0;
static HDC fdc = 0;

LRESULT CALLBACK WindowProcessMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_QUIT:
    case WM_DESTROY: {
        quit = true;
    } break;

    case WM_PAINT: {
        static PAINTSTRUCT paint;
        static HDC dc;
        dc = BeginPaint(windowHandle, &paint);
        BitBlt(dc,
            paint.rcPaint.left, paint.rcPaint.top,
            paint.rcPaint.right - paint.rcPaint.left, paint.rcPaint.bottom - paint.rcPaint.top,
            fdc,
            paint.rcPaint.left, paint.rcPaint.top,
            SRCCOPY);
        EndPaint(windowHandle, &paint);
    } break;

    case WM_SIZE: {
        bmi.bmiHeader.biWidth = LOWORD(lParam);
        bmi.bmiHeader.biHeight = HIWORD(lParam);

        if (frameBitmap) DeleteObject(frameBitmap);
        frameBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **)&frame.pixels, 0, 0);
        if (frameBitmap == NULL) {
            exit(-1);
        }
        SelectObject(fdc, frameBitmap);

        frame.width = LOWORD(lParam);
        frame.height = HIWORD(lParam);
    } break;

    default: {
        return DefWindowProc(windowHandle, message, wParam, lParam);
    }
    }
    return 0;
}

uint32_t getColor(uint8_t red, uint8_t green, uint8_t blue) {
    uint32_t color = 0;
    color += red;
    color = color << 8;
    color += green;
    color = color << 8;
    color += blue;
    return color;
}

/* putPixelRawVal - Puts a pixel of a specified color on the window, with the bottom left corner as the origin.
*/
void putPixelRawVal(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= frame.width || y >= frame.height) {
        fprintf(stderr, "Pixel out of bounds! x: %d, y: %d\n", x, y);
        return;
    }
    frame.pixels[y * frame.width + x] = color;
}

void putPixel(int32_t x, int32_t y, uint32_t color) {
    int32_t offsetX = x + (frame.width / 2);
    int32_t offsetY = y + (frame.height / 2);
    if (offsetX >= frame.width || offsetY >= frame.height || offsetX < 0 || offsetY < 0) {
        fprintf(stderr, "Pixel out of bounds! x: %d, y: %d\n", x, y);
        return;
    }
    frame.pixels[offsetY * frame.width + offsetX] = color;
}

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    const wchar_t windowClassName[] = L"Ray Tracer";
    static WNDCLASS windowClass = { 0 };
    windowClass.lpfnWndProc = WindowProcessMessage;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = windowClassName;
    RegisterClass(&windowClass);

    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    fdc = CreateCompatibleDC(0);

    static HWND windowHandle;
    windowHandle = CreateWindow(windowClassName, L"Test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        0, 0, 1920, 1080, NULL, NULL, hInstance, NULL);
    if (windowHandle == NULL) { return -1; }

    while (!quit) {
        static MSG message = { 0 };
        while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
            DispatchMessage(&message);
        }

        for (int i = -frame.width / 2; i < frame.width / 2; i++) {
            for (int j = -frame.height / 2; j < frame.height / 2; j++) {
                putPixel(i, j, getColor(i % 256, j % 256, (i + j) % 256));
            }
        }

        InvalidateRect(windowHandle, NULL, FALSE);
        UpdateWindow(windowHandle);
    }

    return 0;
}