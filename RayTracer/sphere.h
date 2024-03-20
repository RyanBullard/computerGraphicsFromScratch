#pragma once

#include "vec3.h"
#include "color.h"
#include <stdint.h>

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

typedef struct sphereResult { // Used to hold information when determining which sphere is closest to the camera.
    double firstT;
    double secondT;
} sphereResult;