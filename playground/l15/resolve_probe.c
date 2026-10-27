#include "app/logic.h"

#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static void mkdirs(const char *path) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 3; *p; p++) {
        if (*p == '\\') {
            *p = '\0';
            _mkdir(buf);
            *p = '\\';
        }
    }
    _mkdir(buf);
}

int main(void) {
    const char *install = "C:\\Users\\lavantien\\AppData\\Local\\Temp\\l15-resolve-probe";
    char dir[1024];
    snprintf(dir, sizeof dir, "%s\\ref_ground\\packs", install);
    mkdirs(dir);
    snprintf(dir, sizeof dir, "%s\\assets", install);
    mkdirs(dir);
    snprintf(dir, sizeof dir, "%s\\config", install);
    mkdirs(dir);
    char config[1024];
    snprintf(config, sizeof config, "%s\\config\\config.json", install);
    FILE *file = fopen(config, "wb");
    fputs("{}", file);
    fclose(file);
    bt_app_runtime_paths missing;
    snprintf(missing.config_path, sizeof missing.config_path, "%s", "C:\\nope\\config.absent");
    snprintf(missing.vendor_root, sizeof missing.vendor_root, "%s", "C:\\nope\\vendor");
    snprintf(missing.packs_root, sizeof missing.packs_root, "%s", "C:\\nope\\packs");
    snprintf(missing.user_root, sizeof missing.user_root, "%s", "C:\\nope\\user");
    snprintf(missing.assets_root, sizeof missing.assets_root, "%s", "C:\\nope\\assets");
    bt_app_runtime_paths resolved;
    bt_app_paths_resolve(&missing, install, &resolved);
    struct stat info;
    char probe[1024];
    snprintf(probe, sizeof probe, "%s\\ref_ground", install);
    printf("stat ref_ground=%d path=%s\n", stat(probe, &info), probe);
    printf("config  =%s\nvendor  =%s\npacks   =%s\nassets  =%s\nuser    =%s\n",
           resolved.config_path, resolved.vendor_root, resolved.packs_root,
           resolved.assets_root, resolved.user_root);
    return 0;
}
