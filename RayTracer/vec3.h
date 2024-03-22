#pragma once

typedef struct vec3 { // Represents both a point in space, and a mathematical vector.
    double x;
    double y;
    double z;
} vec3;

/*
 * dotProduct - Computes the dot product of two vectors.
 */
inline double dotProduct(vec3 *vector1, vec3 *vector2) {
    return vector1->x * vector2->x + vector1->y * vector2->y + vector1->z * vector2->z;
}

/*
 * vecSub - Subtracts two vectors component wise.
 */
inline vec3 vecSub(vec3 *vector1, vec3 *vector2) {
    return (vec3) {
        .x = vector1->x - vector2->x,
            .y = vector1->y - vector2->y,
            .z = vector1->z - vector2->z
    };
}

/*
 * vecAdd - Adds to vectors component wise.
 */
inline vec3 vecAdd(vec3 *vector1, vec3 *vector2) {
    return (vec3) {
        .x = vector1->x + vector2->x,
            .y = vector1->y + vector2->y,
            .z = vector1->z + vector2->z
    };
}

/*
 * vecConstMul - Multiplies each component of a vector by a constant.
 */
inline vec3 vecConstMul(double constant, vec3 *vector) {
    return (vec3) {
        .x = constant * vector->x,
            .y = constant * vector->y,
            .z = constant * vector->z
    };
}

/*
 * magnitude - Computes the magnitude of a 3D vector.
 */
inline double magnitude(vec3 *vector) {
    return sqrt((vector->x * vector->x) + (vector->y * vector->y) + (vector->z * vector->z));
}

/*
 * normalize - Returns the normalized vector of the passed in vector. That is - each component is divided
 * by the overall magnitude of the vector.
 */
vec3 normalize(vec3 *vector) {
    double mag = magnitude(vector);
    return (vec3) {
        .x = mag * vector->x,
            .y = mag * vector->y,
            .z = mag * vector->z
    };
}

/*
 * reflectRay - Reflects a ray with respect to a normal.
 */
inline vec3 reflectRay(vec3 *ray, vec3 *normal) {
    double dot = dotProduct(normal, ray);
    vec3 vec = vecConstMul((2 * dot), normal);
    return vecSub(&vec, ray);
}

/*
 * multiplyMV - Multiplies a 3x3 matrix with a 3D vector. Uses the simplified formula for quicker calculations.
 */
inline vec3 multiplyMV(double matrix[3][3], vec3 *vector) {
    return (vec3) {
        .x = matrix[0][0] * vector->x + matrix[0][1] * vector->y + matrix[0][2] * vector->z,
        .y = matrix[1][0] * vector->x + matrix[1][1] * vector->y + matrix[1][2] * vector->z,
        .z = matrix[2][0] * vector->x + matrix[2][1] * vector->y + matrix[2][2] * vector->z
    };
}