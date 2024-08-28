#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <float.h>

#include "color.h"

const int VIEWPORT_WIDTH = 1;
const int VIEWPORT_HEIGHT = 1;
const int DISTANCE = 1;

const double M_PI = 3.14159265358979323846;
const double M_2PI = 6.2831853071795865;

#define FRAMESPERSECOND 60
#define MAXTHREADS 10

const int MOVESPEED = 5;
const double sensitivity = 0.001;

static uint8_t quit = 0;
static uint8_t pauseCursorLock = 0;

struct frame { // Represents the frame we are drawing to.
	int width;
	int height;
	uint32_t *pixels;
} frame = { 0 };

static BITMAPINFO bmi; // The header for the bitmap that is drawn to the screen.
static HBITMAP frameBitmap = NULL; // The pointer to the bitmap we draw.
static HDC fdc = NULL; // Represents the device context of our frame.

static rgb background = { // Holds our background color for the scene.
	.red = 255,
	.green = 255,
	.blue = 255
};

/*
 * WindowProcessMessage - Handler to process messages sent from windows to this program.
 */
static LRESULT CALLBACK WindowProcessMessage(const HWND windowHandle, const UINT message, const WPARAM wParam, const LPARAM lParam) {
	switch (message) {
		case WM_QUIT:
		case WM_DESTROY: {
			quit = 1;
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
		//fprintf(stderr, "Pixel out of bounds! x: %d, y: %d\n", x, y);
		return;
	}
	frame.pixels[offsetY * frame.width + offsetX] = getColor(c);
}

static double *interpolate(int32_t startI, double startD, int32_t destI, double destD) {
	if (startI == destI) {
		double *arr = malloc(sizeof(double));
		checkalloc(arr);
		arr[0] = startD;
		return arr;
	}

	double *vals = malloc((destI - startI + 1) * sizeof(double));
	checkalloc(vals);

	double a = (destD - startD) / (destI - startI);
	double d = startD;

	for (int i = startI; i <= destI; i++) {
		vals[i - startI] = d;
		d += a;
	}

	return vals;
}

static void drawLine(int32_t startX, int32_t startY, int32_t destX, int32_t destY, const rgb color) {
	if (abs(destX - startX) > abs(destY - startY)) {
		if (destX < startX) { // Swap the coords if dest is before start.
			int32_t tempX = startX;
			startX = destX;
			destX = tempX;

			int32_t tempY = startY;
			startY = destY;
			destY = tempY;
		}

		double *vals = interpolate(startX, startY, destX, destY);

		for (int32_t x = startX; x <= destX; x++) {
			putPixel(x, round(vals[x - startX]), color);
		}

		free(vals);
	} else {
		if (destY < startY) { // Swap the coords if they are dest is before start.
			int32_t tempX = startX;
			startX = destX;
			destX = tempX;

			int32_t tempY = startY;
			startY = destY;
			destY = tempY;
		}

		double *vals = interpolate(startY, startX, destY, destX);

		for (int32_t y = startY; y <= destY; y++) {
			putPixel(round(vals[y - startY]), y, color);
		}

		free(vals);
	}
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

	const wchar_t windowClassName[] = L"Rasterizer";
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

	while (!quit) {

		static MSG message = { 0 };
		while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessage(&message);
		}

		drawLine(-50, -200, 60, 240, (rgb) { .blue = 255, .red = 255, .green = 255 });
		drawLine(-200, -100, 240, 120, (rgb) { .blue = 255, .red = 255, .green = 255 });

		InvalidateRect(windowHandle, NULL, FALSE);
		UpdateWindow(windowHandle);
	}

	return 0;
}