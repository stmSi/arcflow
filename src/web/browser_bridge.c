#include "web/browser_bridge.h"
#include <emscripten.h>

EM_JS(void, af_web_set_ui_colors, (uint32_t panel, uint32_t accent, uint32_t text), {
    function color(value) { return "#" + value.toString(16).padStart(6, "0"); }
    const style = document.documentElement.style;
    style.setProperty("--ui-panel", color(panel));
    style.setProperty("--ui-accent", color(accent));
    style.setProperty("--ui-text", color(text));
})

EM_ASYNC_JS(char *, af_web_prepare_image, (const char *path), {
    const filename = UTF8ToString(path);
    const extension = filename.split(".").pop().toLowerCase();
    if (![ "webp", "avif", "gif", "bmp" ].includes(extension))
        return stringToNewUTF8(filename);
    try {
        if (Module["setBusy"])
            Module["setBusy"](true, "Decoding image…");
        const bitmap = await createImageBitmap(new Blob([FS.readFile(filename)]));
        if (bitmap.width > 4096 || bitmap.height > 4096 ||
            bitmap.width * bitmap.height > 16777216) {
            bitmap.close();
            throw new Error("Image exceeds 4096 pixels per side");
        }
        const canvas = new OffscreenCanvas(bitmap.width, bitmap.height);
        canvas.getContext("2d").drawImage(bitmap, 0, 0);
        bitmap.close();
        const png = await canvas.convertToBlob({type : "image/png"});
        const output = filename.substring(0, filename.lastIndexOf(".")) + ".png";
        FS.writeFile(output, new Uint8Array(await png.arrayBuffer()));
        if (Module["setBusy"])
            Module["setBusy"](false, "");
        return stringToNewUTF8(output);
    }
    catch(error) {
        console.error("Image decode failed", error);
        if (Module["setBusy"])
            Module["setBusy"](false, "Image could not be decoded");
        return 0;
    }
})

EM_JS(int, af_web_confirm_discard, (const char *title),
      { return window.confirm(UTF8ToString(title) + "\nDiscard unsaved changes?") ? 1 : 0; })

EM_JS(void, af_web_download_file, (const char *path), {
    const filename = UTF8ToString(path);
    try {
        const blob = new Blob([FS.readFile(filename)], {
            type:
                "application/octet-stream"
        });
        const url = URL.createObjectURL(blob);
        const link = document.createElement("a");
        link.href = url;
        link.download = filename.split("/").pop();
        document.body.appendChild(link);
        link.click();
        link.remove();
        if (Module["showDownload"])
            Module["showDownload"](url, link.download);
        else
            setTimeout(function() { URL.revokeObjectURL(url); }, 60000);
    }
    catch(error) {
        console.error("Download failed", error);
        Module["setStatus"]("Download failed. Please try exporting again.");
    }
})

EM_JS(void, af_web_sync_files, (void), {
    if (Module["syncDocuments"])
        Module["syncDocuments"]();
})

EM_JS(void, af_web_remember_project, (const char *path), {
    const project = UTF8ToString(path);
    if (project.startsWith("/documents/")) {
        FS.writeFile("/documents/session.json", JSON.stringify({project : project}));
        if (Module["syncDocuments"])
            Module["syncDocuments"]();
    }
})

EM_ASYNC_JS(void, af_web_task_begin, (const char *message), {
    if (Module["setBusy"])
        Module["setBusy"](true, UTF8ToString(message));
    await new Promise(function(resolve) {
        requestAnimationFrame(function() { requestAnimationFrame(resolve); });
    });
})

EM_JS(void, af_web_task_end, (int success), {
    if (Module["setBusy"])
        Module["setBusy"](false,
                          success ? "" : "Operation failed. See the editor status for details.");
})
