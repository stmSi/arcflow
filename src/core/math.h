#ifndef AF_MATH_H
#define AF_MATH_H

#include <stdbool.h>

typedef struct AfVec2 {
    float x;
    float y;
} AfVec2;

typedef struct AfColor {
    float r;
    float g;
    float b;
    float a;
} AfColor;

typedef struct AfTransform2D {
    AfVec2 position;
    float rotation;
    AfVec2 scale;
} AfTransform2D;

typedef struct AfAffine2 {
    float m00;
    float m01;
    float m10;
    float m11;
    float tx;
    float ty;
} AfAffine2;

AfAffine2 af_affine_identity(void);
AfAffine2 af_affine_from_transform(AfTransform2D transform);
AfAffine2 af_affine_multiply(AfAffine2 parent, AfAffine2 child);
AfVec2 af_affine_transform_point(AfAffine2 transform, AfVec2 point);
bool af_affine_inverse(AfAffine2 transform, AfAffine2 *out_inverse);
AfVec2 af_canvas_project(AfVec2 point, AfVec2 canvas_size);

#endif
