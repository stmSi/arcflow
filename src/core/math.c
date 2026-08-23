#include "core/math.h"

#include <math.h>
#include <stddef.h>

AfAffine2 af_affine_identity(void) { return (AfAffine2){.m00 = 1.0F, .m11 = 1.0F}; }

AfAffine2 af_affine_from_transform(AfTransform2D transform) {
    const float cosine = cosf(transform.rotation);
    const float sine = sinf(transform.rotation);
    const float skew_x = tanf(transform.skew.x);
    const float skew_y = tanf(transform.skew.y);
    const float skew_xy = skew_x * skew_y;
    return (AfAffine2){.m00 = (cosine * (1.0F + skew_xy) - sine * skew_y) * transform.scale.x,
                       .m01 = (cosine * skew_x - sine) * transform.scale.y,
                       .m10 = (sine * (1.0F + skew_xy) + cosine * skew_y) * transform.scale.x,
                       .m11 = (sine * skew_x + cosine) * transform.scale.y,
                       .tx = transform.position.x,
                       .ty = transform.position.y};
}

AfAffine2 af_affine_multiply(AfAffine2 parent, AfAffine2 child) {
    return (AfAffine2){.m00 = parent.m00 * child.m00 + parent.m01 * child.m10,
                       .m01 = parent.m00 * child.m01 + parent.m01 * child.m11,
                       .m10 = parent.m10 * child.m00 + parent.m11 * child.m10,
                       .m11 = parent.m10 * child.m01 + parent.m11 * child.m11,
                       .tx = parent.m00 * child.tx + parent.m01 * child.ty + parent.tx,
                       .ty = parent.m10 * child.tx + parent.m11 * child.ty + parent.ty};
}

AfVec2 af_affine_transform_point(AfAffine2 transform, AfVec2 point) {
    return (AfVec2){.x = transform.m00 * point.x + transform.m01 * point.y + transform.tx,
                    .y = transform.m10 * point.x + transform.m11 * point.y + transform.ty};
}

bool af_affine_inverse(AfAffine2 transform, AfAffine2 *out_inverse) {
    const float determinant = transform.m00 * transform.m11 - transform.m01 * transform.m10;
    float inverse_determinant = 0.0F;
    if (out_inverse == NULL || fabsf(determinant) < 1.0e-8F) {
        return false;
    }
    inverse_determinant = 1.0F / determinant;
    *out_inverse = (AfAffine2){
        .m00 = transform.m11 * inverse_determinant,
        .m01 = -transform.m01 * inverse_determinant,
        .m10 = -transform.m10 * inverse_determinant,
        .m11 = transform.m00 * inverse_determinant,
        .tx = (transform.m01 * transform.ty - transform.m11 * transform.tx) * inverse_determinant,
        .ty = (transform.m10 * transform.tx - transform.m00 * transform.ty) * inverse_determinant};
    return true;
}

AfVec2 af_canvas_project(AfVec2 point, AfVec2 canvas_size) {
    if (canvas_size.x <= 0.0F || canvas_size.y <= 0.0F) {
        return (AfVec2){0};
    }
    return (AfVec2){.x = point.x * 2.0F / canvas_size.x - 1.0F,
                    .y = point.y * 2.0F / canvas_size.y - 1.0F};
}
