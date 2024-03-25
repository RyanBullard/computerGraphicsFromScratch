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

const int VIEWPORT_WIDTH = 2;
const int VIEWPORT_HEIGHT = 2;
const int DISTANCE = 1;
const double M_PI = 3.14159265358979323846;
const double M_2PI = 6.2831853071795865;
#define FRAMESPERSECOND 60
#define MAXTHREADS 10
const int MOVESPEED = 5;
const double sensitivity = 0.001;

static bool quit = false;
static bool pauseCursorLock = false;

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

const sphereList *sceneList; // Global list of objects in the scene.
const light *sceneLight; // Global light identifiers.

const vec3 x = { // The x unit vector in 3 space.
	.x = 1,
	.y = 0,
	.z = 0
};

const vec3 y = { // The y unit vector in 3 space.
	.x = 0,
	.y = 1,
	.z = 0
};

const vec3 z = { // The z unit vector in 3 space.
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

double rotMatrix[3][3] = { 0 }; // Global matrices, so we can reuse the rotation each frame.
double rot2D[3][3] = { 0 };

double deltaTime = 1000000 / FRAMESPERSECOND;

const LARGE_INTEGER frequency;
const LARGE_INTEGER t1, t2;

HCURSOR pointer = NULL;
POINT mouseLoc;
RECT screenCenter;
int centerX = 0;
int centerY = 0;

HANDLE hThreads[MAXTHREADS] = { NULL };

/*
 * generateRotationMatrix - Generates the 3D rotation matrix corresponding to the current roll, yaw, and pitch of the
 * camera. This is regenerated and cached each frame, but can accept an arbitrary destination if needed.
 */
static void generateRotMatrix() {

	const double sinAlpha = sin(camera.zRot);
	const double cosAlpha = cos(camera.zRot);
	const double sinBeta = sin(camera.yRot);
	const double cosBeta = cos(camera.yRot);
	const double sinGamma = sin(camera.xRot);
	const double cosGamma = cos(camera.xRot);

	rotMatrix[0][0] = cosAlpha * cosBeta;
	rotMatrix[0][1] = (cosAlpha * sinBeta * sinGamma) - (sinAlpha * cosGamma);
	rotMatrix[0][2] = (cosAlpha * sinBeta * cosGamma) + (sinAlpha * sinGamma);

	rotMatrix[1][0] = sinAlpha * cosBeta;
	rotMatrix[1][1] = (sinAlpha * sinBeta * sinGamma) + (cosAlpha * cosGamma);
	rotMatrix[1][2] = (sinAlpha * sinBeta * cosGamma) - (cosAlpha * sinGamma);

	rotMatrix[2][0] = -sinBeta;
	rotMatrix[2][1] = cosBeta * sinGamma;
	rotMatrix[2][2] = cosBeta * cosGamma;
}

/*
 * generate2DRotationMatrix - Generates the 2D rotation matrix corresponding to the current roll and pitch of the
 * camera. Allows for the camera to not move up or down when looking up or down, with full speed movement.
 */
static void generate2DRotMatrix() {

	const double sinAlpha = sin(camera.zRot);
	const double cosAlpha = cos(camera.zRot);
	const double sinBeta = sin(camera.yRot);
	const double cosBeta = cos(camera.yRot);

	rot2D[0][0] = cosAlpha * cosBeta;
	rot2D[0][1] =  -sinAlpha;
	rot2D[0][2] = cosAlpha * sinBeta;

	rot2D[1][0] = sinAlpha * cosBeta;
	rot2D[1][1] = cosAlpha;
	rot2D[1][2] = sinAlpha * sinBeta;

	rot2D[2][0] = -sinBeta;
	rot2D[2][1] = 0;
	rot2D[2][2] = cosBeta;
}

/*
 * invalidateRotationCache - Recalculates the 2D and 3D rotation matrices. Instead of calculating these each frame,
 * we recalculate only when the camera's rotation changes. This will only be called one time, because the code only
 * responds to one rotation change key at a time.
 */
void invalidateRotationCache() {
	generate2DRotMatrix();
	generateRotMatrix();
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

static void rotateOnDelta(int dX, int dY) {
	int relX = dX - centerX;
	int relY = dY - centerY;
	camera.yRot += relX * sensitivity * M_PI;
	camera.xRot += relY * sensitivity * M_PI;
	normalizeRotation();
	invalidateRotationCache();
}

/*
 * WindowProcessMessage - Handler to process messages sent from windows to this program.
 */
static LRESULT CALLBACK WindowProcessMessage(const HWND windowHandle, const UINT message, const WPARAM wParam, const LPARAM lParam) {
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

			vec3 totalMovement;
			vec3 movementX = { 0 };
			vec3 movementZ = { 0 };

			if (GetAsyncKeyState(VK_W) < 0) { // These keys can all be held down at once
				movementZ = multiplyMV(rot2D, &z);
			}

			if (GetAsyncKeyState(VK_S) < 0) {
				movementZ = multiplyMV(rot2D, &z);
				movementZ = vecConstMul(-1, &movementZ);
			}

			if (GetAsyncKeyState(VK_A) < 0) {
				movementX = multiplyMV(rot2D, &x);
				movementX = vecConstMul(-1, &movementX);
			}

			if (GetAsyncKeyState(VK_D) < 0) {
				movementX = multiplyMV(rot2D, &x);
			}

			totalMovement = vecAdd(&movementX, &movementZ);
			normalize(&totalMovement);
			totalMovement = vecConstMul(MOVESPEED * deltaTime, &totalMovement);

			camera.cameraPos = vecAdd(&totalMovement, &camera.cameraPos);

			if (GetAsyncKeyState(VK_ESCAPE) < 0) {
				pauseCursorLock = !pauseCursorLock;
				if (pauseCursorLock) {
					ShowCursor(true);
				} else {
					SetCursorPos(screenCenter.left + frame.width / 2, screenCenter.top + frame.height / 2 + 32);
					ShowCursor(false);
				}
			}

			if (GetAsyncKeyState(VK_SPACE) < 0) {
				camera.cameraPos.y += MOVESPEED * deltaTime; // These will always be relative to flat y axis to not lose orientation.
			}

			if (GetAsyncKeyState(VK_SHIFT) < 0) {
				camera.cameraPos.y -= MOVESPEED * deltaTime;
			}

			switch(wParam) { // Only allow one of these at once

				case VK_R: {
					camera.cameraPos.x = 0;
					camera.cameraPos.y = 0;
					camera.cameraPos.z = 0;
					camera.xRot = 0;
					camera.yRot = 0;
					camera.zRot = 0;
					invalidateRotationCache();
				}break;

				case VK_T: {
					camera.xRot = 0;
					camera.yRot = 0;
					camera.zRot = 0;
					invalidateRotationCache();
				}break;

				case VK_J: {
					addSphere(sceneList, camera.cameraPos, (rgb) { .red = 160, .green = 32, .blue = 240 }, 2, 600, 0.1);
				}break;

				case VK_L: {
					addPLight(sceneLight, camera.cameraPos, 0.5);
				}break;
			}
			normalizeRotation();
		} break;

		case WM_SETCURSOR: {
			SetCursor(pointer);
		}

		default: {
			return DefWindowProc(windowHandle, message, wParam, lParam);
		}
	}
	return 0;
}

