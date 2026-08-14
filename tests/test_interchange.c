#include "core/allocator.h"
#include "core/file_io.h"
#include "project/interchange.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expression);   \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct AfInterchangeTransactionTest {
    int destination;
    int staged_value;
    AfResult stage_result;
    AfResult commit_result;
    uint32_t discard_count;
} AfInterchangeTransactionTest;

static AfResult af_test_interchange_stage(void *user_data, AfAllocator *allocator,
                                          void **out_staging,
                                          AfInterchangeDiagnostics *diagnostics) {
    AfInterchangeTransactionTest *test = user_data;
    int *staging = NULL;
    (void)diagnostics;
    if (test->stage_result != AF_RESULT_SUCCESS) {
        return test->stage_result;
    }
    staging = af_allocate(allocator, sizeof(*staging), _Alignof(int));
    if (staging == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    *staging = test->staged_value;
    *out_staging = staging;
    return AF_RESULT_SUCCESS;
}

static AfResult af_test_interchange_commit(void *user_data, const void *staging,
                                           AfInterchangeDiagnostics *diagnostics) {
    AfInterchangeTransactionTest *test = user_data;
    (void)diagnostics;
    if (test->commit_result != AF_RESULT_SUCCESS) {
        return test->commit_result;
    }
    test->destination = *(const int *)staging;
    return AF_RESULT_SUCCESS;
}

static void af_test_interchange_discard(void *user_data, AfAllocator *allocator, void *staging) {
    AfInterchangeTransactionTest *test = user_data;
    ++test->discard_count;
    af_deallocate(allocator, staging);
}

static bool af_test_interchange_diagnostics_and_easing(void) {
    AfInterchangeDiagnostics diagnostics = {0};
    AfInterchangeEasing easing = {0};
    char message[64];
    CHECK(af_interchange_check_import_support(AF_INTERCHANGE_FORMAT_SVG, &diagnostics) ==
          AF_RESULT_SUCCESS);
    CHECK(af_interchange_check_import_support(AF_INTERCHANGE_FORMAT_LOTTIE_JSON, &diagnostics) ==
          AF_RESULT_UNSUPPORTED_FEATURE);
    CHECK(diagnostics.warning_count == 1U && diagnostics.count == 1U);
    CHECK(diagnostics.items[0].code == AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FORMAT);
    af_interchange_diagnostics_clear(&diagnostics);
    for (uint32_t index = 0U; index < AF_INTERCHANGE_MAX_DIAGNOSTICS + 3U; ++index) {
        (void)snprintf(message, sizeof(message), "warning %u", index);
        (void)af_interchange_diagnostics_push(&diagnostics, AF_INTERCHANGE_DIAGNOSTIC_WARNING,
                                              AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FEATURE, index,
                                              1U, 2U, index + 1U, message);
    }
    CHECK(diagnostics.count == AF_INTERCHANGE_MAX_DIAGNOSTICS);
    CHECK(diagnostics.warning_count == AF_INTERCHANGE_MAX_DIAGNOSTICS + 3U);
    CHECK(diagnostics.dropped_count == 3U);

    CHECK(af_interchange_parse_easing(" ease-in-out ", strlen(" ease-in-out "), &easing, NULL) ==
          AF_RESULT_SUCCESS);
    CHECK(easing.kind == AF_INTERCHANGE_EASING_CUBIC_BEZIER);
    CHECK(fabsf(easing.control_points[0] - 0.42F) < 0.0001F);
    CHECK(fabsf(easing.control_points[2] - 0.58F) < 0.0001F);
    CHECK(af_interchange_parse_easing("cubic-bezier(0.1, -0.2, .8, 1.3)",
                                      strlen("cubic-bezier(0.1, -0.2, .8, 1.3)"), &easing,
                                      NULL) == AF_RESULT_SUCCESS);
    CHECK(easing.control_points[1] < 0.0F && easing.control_points[3] > 1.0F);
    CHECK(af_interchange_parse_easing("cubic-bezier(-.1, 0, .8, 1)",
                                      strlen("cubic-bezier(-.1, 0, .8, 1)"), &easing,
                                      NULL) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_interchange_parse_easing("steps(4)", strlen("steps(4)"), &easing, NULL) ==
          AF_RESULT_INVALID_ARGUMENT);
    return true;
}

static bool af_test_interchange_sprite_sheet(void) {
    static const uint8_t first[2U * 2U * 4U] = {255, 0, 0, 255, 255, 0, 0, 255,
                                                255, 0, 0, 255, 255, 0, 0, 255};
    static const uint8_t second[1U * 2U * 4U] = {0, 255, 0, 255, 0, 255, 0, 255};
    static const uint8_t third[2U * 1U * 4U] = {0, 0, 255, 255, 0, 0, 255, 255};
    const AfInterchangeRgbaFrame frames[3] = {
        {.pixels = first, .width = 2U, .height = 2U, .row_stride = 8U},
        {.pixels = second, .width = 1U, .height = 2U, .row_stride = 4U},
        {.pixels = third, .width = 2U, .height = 1U, .row_stride = 8U}};
    const AfSpriteSheetOptions options = {.columns = 2U, .padding = 1U, .power_of_two = true};
    AfSpriteSheetPlan plan = {0};
    AfAllocator allocator;
    AfFileData png = {0};
    uint32_t x = 0U;
    uint32_t y = 0U;
    af_allocator_init_default(&allocator);
    CHECK(af_interchange_plan_sprite_sheet(frames, 3U, &options, &plan, NULL) == AF_RESULT_SUCCESS);
    CHECK(plan.width == 8U && plan.height == 8U && plan.columns == 2U && plan.rows == 2U);
    CHECK(af_interchange_sprite_frame_position(&plan, 2U, &x, &y) == AF_RESULT_SUCCESS);
    CHECK(x == 1U && y == 4U);
    CHECK(af_interchange_export_sprite_sheet_png("arcflow_test_sheet.png", frames, 3U, &options,
                                                 &allocator, &plan, NULL) == AF_RESULT_SUCCESS);
    CHECK(af_file_read_limited("arcflow_test_sheet.png", 4096U, &allocator, &png) ==
          AF_RESULT_SUCCESS);
    CHECK(png.size > 24U && png.data[0] == 0x89U && png.data[1] == 'P' && png.data[2] == 'N' &&
          png.data[3] == 'G');
    CHECK(png.data[16] == 0U && png.data[17] == 0U && png.data[18] == 0U && png.data[19] == 8U &&
          png.data[20] == 0U && png.data[21] == 0U && png.data[22] == 0U && png.data[23] == 8U);
    af_file_data_destroy(&allocator, &png);
    CHECK(remove("arcflow_test_sheet.png") == 0);
    return true;
}

static bool af_test_interchange_static_svg(void) {
    AfSceneNode nodes[3] = {
        {.id = 1U,
         .parent_id = AF_INVALID_NODE_ID,
         .type = AF_NODE_RECTANGLE,
         .local_transform = {.position = {10.0F, 20.0F}, .scale = {1.0F, 1.0F}},
         .bounds = {100.0F, 50.0F},
         .fill = {1.0F, 0.0F, 0.0F, 1.0F},
         .opacity = 0.5F,
         .layer_order = 2,
         .visible = true},
        {.id = 2U,
         .parent_id = AF_INVALID_NODE_ID,
         .type = AF_NODE_ELLIPSE,
         .local_transform = {.position = {150.0F, 30.0F}, .scale = {1.0F, 1.0F}},
         .bounds = {40.0F, 30.0F},
         .fill = {0.0F, 1.0F, 0.0F, 1.0F},
         .opacity = 1.0F,
         .layer_order = 0,
         .visible = true},
        {.id = 3U,
         .parent_id = AF_INVALID_NODE_ID,
         .type = AF_NODE_PATH,
         .local_transform = {.scale = {1.0F, 1.0F}},
         .bounds = {30.0F, 30.0F},
         .fill = {0.0F, 0.0F, 1.0F, 1.0F},
         .opacity = 1.0F,
         .layer_order = 1,
         .visible = true}};
    AfPath path = {.id = 10U,
                   .node_id = 3U,
                   .command_count = 3U,
                   .fill_rule = AF_PATH_FILL_EVEN_ODD,
                   .geometry_generation = 1U};
    AfPathCommand commands[3] = {{.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                 {.type = AF_PATH_CUBIC,
                                  .point = {30.0F, 30.0F},
                                  .control1 = {10.0F, 0.0F},
                                  .control2 = {20.0F, 30.0F}},
                                 {.type = AF_PATH_CLOSE}};
    AfPathData paths = {.paths = &path,
                        .path_count = 1U,
                        .path_capacity = 1U,
                        .commands = commands,
                        .command_count = 3U,
                        .command_capacity = 3U};
    AfScene scene = {.nodes = nodes, .count = 3U, .capacity = 3U, .paths = &paths};
    AfStyle gradient_style = af_style_default(20U, 1U, nodes[0].fill);
    AfStyleData styles = {.styles = &gradient_style, .count = 1U, .capacity = 1U};
    AfInterchangeDiagnostics diagnostics = {0};
    char output[2048];
    char small[24];
    size_t output_size = 0U;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK(af_interchange_export_static_svg(&scene, (AfVec2){320.0F, 180.0F}, output, sizeof(output),
                                           &output_size, NULL) == AF_RESULT_SUCCESS);
    CHECK(output_size == strlen(output));
    CHECK(strstr(output, "<rect") != NULL && strstr(output, "<ellipse") != NULL &&
          strstr(output, "<path") != NULL && strstr(output, "fill-rule=\"evenodd\"") != NULL &&
          strstr(output, "matrix(1 0 -0 1 10 20)") != NULL);
    CHECK(strstr(output, "<ellipse") < strstr(output, "<path") &&
          strstr(output, "<path") < strstr(output, "<rect"));
    CHECK(af_interchange_export_static_svg(&scene, (AfVec2){320.0F, 180.0F}, small, sizeof(small),
                                           &output_size, NULL) == AF_RESULT_LIMIT_EXCEEDED);
    CHECK(small[0] == '\0' && output_size == 0U);
    gradient_style.fill = (AfPaint){.type = AF_PAINT_LINEAR_GRADIENT,
                                    .end = {100.0F, 0.0F},
                                    .transform = {.m00 = 1.0F, .m11 = 1.0F},
                                    .stops = {{.offset = 0.0F, .color = nodes[0].fill},
                                              {.offset = 1.0F, .color = {0.0F, 0.0F, 1.0F, 1.0F}}},
                                    .stop_count = 2U};
    scene.styles = &styles;
    CHECK(af_interchange_export_static_svg(&scene, (AfVec2){320.0F, 180.0F}, output, sizeof(output),
                                           &output_size, &diagnostics) == AF_RESULT_SUCCESS);
    CHECK(strstr(output, "<rect") == NULL && diagnostics.warning_count == 1U &&
          diagnostics.items[0].object_id == 1U);
    return true;
}

static bool af_test_interchange_transactions(void) {
    AfAllocator allocator;
    AfInterchangeTransactionTest transaction = {
        .destination = 3, .staged_value = 9, .commit_result = AF_RESULT_SUCCESS};
    af_allocator_init_default(&allocator);
    CHECK(af_interchange_transaction_execute(
              &allocator, &transaction, af_test_interchange_stage, af_test_interchange_commit,
              af_test_interchange_discard, NULL) == AF_RESULT_SUCCESS);
    CHECK(transaction.destination == 9 && transaction.discard_count == 1U);
    transaction.destination = 3;
    transaction.commit_result = AF_RESULT_IO_ERROR;
    CHECK(af_interchange_transaction_execute(
              &allocator, &transaction, af_test_interchange_stage, af_test_interchange_commit,
              af_test_interchange_discard, NULL) == AF_RESULT_IO_ERROR);
    CHECK(transaction.destination == 3 && transaction.discard_count == 2U);
    transaction.stage_result = AF_RESULT_INVALID_PACKAGE;
    CHECK(af_interchange_transaction_execute(
              &allocator, &transaction, af_test_interchange_stage, af_test_interchange_commit,
              af_test_interchange_discard, NULL) == AF_RESULT_INVALID_PACKAGE);
    CHECK(transaction.destination == 3 && transaction.discard_count == 2U);
    return true;
}

int main(void) {
    return af_test_interchange_diagnostics_and_easing() && af_test_interchange_sprite_sheet() &&
                   af_test_interchange_static_svg() && af_test_interchange_transactions()
               ? 0
               : 1;
}
