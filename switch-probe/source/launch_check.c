#include <switch.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

/* Read-only launch diagnostics: no config writes, title launches or memory maps. */
static const char *log_path = "sdmc:/switch/for-switch/launch-check.log";
static FILE *report;
static char config_data[16385];
static bool config_found, config_truncated, config_bom, config_read_error;
static int config_error;
static size_t config_size;
static char any_app[81] = "(not present in file)";
static char any_app_key[81] = "(not present in file)";
static char hbl_path[161] = "(not present in file)";
static char hbl_path_raw[257];
static char loader_identity[81] = "not supplied";
static bool kernel_ok, memory_ok, api_ok;
static uint64_t kernel_application, memory_allowance, api_version;
static bool default_hbl_found, configured_hbl_found, configured_path_present, hbmenu_found;
static bool configured_path_usable, configured_path_checked;
static bool r_seen, zr_seen;

static char *trim(char *text)
{
    while (isspace((unsigned char)*text)) ++text;
    size_t n = strlen(text);
    while (n && isspace((unsigned char)text[n - 1])) text[--n] = '\0';
    return text;
}

static void copy_visible(char *target, size_t size, const char *source)
{
    size_t i = 0;
    for (; i + 1 < size && source[i]; ++i) {
        unsigned char c = (unsigned char)source[i];
        target[i] = c >= 32 && c <= 126 ? (char)c : ' ';
    }
    target[i] = '\0';
}

static bool check_file(const char *path)
{
    struct stat info;
    errno = 0;
    int rc = stat(path, &info);
    int error = errno;
    bool found = rc == 0 && S_ISREG(info.st_mode);
    if (report) fprintf(report, "File %s: regular=%d rc=%d errno=%d size=%lld\n",
                        path, found, rc, error, rc == 0 ? (long long)info.st_size : -1LL);
    return found;
}

static bool info_value(const char *name, InfoType type, uint64_t *value)
{
    u64 raw = 0;
    Result rc = svcGetInfo(&raw, type, CUR_PROCESS_HANDLE, 0);
    *value = raw;
    if (report) fprintf(report, "%s: rc=0x%08" PRIx32 " value=0x%016" PRIx64 "\n",
                        name, (uint32_t)rc, *value);
    return R_SUCCEEDED(rc);
}

static void read_active_config(void)
{
    const char *path = "sdmc:/atmosphere/config/override_config.ini";
    errno = 0;
    FILE *input = fopen(path, "rb");
    config_error = errno;
    if (!input) {
        if (report) fprintf(report, "Active config open failed: %s errno=%d\n", path, config_error);
        return;
    }
    config_found = true;
    config_size = fread(config_data, 1, sizeof(config_data) - 1, input);
    config_truncated = fgetc(input) != EOF;
    config_read_error = ferror(input) != 0;
    fclose(input);
    config_data[config_size] = '\0';
    config_bom = config_size >= 3 && (unsigned char)config_data[0] == 0xef &&
                 (unsigned char)config_data[1] == 0xbb && (unsigned char)config_data[2] == 0xbf;
    if (report) {
        fprintf(report, "Active config: %s bytes=%zu truncated=%d read_error=%d UTF8_BOM=%d\n",
                path, config_size, config_truncated, config_read_error, config_bom);
        fputs("--- active file bytes begin ---\n", report);
        fwrite(config_data, 1, config_size, report);
        fputs("\n--- active file bytes end ---\n", report);
    }
    /* These are observed file values, not the loader's cached/effective state. */
    bool hbl_section = false;
    char *line = config_data + (config_bom ? 3 : 0);
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) *next++ = '\0';
        char *entry = trim(line);
        if (*entry == '[') {
            char *end = strchr(entry, ']');
            if (end) {
                *end = '\0';
                hbl_section = strcasecmp(trim(entry + 1), "hbl_config") == 0;
            } else hbl_section = false;
        } else if (hbl_section && *entry != ';' && *entry != '#') {
            char *equals = strchr(entry, '=');
            if (equals) {
                *equals = '\0';
                char *key = trim(entry), *value = trim(equals + 1);
                if (strcasecmp(key, "override_any_app") == 0)
                    copy_visible(any_app, sizeof(any_app), value);
                else if (strcasecmp(key, "override_any_app_key") == 0)
                    copy_visible(any_app_key, sizeof(any_app_key), value);
                else if (strcasecmp(key, "path") == 0) {
                    copy_visible(hbl_path, sizeof(hbl_path), value);
                    configured_path_present = true;
                    size_t n = strlen(value);
                    configured_path_usable = n < sizeof(hbl_path_raw);
                    if (configured_path_usable) memcpy(hbl_path_raw, value, n + 1);
                }
            }
        }
        line = next;
    }
}

