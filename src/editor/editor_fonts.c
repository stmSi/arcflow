#include "editor/editor_fonts.h"
#include <SDL3/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AfFontScan {
    AfEditorFontCatalog *catalog;
    FT_Library library;
    unsigned depth, visited;
} AfFontScan;

static SDL_EnumerationResult SDLCALL af_font_scan(void *userdata, const char *directory,
                                                  const char *name) {
    AfFontScan *scan = userdata;
    if (++scan->visited > 100000 || scan->catalog->count == AF_EDITOR_FONT_LIMIT) {
        scan->catalog->truncated = true;
        return SDL_ENUM_SUCCESS;
    }
    char path[AF_EDITOR_FONT_PATH];
    if (snprintf(path, sizeof(path), "%s%s", directory, name) >= (int)sizeof(path))
        return SDL_ENUM_CONTINUE;
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info))
        return SDL_ENUM_CONTINUE;
    if (info.type == SDL_PATHTYPE_DIRECTORY) {
        if (scan->depth < 12 && name[0] != '.') {
            ++scan->depth;
            (void)SDL_EnumerateDirectory(path, af_font_scan, scan);
            --scan->depth;
        }
        return SDL_ENUM_CONTINUE;
    }
    const char *extension = strrchr(name, '.');
    if (!extension || (SDL_strcasecmp(extension, ".ttf") && SDL_strcasecmp(extension, ".otf") &&
                       SDL_strcasecmp(extension, ".ttc") && SDL_strcasecmp(extension, ".otc")))
        return SDL_ENUM_CONTINUE;
    FT_Long faces = 1;
    for (FT_Long index = 0; index < faces && scan->catalog->count < AF_EDITOR_FONT_LIMIT; ++index) {
        FT_Face face = NULL;
        if (FT_New_Face(scan->library, path, index, &face))
            break;
        faces = face->num_faces;
        if (FT_IS_SCALABLE(face) && face->family_name &&
            strlen(face->family_name) < AF_EDITOR_FONT_NAME) {
            AfEditorFontFace *entry = &scan->catalog->faces[scan->catalog->count++];
            (void)snprintf(entry->family, sizeof(entry->family), "%s", face->family_name);
            (void)snprintf(entry->path, sizeof(entry->path), "%s", path);
            entry->face_index = (uint32_t)index;
            entry->bold = (face->style_flags & FT_STYLE_FLAG_BOLD) != 0;
            entry->italic = (face->style_flags & FT_STYLE_FLAG_ITALIC) != 0;
        }
        (void)FT_Done_Face(face);
    }
    return SDL_ENUM_CONTINUE;
}
static int af_font_compare(const void *a, const void *b) {
    const AfEditorFontFace *left = a, *right = b;
    int result = SDL_strcasecmp(left->family, right->family);
    if (!result)
        result = (int)(left->bold + 2 * left->italic) - (int)(right->bold + 2 * right->italic);
    if (!result)
        result = strcmp(left->path, right->path);
    return result;
}
bool af_editor_fonts_discover(AfEditorFontCatalog *catalog, const char *bundled_directory) {
    if (!catalog)
        return false;
    if (catalog->faces)
        return true;
    catalog->faces = calloc(AF_EDITOR_FONT_LIMIT, sizeof(*catalog->faces));
    FT_Library library = NULL;
    if (!catalog->faces || FT_Init_FreeType(&library)) {
        af_editor_fonts_destroy(catalog);
        return false;
    }
    AfFontScan scan = {.catalog = catalog, .library = library};
    if (bundled_directory)
        (void)SDL_EnumerateDirectory(bundled_directory, af_font_scan, &scan);
#ifdef _WIN32
    const char *root = SDL_getenv("WINDIR");
    char path[AF_EDITOR_FONT_PATH];
    if (root && snprintf(path, sizeof(path), "%s/Fonts", root) < (int)sizeof(path))
        (void)SDL_EnumerateDirectory(path, af_font_scan, &scan);
    root = SDL_getenv("LOCALAPPDATA");
    if (root &&
        snprintf(path, sizeof(path), "%s/Microsoft/Windows/Fonts", root) < (int)sizeof(path))
        (void)SDL_EnumerateDirectory(path, af_font_scan, &scan);
#else
    const char *roots[] = {"/usr/share/fonts", "/usr/local/share/fonts", "/System/Library/Fonts",
                           "/Library/Fonts"};
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i)
        (void)SDL_EnumerateDirectory(roots[i], af_font_scan, &scan);
    const char *home = SDL_getenv("HOME");
    const char *suffixes[] = {"/.fonts", "/.local/share/fonts", "/Library/Fonts"};
    char path[AF_EDITOR_FONT_PATH];
    if (home)
        for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i)
            if (snprintf(path, sizeof(path), "%s%s", home, suffixes[i]) < (int)sizeof(path))
                (void)SDL_EnumerateDirectory(path, af_font_scan, &scan);
    const char *data = SDL_getenv("XDG_DATA_HOME");
    if (data && snprintf(path, sizeof(path), "%s/fonts", data) < (int)sizeof(path))
        (void)SDL_EnumerateDirectory(path, af_font_scan, &scan);
#endif
    (void)FT_Done_FreeType(library);
    qsort(catalog->faces, catalog->count, sizeof(*catalog->faces), af_font_compare);
    return catalog->count != 0;
}
void af_editor_fonts_destroy(AfEditorFontCatalog *catalog) {
    if (catalog) {
        free(catalog->faces);
        *catalog = (AfEditorFontCatalog){0};
    }
}
const AfEditorFontFace *af_editor_fonts_find(const AfEditorFontCatalog *catalog, const char *family,
                                             bool bold, bool italic) {
    if (catalog && family)
        for (uint32_t i = 0; i < catalog->count; ++i) {
            const AfEditorFontFace *face = &catalog->faces[i];
            if (strcmp(face->family, family) == 0 && face->bold == bold && face->italic == italic)
                return face;
        }
    return NULL;
}
