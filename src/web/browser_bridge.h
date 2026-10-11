#ifndef AF_WEB_BROWSER_BRIDGE_H
#define AF_WEB_BROWSER_BRIDGE_H

void af_web_download_file(const char *path);
void af_web_sync_files(void);
void af_web_remember_project(const char *path);
int af_web_confirm_discard(const char *title);
void af_web_task_begin(const char *message);
void af_web_task_end(int success);
char *af_web_prepare_image(const char *path);

#endif
