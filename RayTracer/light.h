#pragma once

#include "vec3.h"
#include "standardHeader.h"

typedef struct pointLight { // Represents light emitted from a singular point in the scene. Similar to a light bulb.
    double intensity;
    vec3 pos;
} pointLight;

typedef struct dirLight { // Represents a light coming from a certain direction. Similar to the sun.
    double intensity;
    vec3 dir;
} dirLight;

typedef struct pointLightList { // Holds a list of all point lights placed in the scene.
    pointLight *data;
    struct pointLightList *next;
} pointLightList;

typedef struct dirLightList { // Holds a list of all directional lights used in the scene.
    dirLight *data;
    struct dirLightList *next;
} dirLightList;

typedef struct light { // Represents the overall lights in the scene.
    double ambient;
    dirLightList *dirList;
    pointLightList *pointList;
} light;

void freeLights(light*);
void addDLight(light*, vec3, double);
void addPLight(light*, vec3, double);
void setAmbient(light*, double);
light *initLights();