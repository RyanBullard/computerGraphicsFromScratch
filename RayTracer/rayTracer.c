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

struct { // Represents the frame we are drawing to.
    int width;
    int height;
    uint32_t *pixels;
} frame = { 0 };

typedef struct vec3 { // Represents both a point in space, and a mathematical vector.
    double x;
    double y;
    double z;
} vec3;

typedef struct rgb { // Stores color information in a more readable way compared to a uint32_t.
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb;

typedef struct pointLight { // Represents light emitted from a singular point in the scene. Similar to a light bulb.
    double intensity;
    vec3 pos;
} pointLight;

typedef struct dirLight { // Represents a light coming from a certain direction. Similar to the sun.
    double intensity;
    vec3 dir;
} dirLight;

typedef struct sphere { // Represents one of the objects in the scene, a sphere.
    vec3 center;
    uint32_t radius;
    rgb color;
    uint32_t specular;
    double reflectivity;
} sphere;

typedef struct sphereList { // Holds the list of all objects in the hard coded scene.
    sphere data;
    struct sphereList *next;
} sphereList;

typedef struct pointLightList { // Holds a list of all point lights placed in the scene.
    pointLight data;
    struct pointLightList *next;
} pointLightList;

typedef struct dirLightList { // Holds a list of all directional lights used in the scene.
    dirLight data;
    struct dirLightList *next;
} dirLightList;

typedef struct light { // Represents the overall lights in the scene.
    double ambient;
    dirLightList *dirList;
    pointLightList *pointList;
} light;

typedef struct sphereResult { // Used to hold information when determining which sphere is closest to the camera.
    double firstT;
    double secondT;
} sphereResult;

typedef struct intersectResult { // Used to hold information about the sphere that may intersect a ray.
    sphere *s;
    double t;
} intersectResult;

static BITMAPINFO bmi; // The header for the bitmap that is drawn to the screen.
static HBITMAP frameBitmap = NULL; // The pointer to the bitmap we draw.
static HDC fdc = NULL; // Represents the device context of our frame.

rgb background; // Holds our background color for the scene.

sphereList sceneList; // Global list of objects in the scene.
light sceneLight; // Global light identifiers.

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

    default: {
        return DefWindowProc(windowHandle, message, wParam, lParam);
    }
    }
    return 0;
}

/*
 * getColor - Converts the color struct to a 32-bit int containing 8 bits of filler, the 8 red bits, the 8 green bits, then the 8
 * blue bits, in that order. Used to set the appropriate pixel in the bitmap to a color.
 */
