#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <time.h>

#include "color.h"
#include "vec3.h"
#include "light.h"
#include "sphere.h"
#include "missingKeys.h"
#include "standardHeader.h"

#define VIEWPORT_WIDTH 1
#define VIEWPORT_HEIGHT 1
#define DISTANCE 1
#define M_PI 3.14159265358979323846
#define M_2PI 6.2831853071795865
#define FRAMESPERSECOND 60
#define MAXTHREADS 10
#define MOVESPEED 10

static bool quit = false;

struct { // Represents the frame we are drawing to.
    int width;
    int height;
    uint32_t *pixels;
} frame = { 0 };

typedef struct intersectResult { // Used to hold information about the sphere that may intersect a ray.
    sphere *s;
    double t;
} intersectResult;

typedef struct camInfo {
    double xRot;
    double yRot;
    double zRot;

    vec3 cameraPos;
} camInfo;

static BITMAPINFO bmi; // The header for the bitmap that is drawn to the screen.
static HBITMAP frameBitmap = NULL; // The pointer to the bitmap we draw.
static HDC fdc = NULL; // Represents the device context of our frame.

static rgb background = { // Holds our background color for the scene.
    .red = 0,
    .green = 0,
    .blue = 0
}; 

sphereList *sceneList; // Global list of objects in the scene.
light *sceneLight; // Global light identifiers.

vec3 x = { // The x unit vector in 3 space.
    .x = 1,
    .y = 0,
    .z = 0
};

vec3 y = { // The y unit vector in 3 space.
    .x = 0,
    .y = 1,
    .z = 0
};

vec3 z = { // The z unit vector in 3 space.
    .x = 0,
    .y = 0,
    .z = 1
};

camInfo camera = {
    .xRot = 0.0,
    .yRot = 0.0,
    .zRot = 0.0,

    .cameraPos = {
        .x = 0.0,
        .y = 0.0,
        .z = 0.0
    }
};

double rotMatrix[3][3] = { 0 };

double deltaTime = 1000000 / FRAMESPERSECOND;

LARGE_INTEGER frequency;
LARGE_INTEGER t1, t2;

HANDLE hThreads[MAXTHREADS] = { NULL };

/*
 * generateRotationMatrix - Generates the 3D rotation matrix corresponding to the current roll, yaw, and pitch of the
 * camera. This is regenerated and cached each frame, but can accept an arbitrary destination if needed.
 */
static void generateRotMatrix(double dest[3][3]) {

    double sinAlpha = sin(camera.zRot);
    double cosAlpha = cos(camera.zRot);
    double sinBeta = sin(camera.yRot);
    double cosBeta = cos(camera.yRot);
    double sinGamma = sin(camera.xRot);
    double cosGamma = cos(camera.xRot);

    dest[0][0] = cosAlpha * cosBeta;
    dest[0][1] = (cosAlpha * sinBeta * sinGamma) - (sinAlpha * cosGamma);
    dest[0][2] = (cosAlpha * sinBeta * cosGamma) + (sinAlpha * sinGamma);

    dest[1][0] = sinAlpha * cosBeta;
    dest[1][1] = (sinAlpha * sinBeta * sinGamma) + (cosAlpha * cosGamma);
    dest[1][2] = (sinAlpha * sinBeta * cosGamma) - (cosAlpha * sinGamma);

    dest[2][0] = -sinBeta;
    dest[2][1] = cosBeta * sinGamma;
    dest[2][2] = cosBeta * cosGamma;
}

/*
 * generate2DRotationMatrix - Generates the 2D rotation matrix corresponding to the current roll and pitch of the
 * camera. Allows for the camera to not move up or down when looking up or down, with full speed movement.
 */
static void generate2DRotMatrix(double dest[3][3]) {

    double sinAlpha = sin(camera.zRot);
    double cosAlpha = cos(camera.zRot);
    double sinBeta = sin(camera.yRot);
    double cosBeta = cos(camera.yRot);

    dest[0][0] = cosAlpha * cosBeta;
    dest[0][1] =  -sinAlpha;
    dest[0][2] = cosAlpha * sinBeta;

    dest[1][0] = sinAlpha * cosBeta;
    dest[1][1] = cosAlpha;
    dest[1][2] = sinAlpha * sinBeta;

    dest[2][0] = -sinBeta;
    dest[2][1] = 0;
    dest[2][2] = cosBeta;
}

/*
 * normalizeRotation - Ensures the rotation of the camera remains in the bounds [0, 2Pi]. Ensures that rotation does
 * not underflow or overflow.
 */
