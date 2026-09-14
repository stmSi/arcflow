#include "path/path_boolean.h"

#include <math.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

typedef struct AfBooleanPoint {
    double x, y;
} AfBooleanPoint;
typedef struct AfBooleanSegment {
    AfBooleanPoint a, b;
    uint32_t operand;
} AfBooleanSegment;
typedef struct AfBooleanEdge {
    uint32_t a, b;
    bool used;
} AfBooleanEdge;
typedef struct AfBooleanScratch {
    AfVec2 flat_points[AF_MAX_PATH_FLATTENED_POINTS];
    AfPathContour contours[AF_MAX_PATH_CONTOURS];
    AfBooleanSegment segments[AF_PATH_BOOLEAN_MAX_EDGES];
    AfBooleanPoint vertices[AF_PATH_BOOLEAN_MAX_PIECES * 2U];
    AfBooleanEdge edges[AF_PATH_BOOLEAN_MAX_PIECES];
    double cuts[AF_PATH_BOOLEAN_MAX_EDGES * 2U + 2U];
    uint32_t loop[AF_PATH_BOOLEAN_MAX_PIECES];
    AfPathCommand output[AF_MAX_PATH_COMMANDS_PER_PATH];
    uint32_t segment_count, vertex_count, edge_count, output_count, piece_count;
    AfPathFillRule rules[2];
    double epsilon, extent;
} AfBooleanScratch;

static AfBooleanPoint af_boolean_sub(AfBooleanPoint a, AfBooleanPoint b) {
    return (AfBooleanPoint){a.x - b.x, a.y - b.y};
}
static double af_boolean_cross(AfBooleanPoint a, AfBooleanPoint b) { return a.x * b.y - a.y * b.x; }
static double af_boolean_dot(AfBooleanPoint a, AfBooleanPoint b) { return a.x * b.x + a.y * b.y; }
static AfBooleanPoint af_boolean_at(AfBooleanSegment edge, double t) {
    return (AfBooleanPoint){edge.a.x + (edge.b.x - edge.a.x) * t,
                            edge.a.y + (edge.b.y - edge.a.y) * t};
}

static AfResult af_boolean_flatten(AfBooleanScratch *scratch, const AfPathData *data,
                                   const AfPath *path, uint32_t operand, float tolerance) {
    AfFlattenedPath flat = {.points = scratch->flat_points,
                            .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                            .contours = scratch->contours,
                            .contour_capacity = AF_MAX_PATH_CONTOURS};
    AfResult result = af_path_flatten(data, path, tolerance, &flat);
    if (result != AF_RESULT_SUCCESS)
        return result;
    scratch->rules[operand] = path->fill_rule;
    for (uint32_t i = 0; i < flat.contour_count; ++i) {
        const AfPathContour contour = flat.contours[i];
        if (!contour.closed)
            return AF_RESULT_UNSUPPORTED_FEATURE;
        for (uint32_t j = 0; j < contour.point_count; ++j) {
            const AfVec2 a = flat.points[contour.point_start + j];
            const AfVec2 b = flat.points[contour.point_start + (j + 1U) % contour.point_count];
            if (!isfinite(a.x) || !isfinite(a.y) || !isfinite(b.x) || !isfinite(b.y))
                return AF_RESULT_UNSUPPORTED_FEATURE;
            if (a.x == b.x && a.y == b.y)
                continue;
            if (scratch->segment_count == AF_PATH_BOOLEAN_MAX_EDGES)
                return AF_RESULT_LIMIT_EXCEEDED;
            scratch->segments[scratch->segment_count++] =
                (AfBooleanSegment){{a.x, a.y}, {b.x, b.y}, operand};
        }
    }
    return AF_RESULT_SUCCESS;
}

static bool af_boolean_contains(const AfBooleanScratch *scratch, uint32_t operand,
                                AfBooleanPoint p) {
    int winding = 0;
    for (uint32_t i = 0; i < scratch->segment_count; ++i) {
        const AfBooleanSegment edge = scratch->segments[i];
        if (edge.operand != operand)
            continue;
        const double side =
            af_boolean_cross(af_boolean_sub(edge.b, edge.a), af_boolean_sub(p, edge.a));
        if (edge.a.y <= p.y && edge.b.y > p.y && side > 0)
            ++winding;
        else if (edge.a.y > p.y && edge.b.y <= p.y && side < 0)
            --winding;
    }
    return scratch->rules[operand] == AF_PATH_FILL_EVEN_ODD ? winding % 2 != 0 : winding != 0;
}

