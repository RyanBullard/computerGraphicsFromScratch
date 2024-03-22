#pragma once

#include "vec3.h"
#include "color.h"
#include "standardHeader.h"
#include <stdint.h>

typedef struct sphere { // Represents one of the objects in the scene, a sphere.
    vec3 center;
    uint32_t radius;
    rgb color;
    uint32_t specular;
    double reflectivity;
    uint32_t rSquare;
} sphere;

typedef struct sphereList { // Holds the list of all objects in the hard coded scene.
    sphere *data;
    struct sphereList *next;
} sphereList;

typedef struct sphereResult { // Used to hold information when determining which sphere is closest to the camera.
    double firstT;
    double secondT;
} sphereResult;

void freeSphereList(sphereList*);
void addSphere(sphereList*, vec3, rgb, uint32_t, uint32_t, double);
sphereList *initSpheres();