static void collect(void)
{
    if (report) fprintf(report, "For-Switch Launch Check v0.1.0\nHOS=0x%08" PRIx32
                        " Atmosphere=%d applet_type=%d\n", (uint32_t)hosversionGet(),
                        hosversionIsAtmosphere(), (int)appletGetAppletType());
    kernel_ok = info_value("Kernel IsApplication", InfoType_IsApplication, &kernel_application);
    memory_ok = info_value("Process memory allowance", InfoType_TotalMemorySize, &memory_allowance);
    uint64_t ignored;
    info_value("Program ID", InfoType_ProgramId, &ignored);
    info_value("System resource total", InfoType_SystemResourceSizeTotal, &ignored);
    const char *loader = envGetLoaderInfo();
    u64 loader_size = envGetLoaderInfoSize();
    if (loader && loader_size) {
        size_t n = loader_size < sizeof(loader_identity) - 1
                     ? (size_t)loader_size : sizeof(loader_identity) - 1;
        memcpy(loader_identity, loader, n);
        loader_identity[n] = '\0';
        for (size_t i = 0; i < n && loader_identity[i]; ++i)
            if ((unsigned char)loader_identity[i] < 32 || (unsigned char)loader_identity[i] > 126)
                loader_identity[i] = ' ';
    }
    if (report) fprintf(report, "Loader identity: %s\n", loader_identity);
    Result init_rc = splInitialize();
    if (report) fprintf(report, "SPL init rc=0x%08" PRIx32 "\n", (uint32_t)init_rc);
    if (R_SUCCEEDED(init_rc)) {
        u64 raw = 0;
        Result rc = splGetConfig((SplConfigItem)65000, &raw); /* Atmosphere ExosphereApiVersion. */
        api_version = raw;
        api_ok = R_SUCCEEDED(rc);
        if (report) fprintf(report, "Exosphere API raw: rc=0x%08" PRIx32 " value=0x%016" PRIx64 "\n",
                            (uint32_t)rc, api_version);
        splExit();
    }
    read_active_config();
    default_hbl_found = check_file("sdmc:/atmosphere/hbl.nsp");
    hbmenu_found = check_file("sdmc:/hbmenu.nro");
    check_file("sdmc:/atmosphere/config_templates/override_config.ini");
    if (configured_path_present && configured_path_usable) {
        char path[272];
        const char *relative = hbl_path_raw;
        while (*relative == '/' || *relative == '\\') ++relative;
        snprintf(path, sizeof(path), "sdmc:/%s", relative);
        for (char *p = path; *p; ++p) if (*p == '\\') *p = '/';
        configured_hbl_found = check_file(path);
        configured_path_checked = true;
    }
    if (report) fflush(report);
}

static void draw(void)
{
    consoleClear();
    printf("For-Switch Launch Check v0.1.0\n\n");
    printf("Kernel application: %s\n", kernel_ok ? (kernel_application ? "YES" : "NO") : "UNKNOWN");
    if (memory_ok) printf("Process allowance: %" PRIu64 " MiB\n", memory_allowance >> 20);
    printf("Loader: %.60s\n", loader_identity);
    if (api_ok) printf("Atmosphere: %u.%u.%u (API query)\n",
                       (unsigned)((api_version >> 56) & 255),
                       (unsigned)((api_version >> 48) & 255),
                       (unsigned)((api_version >> 40) & 255));
    else printf("Atmosphere version: unavailable\n");
    printf("\nActive SD config: %s\n", config_found ? "FOUND" : "NOT FOUND / unreadable");
    if (config_found) {
        printf(" override_any_app: %.70s\n", any_app);
        printf(" override_any_app_key: %.66s\n", any_app_key);
        printf(" path: %.77s\n", hbl_path);
        printf(" bytes=%zu BOM=%d truncated=%d read_error=%d\n",
               config_size, config_bom, config_truncated, config_read_error);
    } else printf(" open errno: %d\n", config_error);
    printf("Default hbl.nsp: %s\n", default_hbl_found ? "FOUND" : "MISSING");
    if (configured_path_present) printf("Configured loader file: %s\n", configured_path_checked
                                        ? (configured_hbl_found ? "FOUND" : "MISSING")
                                        : "UNKNOWN (path too long)");
    printf("Root hbmenu.nro: %s\n\n", hbmenu_found ? "FOUND" : "MISSING");
    printf("Press the R bumper once, then release it.\n");
    printf("R bumper detected: %s\n", r_seen ? "YES" : "NO");
    printf("ZR trigger detected: %s\n\n", zr_seen ? "YES" : "NO");
    printf("File values above are not cached loader settings.\n");
    printf("No config changes or game launches are made here.\n\n");
    printf("Log: /switch/for-switch/launch-check.log\n");
    printf("Send this screen or the log. Press + to exit.\n");
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    mkdir("sdmc:/switch/for-switch", 0777);
    report = fopen(log_path, "w");
    collect();
    draw();
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 held = padGetButtons(&pad);
        bool next_r = r_seen || (held & HidNpadButton_R) != 0;
        bool next_zr = zr_seen || (held & HidNpadButton_ZR) != 0;
        if (next_r != r_seen || next_zr != zr_seen) {
            r_seen = next_r;
            zr_seen = next_zr;
            if (report) {
                fprintf(report, "Controller observed R=%d ZR=%d\n", r_seen, zr_seen);
                fflush(report);
            }
            draw();
        }
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    if (report) {
        fprintf(report, "Final R=%d ZR=%d\n", r_seen, zr_seen);
        fclose(report);
    }
    consoleExit(NULL);
    return 0;
}