static void normalizeRotation() {
    if (camera.xRot > M_2PI) {
        camera.xRot -= M_2PI;
    }

    if (camera.yRot > M_2PI) {
        camera.yRot -= M_2PI;
    }

    if (camera.zRot > M_2PI) {
        camera.zRot -= M_2PI;
    }
}

/*
 * WindowProcessMessage - Handler to process messages sent from windows to this program.
 */
static LRESULT CALLBACK WindowProcessMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) {
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

        case WM_KEYDOWN: {
            double rot[3][3];
            generate2DRotMatrix(rot);

            vec3 totalMovement;
            vec3 movementX = { 0 };
            vec3 movementZ = { 0 };

            if (GetAsyncKeyState(VK_W) < 0) { // These keys can all be held down at once
                movementZ = multiplyMV(rot, &z);
            }

            if (GetAsyncKeyState(VK_S) < 0) {
                movementZ = multiplyMV(rot, &z);
                movementZ = vecConstMul(-1, &movementZ);
            }

            if (GetAsyncKeyState(VK_A) < 0) {
                movementX = multiplyMV(rot, &x);
                movementX = vecConstMul(-1, &movementX);
            }

            if (GetAsyncKeyState(VK_D) < 0) {
                movementX = multiplyMV(rot, &x);
            }

            totalMovement = vecAdd(&movementX, &movementZ);
            normalize(&totalMovement);
            totalMovement = vecConstMul(MOVESPEED * deltaTime, &totalMovement);

            camera.cameraPos = vecAdd(&totalMovement, &camera.cameraPos);

            if (GetAsyncKeyState(VK_SPACE) < 0) {
                camera.cameraPos.y += MOVESPEED * deltaTime; // These will always be relative to flat y axis to not lose orientation.
            }

            if (GetAsyncKeyState(VK_SHIFT) < 0) {
                camera.cameraPos.y -= MOVESPEED * deltaTime;
            }

            switch(wParam) { // Only allow one of these at once
                case VK_Q: {
                    camera.yRot -= M_2PI * deltaTime;
                } break;
                case VK_E: {
                    camera.yRot += M_2PI * deltaTime;
                } break;

                case VK_R: {
                    camera.cameraPos.x = 0;
                    camera.cameraPos.y = 0;
                    camera.cameraPos.z = 0;
                    camera.xRot = 0;
                    camera.yRot = 0;
                    camera.zRot = 0;
                }break;

                case VK_T: {
                    camera.xRot = 0;
                    camera.yRot = 0;
                    camera.zRot = 0;
                }break;

                case VK_P: {
                    camera.yRot += M_PI;
                } break;

                case VK_UP: {
                    camera.xRot -= M_2PI * deltaTime;
                } break;
                case VK_DOWN: {
                    camera.xRot += M_2PI * deltaTime;
                } break;

                case VK_RIGHT: {
                    camera.zRot += M_2PI * deltaTime;
                } break;
                case VK_LEFT: {
                    camera.zRot -= M_2PI * deltaTime;
                } break;

                case VK_J: {
                    addSphere(sceneList, camera.cameraPos, (rgb) { .red = 160, .green = 32, .blue = 240 }, 2, 600, 0.1);
                }break;

                case VK_L: {
                    addPLight(sceneLight, camera.cameraPos, 0.5);
                }break;
            }
            normalizeRotation();
        } break;

        default: {
            return DefWindowProc(windowHandle, message, wParam, lParam);
        }
    }
    return 0;
}

/* 
 * putPixelRawVal - Puts a pixel of a specified color on the window, with the bottom left corner as the origin.
 * This function does check that the position is valid. If there is an issue, it prints the attempted value to stderr.
 */