/* 
 * putPixelRawVal - Puts a pixel of a specified color on the window, with the bottom left corner as the origin.
 * This function does check that the position is valid. If there is an issue, it prints the attempted value to stderr,
 * if a console has been allocated to this program.
 */
static void putPixelRawVal(const int32_t x, const int32_t y, const rgb c) {
	if (x >= frame.width || y >= frame.height) {
		fprintf(stderr, "Pixel out of bounds! x: %d, y: %d\n", x, y);
		return;
	}
	frame.pixels[y * frame.width + x] = getColor(c);
}

/*
 * putPixel - Plots a pixel on the screen of a specified color using the center of the screen as the origin.
 * This function does check that the position is valid. If there is an issue, it prints the attempted value to stderr,
 * if a console has been allocated to this program.
 */
static void putPixel(const int32_t x, const int32_t y, const rgb c) {
	const int32_t offsetX = x + (frame.width / 2);
	const int32_t offsetY = y + (frame.height / 2);
	if (offsetX > frame.width || offsetY > frame.height || offsetX < 0 || offsetY < 0) {
		fprintf(stderr, "Pixel out of bounds! x: %d, y: %d\n", x, y);
		return;
	}
	frame.pixels[offsetY * frame.width + offsetX] = getColor(c);
}

/*
 * canvasToViewport - Converts a screen space coordinate to a coordinate in the 3D view plane.
 */
