#include "web/browser_bridge.h"
#include <emscripten.h>

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