static uint32_t getColor(rgb c) {
    uint32_t compColor = 0;
    compColor += c.red;
    compColor <<= 8;
    compColor += c.green;
    compColor <<= 8;
    compColor += c.blue;
    return compColor;
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
 * dotProduct - Computes the dot product of two vectors. 
 */
static double dotProduct(vec3 vector1, vec3 vector2) {
    return vector1.x * vector2.x + vector1.y * vector2.y + vector1.z * vector2.z;
}

/*
 * vecSub - Subtracts two vectors component wise.
 */
static vec3 vecSub(vec3 vector1, vec3 vector2) {
    return (vec3) {
        .x = vector1.x - vector2.x,
        .y = vector1.y - vector2.y,
        .z = vector1.z - vector2.z
    };
}

/*
 * vecAdd - Adds to vectors component wise.
 */
static vec3 vecAdd(vec3 vector1, vec3 vector2) {
    return (vec3) {
        .x = vector1.x + vector2.x,
        .y = vector1.y + vector2.y,
        .z = vector1.z + vector2.z
    };
}

/*
 * vecConstMul - Multiplies each component of a vector by a constant.
 */
static vec3 vecConstMul(double constant, vec3 vector) {
    return (vec3) {
        .x = constant * vector.x,
        .y = constant * vector.y,
        .z = constant * vector.z
    };
}

/*
 * magnitude - Computes the magnitude of a 3D vector.
 */
static double magnitude(vec3 vector) {
    return sqrt((vector.x * vector.x) + (vector.y * vector.y) + (vector.z * vector.z));
}

/*
 * normalize - Returns the normalized vector of the passed in vector. That is - each component is divided
 * by the overall magnitude of the vector.
 */
static vec3 normalize(vec3 vector) {
    double mag = magnitude(vector);
    return (vec3) {
        .x = mag * vector.x,
        .y = mag * vector.y,
        .z = mag * vector.z
    };
}

/*
 * reflectRay - Reflects a ray with respect to a normal.
 */
static vec3 reflectRay(vec3 ray, vec3 normal) {
    return vecSub(vecConstMul((2 * dotProduct(normal, ray)), normal), ray);
}

/*
 * colorMul - Multiplies a color by a constant. This function clamps the color down to 255.
 */
static rgb colorMul(rgb *color, double mul) {
    double red = color->red * mul;
    double green = color->green * mul;
    double blue = color->blue * mul;
    
    uint8_t redComp = 0;
    if (red < 255) {
        redComp = (uint8_t)red;
    } else {
        redComp = 255;
    }
    uint8_t greenComp = 0;
    if (green < 255) {
        greenComp = (uint8_t)green;
    } else {
        greenComp = 255;
    }
    uint8_t blueComp = 0;
    if (blue < 255) {
        blueComp = (uint8_t)blue;
    } else {
        blueComp = 255;
    }
    return (rgb) { .red = redComp, .blue = blueComp, .green = greenComp };
}

/*
 * colorMul - Multiplies a color by a constant. This function clamps the color down to 255.
 */
static rgb colorAdd(rgb color, rgb color2) {
    uint32_t red = color.red + color2.red;
    uint32_t green = color.green + color2.green;
    uint32_t blue = color.blue + color2.blue;

    uint8_t redComp = 0;
    if (red < 255) {
        redComp = (uint8_t)red;
    } else {
        redComp = 255;
    }
    uint8_t greenComp = 0;
    if (green < 255) {
        greenComp = (uint8_t)green;
    } else {
        greenComp = 255;
    }
    uint8_t blueComp = 0;
    if (blue < 255) {
        blueComp = (uint8_t)blue;
    } else {
        blueComp = 255;
    }
    return (rgb) { .red = redComp, .blue = blueComp, .green = greenComp };
}

/*
 * intersectRaySphere - This function finds the closest sphere that intersects a given ray.
 * A result of DBL_MAX means that no sphere intersects this ray at any point.
 */
static sphereResult intersectRaySphere(vec3 *origin, vec3 *direction, sphere *s) {
    uint32_t radius = s->radius;
    vec3 offsetO = vecSub(*origin, s->center);

    double a = dotProduct(*direction, *direction);
    double b = 2 * dotProduct(offsetO, *direction);
    double c = dotProduct(offsetO, offsetO) - (radius*radius);

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
    return (intersectResult) { .s = closestSphere, .t = closestT };
}

/*
 * computeLighting - Computes the intensity of lighting at a certain point in the scene.
 */
static double computeLighting(vec3 *point, vec3 *normal, vec3 v, uint32_t spec) {
    double intensity = 0.0;
    intensity += sceneLight.ambient;
    for (dirLightList *dLightNode = sceneLight.dirList; dLightNode != NULL; dLightNode = dLightNode->next) {
        double nDotL = dotProduct(*normal, dLightNode->data.dir);

        intersectResult shadow = closestIntersection(point, &dLightNode->data.dir, 0.001, DBL_MAX);

        if (shadow.s != NULL) {
            continue;
        }

        if (nDotL > 0) {
            intensity += dLightNode->data.intensity * nDotL / (magnitude(*normal) * magnitude(dLightNode->data.dir));
        }

        if (spec != -1) {
            vec3 r = reflectRay(dLightNode->data.dir, *normal);
            double rDotV = dotProduct(r, v);
            if (rDotV > 0) {
                intensity += dLightNode->data.intensity * pow(rDotV / (magnitude(r) * magnitude(v)), spec);
            }
        }
    }

    for (pointLightList *pLightNode = sceneLight.pointList; pLightNode != NULL; pLightNode = pLightNode->next) {
        vec3 pointNorm = vecSub(pLightNode->data.pos, *point);
        double nDotL = dotProduct(*normal, pointNorm);

        intersectResult shadow = closestIntersection(point, &pointNorm, 0.001, 1.0);

        if (shadow.s != NULL) {
            continue;
        }

        if (nDotL > 0) {
            intensity += pLightNode->data.intensity * nDotL / (magnitude(*normal) * magnitude(pointNorm));
        }

        if (spec != -1) {
            vec3 r = reflectRay(pointNorm, *normal);
            double rDotV = dotProduct(r, v);
            if (rDotV > 0) {
                intensity += pLightNode->data.intensity * pow(rDotV / (magnitude(r) * magnitude(v)), spec);
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
    vec3 p = vecAdd(*origin, vecConstMul(closestT, *D));
    vec3 normal = vecSub(p, closestSphere->center);
    normal = normalize(normal);
    vec3 view = vecConstMul(-1, *D);
    rgb localColor = colorMul(&closestSphere->color, computeLighting(&p, &normal, view, closestSphere->specular));

    double r = closestSphere->reflectivity;
    if (depth == 0 || r <= 0.0) {
        return localColor;
    }

    vec3 ray = reflectRay(view, normal); // Get the ray we are looking out of from the surface of the object

    rgb reflectedColor = traceRay(&p, &ray, 0.001, DBL_MAX, depth - 1);

    return colorAdd(colorMul(&localColor, 1 - r), colorMul(&reflectedColor, r));
}

/*
 * renderScene - Does the needed setup, then calls the required functions to render the raytraced scene.
 */
static void renderScene() {
    uint32_t recursionDepth = 3;
    vec3 O = (vec3){ 0 };
    O.x = 0;
    O.y = 0;
    O.z = 0;
    for (int x = -frame.width / 2; x < frame.width / 2; x++) {
        for (int y = -frame.height / 2; y < frame.height / 2 + 1; y++) {
            vec3 D;
            canvasToViewport(x, y, &D);
            rgb c;
            c = traceRay(&O, &D, DISTANCE, DBL_MAX, recursionDepth);
            putPixel(x, y, c);
        }
    }
}

/*
 * WinMain - The main function of a win32 program. Sets up the graphical scene then begins the rendering process.
 */
int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {

    if (!AllocConsole()) { // If the call fails return
        return -1;
    }

    freopen_s((FILE **)stdout, "CONOUT$", "w", stdout); // Reattach stdout to the allocated console
    freopen_s((FILE **)stderr, "CONOUT$", "w", stderr); // Reattach stderr to the allocated console

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

    sceneList.data = red;
    sphereList second = (sphereList){ 0 };
    second.data = blue;
    sphereList third = (sphereList){ 0 };
    third.data = green;
    sphereList fourth = (sphereList){ 0 };
    fourth.data = yellow;

    sceneList.next = &second;
    second.next = &third;
    third.next = &fourth;
    fourth.next = NULL;

    background.red = 0;
    background.green = 0;
    background.blue = 0;

    //Build our list of lights in the scene
    pointLight pLight = (pointLight){ .intensity = 0.6, .pos = (vec3){ .x=2.0, .y = 1.0, .z = 0.0} };
    dirLight dLight = (dirLight){ .intensity = 0.2, .dir = (vec3){.x = 1.0, .y = 4.0, .z = 4.0} };

    dirLightList dList = (dirLightList){ .data = dLight, .next = NULL };
    pointLightList pList = (pointLightList){ .data = pLight, .next = NULL };

    sceneLight = (light){ .ambient = 0.2, .dirList = &dList, .pointList = &pList };

    while (!quit) {
        static MSG message = { 0 };
        while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
            DispatchMessage(&message);
        }

        renderScene();

        InvalidateRect(windowHandle, NULL, FALSE);
        UpdateWindow(windowHandle);
    }
    FreeConsole();
    return 0;
}