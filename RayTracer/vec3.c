#include "vec3.h"

/*
 * dotProduct - Computes the dot product of two vectors.
 */
double dotProduct(vec3 vector1, vec3 vector2) {
    return vector1.x * vector2.x + vector1.y * vector2.y + vector1.z * vector2.z;
}

/*
 * vecSub - Subtracts two vectors component wise.
 */
vec3 vecSub(vec3 vector1, vec3 vector2) {
    return (vec3) {
        .x = vector1.x - vector2.x,
            .y = vector1.y - vector2.y,
            .z = vector1.z - vector2.z
    };
}

/*
 * vecAdd - Adds to vectors component wise.
 */
vec3 vecAdd(vec3 vector1, vec3 vector2) {
    return (vec3) {
        .x = vector1.x + vector2.x,
            .y = vector1.y + vector2.y,
            .z = vector1.z + vector2.z
    };
}

/*
 * vecConstMul - Multiplies each component of a vector by a constant.
 */
vec3 vecConstMul(double constant, vec3 vector) {
    return (vec3) {
        .x = constant * vector.x,
            .y = constant * vector.y,
            .z = constant * vector.z
    };
}

/*
 * magnitude - Computes the magnitude of a 3D vector.
 */
double magnitude(vec3 vector) {
    return sqrt((vector.x * vector.x) + (vector.y * vector.y) + (vector.z * vector.z));
}

/*
 * normalize - Returns the normalized vector of the passed in vector. That is - each component is divided
 * by the overall magnitude of the vector.
 */
vec3 normalize(vec3 vector) {
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
vec3 reflectRay(vec3 ray, vec3 normal) {
    return vecSub(vecConstMul((2 * dotProduct(normal, ray)), normal), ray);
}