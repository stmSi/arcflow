#ifndef AF_INTERCHANGE_H
#define AF_INTERCHANGE_H

#include "af/core/allocator.h"
#include "af/scene/scene.h"
#include "arcflow/arcflow_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_INTERCHANGE_MAX_DIAGNOSTICS 32U
#define AF_INTERCHANGE_DIAGNOSTIC_MESSAGE_CAPACITY 160U
#define AF_INTERCHANGE_MAX_EASING_TEXT 128U
#define AF_INTERCHANGE_MAX_SPRITE_PADDING 256U
#define AF_INTERCHANGE_DEFAULT_FRAME_RATE 60.0F

struct AfProject;

typedef enum AfInterchangeDiagnosticSeverity {
    AF_INTERCHANGE_DIAGNOSTIC_INFO = 0,
    AF_INTERCHANGE_DIAGNOSTIC_WARNING,
    AF_INTERCHANGE_DIAGNOSTIC_ERROR
} AfInterchangeDiagnosticSeverity;

typedef enum AfInterchangeDiagnosticCode {
    AF_INTERCHANGE_DIAGNOSTIC_NONE = 0,
    AF_INTERCHANGE_DIAGNOSTIC_INVALID_SYNTAX,
    AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FORMAT,
    AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FEATURE,
    AF_INTERCHANGE_DIAGNOSTIC_COMPLEXITY_LIMIT,
    AF_INTERCHANGE_DIAGNOSTIC_OUTPUT_LIMIT,
    AF_INTERCHANGE_DIAGNOSTIC_IO,
    AF_INTERCHANGE_DIAGNOSTIC_TRANSACTION_STAGE,
    AF_INTERCHANGE_DIAGNOSTIC_TRANSACTION_COMMIT
} AfInterchangeDiagnosticCode;

typedef struct AfInterchangeDiagnostic {
    AfInterchangeDiagnosticSeverity severity;
    AfInterchangeDiagnosticCode code;
    uint32_t source_index;
    uint32_t line;
    uint32_t column;
    uint64_t object_id;
    char message[AF_INTERCHANGE_DIAGNOSTIC_MESSAGE_CAPACITY];
} AfInterchangeDiagnostic;

typedef struct AfInterchangeDiagnostics {
    AfInterchangeDiagnostic items[AF_INTERCHANGE_MAX_DIAGNOSTICS];
    uint32_t count;
    uint32_t warning_count;
    uint32_t error_count;
    uint32_t dropped_count;
} AfInterchangeDiagnostics;

typedef enum AfInterchangeFormat {
    AF_INTERCHANGE_FORMAT_SVG = 0,
    AF_INTERCHANGE_FORMAT_SPRITE_SHEET,
    AF_INTERCHANGE_FORMAT_IMAGE_SEQUENCE,
    AF_INTERCHANGE_FORMAT_CSS_EASING,
    AF_INTERCHANGE_FORMAT_LOTTIE_JSON
} AfInterchangeFormat;

typedef enum AfInterchangeEasingKind {
    AF_INTERCHANGE_EASING_LINEAR = 0,
    AF_INTERCHANGE_EASING_STEP_START,
    AF_INTERCHANGE_EASING_STEP_END,
    AF_INTERCHANGE_EASING_CUBIC_BEZIER
} AfInterchangeEasingKind;

typedef struct AfInterchangeEasing {
    AfInterchangeEasingKind kind;
    float control_points[4];
} AfInterchangeEasing;

typedef struct AfInterchangeRgbaFrame {
    const uint8_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t row_stride;
} AfInterchangeRgbaFrame;

typedef struct AfSpriteSheetOptions {
    uint32_t columns;
    uint32_t padding;
    bool power_of_two;
} AfSpriteSheetOptions;

typedef struct AfSpriteSheetPlan {
    uint32_t width;
    uint32_t height;
    uint32_t cell_width;
    uint32_t cell_height;
    uint32_t columns;
    uint32_t rows;
    uint32_t padding;
    uint32_t frame_count;
} AfSpriteSheetPlan;

typedef struct AfLottieExportOptions {
    float frame_rate;
} AfLottieExportOptions;

typedef AfResult (*AfInterchangeStageCallback)(void *user_data, AfAllocator *allocator,
                                               void **out_staging,
                                               AfInterchangeDiagnostics *diagnostics);
typedef AfResult (*AfInterchangeCommitCallback)(void *user_data, const void *staging,
                                                AfInterchangeDiagnostics *diagnostics);
typedef void (*AfInterchangeDiscardCallback)(void *user_data, AfAllocator *allocator,
                                             void *staging);

void af_interchange_diagnostics_clear(AfInterchangeDiagnostics *diagnostics);
bool af_interchange_diagnostics_push(AfInterchangeDiagnostics *diagnostics,
                                     AfInterchangeDiagnosticSeverity severity,
                                     AfInterchangeDiagnosticCode code, uint32_t source_index,
                                     uint32_t line, uint32_t column, uint64_t object_id,
                                     const char *message);
AfResult af_interchange_check_import_support(AfInterchangeFormat format,
                                             AfInterchangeDiagnostics *diagnostics);
AfResult af_interchange_parse_easing(const char *text, size_t text_size,
                                     AfInterchangeEasing *out_easing,
                                     AfInterchangeDiagnostics *diagnostics);
AfResult af_interchange_plan_sprite_sheet(const AfInterchangeRgbaFrame *frames,
                                          uint32_t frame_count, const AfSpriteSheetOptions *options,
                                          AfSpriteSheetPlan *out_plan,
                                          AfInterchangeDiagnostics *diagnostics);
AfResult af_interchange_sprite_frame_position(const AfSpriteSheetPlan *plan, uint32_t frame_index,
                                              uint32_t *out_x, uint32_t *out_y);
AfResult af_interchange_export_sprite_sheet_png(const char *path,
                                                const AfInterchangeRgbaFrame *frames,
                                                uint32_t frame_count,
                                                const AfSpriteSheetOptions *options,
                                                AfAllocator *allocator, AfSpriteSheetPlan *out_plan,
                                                AfInterchangeDiagnostics *diagnostics);
AfResult af_interchange_export_static_svg(const AfScene *scene, AfVec2 canvas_size, char *output,
                                          size_t output_capacity, size_t *out_size,
                                          AfInterchangeDiagnostics *diagnostics);
/* Shared by the export dialog to preview which objects survive SVG conversion. */
bool af_interchange_static_svg_node_supported(const AfScene *scene, const AfSceneNode *node);
AfResult af_interchange_export_lottie_json(const struct AfProject *project, uint64_t clip_id,
                                           const AfLottieExportOptions *options, char *output,
                                           size_t output_capacity, size_t *out_size,
                                           AfInterchangeDiagnostics *diagnostics);
AfResult af_interchange_pack_dotlottie(const char *lottie_json, size_t lottie_size, uint8_t *output,
                                       size_t output_capacity, size_t *out_size,
                                       AfInterchangeDiagnostics *diagnostics);
AfResult af_interchange_transaction_execute(AfAllocator *allocator, void *user_data,
                                            AfInterchangeStageCallback stage,
                                            AfInterchangeCommitCallback commit,
                                            AfInterchangeDiscardCallback discard,
                                            AfInterchangeDiagnostics *diagnostics);

#endif
