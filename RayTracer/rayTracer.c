#include <windows.h>
#include <ConsoleApi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <float.h>
#include <limits.h>

#define VIEWPORT_WIDTH 1
#define VIEWPORT_HEIGHT 1
#define DISTANCE 1

static bool quit = false;

struct {
    int width;
    int height;
    uint32_t *pixels;
} frame = { 0 };

typedef struct vec3 {
    double x;
    double y;
    double z;
} vec3;

typedef struct rgb {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb;

typedef struct sphere {
    vec3 center;
    uint32_t radius;
    rgb color;
} sphere;

typedef struct sphereList {
    sphere data;
    struct sphereList *next;
} sphereList;

typedef struct sphereResult {
    double firstT;
    double secondT;
} sphereResult;

static BITMAPINFO bmi;
static HBITMAP frameBitmap = 0;
static HDC fdc = 0;

sphere red;
sphere blue;
sphere green;

rgb background;

sphereList sceneList;

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

uint32_t getColor(rgb *c) {
    uint32_t compColor = 0;
    compColor += c->red;
    compColor = compColor << 8;
    compColor += c->green;
    compColor = compColor << 8;
    compColor += c->blue;
    return compColor;
}

/* 
 * putPixelRawVal - Puts a pixel of a specified color on the window, with the bottom left corner as the origin.
 * This function does check that the position is valid. If there is an issue, it prints the attempted value to stderr.
 */
void putPixelRawVal(int32_t x, int32_t y, rgb *c) {
    if (x >= frame.width || y >= frame.height) {
        fprintf(stderr, "Pixel out of bounds! x: %d, y: %d\n", x, y);
        return;
    }
    frame.pixels[y * frame.width + x] = getColor(c);
}

/*
 * putPixel - Plots a pixel on the screen of a specified color using the center of the screen as the origin.
 * This function does check that the position is valid. If there is an issue, it prints the attempted value to stderr.
 */
void putPixel(int32_t x, int32_t y, rgb *c) {
    int32_t offsetX = x + (frame.width / 2);
    int32_t offsetY = y + (frame.height / 2);
    if (offsetX > frame.width || offsetY > frame.height || offsetX < 0 || offsetY < 0) {
        fprintf(stderr, "Pixel out of bounds! x: %d, y: %d\n", x, y);
        return;
    }
    frame.pixels[offsetY * frame.width + offsetX] = getColor(c);
}

void canvasToViewport(int x, int y, vec3 *dest) {
    dest->x = x * ((double) VIEWPORT_WIDTH / frame.width);
    dest->y = y * ((double) VIEWPORT_HEIGHT / frame.height);
    dest->z = (double)DISTANCE;
}

double dotProduct(vec3 *vector1, vec3 *vector2) {
    return vector1->x * vector2->x + vector1->y * vector2->y + vector1->z * vector2->z;
}

vec3 vecSub(vec3 *vector1, vec3 *vector2) {
    return (vec3) { .x = vector1->x - vector2->x,
        .y = vector1->y - vector2->y,
        .z = vector1->z - vector2->z
    };
}

sphereResult intersectRaySphere(vec3 *origin, vec3 *direction, sphere *s) {
    uint32_t radius = s->radius;
    vec3 offsetO = vecSub(origin, &s->center);

    double a = dotProduct(direction, direction);
    double b = 2 * dotProduct(&offsetO, direction);
    double c = dotProduct(&offsetO, &offsetO) - (radius*radius);

    double discriminant = (b * b) - (4 * a * c);

    if (discriminant < 0) {
        return (sphereResult) {.firstT = DBL_MAX, .secondT = DBL_MAX };
    }
    double t1 = (-b + sqrt(discriminant)) / (2 * a);
    double t2 = (-b - sqrt(discriminant)) / (2 * a);
    return (sphereResult) { .firstT = t1, .secondT = t2 };
}

rgb* traceRay(vec3 *origin, vec3 *D, uint32_t t_min, uint32_t t_max) {
    double closestT = DBL_MAX;
    sphere *closestSphere = NULL;
    for (sphereList *node = &sceneList; node != NULL; node = node->next) {
        sphereResult result = intersectRaySphere(origin, D, &node->data);
        if (result.firstT >= t_min && result.firstT < t_max && result.firstT < closestT) {
            closestT = result.firstT;
            closestSphere = &node->data;
        }

        if (result.secondT >= t_min && result.secondT < t_max && result.secondT < closestT) {
            closestT = result.secondT;
            closestSphere = &node->data;
        }
    }
    if (closestSphere == NULL) {
        return &background;
    }
    return &closestSphere->color;
}

void renderScene() {
    vec3 O;
    O.x = 0;
    O.y = 0;
    O.z = 0;
    for (int x = -frame.width / 2; x < frame.width / 2; x++) {
        for (int y = -frame.height / 2; y < frame.height / 2; y++) {
            vec3 D;
            canvasToViewport(x, y, &D);
            rgb *c;
            c = traceRay(&O, &D, DISTANCE, INT32_MAX);
            putPixel(x, y, c);
        }
    }
}

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {

    if (!AllocConsole()) {
        return -1;
    }

    freopen_s((FILE **)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE **)stderr, "CONOUT$", "w", stderr);

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

    HWND windowHandle = CreateWindow(windowClassName, L"Ray Tracer", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        0, 0, 500, 500, NULL, NULL, hInstance, NULL);
    if (windowHandle == NULL) {
        return -1;
    }

    red = (sphere){ .center = (vec3){.x = 0.0, .y = -1.0, .z = -3.0},
    .radius = 1,
    .color = (rgb) {.red = 255, .green = 0, .blue = 0 }
    };

    blue = (sphere){ .center = (vec3){.x = 2.0, .y = 0.0, .z = 4.0},
        .radius = 1,
        .color = (rgb) {.red = 0, .green = 0, .blue = 255 }
    };

    green = (sphere){ .center = (vec3){.x = -2.0, .y = 0.0, .z = 4.0},
        .radius = 1,
        .color = (rgb) {.red = 0, .green = 255, .blue = 0 }
    };

    // Build our list of spheres in the scene
    sceneList.data = red;
    sphereList second;
    second.data = blue;
    sphereList third;
    third.data = green;

    sceneList.next = &second;
    second.next = &third;
    third.next = NULL;

    background.red = 255;
    background.green = 255;
    background.blue = 255;


    while (!quit) {
        static MSG message = { 0 };
        while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
            DispatchMessage(&message);
        }

        renderScene();

        InvalidateRect(windowHandle, NULL, FALSE);
        UpdateWindow(windowHandle);
    }

    return 0;
}