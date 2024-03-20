#pragma once

typedef struct vec3 { // Represents both a point in space, and a mathematical vector.
    double x;
    double y;
    double z;
} vec3;

vec3 reflectRay(vec3, vec3);
vec3 normalize(vec3);
double magnitude(vec3);
vec3 vecConstMul(double, vec3);
vec3 vecAdd(vec3, vec3);
vec3 vecSub(vec3, vec3);
double dotProduct(vec3, vec3);
vec3 multiplyMV(double[3][3], vec3);