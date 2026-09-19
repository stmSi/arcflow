#ifndef AF_EDITOR_FONTS_H
#define AF_EDITOR_FONTS_H
#include <stdbool.h>
#include <stdint.h>
#define AF_EDITOR_FONT_NAME 128U
#define AF_EDITOR_FONT_PATH 1024U
#define AF_EDITOR_FONT_LIMIT 8192U
typedef struct AfEditorFontFace {
    char family[AF_EDITOR_FONT_NAME];
    char path[AF_EDITOR_FONT_PATH];
    uint32_t face_index;
    bool bold, italic;
} AfEditorFontFace;
typedef struct AfEditorFontCatalog {
    AfEditorFontFace *faces;
    uint32_t count;
    bool truncated;
} AfEditorFontCatalog;
bool af_editor_fonts_discover(AfEditorFontCatalog *catalog, const char *bundled_directory);
void af_editor_fonts_destroy(AfEditorFontCatalog *catalog);
const AfEditorFontFace *af_editor_fonts_find(const AfEditorFontCatalog *catalog, const char *family,
                                             bool bold, bool italic);
#endif