static bool af_boolean_value(const AfBooleanScratch *scratch, AfPathBooleanOperation operation,
                             AfBooleanPoint p) {
    const bool a = af_boolean_contains(scratch, 0U, p), b = af_boolean_contains(scratch, 1U, p);
    return operation == AF_PATH_BOOLEAN_UNION      ? a || b
           : operation == AF_PATH_BOOLEAN_SUBTRACT ? a && !b
                                                   : a && b;
}

static int af_boolean_compare_cut(const void *a, const void *b) {
    const double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void af_boolean_add_cut(AfBooleanScratch *scratch, uint32_t *count, double t, double slack) {
    if (t >= -slack && t <= 1 + slack)
        scratch->cuts[(*count)++] = fmax(0, fmin(1, t));
}

/* Split every boundary at crossings, self-crossings, and both ends of collinear overlaps.
 * Each other segment contributes at most two cuts to this fixed-size array. */
static uint32_t af_boolean_cuts(AfBooleanScratch *scratch, uint32_t index) {
    const AfBooleanSegment edge = scratch->segments[index];
    const AfBooleanPoint r = af_boolean_sub(edge.b, edge.a);
    const double rr = af_boolean_dot(r, r), length = sqrt(rr), slack = scratch->epsilon / length;
    uint32_t count = 2U;
    scratch->cuts[0] = 0;
    scratch->cuts[1] = 1;
    for (uint32_t i = 0; i < scratch->segment_count; ++i) {
        if (i == index)
            continue;
        const AfBooleanSegment other = scratch->segments[i];
        const AfBooleanPoint s = af_boolean_sub(other.b, other.a),
                             q = af_boolean_sub(other.a, edge.a);
        const double ss = af_boolean_dot(s, s), cross = af_boolean_cross(r, s);
        if (fabs(cross) > sqrt(rr * ss) * 1e-12) {
            const double t = af_boolean_cross(q, s) / cross, u = af_boolean_cross(q, r) / cross;
            const double other_slack = scratch->epsilon / sqrt(ss);
            if (u >= -other_slack && u <= 1 + other_slack)
                af_boolean_add_cut(scratch, &count, t, slack);
        } else if (fabs(af_boolean_cross(q, r)) <= scratch->epsilon * length) {
            af_boolean_add_cut(scratch, &count, af_boolean_dot(q, r) / rr, slack);
            af_boolean_add_cut(scratch, &count,
                               af_boolean_dot(af_boolean_sub(other.b, edge.a), r) / rr, slack);
        }
    }
    qsort(scratch->cuts, count, sizeof(*scratch->cuts), af_boolean_compare_cut);
    uint32_t unique = 1U;
    for (uint32_t i = 1; i < count; ++i)
        if ((scratch->cuts[i] - scratch->cuts[unique - 1U]) * length > scratch->epsilon)
            scratch->cuts[unique++] = scratch->cuts[i];
    return unique;
}

static uint32_t af_boolean_vertex(AfBooleanScratch *scratch, AfBooleanPoint p) {
    for (uint32_t i = 0; i < scratch->vertex_count; ++i) {
        AfBooleanPoint d = af_boolean_sub(scratch->vertices[i], p);
        if (af_boolean_dot(d, d) <= scratch->epsilon * scratch->epsilon)
            return i;
    }
    const uint32_t index = scratch->vertex_count++;
    scratch->vertices[index] = p;
    return index;
}

static AfResult af_boolean_piece(AfBooleanScratch *scratch, AfBooleanSegment segment, double start,
                                 double end, AfPathBooleanOperation operation) {
    if (++scratch->piece_count > AF_PATH_BOOLEAN_MAX_PIECES)
        return AF_RESULT_LIMIT_EXCEEDED;
    AfBooleanPoint a = af_boolean_at(segment, start), b = af_boolean_at(segment, end);
    const AfBooleanPoint mid = {(a.x + b.x) * 0.5, (a.y + b.y) * 0.5},
                         direction = af_boolean_sub(b, a);
    const double length = hypot(direction.x, direction.y);
    double offset = fmin(length * 0.1, scratch->extent * 1e-6);
    /* Keep classification samples away from nearby parallel boundaries and narrow features. */
    for (uint32_t i = 0; i < scratch->segment_count; ++i) {
        const AfBooleanSegment edge = scratch->segments[i];
        const AfBooleanPoint d = af_boolean_sub(edge.b, edge.a);
        const double t =
            fmax(0, fmin(1, af_boolean_dot(af_boolean_sub(mid, edge.a), d) / af_boolean_dot(d, d)));
        const AfBooleanPoint delta = af_boolean_sub(mid, af_boolean_at(edge, t));
        const double distance = hypot(delta.x, delta.y);
        if (distance > scratch->epsilon * 2)
            offset = fmin(offset, distance * 0.2);
    }
    if (offset <= scratch->epsilon * 0.01)
        return AF_RESULT_UNSUPPORTED_FEATURE;
    const AfBooleanPoint normal = {-direction.y / length * offset, direction.x / length * offset};
    const bool left =
        af_boolean_value(scratch, operation, (AfBooleanPoint){mid.x + normal.x, mid.y + normal.y});
    const bool right =
        af_boolean_value(scratch, operation, (AfBooleanPoint){mid.x - normal.x, mid.y - normal.y});
    if (left == right)
        return AF_RESULT_SUCCESS;
    if (!left) {
        const AfBooleanPoint swap = a;
        a = b;
        b = swap;
    }
    const uint32_t va = af_boolean_vertex(scratch, a), vb = af_boolean_vertex(scratch, b);
    if (va == vb)
        return AF_RESULT_UNSUPPORTED_FEATURE;
    for (uint32_t i = 0; i < scratch->edge_count; ++i) {
        const AfBooleanEdge edge = scratch->edges[i];
        if (edge.a == va && edge.b == vb)
            return AF_RESULT_SUCCESS;
        if (edge.a == vb && edge.b == va)
            return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    scratch->edges[scratch->edge_count++] = (AfBooleanEdge){va, vb, false};
    return AF_RESULT_SUCCESS;
}

static AfResult af_boolean_contours(AfBooleanScratch *scratch, uint32_t capacity) {
    for (uint32_t first = 0; first < scratch->edge_count; ++first) {
        if (scratch->edges[first].used)
            continue;
        uint32_t current = first, count = 0U;
        const uint32_t origin = scratch->edges[first].a;
        for (;;) {
            AfBooleanEdge *edge = &scratch->edges[current];
            if (edge->used || count == AF_PATH_BOOLEAN_MAX_PIECES)
                return AF_RESULT_UNSUPPORTED_FEATURE;
            scratch->loop[count++] = edge->a;
            edge->used = true;
            if (edge->b == origin)
                break;
            const AfBooleanPoint direction =
                af_boolean_sub(scratch->vertices[edge->b], scratch->vertices[edge->a]);
            double best_turn = -4;
            uint32_t next = UINT32_MAX;
            for (uint32_t i = 0; i < scratch->edge_count; ++i) {
                const AfBooleanEdge candidate = scratch->edges[i];
                if (candidate.used || candidate.a != edge->b)
                    continue;
                const AfBooleanPoint outgoing =
                    af_boolean_sub(scratch->vertices[candidate.b], scratch->vertices[candidate.a]);
                const double turn = atan2(af_boolean_cross(direction, outgoing),
                                          af_boolean_dot(direction, outgoing));
                if (turn > best_turn) {
                    best_turn = turn;
                    next = i;
                }
            }
            if (next == UINT32_MAX)
                return AF_RESULT_UNSUPPORTED_FEATURE;
            current = next;
        }
        /* Remove redundant collinear split points before applying the authoring limit. */
        bool changed = true;
        while (changed && count >= 3U) {
            changed = false;
            for (uint32_t i = 0; i < count; ++i) {
                const AfBooleanPoint a = scratch->vertices[scratch->loop[(i + count - 1U) % count]],
                                     b = scratch->vertices[scratch->loop[i]],
                                     c = scratch->vertices[scratch->loop[(i + 1U) % count]];
                const AfBooleanPoint ab = af_boolean_sub(b, a), bc = af_boolean_sub(c, b);
                if (fabs(af_boolean_cross(ab, bc)) <=
                        scratch->epsilon * (hypot(ab.x, ab.y) + hypot(bc.x, bc.y)) &&
                    af_boolean_dot(ab, bc) >= 0) {
                    memmove(scratch->loop + i, scratch->loop + i + 1U,
                            (count - i - 1U) * sizeof(*scratch->loop));
                    --count;
                    changed = true;
                    break;
                }
            }
        }
        if (count < 3U)
            return AF_RESULT_UNSUPPORTED_FEATURE;
        if (count + 1U > capacity - scratch->output_count)
            return AF_RESULT_LIMIT_EXCEEDED;
        for (uint32_t i = 0; i < count; ++i) {
            const AfBooleanPoint point = scratch->vertices[scratch->loop[i]],
                                 next = scratch->vertices[scratch->loop[(i + 1U) % count]];
            const AfVec2 p = {(float)point.x, (float)point.y};
            if (!isfinite(p.x) || !isfinite(p.y) || (p.x == (float)next.x && p.y == (float)next.y))
                return AF_RESULT_UNSUPPORTED_FEATURE;
            scratch->output[scratch->output_count++] =
                (AfPathCommand){.type = i == 0U ? AF_PATH_MOVE : AF_PATH_LINE, .point = p};
        }
        scratch->output[scratch->output_count++] = (AfPathCommand){.type = AF_PATH_CLOSE};
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_path_boolean(AfAllocator *allocator, const AfPathData *data, const AfPath *a,
                         const AfPath *b, AfPathBooleanOperation operation, float tolerance,
                         AfPathCommand *output, uint32_t capacity, uint32_t *out_count) {
    if (allocator == NULL || data == NULL || a == NULL || b == NULL || output == NULL ||
        out_count == NULL || capacity > AF_MAX_PATH_COMMANDS_PER_PATH ||
        operation > AF_PATH_BOOLEAN_INTERSECT || !isfinite(tolerance) || tolerance <= 0)
        return AF_RESULT_INVALID_ARGUMENT;
    AfBooleanScratch *scratch = af_allocate(allocator, sizeof(*scratch), alignof(AfBooleanScratch));
    if (scratch == NULL)
        return AF_RESULT_OUT_OF_MEMORY;
    memset(scratch, 0, sizeof(*scratch));
    AfResult result = af_boolean_flatten(scratch, data, a, 0U, tolerance);
    if (result == AF_RESULT_SUCCESS)
        result = af_boolean_flatten(scratch, data, b, 1U, tolerance);
    if (result == AF_RESULT_SUCCESS && scratch->segment_count != 0U) {
        AfBooleanPoint minimum = scratch->segments[0].a, maximum = minimum;
        for (uint32_t i = 0; i < scratch->segment_count; ++i) {
            const AfBooleanPoint points[] = {scratch->segments[i].a, scratch->segments[i].b};
            for (uint32_t j = 0; j < 2U; ++j) {
                minimum.x = fmin(minimum.x, points[j].x);
                minimum.y = fmin(minimum.y, points[j].y);
                maximum.x = fmax(maximum.x, points[j].x);
                maximum.y = fmax(maximum.y, points[j].y);
            }
        }
        scratch->extent = fmax(maximum.x - minimum.x, maximum.y - minimum.y);
        scratch->epsilon = fmax(1e-9, scratch->extent * 1e-8);
        for (uint32_t i = 0; result == AF_RESULT_SUCCESS && i < scratch->segment_count; ++i) {
            const uint32_t count = af_boolean_cuts(scratch, i);
            for (uint32_t j = 1U; result == AF_RESULT_SUCCESS && j < count; ++j)
                result = af_boolean_piece(scratch, scratch->segments[i], scratch->cuts[j - 1U],
                                          scratch->cuts[j], operation);
        }
        if (result == AF_RESULT_SUCCESS)
            result = af_boolean_contours(scratch, capacity);
    }
    if (result == AF_RESULT_SUCCESS) {
        memcpy(output, scratch->output, scratch->output_count * sizeof(*output));
        *out_count = scratch->output_count;
    }
    af_deallocate(allocator, scratch);
    return result;
}