static void putPixelRawVal(int32_t x, int32_t y, rgb c) {
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
static void putPixel(int32_t x, int32_t y, rgb c) {
    int32_t offsetX = x + (frame.width / 2);
    int32_t offsetY = y + (frame.height / 2);
    if (offsetX > frame.width || offsetY > frame.height || offsetX < 0 || offsetY < 0) {
        fprintf(stderr, "Pixel out of bounds! x: %d, y: %d\n", x, y);
        return;
    }
    frame.pixels[offsetY * frame.width + offsetX] = getColor(c);
}

/*
 * canvasToViewport - Converts a screen space coordinate to a coordinate in the 3D view plane.
 */
static void canvasToViewport(int x, int y, vec3 *dest) {
    dest->x = x * ((double) VIEWPORT_WIDTH / frame.width);
    dest->y = y * ((double) VIEWPORT_HEIGHT / frame.height);
    dest->z = (double)DISTANCE;
}

/*
 * intersectRaySphere - This function finds the closest sphere that intersects a given ray.
 * A result of DBL_MAX means that no sphere intersects this ray at any point.
 */
static sphereResult intersectRaySphere(vec3 *origin, vec3 *direction, sphere *s, double dDotD) {
    uint32_t radiusSquare = s->rSquare;
    vec3 offsetO = vecSub(origin, &s->center);

    double a = dDotD;
    double b = 2 * dotProduct(&offsetO, direction);
    double c = dotProduct(&offsetO, &offsetO) - radiusSquare;

    double discriminant = (b * b) - (4 * a * c);

    if (discriminant < 0) {
        return (sphereResult) {.firstT = DBL_MAX, .secondT = DBL_MAX };
    }
    double t1 = (-b + sqrt(discriminant)) / (2 * a);
    double t2 = (-b - sqrt(discriminant)) / (2 * a);
    return (sphereResult) { .firstT = t1, .secondT = t2 };
}

/*
 * closestIntersection - Finds the closest sphere to a point that intersects a given vector. If the .s field of the
 * returned intersectResult struct is NULL, then no sphere intersects this vector.
 */
static intersectResult closestIntersection(vec3 *origin, vec3 *D, double t_min, double t_max, double dDotD) {
    double closestT = DBL_MAX;
    sphere *closestSphere = NULL;
    for (sphereList *node = sceneList; node != NULL; node = node->next) {
        sphereResult result = intersectRaySphere(origin, D, node->data, dDotD);

        if (result.firstT > t_min && result.firstT < t_max && result.firstT < closestT) {
            closestT = result.firstT;
            closestSphere = node->data;
        }

        if (result.secondT > t_min && result.secondT < t_max && result.secondT < closestT) {
            closestT = result.secondT;
            closestSphere = node->data;
        }
    }
    return (intersectResult) { .s = closestSphere, .t = closestT };
}

/*
 * anyIntersection - Returns if there is any intersection with this ray at all. Used for shadow calculations.
 * With shadows, we only care if the directed light is blocked at all, instead of finding the closest.
 */
static bool anyIntersection(vec3 *origin, vec3 *D, double t_min, double t_max, double dDotD) {
    for (sphereList *node = sceneList; node != NULL; node = node->next) {
        sphereResult result = intersectRaySphere(origin, D, node->data, dDotD);

        if (result.firstT > t_min && result.firstT < t_max) {
            return true;
        }

        if (result.secondT > t_min && result.secondT < t_max) {
            return true;
        }
    }
    return false;
}

/*
 * computeLighting - Computes the intensity of lighting at a certain point in the scene.
 */
static double computeLighting(vec3 *point, vec3 *normal, vec3 v, uint32_t spec) {
    double intensity = 0.0;
    intensity += sceneLight->ambient;
    for (dirLightList *dLightNode = sceneLight->dirList; dLightNode != NULL; dLightNode = dLightNode->next) {
        double nDotL = dotProduct(normal, &dLightNode->data->dir);
        intersectResult shadow = closestIntersection(point, &dLightNode->data->dir, 0.001, DBL_MAX,
            dotProduct(&dLightNode->data->dir, &dLightNode->data->dir));

        if (shadow.s != NULL) {
            continue;
        }

        if (nDotL > 0) {
            intensity += dLightNode->data->intensity * nDotL / (magnitude(normal) * magnitude(&dLightNode->data->dir));
        }

        intensity += dLightNode->data->intensity * nDotL / (magnitude(normal) * magnitude(&dLightNode->data->dir));

        if (spec != -1) {
            vec3 r = reflectRay(&dLightNode->data->dir, normal);
            double rDotV = dotProduct(&r, &v);
            if (rDotV > 0) {
                intensity += dLightNode->data->intensity * pow(rDotV / (magnitude(&r) * magnitude(&v)), spec);
            }
        }
    }

    for (pointLightList *pLightNode = sceneLight->pointList; pLightNode != NULL; pLightNode = pLightNode->next) {
        vec3 pointNorm = vecSub(&pLightNode->data->pos, point);
        double nDotL = dotProduct(normal, &pointNorm);

        if (anyIntersection(point, &pointNorm, 0.001, 1.0, dotProduct(&pointNorm, &pointNorm))) {
            continue;
        }

        if (nDotL > 0) {
            intensity += pLightNode->data->intensity * nDotL / (magnitude(normal) * magnitude(&pointNorm));
        }

        if (spec != -1) {
            vec3 r = reflectRay(&pointNorm, normal);
            double rDotV = dotProduct(&r, &v);
            if (rDotV > 0) {
                intensity += pLightNode->data->intensity * pow(rDotV / (magnitude(&r) * magnitude(&v)), spec);
            }
        }
    }

    return intensity;
}

/*
 * traceRay - Follows a ray from the view plane into the scene, and finds the color that needs to be plotted.
 */
static rgb traceRay(vec3 *origin, vec3 *D, double t_min, double t_max, uint32_t depth) {

    double dDotD = dotProduct(D, D);

    intersectResult res = closestIntersection(origin, D, t_min, t_max, dDotD);

    sphere *closestSphere = res.s;
    double closestT = res.t;
    
    if (closestSphere == NULL) {
        return background;
    }
    vec3 tD = vecConstMul(closestT, D);
    vec3 p = vecAdd(origin, &tD);
    vec3 normal = vecSub(&p, &closestSphere->center);
    normalize(&normal);
    vec3 view = vecConstMul(-1, D);
    rgb localColor = colorMul(closestSphere->color, computeLighting(&p, &normal, view, closestSphere->specular));

    double r = closestSphere->reflectivity;
    if (depth == 0 || r <= 0.0) {
        return localColor;
    }

    vec3 ray = reflectRay(&view, &normal); // Get the ray we are looking out of from the surface of the object

    rgb reflectedColor = traceRay(&p, &ray, 0.001, DBL_MAX, depth - 1);

    return colorAdd(colorMul(localColor, 1 - r), colorMul(reflectedColor, r)); // Blend the colors of the reflection and the actual color.
}

void renderOnThreadID(void* pMyID) {
    int MyID = (int)(uintptr_t)pMyID;
    uint32_t recursionDepth = 3;
    for (int x = -frame.width / 2; x < frame.width / 2; x++) {
        for (int y = -frame.height / 2 + (frame.height / MAXTHREADS) * MyID; 
            y < -frame.height / 2 + (frame.height / MAXTHREADS) * (MyID + 1) + 1; y++) { // Evil hack to get each thread to render the same amount of lines
            vec3 D;
            canvasToViewport(x, y, &D);
            D = multiplyMV(rotMatrix, &D);
            rgb c = traceRay(&camera.cameraPos, &D, DISTANCE, DBL_MAX, recursionDepth);
            putPixel(x, y, c);
        }
    }
}

/*
 * renderScene - Does the needed setup, then calls the required functions to render the raytraced scene.
 */
static void renderScene() {
    for (int i = 0; i < MAXTHREADS; i++) {
        hThreads[i] = (HANDLE)_beginthread(renderOnThreadID, 0, (void *)(uintptr_t)i);
    }
    WaitForMultipleObjects(MAXTHREADS, hThreads, TRUE, INFINITE);
}

/*
 * WinMain - The main function of a win32 program. Sets up the graphical scene then begins the rendering process.
 */
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

    HWND windowHandle = CreateWindow(windowClassName, L"Ray Tracer", 
        ((WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME) ^ WS_MAXIMIZEBOX) | WS_VISIBLE, 0, 0, 500, 500,
        NULL, NULL, hInstance, NULL);
    if (windowHandle == NULL) {
        return -1;
    }

    // Build our list of spheres in the scene

    sceneList = initSpheres();
    addSphere(sceneList, (vec3) { .x = 0.0, .y = -1.0, .z = 3.0 }, (rgb) { .red = 255, .green = 0, .blue = 0 },
        1, 500, 0.2);
    addSphere(sceneList, (vec3) { .x = 2.0, .y = 0.0, .z = 4.0 }, (rgb) { .red = 0, .green = 0, .blue = 255 },
        1, 500, 0.3);
    addSphere(sceneList, (vec3) { .x = -2.0, .y = 0.0, .z = 4.0 }, (rgb) { .red = 0, .green = 255, .blue = 0 },
        1, 10, 0.4);
    addSphere(sceneList, (vec3) { .x = 0.0, .y = -5001.0, .z = 0.0 }, (rgb) { .red = 255, .green = 255, .blue = 0 },
        5000, 1000, 0.5);

    sceneLight = initLights();

    addPLight(sceneLight, (vec3) { .x = 2.0, .y = 1.0, .z = 0.0 }, 0.6);
    addDLight(sceneLight, (vec3) { .x = 1.0, .y = 4.0, .z = 4.0 }, 0.2);
    setAmbient(sceneLight, 0.2);

    while (!quit) {

        QueryPerformanceFrequency(&frequency);

        QueryPerformanceCounter(&t1);

        static MSG message = { 0 };
        while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        generateRotMatrix(rotMatrix);

        renderScene();

        InvalidateRect(windowHandle, NULL, FALSE);
        UpdateWindow(windowHandle);

        QueryPerformanceCounter(&t2);

        deltaTime = (double)(t2.QuadPart - t1.QuadPart) / frequency.QuadPart;
    }

    freeLights(sceneLight);
    freeSphereList(sceneList);
    return 0;
}