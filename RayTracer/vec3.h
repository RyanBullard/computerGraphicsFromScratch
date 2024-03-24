#pragma once

typedef struct vec3 { // Represents both a point in space, and a mathematical vector.
    double x;
    double y;
    double z;
} vec3;

/*
 * dotProduct - Computes the dot product of two vectors.
 */
inline double dotProduct(const vec3 *vector1, const vec3 *vector2) {
    return vector1->x * vector2->x + vector1->y * vector2->y + vector1->z * vector2->z;
}

/*
 * vecSub - Subtracts two vectors component wise.
 */
inline vec3 vecSub(const vec3 *vector1, const vec3 *vector2) {
    return (vec3) {
        .x = vector1->x - vector2->x,
            .y = vector1->y - vector2->y,
            .z = vector1->z - vector2->z
    };
}

/*
 * vecAdd - Adds to vectors component wise.
 */
inline vec3 vecAdd(const vec3 *vector1, const vec3 *vector2) {
    return (vec3) {
        .x = vector1->x + vector2->x,
            .y = vector1->y + vector2->y,
            .z = vector1->z + vector2->z
    };
}

/*
 * vecConstMul - Multiplies each component of a vector by a constant.
 */
inline vec3 vecConstMul(const double constant, const vec3 *vector) {
    return (vec3) {
        .x = constant * vector->x,
            .y = constant * vector->y,
            .z = constant * vector->z
    };
}

/*
 * magnitude - Computes the magnitude of a 3D vector.
 */
inline double magnitude(const vec3 *vector) {
    return sqrt((vector->x * vector->x) + (vector->y * vector->y) + (vector->z * vector->z));
}

/*
 * normalize -  normalizes a vector in place. That is - each component is divided
 * by the overall magnitude of the vector.
 */
inline void normalize(vec3 *vector) {
    double mag = magnitude(vector);
    vector->x = vector->x * mag;
    vector->y = vector->y * mag;
    vector->z = vector->z * mag;
}

/*
 * reflectRay - Reflects a ray with respect to a normal.
 */
inline vec3 reflectRay(const vec3 *ray, const vec3 *normal) {
    double dot = dotProduct(normal, ray);
    vec3 vec = vecConstMul((2 * dot), normal);
    return vecSub(&vec, ray);
}

/*
 * multiplyMV - Multiplies a 3x3 matrix with a 3D vector. Uses the simplified formula for quicker calculations.
 */
inline vec3 multiplyMV(const double matrix[3][3], const vec3 *vector) {
    return (vec3) {
        .x = matrix[0][0] * vector->x + matrix[0][1] * vector->y + matrix[0][2] * vector->z,
        .y = matrix[1][0] * vector->x + matrix[1][1] * vector->y + matrix[1][2] * vector->z,
        .z = matrix[2][0] * vector->x + matrix[2][1] * vector->y + matrix[2][2] * vector->z
    };
}