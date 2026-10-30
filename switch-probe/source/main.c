#include <switch.h>

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* This diagnostic contains no game code. Never access an unbacked guest window. */
static const uint64_t window_size = UINT64_C(0x100010000);
static const uint64_t base_alignment = UINT64_C(0x100000000);
static const uint64_t test_offset = UINT64_C(0x10000);
static const uint64_t test_size = UINT64_C(0x1000);
static const char *log_path = "sdmc:/switch/for-switch/memory-probe-v0.1.1.log";
static FILE *report;
static bool must_exit_process;
static uint64_t kernel_application, program_id, total_memory, resource_total;
static bool kernel_application_ok, program_id_ok, total_memory_ok, resource_total_ok;
static bool window_found;
static char loader_identity[81] = "not supplied";
static const char *page_result = "Not attempted";

static void record(const char *format, ...)
    __attribute__((format(printf, 1, 2)));

static void record(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    if (report) {
        va_start(args, format);
        vfprintf(report, format, args);
        va_end(args);
        fflush(report);
    }
}

static bool query_info(const char *name, InfoType type, uint64_t *value)
{
    u64 raw = 0;
    Result rc = svcGetInfo(&raw, type, CUR_PROCESS_HANDLE, 0);
    *value = (uint64_t)raw;
    record("%s: rc=0x%08" PRIx32 " value=0x%016" PRIx64 "\n",
           name, (uint32_t)rc, *value);
    return R_SUCCEEDED(rc);
}

static void query_address(const char *label, uint64_t address)
{
    MemoryInfo info = {0};
    u32 page_info = 0;
    Result rc = svcQueryMemory(&info, &page_info, address);
    record("%s @0x%016" PRIx64 ": rc=0x%08" PRIx32,
           label, address, (uint32_t)rc);
    if (R_SUCCEEDED(rc)) {
        record(" region=0x%016" PRIx64 "+0x%016" PRIx64
               " type=0x%08" PRIx32 " perm=0x%08" PRIx32
               " attr=0x%08" PRIx32 "\n",
               (uint64_t)info.addr, (uint64_t)info.size,
               (uint32_t)info.type, (uint32_t)info.perm, (uint32_t)info.attr);
    } else {
        record("\n");
    }
}

static bool covers_page(const MemoryInfo *info, uint64_t address)
{
    return info->addr <= address && address - info->addr <= info->size &&
           test_size <= info->size - (address - info->addr);
}

/* Caller holds virtmemLock. A libnx reservation is bookkeeping, not a kernel map. */
static VirtmemReservation *reserve_alias_window(uint64_t alias_base,
                                              uint64_t alias_size,
                                              uint64_t *chosen,
                                              Result *query_rc)
{
    *chosen = 0;
    *query_rc = 0;
    if (alias_size < window_size || alias_base > UINT64_MAX - alias_size)
        return NULL;
    uint64_t limit = alias_base + alias_size;
    if (alias_base > UINT64_MAX - (base_alignment - 1))
        return NULL;
    uint64_t candidate = (alias_base + base_alignment - 1) & ~(base_alignment - 1);
    if (!candidate)
        candidate = base_alignment;

    /* The explicit bound also prevents a malformed query response causing a loop. */
    for (unsigned count = 0; count < 4096; ++count) {
        if (candidate >= limit || window_size > limit - candidate)
            return NULL;
        MemoryInfo info = {0};
        u32 page_info = 0;
        *query_rc = svcQueryMemory(&info, &page_info, candidate);
        if (R_FAILED(*query_rc) || info.size == 0 || info.addr > candidate)
            return NULL;
        uint64_t end = info.size > UINT64_MAX - info.addr
                         ? UINT64_MAX : info.addr + info.size;
        if (end <= candidate)
            return NULL;
        if ((info.type & MemState_Type) == MemType_Unmapped &&
            window_size <= end - candidate) {
            VirtmemReservation *reservation =
                virtmemAddReservation((void *)(uintptr_t)candidate, (size_t)window_size);
            if (reservation)
                *chosen = candidate;
            return reservation;
        }
        if (end > UINT64_MAX - (base_alignment - 1))
            return NULL;
        candidate = (end + base_alignment - 1) & ~(base_alignment - 1);
    }
    return NULL;
}

