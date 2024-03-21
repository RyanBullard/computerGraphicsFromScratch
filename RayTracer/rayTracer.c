#include <windows.h>
#include <ConsoleApi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <float.h>
#include <limits.h>

#include "color.h"
#include "vec3.h"
#include "light.h"
#include "sphere.h"
#include "missingKeys.h"

#define VIEWPORT_WIDTH 1
#define VIEWPORT_HEIGHT 1
#define DISTANCE 1
#define M_PI 3.14159265358979323846
#define M_2PI 6.2831853071795865

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

sphereList sceneList; // Global list of objects in the scene.
light sceneLight; // Global light identifiers.

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

/*
 * generateRotationMatrix - Generates the 3D rotation matrix corresponding to the current roll, yaw, and pitch of the camera.
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
 * normalizeRotation - Ensures the rotation of the camera remains in the bounds [0, 2Pi].
 */
void normalizeRotation() {
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
            switch (wParam) {
                case VK_W: {
                    camera.cameraPos.z += 0.1;
                } break;
                case VK_S: {
                    camera.cameraPos.z -= 0.1;
                } break;

                case VK_A: {
                    camera.cameraPos.x -= 0.1;
                } break;
                case VK_D: {
                    camera.cameraPos.x += 0.1;
                } break;

                case VK_SPACE: {
                    camera.cameraPos.y += 0.1;
                } break;
                case VK_SHIFT: {
                    camera.cameraPos.y -= 0.1;
                } break;

                case VK_Q: {
                    camera.yRot -= 0.01 * M_PI;
                } break;
                case VK_E: {
                    camera.yRot += 0.01 * M_PI;
                } break;

                case VK_R: {
                    camera.cameraPos.x = 0;
                    camera.cameraPos.y = 0;
                    camera.cameraPos.z = 0;
                    camera.xRot = 0;
                    camera.yRot = 0;
                    camera.zRot = 0;
                }break;

                case VK_P: {
                    camera.yRot += M_PI;
                } break;

                case VK_UP: {
                    camera.xRot -= 0.05 * M_PI;
                } break;
                case VK_DOWN: {
                    camera.xRot += 0.05 * M_PI;
                } break;

                case VK_RIGHT: {
                    camera.zRot += 0.05 * M_PI;
                } break;
                case VK_LEFT: {
                    camera.zRot -= 0.05 * M_PI;
                } break;
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
static sphereResult intersectRaySphere(vec3 *origin, vec3 *direction, sphere *s) {
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

/*
 * closestIntersection - Finds the closest sphere to a point that intersects a given vector. If the .s field of the
 * returned intersectResult struct is NULL, then no sphere intersects this vector.
 */
static intersectResult closestIntersection(vec3 *origin, vec3 *D, double t_min, double t_max) {
    double closestT = DBL_MAX;
    sphere *closestSphere = NULL;
    for (sphereList *node = &sceneList; node != NULL; node = node->next) {
        sphereResult result = intersectRaySphere(origin, D, node->data);

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
 * computeLighting - Computes the intensity of lighting at a certain point in the scene.
 */
static double computeLighting(vec3 *point, vec3 *normal, vec3 v, uint32_t spec) {
    double intensity = 0.0;
    intensity += sceneLight.ambient;
    for (dirLightList *dLightNode = sceneLight.dirList; dLightNode != NULL; dLightNode = dLightNode->next) {
        double nDotL = dotProduct(normal, &dLightNode->data.dir);
        intersectResult shadow = closestIntersection(point, &dLightNode->data.dir, 0.001, DBL_MAX);

        if (shadow.s != NULL) {
            continue;
        }

        if (nDotL > 0) {
            intensity += dLightNode->data.intensity * nDotL / (magnitude(normal) * magnitude(&dLightNode->data.dir));
        }

        if (spec != -1) {
            vec3 r = reflectRay(&dLightNode->data.dir, normal);
            double rDotV = dotProduct(&r, &v);
            if (rDotV > 0) {
                intensity += dLightNode->data.intensity * pow(rDotV / (magnitude(&r) * magnitude(&v)), spec);
            }
        }
    }

    for (pointLightList *pLightNode = sceneLight.pointList; pLightNode != NULL; pLightNode = pLightNode->next) {
        vec3 pointNorm = vecSub(&pLightNode->data.pos, point);
        double nDotL = dotProduct(normal, &pointNorm);

        intersectResult shadow = closestIntersection(point, &pointNorm, 0.001, 1.0);

        if (shadow.s != NULL) {
            continue;
        }

        if (nDotL > 0) {
            intensity += pLightNode->data.intensity * nDotL / (magnitude(normal) * magnitude(&pointNorm));
        }

        if (spec != -1) {
            vec3 r = reflectRay(&pointNorm, normal);
            double rDotV = dotProduct(&r, &v);
            if (rDotV > 0) {
                intensity += pLightNode->data.intensity * pow(rDotV / (magnitude(&r) * magnitude(&v)), spec);
            }
        }
    }

    return intensity;
}

/*
 * traceRay - Follows a ray from the view plane into the scene, and finds the color that needs to be plotted.
 */
static rgb traceRay(vec3 *origin, vec3 *D, double t_min, double t_max, uint32_t depth) {
    intersectResult res = closestIntersection(origin, D, t_min, t_max);

    sphere *closestSphere = res.s;
    double closestT = res.t;
    
    if (closestSphere == NULL) {
        return background;
    }
    vec3 tD = vecConstMul(closestT, D);
    vec3 p = vecAdd(origin, &tD);
    vec3 normal = vecSub(&p, &closestSphere->center);
    normal = normalize(&normal);
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

/*
 * renderScene - Does the needed setup, then calls the required functions to render the raytraced scene.
 */
static void renderScene() {
    uint32_t recursionDepth = 3;
    double rotMatrix[3][3] = { 0 };
    generateRotMatrix(rotMatrix);
    for (int x = -frame.width / 2; x < frame.width / 2; x++) {
        for (int y = -frame.height / 2; y < frame.height / 2 + 1; y++) {
            vec3 D;
            canvasToViewport(x, y, &D);
            D = multiplyMV(rotMatrix, &D);
            rgb c = traceRay(&camera.cameraPos, &D, DISTANCE, DBL_MAX, recursionDepth);
            putPixel(x, y, c);
        }
    }
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

    HWND windowHandle = CreateWindow(windowClassName, L"Ray Tracer", ((WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME) ^ WS_MAXIMIZEBOX) | WS_VISIBLE,
        0, 0, 500, 500, NULL, NULL, hInstance, NULL);
    if (windowHandle == NULL) {
        return -1;
    }

    // Build our list of spheres in the scene

    sphere red = (sphere){ .center = (vec3){.x = 0.0, .y = -1.0, .z = 3.0},
        .radius = 1,
        .color = (rgb) {.red = 255, .green = 0, .blue = 0 },
        .specular = 500,
        .reflectivity = 0.2
    };

    sphere blue = (sphere){ .center = (vec3){.x = 2.0, .y = 0.0, .z = 4.0},
        .radius = 1,
        .color = (rgb) {.red = 0, .green = 0, .blue = 255 },
        .specular = 500,
        .reflectivity = 0.3
    };

    sphere green = (sphere){ .center = (vec3){.x = -2.0, .y = 0.0, .z = 4.0},
        .radius = 1,
        .color = (rgb) {.red = 0, .green = 255, .blue = 0 },
        .specular = 10,
        .reflectivity = 0.4
    };

    sphere yellow = (sphere){ .center = (vec3){.x = 0.0, .y = -5001.0, .z = 0.0},
        .radius = 5000,
        .color = (rgb) {.red = 255, .green = 255, .blue = 0 },
        .specular = 1000,
        .reflectivity = 0.5
    };

    sceneList.data = &red;
    sphereList second = (sphereList){ 0 };
    second.data = &blue;
    sphereList third = (sphereList){ 0 };
    third.data = &green;
    sphereList fourth = (sphereList){ 0 };
    fourth.data = &yellow;

    sceneList.next = &second;
    second.next = &third;
    third.next = &fourth;
    fourth.next = NULL;

    //Build our list of lights in the scene
    pointLight pLight = (pointLight){
        .intensity = 0.6,
        .pos = (vec3){
            .x = 2.0,
            .y = 1.0,
            .z = 0.0
        }
    };
    dirLight dLight = (dirLight){
        .intensity = 0.2,
        .dir = (vec3){
            .x = 1.0,
            .y = 4.0,
            .z = 4.0
        } 
    };

    dirLightList dList = (dirLightList){
        .data = dLight,
        .next = NULL
    };
    pointLightList pList = (pointLightList){
        .data = pLight,
        .next = NULL
    };

    sceneLight = (light){
        .ambient = 0.2,
        .dirList = &dList,
        .pointList = &pList
    };

    while (!quit) {
        static MSG message = { 0 };
        while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        renderScene();

        InvalidateRect(windowHandle, NULL, FALSE);
        UpdateWindow(windowHandle);
    }
    return 0;
}