static void canvasToViewport(const int x, const int y, vec3 *dest) {
	dest->x = x * ((double) VIEWPORT_WIDTH / frame.width);
	dest->y = y * ((double) VIEWPORT_HEIGHT / frame.height);
	dest->z = (double)DISTANCE;
}

/*
 * intersectRaySphere - This function finds the closest sphere that intersects a given ray.
 * A result of DBL_MAX means that no sphere intersects this ray at any point.
 */
static sphereResult intersectRaySphere(const vec3 *origin, const vec3 *direction, const sphere *s, const double dDotD) {
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
static intersectResult closestIntersection(const vec3 *origin, const vec3 *D, const double t_min, const double t_max, const double dDotD) {
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
static bool anyIntersection(const vec3 *origin, const vec3 *D, const double t_min, const double t_max, const double dDotD) {
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
static double computeLighting(const vec3 *point, const vec3 *normal, const vec3 v, const uint32_t spec) {
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
static rgb traceRay(const vec3 *origin, const vec3 *D, const double t_min, const double t_max, const uint32_t depth) {

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

/*
 * renderOnThreadID - Dispatches the lines to render to each thread based on the program's assigned ID for it.
 * The program avoids overdraw.
 */
static void renderOnThreadID(const void* pMyID) {
	int MyID = (int)(uintptr_t)pMyID;
	uint32_t recursionDepth = 3;
	for (int x = -frame.width / 2; x < frame.width / 2; x++) {
		for (int y = -frame.height / 2 + (frame.height / MAXTHREADS) * MyID + (MyID == 0 ? 0 : 1); // Hack to avoid overdraw
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

	if (!AllocConsole()) {
		return -1;
	}

	freopen_s((FILE **)stdout, "CONOUT$", "w", stdout); // Reattach stdout to the allocated console
	freopen_s((FILE **)stderr, "CONOUT$", "w", stderr); // Reattach stderr to the allocated console

	// Windows setup, creates our window and the bitmap we will display to the window.

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
		((WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME) ^ WS_MAXIMIZEBOX) | WS_VISIBLE, 0, 0, 1000, 1000,
		NULL, NULL, hInstance, NULL);
	if (windowHandle == NULL) {
		return -1;
	}

	// Build our list of spheres in the scene, then the list of lights

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

	// Generate the initial values for our rotation matrices.

	invalidateRotationCache();

	// Cursor setup

	GetWindowRect(windowHandle, &screenCenter);
	pointer = LoadCursor(NULL, IDC_ARROW);
	ShowCursor(false);
	SetCursorPos(screenCenter.left + frame.width / 2 - 8, screenCenter.top + frame.height / 2 + 1);
	GetCursorPos(&mouseLoc);
	centerX = mouseLoc.x;
	centerY = mouseLoc.y;

	while (!quit) {

		QueryPerformanceFrequency(&frequency); // Setup to calculate the delta time between the frames.

		QueryPerformanceCounter(&t1); // Get starting time

		static MSG message = { 0 };
		while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessage(&message);
		}

		GetWindowRect(windowHandle, &screenCenter);

		if (!pauseCursorLock) {
			GetCursorPos(&mouseLoc);
			ScreenToClient(windowHandle, &mouseLoc); // Gets the current pos relative to our window
			if (mouseLoc.x != centerX && mouseLoc.y != centerY) {
				rotateOnDelta(mouseLoc.x, mouseLoc.y);
				SetCursorPos(screenCenter.left + frame.width / 2, screenCenter.top + frame.height / 2 + 32);
			}
		}

		renderScene();

		InvalidateRect(windowHandle, NULL, FALSE);
		UpdateWindow(windowHandle);

		QueryPerformanceCounter(&t2); // Get end time

		deltaTime = (double)(t2.QuadPart - t1.QuadPart) / frequency.QuadPart; // Calculate time passed
	}

	freeLights(sceneLight);
	freeSphereList(sceneList);
	return 0;
}