static void run_probe(void)
{
    uint64_t alias_base = 0, alias_size = 0, alias_extra = 0;
    uint64_t system_total = 0, ignored = 0;
    record("For-Switch memory diagnostic 0.1.1\n");
    record("This measures this launch only; it is not a game or a performance test.\n");
    record("Built with devkitA64 GCC %s and libnx 4.12.0.\n", __VERSION__);
    record("HOS=0x%08" PRIx32 " Atmosphere=%d applet_type=%d\n",
           (uint32_t)hosversionGet(), hosversionIsAtmosphere(), (int)appletGetAppletType());
    const char *loader = envGetLoaderInfo();
    size_t loader_size = (size_t)envGetLoaderInfoSize();
    if (loader && loader_size) {
        if (loader_size >= sizeof(loader_identity))
            loader_size = sizeof(loader_identity) - 1;
        memcpy(loader_identity, loader, loader_size);
        loader_identity[loader_size] = '\0';
        for (size_t i = 0; i < loader_size && loader_identity[i]; ++i)
            if ((unsigned char)loader_identity[i] < 32 || (unsigned char)loader_identity[i] > 126)
                loader_identity[i] = ' ';
    }
    record("Loader identity: %s\n", loader_identity);
    kernel_application_ok = query_info("Kernel IsApplication", InfoType_IsApplication,
                                       &kernel_application);
    program_id_ok = query_info("Kernel ProgramId", InfoType_ProgramId, &program_id);
    record("Heap override=%d address=0x%016" PRIx64 " size=0x%016" PRIx64 "\n",
           envHasHeapOverride(), (uint64_t)(uintptr_t)envGetHeapOverrideAddr(),
           (uint64_t)envGetHeapOverrideSize());
    record("Real process handle=0x%08" PRIx32 "\n", (uint32_t)envGetOwnProcessHandle());
    record("SVC hints: QueryMemory=%d GetInfo=%d MapPhysical=%d UnmapPhysical=%d\n",
           envIsSyscallHinted(0x06), envIsSyscallHinted(0x29),
           envIsSyscallHinted(0x2c), envIsSyscallHinted(0x2d));
    bool alias_ok = query_info("Alias address", InfoType_AliasRegionAddress, &alias_base);
    alias_ok &= query_info("Alias size", InfoType_AliasRegionSize, &alias_size);
    bool extra_ok = query_info("Alias extra size", InfoType_AliasRegionExtraSize, &alias_extra);
    /* Match libnx4.12.0's alias exclusion in ALL three standard selectors.
     * Its extra tail is available to those selectors, so never claim it here.
     * This isolated application has no other direct AddReservation caller. */
    if (extra_ok) {
        if (alias_extra > alias_size)
            alias_ok = false;
        else
            alias_size -= alias_extra;
    } else if (hosversionAtLeast(18, 0, 0)) {
        alias_ok = false; /* Cannot establish the selector exclusion boundary. */
    }
    query_info("ASLR address", InfoType_AslrRegionAddress, &ignored);
    query_info("ASLR size", InfoType_AslrRegionSize, &ignored);
    query_info("Heap address", InfoType_HeapRegionAddress, &ignored);
    query_info("Heap size", InfoType_HeapRegionSize, &ignored);
    query_info("Stack address", InfoType_StackRegionAddress, &ignored);
    query_info("Stack size", InfoType_StackRegionSize, &ignored);
    total_memory_ok = query_info("Total memory", InfoType_TotalMemorySize, &total_memory);
    query_info("Used memory before reservation", InfoType_UsedMemorySize, &ignored);
    bool system_ok = query_info("System resource total", InfoType_SystemResourceSizeTotal,
                                &system_total);
    resource_total_ok = system_ok;
    resource_total = system_total;
    query_info("System resource used", InfoType_SystemResourceSizeUsed, &ignored);
    query_info("Total non-system memory", InfoType_TotalNonSystemMemorySize, &ignored);
    query_info("Used non-system memory", InfoType_UsedNonSystemMemorySize, &ignored);

    if (!alias_ok) {
        record("SKIP reservation: alias region queries failed.\n");
        return;
    }
    uint64_t chosen = 0;
    Result query_rc = 0;
    virtmemLock();
    VirtmemReservation *reservation =
        reserve_alias_window(alias_base, alias_size, &chosen, &query_rc);
    virtmemUnlock();
    if (!reservation) {
        record("INCONCLUSIVE: no reserved 4GiB-aligned alias window; last query=0x%08"
               PRIx32 ".\n", (uint32_t)query_rc);
        record("This does not establish that another guest-memory design cannot work.\n");
        return;
    }
    window_found = true;
    record("Virtual bookkeeping reservation: base=0x%016" PRIx64
           " size=0x%016" PRIx64 " low32(base)=0x%08" PRIx32 "\n",
           chosen, window_size, (uint32_t)chosen);
    record("No 4GiB physical allocation. No access to unmapped addresses.\n");
    record("Search excludes alias extra tail, matching libnx selector exclusions.\n");
    query_address("Reservation remains unmapped", chosen);
    query_info("Used memory after reservation", InfoType_UsedMemorySize, &ignored);

    bool keep_reservation = false;
    if (!envIsSyscallHinted(0x2c) || !envIsSyscallHinted(0x2d) ||
        !hosversionAtLeast(3, 0, 0) || !system_ok || !system_total) {
        page_result = system_ok && !system_total ? "Skipped: zero system resources"
                                                 : "Skipped: SVC/resource unavailable";
        record("SKIP explicit-map test: required SVC hints/system resources unavailable.\n");
        record("Zero system resources may also occur in application mode.\n");
    } else {
        uint64_t test_address = chosen + test_offset;
        MemoryInfo before = {0}, after = {0};
        u32 page_info = 0;
        Result map_rc = 0, after_rc = 0, unmap_rc = 0;
        u64 mapped_used = 0;
        Result used_rc = 0;
        bool attempted = false, accessible = false, matched = false;
        /* MapPhysicalMemory can succeed on existing mappings. Establish an
         * unmapped entire page immediately before mapping, under the VM lock.
         * No logging or other allocation occurs between these operations. */
        virtmemLock();
        Result before_rc = svcQueryMemory(&before, &page_info, test_address);
        if (R_SUCCEEDED(before_rc) && covers_page(&before, test_address) &&
            (before.type & MemState_Type) == MemType_Unmapped) {
            attempted = true;
            map_rc = svcMapPhysicalMemory((void *)(uintptr_t)test_address, test_size);
            if (R_SUCCEEDED(map_rc)) {
                after_rc = svcQueryMemory(&after, &page_info, test_address);
                accessible = R_SUCCEEDED(after_rc) && covers_page(&after, test_address) &&
                             (after.type & MemState_Type) == MemType_Heap &&
                             (after.perm & Perm_Rw) == Perm_Rw && after.attr == 0;
                used_rc = svcGetInfo(&mapped_used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
                if (accessible) {
                    volatile uint64_t *page = (volatile uint64_t *)(uintptr_t)test_address;
                    const uint64_t first = UINT64_C(0x123456789abcdef0);
                    const uint64_t last = UINT64_C(0xfedcba9876543210);
                    page[0] = first;
                    page[(test_size / sizeof(*page)) - 1] = last;
                    matched = page[0] == first &&
                              page[(test_size / sizeof(*page)) - 1] == last;
                }
                /* Always undo our successful map, including postquery failures. */
                unmap_rc = svcUnmapPhysicalMemory((void *)(uintptr_t)test_address,
                                                  test_size);
            }
        }
        virtmemUnlock();
        record("Immediate page prequery: rc=0x%08" PRIx32 " unmapped=%d\n",
               (uint32_t)before_rc, attempted);
        if (!attempted) {
            page_result = "Skipped: page not verified free";
            record("INCONCLUSIVE: exact page no longer verified free; no map or access.\n");
        } else {
            record("MapPhysicalMemory own 4KiB page: rc=0x%08" PRIx32 "\n",
                   (uint32_t)map_rc);
        }
        if (attempted && R_SUCCEEDED(map_rc)) {
            page_result = accessible ? (matched ? "Sentinels passed" : "Sentinels failed")
                                     : "Skipped: mapped page not accessible";
            record("Mapped page query: rc=0x%08" PRIx32
                   " type=0x%08" PRIx32 " perm=0x%08" PRIx32
                   " attr=0x%08" PRIx32 " covers-page=%d accessible=%d\n",
                   (uint32_t)after_rc, (uint32_t)after.type, (uint32_t)after.perm,
                   (uint32_t)after.attr, covers_page(&after, test_address), accessible);
            record("Used memory while mapped: rc=0x%08" PRIx32 " value=0x%016" PRIx64 "\n",
                   (uint32_t)used_rc, (uint64_t)mapped_used);
            record("Own page sentinel read/write: %s\n",
                   accessible ? (matched ? "PASS" : "FAIL") : "SKIP; page query/permissions inconclusive");
            record("UnmapPhysicalMemory own page: rc=0x%08" PRIx32 "\n",
                   (uint32_t)unmap_rc);
            if (R_SUCCEEDED(unmap_rc)) {
                query_address("After page cleanup", test_address);
                record("Explicit page mapping was released.\n");
            } else {
                /* Returning to hbmenu alone does not tear down the process.
                 * Force process exit on this exceptional path to reclaim it. */
                keep_reservation = true;
                must_exit_process = true;
                page_result = "Cleanup failed; process will exit";
                record("FAIL cleanup: closing host process to reclaim own page.\n");
            }
        } else if (attempted) {
            page_result = "Mapping failed; see log result";
            record("INCONCLUSIVE mapping failure: loader heap/resource budget may consume allowance.\n");
        }
    }
    if (!keep_reservation) {
        virtmemLock();
        virtmemRemoveReservation(reservation);
        virtmemUnlock();
        record("Removed own libnx reservation.\n");
    }
    query_info("Used memory after cleanup", InfoType_UsedMemorySize, &ignored);
    record("A reserved window/page test does not prove that the runtime can boot or fit.\n");
}

static void show_summary(void)
{
    consoleClear();
    record("For-Switch Memory Probe v0.1.1\n\n");
    record("Kernel application: %s\n", kernel_application_ok
           ? (kernel_application ? "YES" : "NO") : "UNKNOWN (query failed)");
    record("Loader applet type: %d\n", (int)appletGetAppletType());
    record("Loader: %.60s\n", loader_identity);
    if (program_id_ok)
        record("Program ID: %016" PRIx64 "\n", program_id);
    if (total_memory_ok)
        record("Process memory allowance: %" PRIu64 " MiB\n", total_memory >> 20);
    else
        record("Process memory allowance: UNKNOWN\n");
    if (envHasHeapOverride())
        record("Loader heap override: %" PRIu64 " MiB\n", (uint64_t)envGetHeapOverrideSize() >> 20);
    else
        record("Loader heap override: none\n");
    if (resource_total_ok)
        record("System resources: %" PRIu64 " KiB\n", resource_total >> 10);
    else
        record("System resources: UNKNOWN\n");
    record("Virtual window: %s\n", window_found ? "Found" : "Not established");
    record("4 KiB page test: %s\n\n", page_result);
    record("Zero system resources does not identify launch mode.\n");
    record("This diagnostic does not run the game.\n\n");
    record("Log: /switch/for-switch/\n");
    record("     memory-probe-v0.1.1.log\n\n");
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
    record("Log: %s (%s)\n", log_path, report ? "opened" : "console only; open failed");
    run_probe();
    show_summary();
    record(must_exit_process ? "Cleanup failed; closing the host process.\n"
                             : "Probe complete. Press + to exit.\n");
    if (report) {
        fclose(report);
        report = NULL;
    }
    if (must_exit_process) {
        consoleUpdate(NULL);
        consoleExit(NULL);
        svcExitProcess();
    }
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}
