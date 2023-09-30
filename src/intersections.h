#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/intersect.hpp>

#include "sceneStructs.h"
#include "utilities.h"

/**
 * Handy-dandy hash function that provides seeds for random number generation.
 */
__host__ __device__ inline unsigned int utilhash(unsigned int a) {
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

// CHECKITOUT
/**
 * Compute a point at parameter value `t` on ray `r`.
 * Falls slightly short so that it doesn't intersect the object it's hitting.
 */
__host__ __device__ glm::vec3 getPointOnRay(Ray r, float t) {
    return r.origin + (t - .0001f) * glm::normalize(r.direction);
}

/**
 * Multiplies a mat4 and a vec4 and returns a vec3 clipped from the vec4.
 */
__host__ __device__ glm::vec3 multiplyMV(glm::mat4 m, glm::vec4 v) {
    return glm::vec3(m * v);
}

__host__ __device__ bool boundingboxIntersectionTest(Ray& r,
    glm::vec3& minv, glm::vec3& maxv) {

    //if (r.origin[0] > minv[0] - 0.01f && r.origin[0] < maxv[0] + 0.01f &&
    //    r.origin[1] > minv[1] - 0.01f && r.origin[0] < maxv[1] + 0.01f &&
    //    r.origin[2] > minv[2] - 0.01f && r.origin[0] < maxv[2] + 0.01f) { return true; }

    if (r.origin[0] > minv[0] && r.origin[0] < maxv[0] &&
        r.origin[1] > minv[1] && r.origin[0] < maxv[1] &&
        r.origin[2] > minv[2] && r.origin[0] < maxv[2]) {
        return true;
    }

    float tmin[3] = {0, 0, 0};
    // float tmax[3] = {0, 0, 0};

    #pragma unroll
    for (int i = 0; i < 3; i++) {
        if (abs(r.direction[i]) < FLT_EPSILON) {
            if (r.origin[i] < minv[i] - 0.01f || r.origin[i] > maxv[i] + 0.01f) {
                return false;
            }
            tmin[i] = -1;
            // tmax[i] = 1e5;
        }
        else {
            tmin[i] = min((minv[i] - r.origin[i]) / r.direction[i], (maxv[i] - r.origin[i]) / r.direction[i]);
            // tmax[i] = glm::max(t0, t1);

        }
    }

    // float dstB = glm::min(tmax[0], glm::min(tmax[1], tmax[2]));

    //float dstToBox = std::max(0, dstA);
    //float dstInsideBox = max(0, dstB - dstToBox);
    //return float2(dstToBox, dstInsideBox);
    return max(max(tmin[0], tmin[1]), tmin[2]) > -0.001f;
}

// CHECKITOUT
/**
 * Test intersection between a ray and a transformed cube. Untransformed,
 * the cube ranges from -0.5 to 0.5 in each axis and is centered at the origin.
 *
 * @param intersectionPoint  Output parameter for point of intersection.
 * @param normal             Output parameter for surface normal.
 * @param outside            Output param for whether the ray came from outside.
 * @return                   Ray parameter `t` value. -1 if no intersection.
 */
__host__ __device__ float boxIntersectionTest(Geom box, Ray r,
        glm::vec3 &intersectionPoint, glm::vec3 &normal, bool &outside) {
    Ray q;
    q.origin    =                multiplyMV(box.inverseTransform, glm::vec4(r.origin   , 1.0f));
    q.direction = glm::normalize(multiplyMV(box.inverseTransform, glm::vec4(r.direction, 0.0f)));

    float tmin = -1e38f;
    float tmax = 1e38f;
    glm::vec3 tmin_n;
    glm::vec3 tmax_n;
    for (int xyz = 0; xyz < 3; ++xyz) {
        float qdxyz = q.direction[xyz];
        /*if (glm::abs(qdxyz) > 0.00001f)*/ {
            float t1 = (-0.5f - q.origin[xyz]) / qdxyz;
            float t2 = (+0.5f - q.origin[xyz]) / qdxyz;
            float ta = glm::min(t1, t2);
            float tb = glm::max(t1, t2);
            glm::vec3 n;
            n[xyz] = t2 < t1 ? +1 : -1;
            if (ta > 0 && ta > tmin) {
                tmin = ta;
                tmin_n = n;
            }
            if (tb < tmax) {
                tmax = tb;
                tmax_n = n;
            }
        }
    }

    if (tmax >= tmin && tmax > 0) {
        outside = true;
        if (tmin <= 0) {
            tmin = tmax;
            tmin_n = tmax_n;
            outside = false;
        }
        intersectionPoint = multiplyMV(box.transform, glm::vec4(getPointOnRay(q, tmin), 1.0f));
        normal = glm::normalize(multiplyMV(box.invTranspose, glm::vec4(tmin_n, 0.0f)));
        return glm::length(r.origin - intersectionPoint);
    }
    return -1;
}

// CHECKITOUT
/**
 * Test intersection between a ray and a transformed sphere. Untransformed,
 * the sphere always has radius 0.5 and is centered at the origin.
 *
 * @param intersectionPoint  Output parameter for point of intersection.
 * @param normal             Output parameter for surface normal.
 * @param outside            Output param for whether the ray came from outside.
 * @return                   Ray parameter `t` value. -1 if no intersection.
 */
__host__ __device__ float sphereIntersectionTest(Geom sphere, Ray r,
        glm::vec3 &intersectionPoint, glm::vec3 &normal, bool &outside) {
    float radius = .5;

    glm::vec3 ro = multiplyMV(sphere.inverseTransform, glm::vec4(r.origin, 1.0f));
    glm::vec3 rd = glm::normalize(multiplyMV(sphere.inverseTransform, glm::vec4(r.direction, 0.0f)));

    Ray rt;
    rt.origin = ro;
    rt.direction = rd;

    float vDotDirection = glm::dot(rt.origin, rt.direction);
    float radicand = vDotDirection * vDotDirection - (glm::dot(rt.origin, rt.origin) - powf(radius, 2));
    if (radicand < 0) {
        return -1;
    }

    float squareRoot = sqrt(radicand);
    float firstTerm = -vDotDirection;
    float t1 = firstTerm + squareRoot;
    float t2 = firstTerm - squareRoot;

    float t = 0;
    if (t1 < 0 && t2 < 0) {
        return -1;
    } else if (t1 > 0 && t2 > 0) {
        t = min(t1, t2);
        outside = true;
    } else {
        t = max(t1, t2);
        outside = false;
    }

    glm::vec3 objspaceIntersection = getPointOnRay(rt, t);

    intersectionPoint = multiplyMV(sphere.transform, glm::vec4(objspaceIntersection, 1.f));
    normal = glm::normalize(multiplyMV(sphere.invTranspose, glm::vec4(objspaceIntersection, 0.f)));
    if (!outside) {
        normal = -normal;
    }

    return glm::length(r.origin - intersectionPoint);
}

// inspired by glm::intersectRayTriangle
/**
 * Test intersection between a ray and a triangle. 
 *
 * @param intersectionPoint  Output parameter for point of intersection.
 * @param normal             Output parameter for surface normal.
 * @param outside            Output param for whether the ray came from outside.
 * @return                   Ray parameter `t` value. -1 if no intersection.
 */
__host__ __device__ float triangleIntersectionTest(Triangle tri, Ray r,
    glm::vec3& intersectionPoint, glm::vec3& normal, bool& outside) {

    outside = true;
    glm::vec3 v01 = tri.pos[1] - tri.pos[0];
    glm::vec3 v02 = tri.pos[2] - tri.pos[0];
    glm::vec3 dx02 = glm::cross(r.direction, v02);

    float d0 = glm::dot(v01, dx02);
    if (d0 < 0.000001f){ return -1.0f; }

    float d = 1.0f / d0;
    glm::vec3 v0o = r.origin - tri.pos[0];
    float b1 = d * glm::dot(v0o, dx02);
    if (b1 < 0.0f || b1 > 1.0f) { return -1.0f; }

    glm::vec3 v0ox01 = glm::cross(v0o, v01);
    float b2 = d * glm::dot(r.direction, v0ox01);
    if (b2 < 0.0f || b2 + b1 > 1.0f) { return -1.0f; }

    float t = d * glm::dot(v02, v0ox01);
    if (t < 0.0f) { return -1.0f;}

    intersectionPoint = t * r.direction + r.origin;
    normal = normalize(glm::cross(v01, v02));

    return t;
}
