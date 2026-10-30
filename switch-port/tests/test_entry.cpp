#include "ds_entry.h"
#include "ds_clock.h"
#include "ds_sync.h"
#include "ds_rwlock.h"
#include "ds_kernel_mutex.h"
#include "ds_reference.h"

#include <cstdio>
#include <cstring>
#include <time.h>

static unsigned checks, failures;
static const char *image_path;
static constexpr size_t image_budget = 48u * 1024u * 1024u;
static constexpr size_t arena_bytes = 0x100000u + 0x10000u;

#define CHECK(condition, description) do { \
    ++checks; \
    if (!(condition)) { \
        std::fprintf(stderr, "FAIL %s (%s:%d)\n", description, __FILE__, __LINE__); \
        ++failures; \
    } \
} while (0)

static bool load(DsImage &image, size_t budget = image_budget)
{
    char error[256] = {};
    bool ok = ds_image_load_file(&image, image_path, budget, error, sizeof(error));
    CHECK(ok, "real verified ELF loads for entry test");
    if (!ok) std::fprintf(stderr, "Loader error: %s\n", error);
    return ok;
}

static void cleanup(DsImage &image)
{
    ds_image_destroy(&image);
    CHECK(image.segment_count == 0 && image.allocated_bytes == 0, "entry image and arenas cleaned up");
}

static bool execute(DsImage &image, DsEntryResult &result, char error[256])
{
    return ds_entry_run(&image, &result, error, 256);
}

static void check_tls_boundary(const DsImage &image, const DsEntryResult &result)
{
    CHECK(result.stop == DS_ENTRY_TLS_BOUNDARY, "stops at first TLS import, not a fabricated return");
    CHECK(result.boundary_nid == 0x744680A2u && result.callee == 0x016BB3E4u,
          "actual sys_initialize_tls boundary ID and callsite");
    CHECK(result.r3 == 1 && result.r4 == DS_REF_TLS_ADDRESS && result.r5 == 4 && result.r6 == 1076,
          "firmware arguments match the real game's startup ABI");
    CHECK(result.lr == 0x12648 && result.stack_pointer == 0x0FEFFEF0,
          "actual guest link register and stack pointer");
    CHECK(result.function_count == 3 && result.functions[0] == 0x000124C0 &&
          result.functions[1] == 0x000125F0 && result.functions[2] == 0x016BB3E4,
          "three real translated functions run in order");
    CHECK(!result.memory_fault, "valid startup has no fabricated null reads or guest-memory faults");
    CHECK(result.trace_count == 13 && result.trace_stored == 13, "all thirteen guest accesses recorded");
    const DsEntryAccess &first = result.trace[0];
    CHECK(first.address == DS_REF_ENTRY_DESCRIPTOR + 4 && first.size == 4 &&
          !first.write && first.value == DS_REF_TOC, "first access reads actual entry TOC");
    const DsEntryAccess &last = result.trace[12];
    CHECK(last.address == 0x01AD5BC0 && last.size == 4 && last.write && last.value == 0x100000,
          "real startup writes its process parameter word");
    uint32_t word = 0;
    uint64_t wide = 0;
    CHECK(ds_image_read_be32(&image, 0x01AD5BC0, &word) && word == 0x100000,
          "process parameter write exists in actual guest backing");
    CHECK(ds_image_read_be64(&image, 0x0FEFFEF0, &wide) && wide == 0x0FEFFF90,
          "inner stack backchain was written through segmented memory");
    CHECK(ds_image_read_be64(&image, 0x0FEFFF90, &wide) && wide == 0,
          "outer stack chain terminates exactly as startup writes it");
    CHECK(ds_image_read_be64(&image, 0x0FEFFFA0, &wide) && wide == 0x124E0,
          "guest return address saved to the expected stack slot");
    const void *tls_source = ds_image_address(&image, DS_REF_TLS_ADDRESS, DS_REF_TLS_FILE_SIZE, DS_PF_R);
    const void *tls_copy = ds_image_address(&image, 0x10F00000, DS_REF_TLS_FILE_SIZE, DS_PF_R);
    CHECK(tls_source && tls_copy && std::memcmp(tls_source, tls_copy, DS_REF_TLS_FILE_SIZE) == 0,
          "startup host ABI preparation copies real TLS template bytes");
    CHECK(image.segment_count == DS_REF_SEGMENT_COUNT + 2 &&
          image.allocated_bytes == DS_REF_ALLOCATED_BYTES + arena_bytes,
          "startup uses only bounded stack and TLS arenas");
}

static void actual_startup()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult result = {};
    char error[256] = {};
    bool ok = execute(image, result, error);
    CHECK(ok, "real translated entry reaches first firmware boundary");
    if (!ok) std::fprintf(stderr, "Entry error: %s (stop=%d)\n", error, result.stop);
    if (ok) check_tls_boundary(image, result);
    size_t allocations = image.allocated_bytes;
    CHECK(ds_image_write_be32(&image, 0x01AD5BC0, 0),
          "clear process parameter to distinguish an actual write from its file value");
    uint32_t word = 1;
    CHECK(ds_image_read_be32(&image, 0x01AD5BC0, &word) && word == 0,
          "cleared process parameter is observable before rerun");
    for (unsigned i = 0; i < 3; ++i) {
        error[0] = 0;
        CHECK(execute(image, result, error), "repeated entry run reuses its own arenas");
        CHECK(image.allocated_bytes == allocations, "repeated startup does not leak/add arenas");
    }
    CHECK(ds_image_read_be32(&image, 0x01AD5BC0, &word) && word == 0x100000,
          "real translated startup restores the changed process parameter in memory");
    cleanup(image);
}

static void initialized_tls()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult result = {};
    char error[256] = {};
    CHECK(ds_entry_run_tls(&image, &result, error, sizeof(error)), "real startup implements main-thread TLS");
    CHECK(result.stop == DS_ENTRY_TLS_INITIALIZED && result.tls_initialized,
          "post-service checkpoint is distinct from the old import boundary");
    CHECK(result.r3 == 0 && result.thread_pointer == 0x0E007000 && result.tls_block == 0x0E000000,
          "TLS service sets CELL_OK and biased PPC thread pointer");
    CHECK(result.lr == 0x12648 && result.function_count == 3 && !result.memory_fault,
          "no later guest instructions run after TLS service");
    CHECK(result.trace_count == 1093 && result.trace_stored == 32,
          "all template reads and TLS writes counted with bounded trace storage");
    const auto *source = static_cast<const unsigned char *>(
        ds_image_address(&image, DS_REF_TLS_ADDRESS, 4, DS_PF_R));
    auto *block = static_cast<unsigned char *>(
        ds_image_address(&image, 0x0E000000, 0x9000, DS_PF_R | DS_PF_W));
    CHECK(source && block && std::memcmp(source, block, 4) == 0, "real TLS template copied to service allocation");
    bool zero = block != nullptr;
    if (block) for (size_t i = 4; i < 0x9000; ++i) zero &= block[i] == 0;
    CHECK(zero, "TLS BSS and arena padding are zero");
    CHECK(image.allocated_bytes == DS_REF_ALLOCATED_BYTES + arena_bytes + 0x9000,
          "service allocation matches pinned runtime's page-rounded TLS size");
    if (block) std::memset(block, 0xA5, 0x9000);
    const size_t allocated = image.allocated_bytes;
    CHECK(ds_entry_run_tls(&image, &result, error, sizeof(error)), "TLS rerun resets owned arena");
    CHECK(image.allocated_bytes == allocated, "TLS rerun does not leak allocations");
    CHECK(block && source && std::memcmp(source, block, 4) == 0 && block[4] == 0 && block[1075] == 0 && block[0x8FFF] == 0,
          "rerun restores template, BSS and padding after contamination");
    CHECK(execute(image, result, error) && result.stop == DS_ENTRY_TLS_BOUNDARY && !result.tls_initialized,
          "original pre-service API remains available after a TLS run");
    cleanup(image);

    if (!load(image, DS_REF_ALLOCATED_BYTES + arena_bytes)) return;
    CHECK(!ds_entry_run_tls(&image, &result, error, sizeof(error)) && result.stop == DS_ENTRY_BAD_INPUT,
          "insufficient service budget fails without claiming TLS success");
    CHECK(!result.tls_initialized && image.allocated_bytes <= image.memory_limit && error[0],
          "failed TLS allocation remains bounded and reports error");
    cleanup(image);

    if (!load(image)) return;
    CHECK(ds_image_add_zero_region(&image, 0x0E000000, 64, DS_PF_R | DS_PF_W, error, sizeof(error)),
          "incompatible TLS region fixture created");
    auto *sentinel = static_cast<unsigned char *>(ds_image_address(&image, 0x0E000000, 64, DS_PF_W));
    if (sentinel) sentinel[0] = 0x5A;
    CHECK(!ds_entry_run_tls(&image, &result, error, sizeof(error)) && !result.tls_initialized,
          "overlapping TLS arena rejected");
    CHECK(sentinel && sentinel[0] == 0x5A, "rejected overlap preserves existing bytes");
    cleanup(image);
}

static void crt_startup()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult result = {};
    char error[256] = {};
    bool ok = ds_entry_run_crt(&image, &result, error, sizeof(error));
    CHECK(ok, "real CRT reaches memory query after TLS and mutex creation");
    if (!ok) std::fprintf(stderr, "CRT error: %s stop=%d last=%08x lr=%llx sp=%llx r3=%llx toc=%llx functions=%u accesses=%u\n",
                         error, result.stop, result.callee, (unsigned long long)result.lr,
                         (unsigned long long)result.stack_pointer, (unsigned long long)result.r3,
                         (unsigned long long)result.toc, result.function_count, result.trace_count);
    CHECK(result.stop == DS_ENTRY_MEMORY_QUERY_BOUNDARY && result.syscall_number == 352 &&
          result.tls_initialized && result.mutex_initialized && !result.memory_fault,
          "memory query is a deliberate boundary after both service effects");
    CHECK(result.thread_pointer == 0xE007000 && result.toc == 0x19D3BAC && result.r3 == 0xFEFFE04,
          "syscall receives backed output and expected library TOC/thread pointer");
    CHECK(result.mutex_address == 0x1CE6450 && result.mutex_attribute == 0x176F780,
          "actual CRT mutex and attribute locations captured");
    uint32_t protocol = 0, word = 0;
    CHECK(ds_image_read_be32(&image, result.mutex_attribute, &protocol), "actual mutex attribute protocol readable");
    bool mutex_ok = result.mutex_initialized;
    for (unsigned i = 0; i < 24; i += 4)
        mutex_ok &= ds_image_read_be32(&image, result.mutex_address + i, &word) && word == (i == 8 ? protocol : 0);
    CHECK(mutex_ok, "mutex fields initialized to unlocked state and actual protocol");
    uint64_t wide = 0;
    CHECK(ds_image_read_be64(&image, 0xFEFFF18, &wide) && wide == DS_REF_TOC,
          "TLS import preserves outer CRT TOC slot");
    CHECK(ds_image_read_be64(&image, 0xFEFFDB8, &wide) && wide == result.toc,
          "mutex import preserves library TOC in its own stack frame");
    CHECK(result.argv_address == 0x10000000 && result.envp_address == 0x10000010,
          "argv/envp use separately backed writable arena");
    CHECK(ds_image_read_be32(&image, result.argv_address, &word) && word == 0x10000020,
          "real CRT compacts 64-bit argv pointer into 32-bit slot");
    CHECK(ds_image_read_be32(&image, result.argv_address + 4, &word) && word == 0,
          "compacted argv includes null terminator");
    CHECK(ds_image_read_be32(&image, result.envp_address, &word) && word == 0,
          "real CRT compacts empty envp without a null-address access");
    const char boot[] = "/dev_bdvd/PS3_GAME/USRDIR/EBOOT.BIN";
    const void *actual_boot = ds_image_address(&image, 0x10000020, sizeof(boot), DS_PF_R);
    CHECK(actual_boot && std::memcmp(actual_boot, boot, sizeof(boot)) == 0,
          "actual guest boot path materialized and preserved");
    CHECK(ds_image_read_be32(&image, 0x1AD6100, &word) && word == 1 &&
          ds_image_read_be32(&image, 0x1AD6104, &word) && word == 0x10000000,
          "CRT stores real argc and compacted argv in its globals");
    CHECK(!ds_image_address(&image, 0xB00000, 4, DS_PF_W),
          "original executable remains read-only at desktop argv scratch address");
    CHECK(image.allocated_bytes == DS_REF_ALLOCATED_BYTES + arena_bytes + 0x9000 + 0x1000,
          "CRT stays within explicit stack/TLS/argv budgets");
    const size_t allocated = image.allocated_bytes;
    CHECK(ds_entry_run_crt(&image, &result, error, sizeof(error)), "CRT checkpoint can rerun after guest globals changed");
    CHECK(image.allocated_bytes == allocated, "CRT rerun reuses arenas without growth");
    cleanup(image);

    if (!load(image, DS_REF_ALLOCATED_BYTES + arena_bytes + 0x1000 + 0x9000 - 1)) return;
    CHECK(!ds_entry_run_crt(&image, &result, error, sizeof(error)) && !result.tls_initialized &&
          !result.mutex_initialized, "CRT insufficient TLS budget stops before guest services claim success");
    cleanup(image);

    if (!load(image)) return;
    CHECK(ds_image_add_zero_region(&image, 0x10000000, 64, DS_PF_R | DS_PF_W, error, sizeof(error)),
          "incompatible argv region fixture created");
    CHECK(!ds_entry_run_crt(&image, &result, error, sizeof(error)) && result.function_count == 0,
          "CRT refuses overlapping argv backing before execution");
    cleanup(image);

    if (!load(image)) return;
    CHECK(ds_image_add_zero_region(&image, 0x11000000, 0x1000, DS_PF_R, error, sizeof(error)),
          "read-only mutex target fixture created");
    CHECK(ds_image_write_be32(&image, 0x19D85F8, 0x11000000),
          "fault fixture redirects CRT heap-state pointer");
    CHECK(!ds_entry_run_crt(&image, &result, error, sizeof(error)) && result.memory_fault &&
          result.fault_write && !result.mutex_initialized,
          "read-only CRT state rejects write without claiming mutex creation");
    cleanup(image);
}

static void memory_accounting()
{
    DsImage image = {};
    uint32_t total = 0, available = 0;
    CHECK(!ds_image_memory_info(nullptr, &total, &available) &&
          !ds_image_memory_info(&image, nullptr, &available), "memory query rejects missing input/output");
    image.memory_limit = 48u << 20;
    CHECK(ds_image_memory_info(&image, &total, &available) && total == (48u << 20) && available == total,
          "empty configured pool reports its bounded allowance");
    image.allocated_bytes = image.memory_limit;
    CHECK(ds_image_memory_info(&image, &total, &available) && available == 0,
          "fully charged guest budget reports zero available");
    ++image.allocated_bytes;
    CHECK(!ds_image_memory_info(&image, &total, &available), "overcharged accounting rejected without unsigned underflow");
    image.memory_limit = SIZE_MAX;
    image.allocated_bytes = 0;
    CHECK(ds_image_memory_info(&image, &total, &available) && total == 0xFFF00000u && available == total,
          "oversized host allowance is clamped without 32-bit wrap");
    image.memory_limit = 0xFFFFF;
    CHECK(ds_image_memory_info(&image, &total, &available) && total == 0 && available == 0,
          "sub-page allowance never rounds up to promise more memory");
    image = {};
    if (!load(image)) return;
    CHECK(ds_image_memory_info(&image, &total, &available) && total == (48u << 20) && available == (18u << 20),
          "real ELF backing is charged against configured pool");
    char error[256] = {};
    CHECK(ds_image_add_zero_region(&image, 0x40000000, 0x100000, DS_PF_R | DS_PF_W, error, sizeof(error)),
          "explicit extra guest allocation succeeds within budget");
    CHECK(ds_image_memory_info(&image, &total, &available) && available == (17u << 20),
          "new backing reduces available allowance by its actual size");
    cleanup(image);
}

static void memory_startup()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult result = {};
    char error[256] = {};
    bool ok = ds_entry_run_memory(&image, &result, error, sizeof(error));
    CHECK(ok, "real CRT continues after memory query to first clock import");
    if (!ok) std::fprintf(stderr, "Memory CRT error: %s stop=%d last=%08x lr=%llx sp=%llx functions=%u mutexes=%u accesses=%u\n",
        error, result.stop, result.callee, (unsigned long long)result.lr,
        (unsigned long long)result.stack_pointer, result.function_count, result.mutex_count, result.trace_count);
    CHECK(result.stop == DS_ENTRY_TIME_BOUNDARY && result.boundary_nid == 0x8461E528 &&
          result.memory_query_count == 1 && result.syscall_number == 352 && !result.memory_fault,
          "query executed once and stopped before clock implementation");
    CHECK(result.memory_total == (48u << 20) && result.memory_available == (17u << 20) &&
          result.memory_output_address == 0xFEFFE04 && result.memory_backed_at_query == image.allocated_bytes,
          "query reports conservative allowance after ELF, stack, TLS and argv charges");
    uint32_t word = 0;
    CHECK(ds_image_read_be32(&image, result.memory_output_address, &word) && word == result.memory_total &&
          ds_image_read_be32(&image, result.memory_output_address + 4, &word) && word == result.memory_available,
          "both big-endian memory-size output words reach actual guest backing");
    CHECK(result.mutex_count == 10 && result.function_count == 31 && result.toc == DS_REF_TOC &&
          result.stack_pointer == 0xFEFFEF0 && result.thread_pointer == 0xE007000,
          "heap/CRT returns restore main TOC, stack and TLS after ten mutex records");
    bool records_ok = true;
    for (uint32_t i = 0; i < result.mutex_count; ++i) {
        uint32_t protocol = 0;
        uint32_t attr = i == 0 ? 0x176F780u : result.mutex_attribute;
        records_ok &= ds_image_read_be32(&image, attr, &protocol);
        for (unsigned j = 0; j < 24; j += 4)
            records_ok &= ds_image_read_be32(&image, result.mutex_addresses[i] + j, &word) &&
                          word == (j == 8 ? protocol : 0);
    }
    CHECK(records_ok, "all ten distinct guest mutex records have actual protocol and unlocked fields");
    CHECK(ds_image_read_be32(&image, 0x1CE5900, &word) && word == 5 &&
          ds_image_read_be32(&image, 0x1CE5A48, &word) && word == 0x100,
          "real stream initializer sets its count and capacity globals");
    CHECK(ds_image_read_be32(&image, 0x1CE5A58, &word) && word == 0 &&
          ds_image_read_be32(&image, 0x1CE5A5C, &word) && word == 1 &&
          ds_image_read_be32(&image, 0x1CE5A60, &word) && word == 2,
          "real descriptor initializer installs standard descriptor entries");
    bool descriptor_ok = true;
    for (unsigned i = 0; i < 253; ++i) {
        descriptor_ok &= ds_image_read_be32(&image, 0x1CE5A64 + i * 4, &word) && word == UINT32_MAX;
        descriptor_ok &= ds_image_read_be32(&image, 0x1CE5E64 + i * 4, &word) && word == 0;
    }
    CHECK(descriptor_ok, "all 253 remaining descriptor slots and auxiliary slots initialized");
    CHECK(image.allocated_bytes == DS_REF_ALLOCATED_BYTES + arena_bytes + 0xA000,
          "memory query and CRT table writes allocate no additional arenas");
    size_t allocated = image.allocated_bytes;
    CHECK(ds_entry_run_memory(&image, &result, error, sizeof(error)), "extended memory checkpoint repeats after CRT globals changed");
    CHECK(image.allocated_bytes == allocated && result.memory_available == (17u << 20),
          "repeat checkpoint keeps the same accounting and allocation count");
    cleanup(image);

    if (!load(image, 64u << 20)) return;
    CHECK(ds_entry_run_memory(&image, &result, error, sizeof(error)) && result.memory_total == (64u << 20) &&
          result.memory_available == (33u << 20), "guest budget changes propagate to the actual syscall output");
    cleanup(image);

    if (!load(image)) return;
    CHECK(ds_image_add_zero_region(&image, 0x40000000, 4u << 20, DS_PF_R | DS_PF_W, error, sizeof(error)),
          "additional backed arena created before actual query");
    CHECK(ds_entry_run_memory(&image, &result, error, sizeof(error)) && result.memory_available == (13u << 20),
          "actual syscall charges independent extra guest backing");
    cleanup(image);

    if (!load(image)) return;
    CHECK(ds_image_add_zero_region(&image, 0x40000000, 0x1000, DS_PF_R | DS_PF_W, error, sizeof(error)) &&
          ds_image_write_be32(&image, 0x40000000, 18) &&
          ds_image_write_be32(&image, 0x19D860C, 0x40000000),
          "redirect heap threshold to test-only writable value for low-memory branch");
    CHECK(!ds_entry_run_memory(&image, &result, error, sizeof(error)) && result.stop == DS_ENTRY_UNSUPPORTED_CALL &&
          result.syscall_number == 403 && result.memory_query_count == 1 && result.mutex_count == 1,
          "alternate low-memory branch stops at unsupported tty syscall without inventing a return");
    cleanup(image);
}

static void monotonic_clock()
{
    uint64_t value = 123;
    CHECK(ds_clock_ticks_to_us(19200000, 19200000, &value) && value == 1000000,
          "Switch counter frequency converts exactly to microseconds");
    CHECK(ds_clock_ticks_to_us(19199999, 19200000, &value) && value == 999999,
          "fractional microseconds truncate without rounding up");
    CHECK(ds_clock_ticks_to_us(0, 19200000, &value) && value == 0,
          "zero system tick remains a valid zero timestamp");
    CHECK(ds_clock_ticks_to_us(UINT64_MAX, 19200000, &value) && value == 960767920505705813ULL,
          "long uptime conversion avoids overflowing intermediate multiply");
    value = 123;
    CHECK(!ds_clock_ticks_to_us(UINT64_MAX, 1, &value) && value == 123,
          "unsupported signed timestamp range rejected without changing output");
    CHECK(!ds_clock_ticks_to_us(123, 0, &value) && value == 123 &&
          !ds_clock_ticks_to_us(123, 1, nullptr), "invalid frequency or output rejected");
    DsClockSample first = {}, last = {};
    CHECK(ds_clock_now(&first) && !ds_clock_now(nullptr), "native monotonic provider succeeds and rejects null output");
    const timespec delay = {0, 10000000};
    CHECK(nanosleep(&delay, nullptr) == 0, "test waits for real elapsed time");
    CHECK(ds_clock_now(&last) && last.microseconds >= first.microseconds + 5000,
          "native clock advances with elapsed time without call-rate increments");
    CHECK(last.frequency == 1000000000 && last.microseconds == last.ticks / 1000,
          "Linux provider exposes consistent monotonic tick/microsecond units");
}

static void clock_startup()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult result = {};
    char error[256] = {};
    DsClockSample before = {}, after = {};
    CHECK(ds_clock_now(&before), "sample native time before real CRT service");
    bool ok = ds_entry_run_clock(&image, &result, error, sizeof(error));
    CHECK(ok, "real startup reaches first constructor dispatch after clock service");
    if (!ok) std::fprintf(stderr, "Clock CRT error: %s stop=%d last=%08x lr=%llx sp=%llx functions=%u target=%08x\n",
        error, result.stop, result.callee, (unsigned long long)result.lr,
        (unsigned long long)result.stack_pointer, result.function_count, result.constructor_target);
    CHECK(ds_clock_now(&after) && result.clock_microseconds >= before.microseconds &&
          result.clock_microseconds <= after.microseconds, "guest time is an actual monotonic sample during startup");
    CHECK(result.stop == DS_ENTRY_CONSTRUCTOR_BOUNDARY && result.clock_calls == 1 &&
          result.function_count == 35 && !result.memory_fault, "one clock call and 35 entries reach deliberate constructor boundary");
    CHECK(result.constructor_descriptor == 0x1951EA8 && result.constructor_target == 0x7CF848 &&
          result.constructor_toc == DS_REF_TOC && result.toc == DS_REF_TOC,
          "original constructor walker resolves the real first OPD, executable target and TOC");
    uint64_t timestamp = 0;
    CHECK(ds_image_read_be64(&image, 0x1D2B5C8, &timestamp) && timestamp == result.clock_microseconds,
          "original guest instruction stores actual clock return into CRT timestamp global");
    CHECK(result.lr == 0x10228 && result.stack_pointer == 0xFEFFE00 &&
          result.thread_pointer == 0xE007000, "constructor boundary preserves expected LR, stack and TLS ABI");
    uint64_t saved_toc = 0;
    CHECK(ds_image_read_be64(&image, 0xFEFFF18, &saved_toc) && saved_toc == DS_REF_TOC,
          "clock import saves TOC for the actual post-call reload");
    CHECK(image.allocated_bytes == DS_REF_ALLOCATED_BYTES + arena_bytes + 0xA000,
          "clock and constructor enumeration require no additional guest backing");
    const uint64_t previous = result.clock_microseconds;
    CHECK(ds_entry_run_clock(&image, &result, error, sizeof(error)) && result.clock_microseconds >= previous,
          "reload checkpoint keeps native clock epoch rather than resetting time");
    CHECK(ds_entry_run_memory(&image, &result, error, sizeof(error)) && result.clock_calls == 0,
          "old pre-clock checkpoint stays available and does not read the clock");
    cleanup(image);
}

static void clock_faults()
{
    DsImage image = {};
    DsEntryResult result = {};
    char error[256] = {};
    if (!load(image)) return;
    CHECK(ds_image_write_be32(&image, 0x19AD060, 0x10000), "redirect timestamp store to RX test address");
    CHECK(!ds_entry_run_clock(&image, &result, error, sizeof(error)) && result.stop == DS_ENTRY_MEMORY_FAULT &&
          result.fault_address == 0x10000 && result.fault_size == 8 && result.fault_write && result.clock_calls == 1,
          "actual timestamp store respects executable segment's read-only permission");
    cleanup(image);

    if (!load(image)) return;
    CHECK(ds_image_write_be32(&image, 0x1851824, 0xF0000000), "corrupt first constructor descriptor pointer fixture");
    CHECK(!ds_entry_run_clock(&image, &result, error, sizeof(error)) && result.stop == DS_ENTRY_MEMORY_FAULT &&
          result.fault_address == 0xF0000000 && !result.fault_write && result.clock_calls == 1,
          "real constructor walker rejects unmapped OPD without fabricating data");
    cleanup(image);

    if (!load(image)) return;
    CHECK(ds_image_write_be32(&image, 0x1951EAC, 0), "corrupt first constructor TOC fixture");
    CHECK(!ds_entry_run_clock(&image, &result, error, sizeof(error)) && result.stop == DS_ENTRY_ABI_MISMATCH &&
          result.constructor_toc == 0 && result.clock_calls == 1,
          "constructor target with wrong TOC is not accepted or invoked");
    cleanup(image);

    if (!load(image)) return;
    image.segments[0].flags &= ~DS_PF_X;
    CHECK(!ds_entry_run_clock(&image, &result, error, sizeof(error)) && result.stop == DS_ENTRY_ABI_MISMATCH &&
          result.constructor_target == 0x7CF848 && error[0],
          "constructor target requires executable backing as well as matching address");
    cleanup(image);
}

static void clock_provider_failure()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult result = {};
    char error[256] = {};
    CHECK(!ds_entry_run_clock(&image, &result, error, sizeof(error)) && result.stop == DS_ENTRY_CLOCK_FAILURE && error[0],
          "unavailable native provider fails explicitly rather than returning fabricated time");
    CHECK(result.clock_calls == 0 && result.constructor_target == 0 && result.function_count == 31,
          "clock failure stops before timestamp return and constructor enumeration");
    uint64_t value = 123;
    CHECK(ds_image_read_be64(&image, 0x1D2B5C8, &value) && value == 0,
          "failed provider leaves guest timestamp global untouched");
    cleanup(image);
}

static void native_mutex_pool()
{
    CHECK(ds_sync_reset() && ds_sync_live() == 0 && ds_sync_held() == 0, "native mutex pool starts empty");
    CHECK(ds_sync_create(0) && ds_sync_create(DS_CRT_MUTEX_CAPACITY-1) && ds_sync_live() == 2,
          "native mutex records can be created at both pool bounds");
    CHECK(!ds_sync_create(DS_CRT_MUTEX_CAPACITY) && !ds_sync_create(0), "native pool rejects overflow and duplicate creation");
    CHECK(!ds_sync_acquire(1, 1) && !ds_sync_acquire(0, 0), "uninitialized mutex and zero owner rejected");
    CHECK(ds_sync_acquire(0, 1) && ds_sync_held() == 1, "native uncontended lock acquires real mutex");
    CHECK(!ds_sync_acquire(0, 1) && !ds_sync_acquire(0, 2) && ds_sync_held() == 1,
          "recursive and contended acquire refuse without manufacturing success");
    CHECK(!ds_sync_release(0, 2) && ds_sync_held() == 1, "wrong owner cannot release mutex");
    CHECK(ds_sync_release(0, 1) && ds_sync_held() == 0, "correct owner releases native mutex");
    CHECK(!ds_sync_release(0, 1), "unheld mutex cannot be released twice");
    CHECK(ds_sync_acquire(0, 2) && ds_sync_reset() && ds_sync_held() == 0 && ds_sync_live() == 0,
          "pool teardown releases held mutex and destroys every native record");
}

static void constructor_startup()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult result = {};
    char error[256] = {};
    bool ok = ds_entry_run_constructor(&image, &result, error, sizeof(error));
    CHECK(ok, "first constructor executes through destructor registration and native synchronization");
    if (!ok) std::fprintf(stderr, "Constructor error: %s stop=%d last=%08x lr=%llx sp=%llx functions=%u locks=%u unlocks=%u mutexes=%u\n",
        error, result.stop, result.callee, (unsigned long long)result.lr,
        (unsigned long long)result.stack_pointer, result.function_count, result.lock_calls,
        result.unlock_calls, result.mutex_count);
    CHECK(result.stop == DS_ENTRY_SINGLETON_BOUNDARY && result.constructor_started && !result.constructor_completed,
          "constructor prefix reaches explicit singleton boundary without claiming full completion");
    CHECK(result.thread_id_calls == 1 && result.lock_calls == 1 && result.unlock_calls == 1 && result.mutex_count == 11,
          "real thread ID, one native lock/unlock and eleven mutex creates recorded");
    CHECK(result.last_locked_mutex == 0x1CE5940 && result.mutex_addresses[10] == 0x1B92D00,
          "actual CRT lock and constructor object mutex used");
    CHECK(result.native_held_at_stop == 0 && result.native_live_after_cleanup == 0 && ds_sync_live() == 0,
          "no native mutex is left held or alive after checkpoint");
    uint32_t word = 0;
    CHECK(ds_image_read_be32(&image, 0x1AD5630, &word) && word == 79 &&
          ds_image_read_be32(&image, 0x1CE58E8, &word) && word == 0x1967920,
          "real guest atexit code stores destructor callback and decrements free slot count");
    CHECK(result.lr == 0x7CEF28 && result.stack_pointer == 0xFEFFCA0 && result.toc == DS_REF_TOC,
          "constructor prefix retains expected caller LR, stack and TOC at boundary");
    CHECK(image.allocated_bytes == DS_REF_ALLOCATED_BYTES + arena_bytes + 0xA000,
          "constructor prefix consumes no additional guest arenas");
    CHECK(!ds_entry_run_constructor(&image, &result, error, sizeof(error)) && result.stop == DS_ENTRY_BAD_INPUT &&
          result.function_count == 0, "partially initialized image must be reloaded before a new constructor attempt");
    cleanup(image);
    if (!load(image)) return;
    CHECK(ds_entry_run_constructor(&image, &result, error, sizeof(error)), "fresh image reload repeats constructor prefix");
    cleanup(image);
}

static void constructor_sync_fault(bool after_lock)
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult result = {};
    char error[256] = {};
    CHECK(!ds_entry_run_constructor(&image, &result, error, sizeof(error)), "sync fault fixture cannot claim constructor success");
    CHECK(result.constructor_started && !result.constructor_completed && result.thread_id_calls == 1,
          "fault occurs inside real constructor after actual thread-ID service");
    if (after_lock) {
        CHECK(result.stop == DS_ENTRY_MEMORY_FAULT && result.lock_calls == 1 && result.unlock_calls == 0 &&
              result.native_held_at_stop == 1, "injected guest fault occurs with actual native mutex held");
    } else {
        CHECK(result.stop == DS_ENTRY_SYNC_FAILURE && result.lock_calls == 0 && result.native_held_at_stop == 0,
              "unsupported wait timeout does not fake a successful lock");
    }
    CHECK(result.native_live_after_cleanup == 0 && ds_sync_live() == 0 && ds_sync_held() == 0,
          "nonlocal stop cleans every native record including held locks");
    cleanup(image);
}

static void native_rwlock_pool()
{
    CHECK(ds_rwlock_reset() && ds_rwlock_live() == 0, "native reader/writer registry starts empty");
    CHECK(!ds_rwlock_create(nullptr) && !ds_rwlock_contains(0) && !ds_rwlock_contains(9),
          "native reader/writer registry rejects invalid output and handles");
    bool unique = true;
    for (uint32_t i = 1; i <= 8; ++i) {
        uint32_t handle = 0;
        unique &= ds_rwlock_create(&handle) && handle == i && ds_rwlock_contains(handle);
    }
    CHECK(unique && ds_rwlock_live() == 8, "every reader/writer handle maps to a distinct native object");
    uint32_t sentinel = 123;
    CHECK(!ds_rwlock_create(&sentinel) && sentinel == 123 && ds_rwlock_live() == 8,
          "reader/writer pool exhaustion preserves output and active records");
    CHECK(ds_rwlock_reset() && ds_rwlock_live() == 0 && !ds_rwlock_contains(1),
          "reader/writer teardown destroys native objects and invalidates handles");
}

static void singleton_startup()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult r = {};
    char error[256] = {};
    bool ok = ds_entry_run_singleton(&image, &r, error, sizeof(error));
    CHECK(ok, "singleton initializer constructs manager prefix and creates actual reader/writer lock");
    if (!ok) std::fprintf(stderr, "Singleton error: %s stop=%d caller=%08x count=%u rw=%u\n",
                         error, r.stop, r.callee, r.function_count, r.rwlock_create_calls);
    CHECK(r.stop == DS_ENTRY_RWLOCK_INITIALIZED && r.singleton_started && r.function_count == 68 &&
          r.constructor_started && !r.constructor_completed, "new boundary is after lock creation inside incomplete constructor");
    CHECK(r.singleton_address == 0x1B4C6E8 && r.manager_address == 0x1B91128 &&
          r.rwlock_output == 0x1B911AC && r.rwlock_attribute == 0xFEFFA70,
          "actual singleton, manager, native-handle output and attribute locations captured");
    CHECK(r.syscall_number == 120 && r.rwlock_create_calls == 1 && r.r3 == 0 && r.rwlock_handle == 1,
          "sys_rwlock_create writes a registered handle and returns CELL_OK");
    CHECK(r.rwlock_attribute_words[0] == 2 && r.rwlock_attribute_words[1] == 0x200 &&
          r.rwlock_attribute_words[2] == 0 && r.rwlock_attribute_words[3] == 0 && r.rwlock_attribute_words[4] == 0,
          "real 32-byte attributes use priority, private scope and zero IPC key/flags");
    uint32_t word = 0;
    CHECK(ds_image_read_be32(&image, r.rwlock_output, &word) && word == r.rwlock_handle,
          "real guest handle field changes from uninitialized marker to native registry handle");
    CHECK(ds_image_read_be32(&image, 0x1B911A8, &word) && word == 0x1872060 &&
          ds_image_read_be32(&image, 0x1B91194, &word) && word == 0x1B91128,
          "original manager/rwlock constructors install actual vtable and self pointer");
    CHECK(r.lock_calls == 2 && r.unlock_calls == 1 && r.last_locked_mutex == 0x1B92D00 &&
          r.native_held_at_stop == 1, "shared-manager mutex really remains held at the deliberate service stop");
    CHECK(r.native_live_after_cleanup == 0 && r.native_rwlocks_after_cleanup == 0 &&
          ds_sync_held() == 0 && ds_sync_live() == 0 && ds_rwlock_live() == 0,
          "teardown releases held manager mutex and destroys new reader/writer object");
    CHECK(image.allocated_bytes == DS_REF_ALLOCATED_BYTES + arena_bytes + 0xA000,
          "manager prefix and lock creation consume no additional guest arenas");
    CHECK(!ds_entry_run_singleton(&image, &r, error, sizeof(error)) && r.stop == DS_ENTRY_BAD_INPUT,
          "new stage also rejects reuse of partially initialized image");
    cleanup(image);
    if (!load(image)) return;
    CHECK(ds_entry_run_singleton(&image, &r, error, sizeof(error)), "fresh image repeats singleton/native-lock startup");
    cleanup(image);
}

static void singleton_failure(const char *kind)
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult r = {};
    char error[256] = {};
    CHECK(!ds_entry_run_singleton(&image, &r, error, sizeof(error)), "singleton service failure cannot fake successful lock creation");
    CHECK(r.singleton_started && r.rwlock_create_calls == 0 && r.native_held_at_stop == 1,
          "failed rwlock call occurs with actual manager mutex held and no completed creation");
    if (std::strcmp(kind, "output") == 0)
        CHECK(r.stop == DS_ENTRY_MEMORY_FAULT && r.fault_address == 0 && r.fault_write && r.fault_size == 4,
              "rwlock output must be backed and writable before native creation");
    else if (std::strcmp(kind, "attribute") == 0)
        CHECK(r.stop == DS_ENTRY_MEMORY_FAULT && r.fault_address == 0 && !r.fault_write && r.fault_size == 32,
              "full rwlock attribute structure validated before native creation");
    else if (std::strcmp(kind, "shared") == 0)
        CHECK(r.stop == DS_ENTRY_UNSUPPORTED_CALL && r.rwlock_attribute_words[1] == 0x100,
              "unsupported interprocess lock attributes cannot fake private native lock creation");
    else
        CHECK(r.stop == DS_ENTRY_SYNC_FAILURE && error[0], "native rwlock provider failure is explicit");
    CHECK(r.native_live_after_cleanup == 0 && r.native_rwlocks_after_cleanup == 0 &&
          ds_sync_live() == 0 && ds_rwlock_live() == 0 && ds_sync_held() == 0,
          "rwlock service failure cleans held mutex and every native object");
    uint32_t word = 0;
    CHECK(ds_image_read_be32(&image, 0x1B911AC, &word) && word == UINT32_MAX,
          "failed creation preserves original uninitialized handle field");
    cleanup(image);
}

static void singleton_dispatch_faults()
{
    DsImage image = {};
    DsEntryResult r = {};
    char error[256] = {};
    if (!load(image)) return;
    CHECK(ds_image_write_be32(&image, 0x19629E8, 0xA664DC), "test-only manager lock vtable target corruption");
    CHECK(!ds_entry_run_singleton(&image, &r, error, sizeof(error)) && r.stop == DS_ENTRY_UNSUPPORTED_CALL &&
          r.indirect_code == 0xA664DC && r.lock_calls == 1 && r.rwlock_create_calls == 0,
          "unknown manager virtual method cannot enter the checked dispatcher whitelist");
    CHECK(ds_sync_live() == 0 && ds_rwlock_live() == 0, "unknown dispatch releases native objects");
    cleanup(image);
    if (!load(image)) return;
    CHECK(ds_image_write_be32(&image, 0x1964BF4, 0), "test-only rwlock-state method TOC corruption");
    CHECK(!ds_entry_run_singleton(&image, &r, error, sizeof(error)) && r.stop == DS_ENTRY_ABI_MISMATCH &&
          r.native_held_at_stop == 1 && r.rwlock_create_calls == 0,
          "wrong indirect TOC rejected while held manager mutex is tracked");
    CHECK(r.native_live_after_cleanup == 0 && ds_sync_live() == 0 && ds_rwlock_live() == 0,
          "indirect TOC failure cleans held native mutex");
    cleanup(image);
}

static void manager_startup()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult r = {};
    char error[256] = {};
    bool ok = ds_entry_run_manager(&image, &r, error, sizeof(error));
    CHECK(ok, "manager consumes rwlock return, publishes object, releases mutex and requests pool initialization");
    if (!ok) std::fprintf(stderr, "Manager error: %s stop=%d caller=%08x count=%u lr=%llx sp=%llx\n",
        error, r.stop, r.callee, r.function_count, (unsigned long long)r.lr, (unsigned long long)r.stack_pointer);
    CHECK(r.stop == DS_ENTRY_MANAGER_POOL_BOUNDARY && r.function_count == 72 && r.manager_published,
          "manager reaches deliberate boundary before allocator initializer executes");
    uint32_t word = 0;
    CHECK(ds_image_read_be32(&image, 0x1B9111C, &word) && word == 0x1B91128 &&
          ds_image_read_be32(&image, 0x1B911B0, &word) && word == 0,
          "original guest code publishes actual manager and initializes its control word");
    CHECK(r.r3 == 0x1B91128 && r.r4 == 0x1AD6B78 && r.r5 == 0x60000 &&
          r.pool_request_base == r.r4 && r.pool_request_size == r.r5,
          "original configuration getter supplies actual 384 KiB static pool arguments");
    CHECK(ds_image_address(&image, r.pool_request_base, r.pool_request_size, DS_PF_R | DS_PF_W) != nullptr,
          "entire requested pool already has readable and writable ELF backing");
    CHECK(r.lock_calls == 2 && r.unlock_calls == 2 && r.native_held_at_stop == 0 &&
          r.native_live_after_cleanup == 0 && r.native_rwlocks_after_cleanup == 0,
          "original manager unlock balances native acquisition before boundary and teardown");
    CHECK(r.rwlock_create_calls == 1 && ds_image_read_be32(&image, r.rwlock_output, &word) && word == r.rwlock_handle,
          "caller continues after consuming actual successful rwlock creation");
    CHECK(!r.constructor_completed && r.singleton_started && r.lr == 0xA3585C && r.stack_pointer == 0xFEFFAB0,
          "pool boundary reports expected ABI and incomplete outer initialization");
    CHECK(image.allocated_bytes == DS_REF_ALLOCATED_BYTES + arena_bytes + 0xA000,
          "manager continuation allocates no extra guest backing");
    cleanup(image);
    if (!load(image)) return;
    CHECK(ds_entry_run_manager(&image, &r, error, sizeof(error)), "fresh reload repeats manager publication and balanced synchronization");
    cleanup(image);
}

static void manager_faults()
{
    DsImage image = {};
    DsEntryResult r = {};
    char error[256] = {};
    if (!load(image)) return;
    CHECK(ds_image_write_be32(&image, 0x19629DC, 0), "test-only manager-unlock descriptor TOC corruption");
    CHECK(!ds_entry_run_manager(&image, &r, error, sizeof(error)) && r.stop == DS_ENTRY_ABI_MISMATCH &&
          r.rwlock_create_calls == 1 && r.native_held_at_stop == 1,
          "invalid unlock dispatch fails before losing native ownership tracking");
    CHECK(ds_sync_live() == 0 && ds_rwlock_live() == 0 && ds_sync_held() == 0,
          "unlock dispatch failure releases held manager mutex and rwlock object");
    cleanup(image);
    if (!load(image)) return;
    CHECK(ds_image_write_be32(&image, 0x18C9218, 0x10000), "test-only static pool pointer redirected into RX image");
    CHECK(!ds_entry_run_manager(&image, &r, error, sizeof(error)) && r.stop == DS_ENTRY_MEMORY_FAULT &&
          r.fault_address == 0x10000 && r.fault_size == 0x60000 && r.fault_write,
          "full pool handoff rejects RX memory without fabricating writable backing");
    CHECK(r.native_held_at_stop == 0 && ds_sync_live() == 0 && ds_rwlock_live() == 0,
          "invalid pool handoff cleans all native state after original manager unlock");
    cleanup(image);
}

static void completed_singleton()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult r = {};
    char error[256] = {};
    bool ok = ds_entry_run_startup(&image, &r, error, sizeof(error));
    CHECK(ok, "broader startup completes pool initialization, actual allocation and singleton construction");
    if (!ok) std::fprintf(stderr, "Startup error: %s stop=%d callee=%08x count=%u\n", error,r.stop,r.callee,r.function_count);
    CHECK(r.stop == DS_ENTRY_SINGLETON_READY && r.function_count == 95 && r.singleton_completed && !r.constructor_completed,
          "checkpoint distinguishes completed singleton from pending outer constructor");
    CHECK(r.pool_initialized && r.pool_request_base == 0x1AD6B78 && r.pool_request_size == 0x60000 &&
          r.allocation_calls == 1 && r.allocation_address == 0x1AD6C00 && r.allocation_size == 0x180,
          "real static pool serves first aligned 384-byte request");
    CHECK(r.atomic_attempts == 1 && r.atomic_successes == 1 && r.bootstrap_fallback_calls == 0,
          "actual guest CAS succeeds without NULL-pool bootstrap fallback");
    uint32_t word = 0;
    static constexpr uint32_t state[][2] = {
        {0x1AD6BF0,0x1B91128},{0x1AD6BF4,0x1AD6D80},{0x1AD6BF8,0},
        {0x1AD6D80,0x1AD6BF0},{0x1AD6D84,0x1B36B68},{0x1B36B68,0x1AD6D80},
        {0x1AD6BE8,0x1AD6D88},{0x1AD6BEC,0x1AD6D88},
        {0x1B91160,0},{0x1B91164,0x5FDE8},{0x1B911B0,0x1000},
        {0x1B9119C,0x1AD6C00},{0x1B911A0,0x1AD6C0C},{0x1B911A4,0x1AD6D80},
        {0x1AD6C00,0x1B91190},{0x1AD6C04,0x1AD6B78},{0x1AD6C08,0x1B36B78},
        {0x1B4C6E0,0x1B4C6E8},{0x1B4C6E8,0x186A628},{0x1B4C6EC,0},{0x1B4C6F0,0},
        {0x1B4C6F4,0x1B91190},{0x1B4C6F8,0},{0x1B4C6FC,0},{0x1B4C700,0},
        {0x1B4C704,0x16F8410},{0x1B4C708,0x16F8320},{0xE000010,0xD1}
    };
    bool state_ok = true;
    for (const auto &entry : state)
        state_ok &= ds_image_read_be32(&image, entry[0], &word) && word == entry[1];
    CHECK(state_ok, "independent chunk split, free-list, range tuple, TLS and singleton memory invariants match");
    CHECK(ds_image_address(&image, r.allocation_address, r.allocation_size, DS_PF_R | DS_PF_W) &&
          (r.allocation_address & 15) == 0, "actual allocated payload has bounded aligned writable backing");
    CHECK(r.lock_calls == 3 && r.unlock_calls == 3 && r.rwlock_write_locks == 1 && r.rwlock_write_unlocks == 1,
          "native mutex and reader/writer ownership balanced through registration");
    CHECK(r.native_held_at_stop == 0 && r.native_rwlocks_held_at_stop == 0 && r.native_live_after_cleanup == 0 &&
          r.native_rwlocks_after_cleanup == 0 && ds_sync_live() == 0 && ds_rwlock_live() == 0,
          "completed singleton checkpoint leaves no native objects or held locks");
    CHECK(r.lr == 0x7CF75C && r.stack_pointer == 0xFEFFD40 && r.r3 == 0x1B4C6E8 && r.toc == DS_REF_TOC,
          "next initializer receives real singleton with expected ABI");
    CHECK(image.allocated_bytes == DS_REF_ALLOCATED_BYTES + arena_bytes + 0xA000,
          "first allocation is served from existing pool without extra guest arenas");
    CHECK(!ds_entry_run_startup(&image, &r, error, sizeof(error)) && r.stop == DS_ENTRY_BAD_INPUT,
          "completed singleton stage also requires fresh image for restart");
    cleanup(image);
    if (!load(image)) return;
    CHECK(ds_entry_run_startup(&image, &r, error, sizeof(error)), "fresh image reproduces full singleton milestone");
    cleanup(image);
}

static void startup_fault(const char *kind)
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult r = {};
    char error[256] = {};
    CHECK(!ds_entry_run_startup(&image,&r,error,sizeof(error)) && !r.singleton_completed,
          "failed broader startup cannot claim singleton completion");
    if (std::strcmp(kind,"writer") == 0)
        CHECK(r.stop == DS_ENTRY_MEMORY_FAULT && r.rwlock_write_locks == 1 && r.rwlock_write_unlocks == 0 &&
              r.native_rwlocks_held_at_stop == 1, "guest fault after native writer acquisition tracks held writer");
    else if (std::strcmp(kind,"wait") == 0)
        CHECK(r.stop == DS_ENTRY_SYNC_FAILURE && r.rwlock_write_locks == 0 && r.native_rwlocks_held_at_stop == 0,
              "unsupported writer wait refuses without fabricated acquisition");
    else if (std::strcmp(kind,"fallback") == 0)
        CHECK(r.stop == DS_ENTRY_UNSUPPORTED_CALL && r.bootstrap_fallback_calls == 1 && r.native_held_at_stop == 1,
              "preexisting bootstrap shim remains an explicit failure boundary");
    else
        CHECK(r.stop == DS_ENTRY_ABI_MISMATCH && r.atomic_successes == 0 && r.allocation_calls == 0,
              "invalid atomic provider cannot produce fake allocation progress");
    CHECK(ds_sync_held() == 0 && ds_rwlock_held() == 0 && ds_sync_live() == 0 && ds_rwlock_live() == 0 &&
          r.native_live_after_cleanup == 0 && r.native_rwlocks_after_cleanup == 0,
          "all failure paths release native writers, mutexes and registry objects");
    cleanup(image);
}

static void startup_dispatch_fault()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult r = {};
    char error[256] = {};
    CHECK(ds_image_write_be32(&image, 0x196854C, 0), "test-only range-method descriptor TOC corruption");
    CHECK(!ds_entry_run_startup(&image, &r, error, sizeof(error)) && r.stop == DS_ENTRY_ABI_MISMATCH &&
          r.indirect_code == 0xB25E58 && !r.singleton_completed && r.rwlock_write_unlocks == 1,
          "range method rejects corrupt TOC after balanced native range-registration locks");
    CHECK(ds_sync_live() == 0 && ds_rwlock_live() == 0 && ds_rwlock_held() == 0,
          "range dispatch rejection cleans native registry objects");
    cleanup(image);
}

static void check_registry_boundary(const DsImage &image, const DsEntryResult &r)
{
    CHECK(r.stop == DS_ENTRY_REGISTRY_READY && r.registry_initialized && r.singleton_completed &&
          !r.constructor_completed && !r.memory_fault,
          "real startup inserts the first name-registry record before the next registry initializer");
    CHECK(r.lr == 0xAE50E0 && r.stack_pointer == 0xFEFFC30 && r.toc == DS_REF_TOC,
          "name-registry checkpoint has the original next-call link register, stack and TOC");
    CHECK(r.registry_address == 0x1B92A78 && r.registry_payload == 0x1AD6D90 &&
          r.registry_entries == 1 && r.registry_allocation_address == r.registry_payload &&
          r.registry_allocation_size == 0x2000,
          "registry reports the real static container, one record and its 8 KiB pool allocation");
    CHECK(r.pool_initialized && r.allocation_calls == 2 && r.allocation_address == 0x1AD6C00 &&
          r.allocation_size == 0x180 && r.bootstrap_fallback_calls == 0 &&
          r.atomic_attempts == 1 && r.atomic_successes == 1,
          "second allocation preserves first-request reporting and uses only the established guest pool");
    uint32_t word = 0;
    static constexpr uint32_t state[][2] = {
        {0x1B92A70,0x1B92A75},{0x1B92A78,0x1B91190},
        {0x1B92A7C,0x1AD6D90},{0x1B92A80,0x1AD6DA0},{0x1B92A84,0x1AD8D90},
        {0x1AD6D90,0x1B4C6E8},{0x1AD6D94,0x16F8410},{0x1AD6D98,0x16F8320},{0x1AD6D9C,17},
        // AC8078 removes the old node, splits a 0x2010-byte chunk, then relinks bin 14.
        {0x1AD6BF0,0x1B91128},{0x1AD6BF4,0x1AD6D80},{0x1AD6BF8,0},
        {0x1AD6D80,0x1AD6BF0},{0x1AD6D84,0x1AD8D90},{0x1AD6D88,0},
        {0x1AD8D90,0x1AD6D80},{0x1AD8D94,0x1B36B68},{0x1B36B68,0x1AD8D90},
        {0x1AD6BE8,0x1AD8D98},{0x1AD6BEC,0x1AD8D98},
        {0x1AD8D98,0x1AD6BE8},{0x1AD8D9C,0x1AD6BE8},
        {0x1B9114C,14},{0x1B91160,0},{0x1B91164,0x5DDD8},
        // The original manager range registration and singleton stay intact.
        {0x1B9119C,0x1AD6C00},{0x1B911A0,0x1AD6C0C},{0x1B911A4,0x1AD6D80},
        {0x1AD6C00,0x1B91190},{0x1AD6C04,0x1AD6B78},{0x1AD6C08,0x1B36B78},
        {0x1B4C6E0,0x1B4C6E8},{0x1B4C6E8,0x186A628}
    };
    bool state_ok = true;
    for (const auto &entry : state)
        state_ok &= ds_image_read_be32(&image, entry[0], &word) && word == entry[1];
    CHECK(state_ok, "independent second chunk split, free-list, accounting and registry record invariants match");
    const auto *ready = static_cast<const unsigned char *>(
        ds_image_address(&image, 0x1B92A88, 1, DS_PF_R));
    const auto *registered = static_cast<const unsigned char *>(
        ds_image_address(&image, 0x1B925B0, 1, DS_PF_R));
    CHECK(ready && *ready == 1 && registered && *registered == 1,
          "original lazy static initialization publishes both registry flags");
    static const char name[] = "DsMenuDlgBloodMsg";
    const void *actual_name = ds_image_address(&image, 0x16F8410, sizeof(name), DS_PF_R);
    const auto *empty = static_cast<const unsigned char *>(
        ds_image_address(&image, 0x16F8320, 1, DS_PF_R));
    CHECK(actual_name && std::memcmp(actual_name, name, sizeof(name)) == 0 && empty && *empty == 0,
          "registered name and empty string point to original image bytes");
    CHECK(ds_image_address(&image, r.registry_payload, 0x2000, DS_PF_R | DS_PF_W) &&
          (r.registry_payload & 15) == 0,
          "entire registry allocation has aligned readable and writable guest backing");
    CHECK(r.lock_calls == 4 && r.unlock_calls == 4 &&
          r.rwlock_write_locks == 1 && r.rwlock_write_unlocks == 1,
          "interface allocation adds exactly one balanced native mutex pair");
    CHECK(r.native_held_at_stop == 0 && r.native_rwlocks_held_at_stop == 0 &&
          r.native_live_after_cleanup == 0 && r.native_rwlocks_after_cleanup == 0 &&
          ds_sync_held() == 0 && ds_sync_live() == 0 && ds_rwlock_held() == 0 && ds_rwlock_live() == 0,
          "registry checkpoint cleans all native lock ownership and objects");
    CHECK(image.allocated_bytes == DS_REF_ALLOCATED_BYTES + arena_bytes + 0xA000,
          "registry record uses existing pool backing without adding guest arenas");
}

static void initialized_registry()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult r = {};
    char error[256] = {};
    bool ok = ds_entry_run_registry(&image, &r, error, sizeof(error));
    CHECK(ok, "broader startup reaches a real first registry insertion");
    if (!ok) std::fprintf(stderr, "Registry error: %s stop=%d callee=%08x indirect=%08x descriptor=%08x count=%u lr=%llx sp=%llx\n",
        error, r.stop, r.callee, r.indirect_code, r.indirect_descriptor, r.function_count,
        (unsigned long long)r.lr, (unsigned long long)r.stack_pointer);
    if (ok) check_registry_boundary(image, r);
    const size_t allocated = image.allocated_bytes;
    DsEntryResult reused = {};
    CHECK(!ds_entry_run_registry(&image, &reused, error, sizeof(error)) &&
          reused.stop == DS_ENTRY_BAD_INPUT && !reused.registry_initialized,
          "registry stage requires a fresh image before replaying constructors");
    uint32_t word = 0;
    CHECK(image.allocated_bytes == allocated &&
          ds_image_read_be32(&image, 0x1B92A80, &word) && word == 0x1AD6DA0 &&
          ds_image_read_be32(&image, 0x1AD6D9C, &word) && word == 17 &&
          ds_sync_live() == 0 && ds_rwlock_live() == 0,
          "rejected registry reuse preserves inserted data and bounded native/guest resources");
    cleanup(image);
    if (!load(image)) return;
    ok = ds_entry_run_registry(&image, &r, error, sizeof(error));
    CHECK(ok, "fresh reload reproduces registry initialization and second allocation");
    if (ok) check_registry_boundary(image, r);
    cleanup(image);
}

static void registry_dispatch_faults()
{
    static constexpr struct {
        uint32_t address, replacement, target;
        DsEntryStop stop;
    } fixtures[] = {
        {0x1968620,0xB27654,0xB27654,DS_ENTRY_UNSUPPORTED_CALL},
        {0x1968624,0,0xB27658,DS_ENTRY_ABI_MISMATCH},
        {0x1951D80,0x7CEC3C,0x7CEC3C,DS_ENTRY_UNSUPPORTED_CALL},
        {0x1951D84,0,0x7CEC40,DS_ENTRY_ABI_MISMATCH}
    };
    for (const auto &fixture : fixtures) {
        DsImage image = {};
        if (!load(image)) return;
        DsEntryResult r = {};
        char error[256] = {};
        CHECK(ds_image_write_be32(&image, fixture.address, fixture.replacement),
              "test-only registry descriptor target or TOC corruption installed");
        CHECK(!ds_entry_run_registry(&image, &r, error, sizeof(error)) && r.stop == fixture.stop &&
              r.indirect_code == fixture.target && r.singleton_completed && !r.registry_initialized &&
              r.allocation_calls == 1 && r.registry_allocation_address == 0 && error[0],
              "registry descriptors reject unknown targets and wrong TOCs before a second allocation");
        CHECK(r.native_held_at_stop == 0 && r.native_rwlocks_held_at_stop == 0 &&
              r.native_live_after_cleanup == 0 && r.native_rwlocks_after_cleanup == 0 &&
              ds_sync_held() == 0 && ds_sync_live() == 0 && ds_rwlock_held() == 0 && ds_rwlock_live() == 0,
              "rejected registry dispatch leaves no held or live native objects");
        cleanup(image);
    }
}

static void registry_probe()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult r = {};
    char error[256] = {};
    bool ok = ds_entry_run_registry(&image, &r, error, sizeof(error));
    std::printf("REGISTRY probe ok=%d stop=%d callee=%08x indirect=%08x descriptor=%08x lr=%llx sp=%llx r3=%llx toc=%llx functions=%u accesses=%u error=%s\n",
        ok, r.stop, r.callee, r.indirect_code, r.indirect_descriptor,
        (unsigned long long)r.lr, (unsigned long long)r.stack_pointer, (unsigned long long)r.r3,
        (unsigned long long)r.toc, r.function_count, r.trace_count, error);
    std::printf("registry=%d address=%08x payload=%08x entries=%u allocation=%08x/%x calls=%u first=%08x/%x singleton=%d pool=%d locks=%u/%u rw=%u/%u held=%u/%u cleanup=%u/%u\n",
        r.registry_initialized, r.registry_address, r.registry_payload, r.registry_entries,
        r.registry_allocation_address, r.registry_allocation_size, r.allocation_calls,
        r.allocation_address, r.allocation_size, r.singleton_completed, r.pool_initialized,
        r.lock_calls, r.unlock_calls, r.rwlock_write_locks, r.rwlock_write_unlocks,
        r.native_held_at_stop, r.native_rwlocks_held_at_stop, r.native_live_after_cleanup,
        r.native_rwlocks_after_cleanup);
    static constexpr uint32_t addresses[] = {
        0x1B92A70,0x1B92A78,0x1B92A7C,0x1B92A80,0x1B92A84,
        0x1AD6D90,0x1AD6D94,0x1AD6D98,0x1AD6D9C,
        0x1AD6D80,0x1AD6D84,0x1AD6D88,0x1AD8D90,0x1AD8D94,
        0x1AD8D98,0x1AD8D9C,0x1AD6BE8,0x1AD6BEC,0x1B36B68,0x1B9114C,0x1B91164
    };
    for (uint32_t address : addresses) {
        uint32_t word = 0;
        bool readable = ds_image_read_be32(&image, address, &word);
        std::printf("  %08x=%08x readable=%d\n", address, word, readable);
    }
    for (unsigned i = 85; i < r.function_count && i < DS_ENTRY_FUNCTION_CAPACITY; ++i)
        std::printf("%u: %08x\n", i, r.functions[i]);
    CHECK(ok, "registry diagnostic probe reaches its checkpoint");
    cleanup(image);
}

static void registry_lock_fault()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult r = {};
    char error[256] = {};
    CHECK(!ds_entry_run_registry(&image, &r, error, sizeof(error)) &&
          r.stop == DS_ENTRY_MEMORY_FAULT && r.memory_fault && r.fault_write &&
          r.fault_address == 0 && r.fault_size == 4 &&
          r.singleton_completed && !r.registry_initialized && error[0],
          "guest write fault inside interface allocation cannot claim registry initialization");
    CHECK(r.lock_calls == 4 && r.unlock_calls == 3 && r.native_held_at_stop == 1 &&
          r.allocation_calls == 2 && r.registry_allocation_address == 0,
          "fault after interface mutex acquisition preserves held ownership before allocation returns");
    CHECK(r.native_live_after_cleanup == 0 && r.native_rwlocks_after_cleanup == 0 &&
          ds_sync_held() == 0 && ds_sync_live() == 0 && ds_rwlock_held() == 0 && ds_rwlock_live() == 0,
          "interface allocation fault releases held native mutex and all native objects");
    cleanup(image);
}

static void check_registration_boundary(const DsImage &image, const DsEntryResult &r)
{
    CHECK(r.stop == DS_ENTRY_REGISTRATION_READY && r.function_count == 137 &&
          r.registry_initialized && r.callbacks_initialized && r.registration_completed &&
          r.singleton_completed && !r.constructor_completed && !r.memory_fault,
          "both registries complete and AE4D38 returns before the next singleton initializer");
    CHECK(r.lr == 0x7CF770 && r.stack_pointer == 0xFEFFD40 && r.r3 == 0x1B4C6E8 && r.toc == DS_REF_TOC,
          "completed registration restores the caller stack and real next-initializer ABI");
    CHECK(r.callback_registry_address == 0x1B92A5C && r.callback_payload == 0x1AD8DA0 &&
          r.callback_entries == 4 && r.callback_allocation_address == r.callback_payload &&
          r.callback_allocation_size == 0x1000,
          "opaque-key registry reports four pairs in its real 4 KiB pool allocation");
    CHECK(r.registry_address == 0x1B92A78 && r.registry_payload == 0x1AD6D90 &&
          r.registry_entries == 1 && r.registry_allocation_address == r.registry_payload &&
          r.registry_allocation_size == 0x2000,
          "name-registry diagnostics survive the following registration stage");
    CHECK(r.allocation_calls == 3 && r.allocation_address == 0x1AD6C00 && r.allocation_size == 0x180 &&
          r.pool_initialized && r.atomic_attempts == 1 && r.atomic_successes == 1 &&
          r.bootstrap_fallback_calls == 0,
          "third allocation preserves earlier requests without new atomics or bootstrap fallback");
    uint32_t word = 0;
    static constexpr uint32_t state[][2] = {
        {0x1B92A54,0x1B92A59},{0x1B92A5C,0x1B91190},
        {0x1B92A60,0x1AD8DA0},{0x1B92A64,0x1AD8DC0},{0x1B92A68,0x1AD9DA0},
        {0x1AD8DA0,0x1B4C70C},{0x1AD8DA4,0x1B4C6E8},
        {0x1AD8DA8,0x1B4C70D},{0x1AD8DAC,0x1B4C6E8},
        {0x1AD8DB0,0x1B4C70E},{0x1AD8DB4,0x1B4C6E8},
        {0x1AD8DB8,0x1B4C70F},{0x1AD8DBC,0x1B4C6E8},
        // AC8078 splits 0x1010 bytes from the prior free chunk and relinks bin 14.
        {0x1AD6BF0,0x1B91128},{0x1AD6BF4,0x1AD6D80},{0x1AD6BF8,0},
        {0x1AD6D80,0x1AD6BF0},{0x1AD6D84,0x1AD8D90},{0x1AD6D88,0},
        {0x1AD8D90,0x1AD6D80},{0x1AD8D94,0x1AD9DA0},{0x1AD8D98,0},
        {0x1AD9DA0,0x1AD8D90},{0x1AD9DA4,0x1B36B68},{0x1B36B68,0x1AD9DA0},
        {0x1AD6BE8,0x1AD9DA8},{0x1AD6BEC,0x1AD9DA8},
        {0x1AD9DA8,0x1AD6BE8},{0x1AD9DAC,0x1AD6BE8},
        {0x1B9114C,14},{0x1B91160,0},{0x1B91164,0x5CDC8},
        // Name record, its backing and manager range remain unchanged.
        {0x1B92A70,0x1B92A75},{0x1B92A78,0x1B91190},
        {0x1B92A7C,0x1AD6D90},{0x1B92A80,0x1AD6DA0},{0x1B92A84,0x1AD8D90},
        {0x1AD6D90,0x1B4C6E8},{0x1AD6D94,0x16F8410},{0x1AD6D98,0x16F8320},{0x1AD6D9C,17},
        {0x1B9119C,0x1AD6C00},{0x1B911A0,0x1AD6C0C},{0x1B911A4,0x1AD6D80},
        {0x1AD6C00,0x1B91190},{0x1AD6C04,0x1AD6B78},{0x1AD6C08,0x1B36B78},
        {0x1B4C6E0,0x1B4C6E8},{0x1B4C6E8,0x186A628}
    };
    bool state_ok = true;
    for (const auto &entry : state)
        state_ok &= ds_image_read_be32(&image, entry[0], &word) && word == entry[1];
    CHECK(state_ok, "independent third split, free-list, accounting and ascending opaque-key pairs match");
    bool flags_ok = true;
    static constexpr uint32_t flag_addresses[] = {0x1B92A6C,0x1B925B8,0x1B92A88,0x1B925B0};
    for (uint32_t address : flag_addresses) {
        const auto *flag = static_cast<const unsigned char *>(ds_image_address(&image, address, 1, DS_PF_R));
        flags_ok &= flag && *flag == 1;
    }
    CHECK(flags_ok, "both lazy registries retain their publication flags");
    CHECK(ds_image_address(&image, r.callback_payload, 0x1000, DS_PF_R | DS_PF_W) &&
          (r.callback_payload & 15) == 0,
          "entire opaque-key registry allocation is aligned and backed by the existing guest pool");
    CHECK(r.lock_calls == 5 && r.unlock_calls == 5 &&
          r.rwlock_write_locks == 1 && r.rwlock_write_unlocks == 1 && r.rwlock_create_calls == 1,
          "registration adds one balanced native mutex pair without new writer operations");
    CHECK(r.native_held_at_stop == 0 && r.native_rwlocks_held_at_stop == 0 &&
          r.native_live_after_cleanup == 0 && r.native_rwlocks_after_cleanup == 0 &&
          ds_sync_held() == 0 && ds_sync_live() == 0 && ds_rwlock_held() == 0 && ds_rwlock_live() == 0,
          "completed registration cleans all native lock ownership and objects");
    CHECK(image.allocated_bytes == DS_REF_ALLOCATED_BYTES + arena_bytes + 0xA000,
          "third allocation adds no guest backing arena");
}

static void completed_registration()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult r = {};
    char error[256] = {};
    bool ok = ds_entry_run_registration(&image, &r, error, sizeof(error));
    CHECK(ok, "broader startup completes both registry insertions and returns from AE4D38");
    if (!ok) std::fprintf(stderr, "Registration error: %s stop=%d callee=%08x indirect=%08x descriptor=%08x count=%u lr=%llx sp=%llx\n",
        error, r.stop, r.callee, r.indirect_code, r.indirect_descriptor, r.function_count,
        (unsigned long long)r.lr, (unsigned long long)r.stack_pointer);
    if (ok) check_registration_boundary(image, r);
    const size_t allocated = image.allocated_bytes;
    DsEntryResult reused = {};
    CHECK(!ds_entry_run_registration(&image, &reused, error, sizeof(error)) &&
          reused.stop == DS_ENTRY_BAD_INPUT && !reused.callbacks_initialized && !reused.registration_completed,
          "completed registration requires a fresh image before replaying constructors");
    uint32_t word = 0;
    CHECK(image.allocated_bytes == allocated &&
          ds_image_read_be32(&image, 0x1B92A64, &word) && word == 0x1AD8DC0 &&
          ds_image_read_be32(&image, 0x1AD8DB8, &word) && word == 0x1B4C70F &&
          ds_sync_live() == 0 && ds_rwlock_live() == 0,
          "rejected registration replay preserves sorted pairs and bounded resources");
    cleanup(image);
    if (!load(image)) return;
    ok = ds_entry_run_registration(&image, &r, error, sizeof(error));
    CHECK(ok, "fresh reload reproduces complete sorted registration");
    if (ok) check_registration_boundary(image, r);
    cleanup(image);
}

static void registration_dispatch_faults()
{
    static constexpr struct {
        uint32_t address, replacement, target;
        DsEntryStop stop;
    } fixtures[] = {
        {0x1951DC8,0x7CEDFC,0x7CEDFC,DS_ENTRY_UNSUPPORTED_CALL},
        {0x1951DCC,0,0x7CEE00,DS_ENTRY_ABI_MISMATCH}
    };
    for (const auto &fixture : fixtures) {
        DsImage image = {};
        if (!load(image)) return;
        DsEntryResult r = {};
        char error[256] = {};
        CHECK(ds_image_write_be32(&image, fixture.address, fixture.replacement),
              "test-only unique opaque-key getter descriptor target or TOC corruption installed");
        CHECK(!ds_entry_run_registration(&image, &r, error, sizeof(error)) && r.stop == fixture.stop &&
              r.indirect_code == fixture.target && r.registry_initialized && r.singleton_completed &&
              !r.callbacks_initialized && !r.registration_completed && r.allocation_calls == 3 &&
              r.callback_allocation_address == 0x1AD8DA0 && error[0],
              "new getter dispatch rejects invalid code and TOC after both registry reserves");
        uint32_t word = 0;
        CHECK(ds_image_read_be32(&image, 0x1B92A54, &word) && word == 0x1B92A59 &&
              ds_image_read_be32(&image, 0x1B92A64, &word) && word == 0x1AD8DA0 &&
              ds_image_read_be32(&image, 0x1AD6D9C, &word) && word == 17,
              "new-path corruption preserves initialized callback container and earlier name record");
        CHECK(r.lock_calls == 5 && r.unlock_calls == 5 && r.native_held_at_stop == 0 &&
              r.native_rwlocks_held_at_stop == 0 && r.native_live_after_cleanup == 0 &&
              r.native_rwlocks_after_cleanup == 0 && ds_sync_held() == 0 && ds_sync_live() == 0 &&
              ds_rwlock_held() == 0 && ds_rwlock_live() == 0,
              "new-path descriptor rejection cleans all native locks and objects");
        cleanup(image);
    }
}

static void registration_probe()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult r = {};
    char error[256] = {};
    bool ok = ds_entry_run_registration(&image, &r, error, sizeof(error));
    std::printf("REGISTRATION probe ok=%d stop=%d callee=%08x indirect=%08x descriptor=%08x lr=%llx sp=%llx r3=%llx toc=%llx functions=%u accesses=%u error=%s\n",
        ok, r.stop, r.callee, r.indirect_code, r.indirect_descriptor,
        (unsigned long long)r.lr, (unsigned long long)r.stack_pointer, (unsigned long long)r.r3,
        (unsigned long long)r.toc, r.function_count, r.trace_count, error);
    std::printf("callbacks=%d returned=%d address=%08x payload=%08x entries=%u allocation=%08x/%x calls=%u name=%d/%08x/%u singleton=%d constructor=%d locks=%u/%u rw=%u/%u held=%u/%u cleanup=%u/%u\n",
        r.callbacks_initialized, r.registration_completed, r.callback_registry_address, r.callback_payload,
        r.callback_entries, r.callback_allocation_address, r.callback_allocation_size, r.allocation_calls,
        r.registry_initialized, r.registry_payload, r.registry_entries, r.singleton_completed,
        r.constructor_completed, r.lock_calls, r.unlock_calls, r.rwlock_write_locks, r.rwlock_write_unlocks,
        r.native_held_at_stop, r.native_rwlocks_held_at_stop, r.native_live_after_cleanup,
        r.native_rwlocks_after_cleanup);
    static constexpr uint32_t addresses[] = {
        0x1B92A54,0x1B92A5C,0x1B92A60,0x1B92A64,0x1B92A68,
        0x1AD8DA0,0x1AD8DA4,0x1AD8DA8,0x1AD8DAC,0x1AD8DB0,0x1AD8DB4,0x1AD8DB8,0x1AD8DBC,
        0x1AD8D90,0x1AD8D94,0x1AD8D98,0x1AD9DA0,0x1AD9DA4,0x1AD9DA8,0x1AD9DAC,
        0x1AD6BE8,0x1AD6BEC,0x1B36B68,0x1B9114C,0x1B91164
    };
    for (uint32_t address : addresses) {
        uint32_t word = 0;
        bool readable = ds_image_read_be32(&image, address, &word);
        std::printf("  %08x=%08x readable=%d\n", address, word, readable);
    }
    for (unsigned i = 110; i < r.function_count && i < DS_ENTRY_FUNCTION_CAPACITY; ++i)
        std::printf("%u: %08x\n", i, r.functions[i]);
    CHECK(ok, "registration diagnostic probe reaches its checkpoint");
    cleanup(image);
}


static void kernel_mutex_pool()
{
    CHECK(ds_kernel_mutex_reset() && ds_kernel_mutex_live() == 0, "kernel mutex pool starts empty");
    CHECK(!ds_kernel_mutex_create(nullptr) && !ds_kernel_mutex_contains(0) && !ds_kernel_mutex_contains(1),
          "kernel mutex handles are private to their validated registry");
    bool distinct = true;
    for (uint32_t i=0; i<8; ++i) {
        uint32_t h=0;
        distinct &= ds_kernel_mutex_create(&h) && h == 0x1001+i && ds_kernel_mutex_contains(h);
    }
    CHECK(distinct && ds_kernel_mutex_live()==8, "eight real recursive native objects receive distinct handles");
    uint32_t sentinel=0xA5A55A5A;
    CHECK(!ds_kernel_mutex_create(&sentinel) && sentinel==0xA5A55A5A && ds_kernel_mutex_live()==8,
          "exhausted kernel pool leaves caller output unchanged");
    CHECK(ds_kernel_mutex_reset() && ds_kernel_mutex_live()==0 && !ds_kernel_mutex_contains(0x1001),
          "kernel object teardown destroys the native registry");
    CHECK(ds_kernel_mutex_create(&sentinel) && sentinel==0x1001 && ds_kernel_mutex_reset(),
          "a fresh process can recreate its native recursive mutex");
}

static void completed_first_constructor()
{
    for (unsigned attempt=0; attempt<2; ++attempt) {
        DsImage image={}; if(!load(image)) return;
        DsEntryResult r={}; char error[256]={};
        bool ok=ds_entry_run_first_constructor(&image,&r,error,sizeof(error));
        CHECK(ok, "real first constructor returns to its original CRT walker");
        if(!ok) std::fprintf(stderr,"First constructor error: %s stop=%d count=%u\n",error,r.stop,r.function_count);
        CHECK(r.stop==DS_ENTRY_FIRST_CONSTRUCTOR_READY && r.constructor_completed && r.function_count==176 &&
              r.next_constructor_descriptor==0x1947390 && r.next_constructor_target==0x66D688 &&
              r.constructor_descriptor==0x1951EA8 && r.constructor_target==0x7CF848,
              "first constructor completion and next constructor descriptor remain distinct");
        CHECK(r.lr==0x16B84F4 && r.stack_pointer==0xFEFFE00 && r.toc==DS_REF_TOC,
              "first constructor restores original CRT walker ABI");
        CHECK(r.registration_completed && r.callbacks_initialized && r.registry_entries==1 && r.callback_entries==4 &&
              r.singleton_address==0x1B4C6E8 && r.second_singleton_address==0x1B4BB0C &&
              r.node_address==0x1AD9DB0 && r.node_vector_address==0x1AD9E00 && r.allocation_calls==5,
              "full constructor preserves first singleton and publishes its real parent and node");
        static constexpr uint32_t words[][2]={
            {0x1B4C6EC,0x1B4BB0C},{0x1B4C6F0,0x1AD9DB0},{0x1B4BB04,0x1B4BB0C},{0x1B4BB0C,0x18682C0},
            {0x1AD9DB0,0x1B4C6E8},{0x1AD9DB4,0x16F8410},{0x1AD9DB8,0x16F8320},
            {0x1AD9DBC,0x1B91190},{0x1AD9DC0,0x1AD9E00},{0x1AD9DC4,0x1AD9E04},{0x1AD9DC8,0x1AD9E10},
            {0x1AD9DCC,0x1B91190},{0x1AD9DD0,0},{0x1AD9DD4,0},{0x1AD9DD8,0},{0x1AD9DDC,0},
            {0x1AD9DE0,0x1874B50},{0x1AD9DE4,0x1001},{0x1AD9E00,0x1B4C6DC},{0x1B4C6DC,0x186A668},
            {0x1AD9DA0,0x1AD8D90},{0x1AD9DA4,0x1AD9DF0},{0x1AD9DA8,0},
            {0x1AD9DF0,0x1AD9DA0},{0x1AD9DF4,0x1AD9E10},{0x1AD9DF8,0},
            {0x1AD9E10,0x1AD9DF0},{0x1AD9E14,0x1B36B68},{0x1B36B68,0x1AD9E10},
            {0x1AD6BE8,0x1AD9E18},{0x1AD6BEC,0x1AD9E18},
            {0x1AD9E18,0x1AD6BE8},{0x1AD9E1C,0x1AD6BE8},{0x1B91164,0x5CD58},
            {0x1AD6D9C,17},{0x1AD8DA0,0x1B4C70C},{0x1AD8DB8,0x1B4C70F}
        };
        bool state=true; uint32_t word=0;
        for(const auto &e:words) state &= ds_image_read_be32(&image,e[0],&word) && word==e[1];
        CHECK(state,"independent parent/node/vector layout, fourth/fifth splits and earlier registries match");
        CHECK(r.kernel_mutex_calls==1 && r.kernel_mutex_handle==0x1001 && r.kernel_mutex_output==0x1AD9DE4 &&
              r.kernel_mutex_attribute==0xFEFFB90 && r.kernel_mutex_attribute_words[0]==2 &&
              r.kernel_mutex_attribute_words[1]==0x10 && r.kernel_mutex_attribute_words[2]==0x200 &&
              r.kernel_mutex_attribute_words[3]==0x2000,
              "original sys_mutex_create returns a real registered recursive native object handle");
        CHECK(r.lock_calls==7 && r.unlock_calls==7 && r.rwlock_write_locks==1 && r.rwlock_write_unlocks==1 &&
              r.native_held_at_stop==0 && r.native_live_after_cleanup==0 && r.native_rwlocks_after_cleanup==0 &&
              r.native_kernel_mutexes_after_cleanup==0 && ds_kernel_mutex_live()==0,
              "completed constructor balances native ownership and destroys every native object");
        CHECK(r.atomic_attempts==1 && r.atomic_successes==1 && r.bootstrap_fallback_calls==0 &&
              image.allocated_bytes==DS_REF_ALLOCATED_BYTES+arena_bytes+0xA000,
              "full constructor uses only backed pool bytes without bootstrap substitution");
        DsEntryResult reused={};
        CHECK(!ds_entry_run_first_constructor(&image,&reused,error,sizeof(error)) && reused.stop==DS_ENTRY_BAD_INPUT,
              "completed constructor cannot replay into an already modified image");
        cleanup(image);
    }
    DsImage image={}; if(!load(image)) return;
    CHECK(ds_image_write_be32(&image,0x1947394,0),"test-only next constructor TOC corruption");
    DsEntryResult r={}; char error[256]={};
    CHECK(!ds_entry_run_first_constructor(&image,&r,error,sizeof(error)) && r.stop==DS_ENTRY_ABI_MISMATCH &&
          r.constructor_completed && r.kernel_mutex_calls==1 && r.native_kernel_mutexes_after_cleanup==0,
          "next constructor mismatch cannot fabricate progress and still cleans new native resources");
    cleanup(image);
}

static void completed_second_constructor()
{
    DsImage image={}; if(!load(image)) return;
    DsEntryResult r={}; char error[256]={};
    CHECK(ds_entry_run_second_constructor(&image,&r,error,sizeof(error)), "second constructor executes actual initialization and returns");
    CHECK(r.stop==DS_ENTRY_SECOND_CONSTRUCTOR_READY && r.constructors_completed==2 && r.function_count==179 &&
          r.next_constructor_descriptor==0x19359B0 && r.next_constructor_target==0x3F4FE0 &&
          r.stack_pointer==0xFEFFE00 && r.lr==0x16B84F4,
          "two completed constructors reach the original third descriptor and walker ABI");
    static constexpr uint32_t words[][2]={
        {0x1B49B98,0x1852728},{0x1B49B9C,0},{0x1B49BA0,0x1852738},{0x1B49BA4,0},
        {0x1B49BA8,0x1852748},{0x1B49BAC,0},
        {0x1B49BB0,0x1852728},{0x1B49BB4,0},{0x1B49BB8,0x1852738},{0x1B49BBC,0}
    };
    bool state=true; uint32_t word=0;
    for(const auto &e:words) state &= ds_image_read_be32(&image,e[0],&word) && word==e[1];
    CHECK(state && r.allocation_calls==5 && r.kernel_mutex_calls==1 && r.lock_calls==7 && r.unlock_calls==7,
          "second constructor installs actual image vtables without inventing new service calls");
    CHECK(r.native_kernel_mutexes_after_cleanup==0 && ds_kernel_mutex_live()==0 &&
          image.allocated_bytes==DS_REF_ALLOCATED_BYTES+arena_bytes+0xA000,
          "two-constructor checkpoint preserves allocation limits and cleans native resources");
    cleanup(image);
}

static void kernel_mutex_fault(const char *kind)
{
    DsImage image={}; if(!load(image)) return;
    DsEntryResult r={}; char error[256]={};
    CHECK(!ds_entry_run_first_constructor(&image,&r,error,sizeof(error)) && !r.constructor_completed &&
          r.allocation_calls==4 && r.node_address==0x1AD9DB0 && !r.node_vector_address,
          "kernel creation failure cannot claim a completed constructor or later allocation");
    const bool after=std::strcmp(kind,"after")==0;
    const DsEntryStop expected=std::strcmp(kind,"shared")==0 ? DS_ENTRY_UNSUPPORTED_CALL :
        std::strcmp(kind,"provider")==0 ? DS_ENTRY_SYNC_FAILURE : DS_ENTRY_MEMORY_FAULT;
    CHECK(r.stop==expected && r.kernel_mutex_calls==(after?1u:0u),
          "invalid kernel output/attribute/provider or post-create guest fault stops explicitly");
    uint32_t handle=0;
    CHECK(ds_image_read_be32(&image,0x1AD9DE4,&handle) && handle==(after?0x1001u:UINT32_MAX),
          "failed creation preserves original handle; a real creation writes its actual opaque handle");
    CHECK(r.native_kernel_mutexes_after_cleanup==0 && ds_kernel_mutex_live()==0 &&
          r.native_live_after_cleanup==0 && r.native_rwlocks_after_cleanup==0 &&
          ds_sync_live()==0 && ds_rwlock_live()==0 && ds_sync_held()==0,
          "all new-service failure paths destroy their native objects");
    cleanup(image);
}

static void readonly_data_fault()
{
    DsImage image = {};
    if (!load(image)) return;
    size_t data_index = 0;
    for (; data_index < image.segment_count; ++data_index)
        if ((image.segments[data_index].flags & DS_PF_W) != 0) break;
    CHECK(data_index < image.segment_count, "writable data segment found for fault injection");
    if (data_index == image.segment_count) { cleanup(image); return; }
    uint32_t flags = image.segments[data_index].flags;
    image.segments[data_index].flags &= ~DS_PF_W;
    DsEntryResult result = {};
    char error[256] = {};
    CHECK(!execute(image, result, error), "read-only data cannot fake successful startup");
    CHECK(result.stop == DS_ENTRY_MEMORY_FAULT && result.memory_fault,
          "actual guest write is rejected as a checked memory fault");
    CHECK(result.fault_address == 0x01AD5BC0 && result.fault_size == 4 &&
          result.fault_write && result.fault_required_flags == DS_PF_W,
          "fault describes exact process parameter write and permission");
    CHECK(result.boundary_nid == 0 && result.function_count == 2,
          "no firmware call is reached after rejected write");
    CHECK(error[0] != 0, "write fault has a useful error");
    image.segments[data_index].flags = flags;
    CHECK(execute(image, result, error), "adapter recovers cleanly after a write fault");
    cleanup(image);
}

static void corrupted_descriptor_fault()
{
    DsImage image = {};
    if (!load(image)) return;
    CHECK(ds_image_write_be32(&image, DS_REF_ENTRY_DESCRIPTOR + 4, 0xF0000000),
          "fault fixture mutates actual guest OPD data");
    DsEntryResult result = {};
    char error[256] = {};
    CHECK(!execute(image, result, error), "corrupt descriptor cannot fake a firmware checkpoint");
    CHECK(result.stop == DS_ENTRY_MEMORY_FAULT && result.memory_fault &&
          !result.fault_write && result.fault_required_flags == DS_PF_R,
          "bad TOC causes a checked read fault");
    CHECK(result.fault_address == 0xEFFF800C && result.fault_size == 4,
          "bad TOC fault uses the actual translated effective address");
    CHECK(result.boundary_nid == 0 && result.function_count == 2,
          "bad TOC stops before any firmware implementation");
    CHECK(ds_image_write_be32(&image, DS_REF_ENTRY_DESCRIPTOR + 4, DS_REF_TOC),
          "restore original descriptor for recovery check");
    CHECK(execute(image, result, error), "adapter recovers after a read fault");
    cleanup(image);
}

static void invalid_inputs_and_budgets()
{
    DsEntryResult result = {};
    char error[256] = {};
    CHECK(!ds_entry_run(nullptr, &result, error, sizeof(error)) && result.stop == DS_ENTRY_BAD_INPUT,
          "null image is rejected");
    DsImage image = {};
    CHECK(!ds_entry_run(&image, nullptr, error, sizeof(error)), "null result is rejected");
    if (!load(image, DS_REF_ALLOCATED_BYTES)) return;
    CHECK(!execute(image, result, error) && result.stop == DS_ENTRY_BAD_INPUT &&
          result.function_count == 0 && result.trace_count == 0,
          "image-only budget cannot begin guest execution");
    CHECK(image.allocated_bytes == DS_REF_ALLOCATED_BYTES, "failed first arena allocation stays bounded");
    cleanup(image);
    if (!load(image, DS_REF_ALLOCATED_BYTES + 0x100000)) return;
    CHECK(!execute(image, result, error) && result.stop == DS_ENTRY_BAD_INPUT && result.function_count == 0,
          "partial stack-only budget stops before execution");
    CHECK(image.allocated_bytes <= image.memory_limit, "partial setup remains within budget");
    cleanup(image);
    if (!load(image)) return;
    image.entry_code ^= 4;
    CHECK(!execute(image, result, error) && result.stop == DS_ENTRY_BAD_INPUT && result.function_count == 0,
          "wrong entry metadata rejects the game-specific slice");
    cleanup(image);
    if (!load(image)) return;
    CHECK(ds_image_add_zero_region(&image, 0x0FE00000, 64, DS_PF_R | DS_PF_W,
                                    error, sizeof(error)), "overlap fixture mapped");
    CHECK(!execute(image, result, error) && result.stop == DS_ENTRY_BAD_INPUT && result.function_count == 0,
          "incompatible existing stack arena is not borrowed or overwritten");
    cleanup(image);
}

static void unsupported_import()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult result = {};
    char error[256] = {};
    CHECK(!execute(image, result, error), "unknown firmware call cannot claim startup success");
    CHECK(result.stop == DS_ENTRY_UNSUPPORTED_CALL && result.boundary_nid == 0xDEADBEEF,
          "test-injected unsupported NID is recorded and rejected");
    CHECK(result.function_count == 3 && result.trace_count == 13 && !result.memory_fault,
          "unknown import halts immediately after the real startup writes");
    CHECK(result.stop != DS_ENTRY_TLS_BOUNDARY && error[0] != 0,
          "unsupported import never continues with a fake successful result");
    cleanup(image);
}

static void unsupported_syscall()
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult result = {};
    char error[256] = {};
    CHECK(!ds_entry_run_crt(&image, &result, error, sizeof(error)), "unknown syscall cannot claim CRT success");
    CHECK(result.stop == DS_ENTRY_UNSUPPORTED_CALL && result.syscall_number == 999 && error[0],
          "test-injected syscall rejected with exact number");
    CHECK(result.tls_initialized && result.mutex_initialized && !result.memory_fault,
          "unknown syscall stops after real service effects without synthetic memory results");
    CHECK(!ds_entry_run_memory(&image, &result, error, sizeof(error)) && result.stop == DS_ENTRY_UNSUPPORTED_CALL &&
          result.syscall_number == 999 && result.memory_query_count == 0,
          "new memory mode also rejects the changed syscall without writing a query result");
    cleanup(image);
}

static void memory_output_fault(bool tail)
{
    DsImage image = {};
    if (!load(image)) return;
    DsEntryResult result = {};
    char error[256] = {};
    CHECK(!ds_entry_run_memory(&image, &result, error, sizeof(error)), "invalid memory-query output cannot claim startup success");
    CHECK(result.stop == DS_ENTRY_MEMORY_FAULT && result.fault_write && result.fault_size == 8 &&
          result.fault_address == (tail ? 0x10000FFCu : 0u), "entire query output span checked before any writes");
    CHECK(result.memory_query_count == 0 && result.mutex_count == 1 && result.tls_initialized,
          "faulted query reports no successful completion");
    uint32_t word = 0;
    if (tail) CHECK(ds_image_read_be32(&image, 0x10000FFC, &word) && word == 0xA5A55A5A,
                    "valid first word of crossing span remains unchanged after failure");
    cleanup(image);
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3) {
        std::fprintf(stderr, "Usage: %s verified-EBOOT.ELF [--unexpected-import]\n", argv[0]);
        return 2;
    }
    image_path = argv[1];
    if (argc == 3 && std::strcmp(argv[2], "--unexpected-import") == 0) unsupported_import();
    else if (argc == 3 && std::strcmp(argv[2], "--unexpected-syscall") == 0) unsupported_syscall();
    else if (argc == 3 && std::strcmp(argv[2], "--memory-output-null") == 0) memory_output_fault(false);
    else if (argc == 3 && std::strcmp(argv[2], "--memory-output-tail") == 0) memory_output_fault(true);
    else if (argc == 3 && std::strcmp(argv[2], "--clock-provider-failure") == 0) clock_provider_failure();
    else if (argc == 3 && std::strcmp(argv[2], "--singleton-probe") == 0) {
        DsImage image = {}; DsEntryResult r = {}; char e[256] = {};
        if (!load(image)) return 1;
        bool ok = ds_entry_run_startup(&image, &r, e, sizeof(e));
        std::printf("SINGLETON probe ok=%d stop=%d callee=%08x indirect=%08x descriptor=%08x lr=%llx sp=%llx r3=%llx locks=%u unlocks=%u count=%u error=%s\n", ok, r.stop, r.callee, r.indirect_code, r.indirect_descriptor, (unsigned long long)r.lr, (unsigned long long)r.stack_pointer, (unsigned long long)r.r3, r.lock_calls, r.unlock_calls, r.function_count, e);
        std::printf("pool=%d alloc=%08x atomic=%u/%u rw=%u/%u singleton=%d\n", r.pool_initialized, r.allocation_address, r.atomic_successes, r.atomic_attempts, r.rwlock_write_locks, r.rwlock_write_unlocks, r.singleton_completed);
        uint32_t w1 = 0, w2 = 0; ds_image_read_be32(&image, 0x1B91194, &w1); ds_image_read_be32(&image, 0x1B911A8, &w2); std::printf("manager words self=%08x vtable=%08x\n", w1, w2);
        for (unsigned i = 35; i < r.function_count && i < DS_ENTRY_FUNCTION_CAPACITY; ++i) std::printf("%u: %08x\n", i, r.functions[i]);
        cleanup(image);
    }
    else if (argc == 3 && std::strcmp(argv[2], "--registry-probe") == 0) registry_probe();
    else if (argc == 3 && std::strcmp(argv[2], "--registry-lock-fault") == 0) registry_lock_fault();
    else if (argc == 3 && std::strcmp(argv[2], "--explore-sixth-state-fault") == 0) {
        DsImage image={}; if(!load(image)) return 1;
        DsEntryResult r={}; char e[256]={};
        CHECK(!ds_entry_run_six_constructors(&image,&r,e,sizeof(e)) &&
              r.stop==DS_ENTRY_ABI_MISMATCH && r.constructors_completed==6 && r.function_count==424,
              "corrupted registered object refuses checkpoint success even after all six constructors return");
        CHECK(std::strstr(e,"0x01AD8DA4")!=nullptr,"memory validation identifies the corrupted key/object word");
        CHECK(!r.native_live_after_cleanup && !r.native_rwlocks_after_cleanup && !r.native_kernel_mutexes_after_cleanup,
              "failed final memory validation leaves every native registry empty");
        cleanup(image);
    }
    else if (argc == 3 && (std::strcmp(argv[2], "--explore-ninth-state-fault")==0 ||
                           std::strcmp(argv[2], "--explore-ninth-native-fault")==0)) {
        const bool state=std::strcmp(argv[2], "--explore-ninth-state-fault")==0;
        DsImage image={}; if(!load(image)) return 1;
        DsEntryResult r={}; char e[256]={};
        const bool ok=ds_entry_run_nine_constructors(&image,&r,e,sizeof(e));
        if(state) {
            CHECK(!ok && r.stop==DS_ENTRY_ABI_MISMATCH && r.constructors_completed==9 && r.function_count==561,
                  "corrupting the thirty-second object refuses success after nine real constructor returns");
            CHECK(std::strstr(e,"0x01AD8E9C")!=nullptr,"validator identifies the corrupted final key/object word");
        } else {
            CHECK(!ok && r.stop==DS_ENTRY_MEMORY_FAULT && r.fault_write && r.fault_address==0 &&
                  r.constructors_completed==8 && r.kernel_mutex_calls==5 && r.kernel_mutex_handle==0x1005,
                  "guest fault after fifth native creation refuses constructor-nine success");
        }
        CHECK(!r.native_live_after_cleanup && !r.native_rwlocks_after_cleanup && !r.native_kernel_mutexes_after_cleanup,
              "late nine-constructor failures clean all five native objects and the other registries");
        cleanup(image);
    }
    else if (argc == 3 && (std::strcmp(argv[2], "--explore-tenth-state-fault")==0 ||
                           std::strcmp(argv[2], "--explore-tenth-sort-fault")==0)) {
        const bool state=std::strcmp(argv[2], "--explore-tenth-state-fault")==0;
        DsImage image={}; if(!load(image)) return 1;
        DsEntryResult r={}; char e[256]={};
        const bool ok=ds_entry_run_ten_constructors(&image,&r,e,sizeof(e));
        if(state) {
            CHECK(!ok && r.stop==DS_ENTRY_ABI_MISMATCH && r.constructors_completed==10 && r.function_count==600,
                  "corrupting the final sorted record refuses success after ten real constructor returns");
            CHECK(std::strstr(e,"0x01AD8EBC")!=nullptr,"validator identifies the corrupt thirty-sixth object");
        } else {
            CHECK(!ok && r.stop==DS_ENTRY_MEMORY_FAULT && r.fault_address==0 && !r.fault_write &&
                  r.constructors_completed==9 && r.callee==0xADF198,
                  "invalid larger-sort input stops at the checked partition read");
        }
        CHECK(!r.native_live_after_cleanup && !r.native_rwlocks_after_cleanup && !r.native_kernel_mutexes_after_cleanup,
              "larger-sort and final-state failures clean all native registries");
        cleanup(image);
    }
    else if (argc == 3 && std::strcmp(argv[2], "--explore-eleventh-state-fault") == 0) {
        DsImage image={}; if(!load(image)) return 1;
        DsEntryResult r={}; char e[256]={};
        CHECK(!ds_entry_run_eleven_constructors(&image,&r,e,sizeof(e)) &&
              r.stop==DS_ENTRY_ABI_MISMATCH && r.constructors_completed==11 && r.function_count==603,
              "corrupt static object refuses success after eleven real constructor returns");
        CHECK(std::strstr(e,"0x01B48C64")!=nullptr,"validator identifies the corrupt static vtable");
        CHECK(!r.native_live_after_cleanup && !r.native_rwlocks_after_cleanup && !r.native_kernel_mutexes_after_cleanup,
              "eleventh-constructor state failure cleans every native registry");
        cleanup(image);
    }
    else if (argc == 3 && std::strcmp(argv[2],"--explore-allocated-probe")==0) {
        DsImage image={};if(!load(image)) return 1;
        DsEntryResult r={};char e[256]={};
        const bool ok=ds_entry_run_runtime_allocated(&image,&r,e,sizeof(e));
        std::printf("ALLOCATED ok=%d stop=%d callee=%08x functions=%u accesses=%u constructors=%u r3=%llx r4=%llx r5=%llx r6=%llx lr=%llx sp=%llx toc=%llx locks=%u/%u mutexes=%u thread_ids=%u error=%s\n",
            ok,r.stop,r.callee,r.function_count,r.trace_count,r.constructors_completed,
            (unsigned long long)r.r3,(unsigned long long)r.r4,(unsigned long long)r.r5,(unsigned long long)r.r6,
            (unsigned long long)r.lr,(unsigned long long)r.stack_pointer,(unsigned long long)r.toc,
            r.lock_calls,r.unlock_calls,r.mutex_count,r.thread_id_calls,e);
        std::printf("HEAP calls=%u failed=%u live=%u bytes=%zu requested=%zu backed=%zu arenas=%u guest=%08x size=%u image=%zu metadata=%u\n",
            r.runtime_heap.allocation_calls,r.runtime_heap.failed_allocations,r.runtime_heap.live_allocations,
            r.runtime_heap.live_bytes,r.runtime_heap.requested_bytes,r.runtime_heap.backed_bytes,r.runtime_heap.arena_count,
            r.runtime_malloc_address,r.runtime_malloc_size,image.allocated_bytes,r.runtime_heap_metadata_after_cleanup);
        for(unsigned i=641;i<r.function_count && i<DS_ENTRY_FUNCTION_CAPACITY;++i) std::printf("%u: %08x\n",i,r.functions[i]);
        for(unsigned i=0;i<48;i+=4) {
            uint32_t value=0;if(ds_image_read_be32(&image,DS_HEAP_BASE+i,&value))std::printf("HEAP WORD %08x=%08x\n",DS_HEAP_BASE+i,value);
        }
        CHECK(!r.native_live_after_cleanup && !r.native_rwlocks_after_cleanup && !r.native_kernel_mutexes_after_cleanup &&
              !r.runtime_heap_metadata_after_cleanup,"new allocation frontier releases every native and metadata registry");
        cleanup(image);
    }
    else if (argc == 3 && std::strncmp(argv[2], "--explore-runtime-",18)==0) {
        const bool normal=std::strcmp(argv[2],"--explore-runtime-heap")==0;
        const bool state=std::strcmp(argv[2],"--explore-runtime-state-fault")==0;
        const bool locked=std::strcmp(argv[2],"--explore-runtime-lock-fault")==0;
        DsImage image={}; if(!load(image)) return 1;
        DsEntryResult r={}; char e[256]={};
        const bool ok=ds_entry_run_runtime_heap(&image,&r,e,sizeof(e));
        std::printf("RUNTIME ok=%d stop=%d callee=%08x functions=%u accesses=%u completed=%u r3=%llx r4=%llx r5=%llx r6=%llx lr=%llx sp=%llx toc=%llx locks=%u/%u held=%u error=%s\n",
            ok,r.stop,r.callee,r.function_count,r.trace_count,r.constructors_completed,
            (unsigned long long)r.r3,(unsigned long long)r.r4,(unsigned long long)r.r5,(unsigned long long)r.r6,
            (unsigned long long)r.lr,(unsigned long long)r.stack_pointer,(unsigned long long)r.toc,
            r.lock_calls,r.unlock_calls,r.native_held_at_stop,e);
        for(unsigned i=641;i<r.function_count && i<DS_ENTRY_FUNCTION_CAPACITY;++i)
            std::printf("%u: %08x\n",i,r.functions[i]);
        if(normal) {
            CHECK(ok && r.stop==DS_ENTRY_RUNTIME_HEAP_BOUNDARY && r.constructors_completed==12 &&
                  r.function_count==652 && r.trace_count==12206 && r.callee==0x168ABD0 && !e[0],
                  "constructor thirteen reaches the validated real runtime allocation boundary");
            CHECK(r.r3==40 && r.r4==0 && r.r5==0 && r.r6==0x1AD8EE0 && r.lr==0x16A051C &&
                  r.stack_pointer==0xFEFFB60 && r.toc==0x19D3BAC,
                  "first operator-new size and original runtime ABI are preserved exactly");
            CHECK(r.registry_entries==10 && r.callback_entries==40 && r.allocation_calls==14 &&
                  r.kernel_mutex_calls==5 && r.lock_calls==17 && r.unlock_calls==17 && !r.native_held_at_stop &&
                  image.allocated_bytes==DS_REF_ALLOCATED_BYTES+arena_bytes+0xA000,
                  "runtime prefix preserves all metadata and guest backing with one extra native lock cycle");
            uint32_t value=0;
            CHECK(ds_image_read_be32(&image,0x1AD6020,&value) && value==1 &&
                  ds_image_read_be32(&image,0x1CE64E8,&value) && value==0x18C1BF0 &&
                  ds_image_read_be32(&image,0x1CE64EC,&value) && value==0 &&
                  ds_image_read_be32(&image,0x1AD6088,&value) && value==0xFFFFFFFF,
                  "original locale counter and vtable update while allocation output and wide initializer stay untouched");
            DsEntryResult reused={};
            CHECK(!ds_entry_run_runtime_heap(&image,&reused,e,sizeof(e)) && reused.stop==DS_ENTRY_BAD_INPUT,
                  "runtime prefix refuses replay into a modified image");
            for(unsigned i=0;i<2;++i) {
                DsImage fresh={}; if(!load(fresh)) return 1;
                DsEntryResult repeated={}; char message[256]={};
                CHECK(ds_entry_run_runtime_heap(&fresh,&repeated,message,sizeof(message)) &&
                      repeated.stop==DS_ENTRY_RUNTIME_HEAP_BOUNDARY && repeated.trace_count==12206,
                      "fresh-image runtime prefix is repeatable with the exact checked trace");
                cleanup(fresh);
            }
            struct Fault { uint32_t address,value; DsEntryStop stop; uint32_t callee; };
            const Fault faults[] = {
                {0x19A3EA4,0,DS_ENTRY_ABI_MISMATCH,0xADE478},
                {0x19A3EA4,DS_REF_TOC,DS_ENTRY_ABI_MISMATCH,0xADE478},
                // Diagnostic callee is the most recent entry (unlock), not a shadow stack.
                {0x19D8810,0,DS_ENTRY_MEMORY_FAULT,0x16BB264},
                {0x1AD6020,0,DS_ENTRY_UNSUPPORTED_CALL,0x168BC68}
            };
            for(const auto &fault:faults) {
                DsImage bad={}; if(!load(bad)) return 1;
                CHECK(ds_image_write_be32(&bad,fault.address,fault.value),"test-only runtime TOC, locale or destination corruption");
                DsEntryResult rejected={}; char message[256]={};
                const bool accepted=ds_entry_run_runtime_heap(&bad,&rejected,message,sizeof(message));
                if(accepted || rejected.stop!=fault.stop || rejected.callee!=fault.callee || rejected.constructors_completed!=12)
                    std::fprintf(stderr,"Runtime fault %08x: ok=%d stop=%d callee=%08x completed=%u error=%s\n",
                        fault.address,accepted,rejected.stop,rejected.callee,rejected.constructors_completed,message);
                CHECK(!accepted && rejected.stop==fault.stop && rejected.callee==fault.callee && rejected.constructors_completed==12,
                      "runtime descriptor and alternate locale paths cannot fabricate the allocation checkpoint");
                if(fault.stop==DS_ENTRY_MEMORY_FAULT)
                    CHECK(rejected.fault_write && rejected.fault_address==0,"null locale destination faults at the original store");
                CHECK(!rejected.native_live_after_cleanup && !rejected.native_rwlocks_after_cleanup &&
                      !rejected.native_kernel_mutexes_after_cleanup,"runtime faults release every native registry");
                cleanup(bad);
            }
            DsImage unchecked={}; if(!load(unchecked)) return 1;
            DsEntryResult frontier={}; char message[256]={};
            CHECK(!ds_entry_run_exploration(&unchecked,&frontier,message,sizeof(message),0) &&
                  frontier.stop==DS_ENTRY_UNSUPPORTED_CALL && frontier.callee==0x168ABD0 &&
                  frontier.function_count==652 && frontier.constructors_completed==12,
                  "ordinary exploration retains an unsupported allocator boundary without the checkpoint mode");
            cleanup(unchecked);
        } else {
            CHECK(!ok && r.constructors_completed==12,"runtime fixture refuses prefix success before thirteen can return");
            if(state) {
                CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && std::strstr(e,"0x01AD6020")!=nullptr,
                      "validator identifies corrupted locale counter after original initialization");
            } else if(locked) {
                CHECK(r.stop==DS_ENTRY_MEMORY_FAULT && r.fault_write && r.fault_address==0 &&
                      r.lock_calls==17 && r.unlock_calls==16 && r.native_held_at_stop==1,
                      "fault while runtime mutex is held retains precise diagnostics and performs teardown");
            } else {
                CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && r.callee==0x168ABD0 && r.r3==41,
                      "unexpected operator-new request size cannot be accepted as this checkpoint");
            }
        }
        CHECK(!r.native_live_after_cleanup && !r.native_rwlocks_after_cleanup && !r.native_kernel_mutexes_after_cleanup,
              "runtime checkpoint and injected failures clean every native registry");
        cleanup(image);
    }
    else if (argc == 3 && (std::strcmp(argv[2], "--explore-twelfth-state-fault")==0 ||
                           std::strcmp(argv[2], "--explore-twelfth-key-fault")==0 ||
                           std::strcmp(argv[2], "--explore-twelfth-sort-fault")==0)) {
        const bool sort=std::strcmp(argv[2], "--explore-twelfth-sort-fault")==0;
        const bool key=std::strcmp(argv[2], "--explore-twelfth-key-fault")==0;
        DsImage image={}; if(!load(image)) return 1;
        DsEntryResult r={}; char e[256]={};
        CHECK(!ds_entry_run_twelve_constructors(&image,&r,e,sizeof(e)),
              "late constructor-twelve corruption refuses checkpoint success");
        if(sort) {
            CHECK(r.stop==DS_ENTRY_MEMORY_FAULT && !r.fault_write && r.fault_address==0 &&
                  r.callee==0xADF198 && r.constructors_completed==11,
                  "forty-pair partition failure stops at its original checked read");
        } else {
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && r.constructors_completed==12 && r.function_count==641,
                  "final-state corruption is rejected after twelve actual returns");
            CHECK(std::strstr(e,key?"0x01AD8E04":"0x01B4287C")!=nullptr,
                  "validator identifies the newly corrupted key payload or parent link");
        }
        CHECK(!r.native_live_after_cleanup && !r.native_rwlocks_after_cleanup && !r.native_kernel_mutexes_after_cleanup,
              "late constructor-twelve faults clean every native registry");
        cleanup(image);
    }
    else if (argc == 3 && std::strncmp(argv[2], "--explore-", 10) == 0) {
        DsImage image={}; if(!load(image)) return 1;
        DsEntryResult r={}; char error[256]={};
        const bool third=std::strcmp(argv[2], "--explore-third")==0;
        const bool fourth=std::strcmp(argv[2], "--explore-fourth")==0;
        const bool sixth=std::strcmp(argv[2], "--explore-sixth")==0;
        const bool ninth=std::strcmp(argv[2], "--explore-ninth")==0;
        const bool tenth=std::strcmp(argv[2], "--explore-tenth")==0;
        const bool eleventh=std::strcmp(argv[2], "--explore-eleventh")==0;
        const bool twelfth=std::strcmp(argv[2], "--explore-twelfth")==0;
        const bool fourth_fault=std::strcmp(argv[2], "--explore-fourth-fault")==0;
        const bool ahead=std::strcmp(argv[2], "--explore-ahead")==0 || third || fourth || sixth || ninth || tenth || eleventh || twelfth || fourth_fault;
        const bool corrupt=std::strcmp(argv[2], "--explore-bad-callsite")==0;
        const bool ok=ds_entry_run_exploration(&image,&r,error,sizeof(error),twelfth?12u:eleventh?11u:tenth?10u:ninth?9u:sixth?6u:fourth?4u:third?3u:ahead?0u:2u);
        std::printf("EXPLORE ok=%d stop=%d callee=%08x lr=%llx sp=%llx functions=%u accesses=%u ctors=%u allocs=%u heavy=%u locks=%u/%u error=%s\n",ok,r.stop,r.callee,(unsigned long long)r.lr,(unsigned long long)r.stack_pointer,r.function_count,r.trace_count,r.constructors_completed,r.allocation_calls,r.kernel_mutex_calls,r.lock_calls,r.unlock_calls,error);
        std::printf("EXPLORE ABI r3=%llx r4=%llx r5=%llx r6=%llx next_descriptor=%08x\n",
            (unsigned long long)r.r3, (unsigned long long)r.r4,
            (unsigned long long)r.r5, (unsigned long long)r.r6,r.next_constructor_descriptor);
        for(unsigned i=137;i<r.function_count && i<DS_ENTRY_FUNCTION_CAPACITY;++i) std::printf("%u: %08x\n",i,r.functions[i]);
        if(twelfth) {
            CHECK(!ok && r.stop==DS_ENTRY_EXECUTION_LIMIT && r.constructors_completed==12 &&
                  r.function_count==641 && r.trace_count==12134 && r.callee==0x15166F0 &&
                  r.next_constructor_descriptor==0x19A3EA0 && r.toc==0x19D3BAC,
                  "twelve constructors return before the exact runtime-TOC constructor descriptor");
            CHECK(r.allocation_calls==14 && r.kernel_mutex_calls==5 && r.lock_calls==16 && r.unlock_calls==16 &&
                  r.r3==0x1B42878 && r.r4==0x1BB6060 && r.r5==0 && r.r6==0x1AD8EE0 &&
                  image.allocated_bytes==DS_REF_ALLOCATED_BYTES+arena_bytes+0xA000,
                  "draw-seed registration preserves exact return ABI and backing without new native services");
            unsigned partitions=0;
            for(unsigned i=0;i<r.function_count;++i) partitions+=r.functions[i]==0xADF198;
            CHECK(partitions==2,"both thirty-six and forty-pair sorts execute the original partition helper");
            for(unsigned attempt=0;attempt<2;++attempt) {
                DsImage stable={}; if(!load(stable)) return 1;
                DsEntryResult completed={}; char e[256]={};
                CHECK(ds_entry_run_twelve_constructors(&stable,&completed,e,sizeof(e)),
                      "production twelve-constructor validator checks all metadata on fresh reload");
                if(e[0]) std::fprintf(stderr,"twelve-constructor validator: %s\n",e);
                CHECK(completed.stop==DS_ENTRY_TWELFTH_CONSTRUCTOR_READY && completed.registry_entries==10 &&
                      completed.callback_entries==40 && !e[0],"ten names and forty complete sorted key pairs are validated");
                DsEntryResult reused={};
                CHECK(!ds_entry_run_twelve_constructors(&stable,&reused,e,sizeof(e)) && reused.stop==DS_ENTRY_BAD_INPUT,
                      "twelve-constructor checkpoint refuses in-place replay");
                cleanup(stable);
            }
            struct Fault { uint32_t address, value, completed; bool memory; };
            const Fault faults[] = {
                {0x1935914,0,11,false}, {0x193588C,0,11,false},
                {0x19A3EA4,0,12,false}, {0x19A3EA4,DS_REF_TOC,12,false},
                {0x19A3EA0,0x15166F4,12,false},
                {0x18DE7CC,0xF0000000,11,true}
            };
            for(const auto &fault : faults) {
                DsImage bad={}; if(!load(bad)) return 1;
                CHECK(ds_image_write_be32(&bad,fault.address,fault.value),"test-only constructor twelve/thirteen corruption");
                DsEntryResult rejected={}; char e[256]={};
                CHECK(!ds_entry_run_twelve_constructors(&bad,&rejected,e,sizeof(e)),
                      "invalid descriptor, TOC or string cannot claim twelve-constructor success");
                CHECK(rejected.constructors_completed==fault.completed &&
                      rejected.stop==(fault.memory?DS_ENTRY_MEMORY_FAULT:DS_ENTRY_ABI_MISMATCH),
                      "exact constructor count and checked fault boundary are preserved");
                if(fault.memory) CHECK(rejected.fault_address==0xF0000000 && !rejected.fault_write,
                                      "original strlen rejects an unbacked name pointer");
                CHECK(!rejected.native_live_after_cleanup && !rejected.native_rwlocks_after_cleanup &&
                      !rejected.native_kernel_mutexes_after_cleanup,"descriptor and guest access failures clean native state");
                cleanup(bad);
            }
        }
        if(eleventh) {
            CHECK(!ok && r.stop==DS_ENTRY_EXECUTION_LIMIT && r.constructors_completed==11 &&
                  r.callee==0x3F46B0 && r.next_constructor_descriptor==0x1935910 &&
                  r.function_count==603 && r.trace_count==11044,
                  "eleven constructors return before the twelfth descriptor with the exact trace");
            CHECK(r.allocation_calls==14 && r.kernel_mutex_calls==5 && r.lock_calls==16 && r.unlock_calls==16 &&
                  image.allocated_bytes==DS_REF_ALLOCATED_BYTES+arena_bytes+0xA000,
                  "static object initialization uses no extra native service or guest arena");
            for(unsigned attempt=0;attempt<2;++attempt) {
                DsImage stable={}; if(!load(stable)) return 1;
                DsEntryResult completed={}; char e[256]={};
                CHECK(ds_entry_run_eleven_constructors(&stable,&completed,e,sizeof(e)),
                      "production eleven-constructor validator accepts every object on fresh reload");
                if(e[0]) std::fprintf(stderr,"eleven-constructor validator: %s\n",e);
                CHECK(completed.stop==DS_ENTRY_ELEVENTH_CONSTRUCTOR_READY && completed.registry_entries==9 &&
                      completed.callback_entries==36 && !e[0],"all previous metadata remains valid after constructor eleven");
                DsEntryResult reused={};
                CHECK(!ds_entry_run_eleven_constructors(&stable,&reused,e,sizeof(e)) && reused.stop==DS_ENTRY_BAD_INPUT,
                      "eleven-constructor checkpoint refuses in-place replay");
                cleanup(stable);
            }
            for(unsigned i=0;i<3;++i) {
                DsImage bad={}; if(!load(bad)) return 1;
                const uint32_t address=i==0?0x1943CD4:i==1?0x1935914:0x18EA9F4;
                CHECK(ds_image_write_be32(&bad,address,0),"test-only constructor TOC or destination corruption");
                DsEntryResult rejected={}; char e[256]={};
                CHECK(!ds_entry_run_eleven_constructors(&bad,&rejected,e,sizeof(e)),"corrupt initialization cannot claim checkpoint success");
                if(i<2) {
                    CHECK(rejected.stop==DS_ENTRY_ABI_MISMATCH && rejected.constructors_completed==10+i,
                          "bad constructor TOC stops before invocation");
                } else {
                    CHECK(rejected.stop==DS_ENTRY_MEMORY_FAULT && rejected.fault_address==0 && rejected.fault_write &&
                          rejected.constructors_completed==10,"unbacked static destination stops at the original checked write");
                }
                CHECK(!rejected.native_live_after_cleanup && !rejected.native_rwlocks_after_cleanup &&
                      !rejected.native_kernel_mutexes_after_cleanup,"constructor failure releases all native state");
                cleanup(bad);
            }
        }
        if(tenth) {
            CHECK(!ok && r.stop==DS_ENTRY_EXECUTION_LIMIT && r.constructors_completed==10 &&
                  r.callee==0x5FD720 && r.next_constructor_descriptor==0x1943CD0 &&
                  r.function_count==600 && r.trace_count==11007,
                  "ten constructors return before the eleventh descriptor with the expected full trace");
            bool partitioned=false;
            for(unsigned i=0;i<r.function_count && i<DS_ENTRY_FUNCTION_CAPACITY;++i)
                partitioned |= r.functions[i]==0xADF198;
            CHECK(partitioned,"actual 36-key startup path executes the original larger-sort partition helper");
            CHECK(r.allocation_calls==14 && r.kernel_mutex_calls==5 && r.lock_calls==16 && r.unlock_calls==16 &&
                  image.allocated_bytes==DS_REF_ALLOCATED_BYTES+arena_bytes+0xA000,
                  "FileCap metadata and larger sorting require no extra allocation, service or guest arena");
            uint32_t value=0;
            CHECK(ds_image_read_be32(&image,0x1AD6DA0,&value) && value==0x1B49B70 &&
                  ds_image_read_be32(&image,0x1AD6DAC,&value) && value==7 &&
                  ds_image_read_be32(&image,0x1AD8EBC,&value) && value==0x1B4F8BC,
                  "nine names and all thirty-six opaque key/object records are sorted and retained");
            for(unsigned attempt=0;attempt<2;++attempt) {
                DsImage stable={}; if(!load(stable)) return 1;
                DsEntryResult completed={}; char e[256]={};
                CHECK(ds_entry_run_ten_constructors(&stable,&completed,e,sizeof(e)),
                      "production ten-constructor validator accepts the complete state on fresh reload");
                if(e[0]) std::fprintf(stderr,"ten-constructor validator: %s\n",e);
                CHECK(completed.stop==DS_ENTRY_TENTH_CONSTRUCTOR_READY && completed.registry_entries==9 &&
                      completed.callback_entries==36 && !e[0],
                      "validated checkpoint reports nine names and thirty-six sorted opaque pairs");
                DsEntryResult reused={};
                CHECK(!ds_entry_run_ten_constructors(&stable,&reused,e,sizeof(e)) && reused.stop==DS_ENTRY_BAD_INPUT,
                      "ten-constructor checkpoint refuses in-place replay");
                cleanup(stable);
            }
            DsImage bad={}; if(!load(bad)) return 1;
            CHECK(ds_image_write_be32(&bad,0x19471C4,0),"test-only FileCap getter TOC corruption");
            DsEntryResult rejected={}; char e[256]={};
            CHECK(!ds_entry_run_ten_constructors(&bad,&rejected,e,sizeof(e)) &&
                  rejected.stop==DS_ENTRY_ABI_MISMATCH && rejected.constructors_completed==9,
                  "bad FileCap descriptor refuses the tenth completed return");
            CHECK(!rejected.native_live_after_cleanup && !rejected.native_rwlocks_after_cleanup &&
                  !rejected.native_kernel_mutexes_after_cleanup,"tenth-constructor dispatch failure releases native state");
            cleanup(bad);
        }
        if(ninth) {
            CHECK(!ok && r.stop==DS_ENTRY_EXECUTION_LIMIT && r.constructors_completed==9 &&
                  r.callee==0x66A4A8 && r.next_constructor_descriptor==0x1947270 &&
                  r.function_count==561 && r.trace_count==10052,
                  "nine constructors return before the tenth descriptor with their exact checked trace");
            CHECK(r.allocation_calls==14 && r.kernel_mutex_calls==5 && r.kernel_mutex_handle==0x1005 &&
                  r.lock_calls==16 && r.unlock_calls==16 && !r.bootstrap_fallback_calls,
                  "ninth constructor performs its actual extra node/vector allocation and fifth native mutex creation");
            CHECK(image.allocated_bytes==DS_REF_ALLOCATED_BYTES+arena_bytes+0xA000 &&
                  r.node_address==0x1AD9DB0 && r.node_vector_address==0x1AD9E00,
                  "new pool objects retain existing backing and earlier node diagnostics");
            uint32_t value=0; uint64_t counter=0;
            CHECK(ds_image_read_be64(&image,0x1AD61A8,&counter) && counter==1 &&
                  ds_image_read_be32(&image,0x1B4E550,&value) && value==0xFFFFFFFF &&
                  ds_image_read_be32(&image,0x1AD9FF4,&value) && value==0x1005,
                  "actual counter increment, guest halfword sentinels and native handle are published");
            CHECK(ds_image_read_be32(&image,0x1AD6D90,&value) && value==0x1B482C8 &&
                  ds_image_read_be32(&image,0x1AD6DE0,&value) && value==0x1B4E560 &&
                  ds_image_read_be32(&image,0x1AD8E9C,&value) && value==0x1B4F8BC,
                  "eight names and all thirty-two keys retain sorted registry order");
            for(unsigned attempt=0;attempt<2;++attempt) {
                DsImage stable={}; if(!load(stable)) return 1;
                DsEntryResult completed={}; char e[256]={};
                CHECK(ds_entry_run_nine_constructors(&stable,&completed,e,sizeof(e)),
                      "production nine-constructor validator accepts the complete state after fresh reload");
                if(e[0]) std::fprintf(stderr,"nine-constructor validator: %s\n",e);
                CHECK(completed.stop==DS_ENTRY_NINTH_CONSTRUCTOR_READY && completed.registry_entries==8 &&
                      completed.callback_entries==32 && !e[0],
                      "checkpoint reports eight verified name records and thirty-two opaque keys");
                DsEntryResult reused={};
                CHECK(!ds_entry_run_nine_constructors(&stable,&reused,e,sizeof(e)) && reused.stop==DS_ENTRY_BAD_INPUT,
                      "nine-constructor checkpoint refuses in-place replay");
                cleanup(stable);
            }
            static constexpr uint32_t bad_tocs[]={0x19206DC,0x1940D94,0x195858C};
            for(unsigned i=0;i<3;++i) {
                DsImage bad={}; if(!load(bad)) return 1;
                CHECK(ds_image_write_be32(&bad,bad_tocs[i],0),"test-only new getter TOC corruption");
                DsEntryResult rejected={}; char e[256]={};
                CHECK(!ds_entry_run_nine_constructors(&bad,&rejected,e,sizeof(e)) &&
                      rejected.stop==DS_ENTRY_ABI_MISMATCH && rejected.constructors_completed==6+i,
                      "corrupt seventh/eighth/ninth descriptor rejects before that constructor completes");
                CHECK(!rejected.native_live_after_cleanup && !rejected.native_rwlocks_after_cleanup &&
                      !rejected.native_kernel_mutexes_after_cleanup,"failed new metadata dispatch cleans every native registry");
                cleanup(bad);
            }
        }
        if(sixth) {
            CHECK(!ok && r.stop==DS_ENTRY_EXECUTION_LIMIT && r.constructors_completed==6 &&
                  r.callee==0xBF6E0 && r.next_constructor_descriptor==0x1920778 &&
                  r.function_count==424 && r.trace_count==7407,
                  "six constructors return before the seventh original descriptor with the expected trace");
            CHECK(r.allocation_calls==12 && r.kernel_mutex_calls==4 && r.lock_calls==14 && r.unlock_calls==14 &&
                  !r.bootstrap_fallback_calls && image.allocated_bytes==DS_REF_ALLOCATED_BYTES+arena_bytes+0xA000,
                  "two new metadata types require no extra allocation, native service or guest arena");
            uint32_t value=0;
            CHECK(ds_image_read_be32(&image,0x1AD6D90,&value) && value==0x1B385D0 &&
                  ds_image_read_be32(&image,0x1AD6D9C,&value) && value==11 &&
                  ds_image_read_be32(&image,0x1AD6DD0,&value) && value==0x1B36EB0 &&
                  ds_image_read_be32(&image,0x1AD6DDC,&value) && value==33,
                  "name insertion places the eleven-byte shadow-camera name first and thirty-three-byte shader name last");
            CHECK(ds_image_read_be32(&image,0x1AD8DA0,&value) && value==0x1B36ED4 &&
                  ds_image_read_be32(&image,0x1AD8E38,&value) && value==0x1B4F8E3,
                  "twenty opaque keys are sorted across the two new and three earlier types");
            for(unsigned attempt=0;attempt<2;++attempt) {
                DsImage stable={}; if(!load(stable)) return 1;
                DsEntryResult completed={}; char e[256]={};
                CHECK(ds_entry_run_six_constructors(&stable,&completed,e,sizeof(e)),
                      "production six-constructor validator accepts the complete metadata graph on fresh reload");
                if(e[0]) std::fprintf(stderr,"six-constructor validator: %s\n",e);
                CHECK(completed.stop==DS_ENTRY_SIXTH_CONSTRUCTOR_READY && completed.registry_entries==5 &&
                      completed.callback_entries==20 && !e[0],
                      "production checkpoint reports five type names and twenty verified key/object pairs");
                DsEntryResult reused={};
                CHECK(!ds_entry_run_six_constructors(&stable,&reused,e,sizeof(e)) && reused.stop==DS_ENTRY_BAD_INPUT,
                      "six-constructor checkpoint refuses in-place replay");
                cleanup(stable);
            }
            static constexpr uint32_t bad_tocs[]={0x1923774,0x191BA8C};
            for(unsigned i=0;i<2;++i) {
                DsImage bad={}; if(!load(bad)) return 1;
                CHECK(ds_image_write_be32(&bad,bad_tocs[i],0),"test-only new type getter TOC corruption");
                DsEntryResult rejected={}; char e[256]={};
                CHECK(!ds_entry_run_six_constructors(&bad,&rejected,e,sizeof(e)) &&
                      rejected.stop==DS_ENTRY_ABI_MISMATCH && rejected.constructors_completed==4+i,
                      "bad fifth/sixth getter TOC rejects before that constructor can complete");
                CHECK(!rejected.native_live_after_cleanup && !rejected.native_rwlocks_after_cleanup &&
                      !rejected.native_kernel_mutexes_after_cleanup,"metadata dispatch failure releases all earlier native resources");
                cleanup(bad);
            }
            DsImage unknown={}; if(!load(unknown)) return 1;
            CHECK(ds_image_write_be32(&unknown,0x191BA88,0x130D48),"test-only executable but unselected getter target");
            DsEntryResult rejected={}; char e[256]={};
            CHECK(!ds_entry_run_six_constructors(&unknown,&rejected,e,sizeof(e)) &&
                  rejected.stop==DS_ENTRY_UNSUPPORTED_CALL && rejected.callee==0x130D48 && rejected.constructors_completed==5,
                  "valid executable addresses cannot dispatch into unselected code with flat VM accesses");
            CHECK(!rejected.native_live_after_cleanup && !rejected.native_rwlocks_after_cleanup &&
                  !rejected.native_kernel_mutexes_after_cleanup,"unselected getter boundary cleans native state");
            cleanup(unknown);
        }
        if(fourth) {
            CHECK(!ok && r.stop==DS_ENTRY_EXECUTION_LIMIT && r.constructors_completed==4 &&
                  r.callee==0x130D38 && r.next_constructor_descriptor==0x1923818 && r.function_count==354,
                  "fourth constructor returns through the real CRT walker before the fifth descriptor");
            CHECK(r.allocation_calls==12 && r.kernel_mutex_calls==4 && r.kernel_mutex_handle==0x1004 &&
                  r.lock_calls==14 && r.unlock_calls==14 && !r.bootstrap_fallback_calls,
                  "fourth constructor uses seven actual pool requests and three additional native mutexes");
            CHECK(r.node_address==0x1AD9DB0 && r.node_vector_address==0x1AD9E00,
                  "later method allocations preserve the first constructor's node and vector diagnostics");
            CHECK(image.allocated_bytes==DS_REF_ALLOCATED_BYTES+arena_bytes+0xA000,
                  "method metadata uses existing ELF pool backing without an extra guest arena");
            uint32_t value=0;
            CHECK(ds_image_read_be32(&image,0x1AD9E94,&value) && value==0x1702678 &&
                  ds_image_read_be32(&image,0x1AD9EA4,&value) && value==0x1702688 &&
                  ds_image_read_be32(&image,0x1AD9E9C,&value) && value==10 &&
                  ds_image_read_be32(&image,0x1AD9EAC,&value) && value==10,
                  "original method insertion retains SetChannel/GetChannel names and ten-byte lengths");
            for(unsigned attempt=0;attempt<2;++attempt) {
                DsImage stable={}; if(!load(stable)) return 1;
                DsEntryResult completed={}; char e[256]={};
                CHECK(ds_entry_run_four_constructors(&stable,&completed,e,sizeof(e)),
                      "production four-constructor validator accepts the actual object graph on fresh reload");
                if(e[0]) std::fprintf(stderr,"four-constructor validator: %s\n",e);
                CHECK(completed.stop==DS_ENTRY_FOURTH_CONSTRUCTOR_READY && completed.registry_entries==3 &&
                      completed.callback_entries==12 && !e[0],
                      "validated checkpoint reports three type names and twelve sorted opaque keys");
                DsEntryResult reused={};
                CHECK(!ds_entry_run_four_constructors(&stable,&reused,e,sizeof(e)) && reused.stop==DS_ENTRY_BAD_INPUT,
                      "four-constructor checkpoint refuses replay into an already modified image");
                cleanup(stable);
            }
            // The method-registering virtual call occurs after native handle 0x1002.
            DsImage corrupt_image={}; if(!load(corrupt_image)) return 1;
            CHECK(ds_image_write_be32(&corrupt_image,0x196668C,0),"test-only method-registration TOC corruption");
            DsEntryResult rejected={}; char e[256]={};
            CHECK(!ds_entry_run_four_constructors(&corrupt_image,&rejected,e,sizeof(e)) &&
                  rejected.stop==DS_ENTRY_ABI_MISMATCH && rejected.constructors_completed==3 && rejected.kernel_mutex_calls==2,
                  "corrupt method descriptor rejects after real native creation without completing constructor four");
            CHECK(!rejected.native_live_after_cleanup && !rejected.native_rwlocks_after_cleanup &&
                  !rejected.native_kernel_mutexes_after_cleanup,"partial fourth constructor cleans every native registry");
            cleanup(corrupt_image);
        }
        if(corrupt) {
            CHECK(!ok && r.stop==DS_ENTRY_ABI_MISMATCH && r.constructors_completed==0 && !r.constructor_started,
                  "wrong annotated return PC rejects before any constructor executes");
        } else if(!ahead) {
            CHECK(!ok && r.stop==DS_ENTRY_EXECUTION_LIMIT && r.callee==0x3F4FE0 && r.constructors_completed==2 &&
                  r.function_count==179 && r.allocation_calls==5 && r.kernel_mutex_calls==1,
                  "automatically annotated dispatch reproduces two constructors and enforces its explicit limit");
        }
        if(fourth_fault) {
            CHECK(!ok && r.stop==DS_ENTRY_MEMORY_FAULT && r.memory_fault && r.fault_write && r.fault_address==0 &&
                  r.constructors_completed==3 && r.kernel_mutex_calls==4 && r.kernel_mutex_handle==0x1004,
                  "late fourth-constructor guest fault follows all four real native creations and refuses success");
        }
        if(third) {
            CHECK(!ok && r.stop==DS_ENTRY_EXECUTION_LIMIT && r.callee==0x9E54C8 && r.constructors_completed==3 &&
                  r.function_count==213 && r.allocation_calls==5 && r.kernel_mutex_calls==1 && r.lock_calls==7 && r.unlock_calls==7,
                  "three original constructors return without adding artificial allocations or services");
            static constexpr uint32_t words[][2]={
                {0x1B428A8,0x1B428B0},{0x1B428B0,0x185D6A8},{0x1B428B4,0x1B37D28},{0x1B428B8,0},
                {0x1B428BC,0x1B91190},{0x1B428CC,0x16DBDA0},{0x1B428D0,0x16DBD70},
                {0x1B37D20,0x1B37D28},{0x1B37D28,0x1855548},
                {0x1B92A80,0x1AD6DB0},{0x1AD6DA0,0x1B428B0},{0x1AD6DA4,0x16DBDA0},
                {0x1AD6DA8,0x16DBD70},{0x1AD6DAC,23},
                {0x1B92A64,0x1AD8DE0},
                {0x1AD8DA0,0x1B428D4},{0x1AD8DA4,0x1B428B0},
                {0x1AD8DA8,0x1B428D5},{0x1AD8DAC,0x1B428B0},
                {0x1AD8DB0,0x1B428D6},{0x1AD8DB4,0x1B428B0},
                {0x1AD8DB8,0x1B428D7},{0x1AD8DBC,0x1B428B0},
                {0x1AD8DC0,0x1B4C70C},{0x1AD8DC4,0x1B4C6E8},
                {0x1AD8DC8,0x1B4C70D},{0x1AD8DCC,0x1B4C6E8},
                {0x1AD8DD0,0x1B4C70E},{0x1AD8DD4,0x1B4C6E8},
                {0x1AD8DD8,0x1B4C70F},{0x1AD8DDC,0x1B4C6E8}
            };
            bool equal=true; uint32_t value=0;
            for(const auto &e:words) equal &= ds_image_read_be32(&image,e[0],&value) && value==e[1];
            CHECK(equal,"third constructor publishes its actual parent, name record and eight globally sorted opaque pairs");
            for(unsigned attempt=0;attempt<2;++attempt) {
                DsImage stable={}; if(!load(stable)) return 1;
                DsEntryResult completed={}; char e[256]={};
                CHECK(ds_entry_run_three_constructors(&stable,&completed,e,sizeof(e)),
                      "production three-constructor validator reaches its real checked checkpoint");
                CHECK(completed.stop==DS_ENTRY_THIRD_CONSTRUCTOR_READY && completed.constructors_completed==3 &&
                      completed.registry_entries==2 && completed.callback_entries==8 && !e[0],
                      "checked checkpoint reports three constructors without claiming complete game boot");
                DsEntryResult reused={};
                CHECK(!ds_entry_run_three_constructors(&stable,&reused,e,sizeof(e)) && reused.stop==DS_ENTRY_BAD_INPUT,
                      "production multi-constructor checkpoint rejects replay into modified image");
                cleanup(stable);
            }
            DsImage corrupt_image={}; if(!load(corrupt_image)) return 1;
            CHECK(ds_image_write_be32(&corrupt_image,0x1935924,0),"test-only third-constructor method TOC corruption");
            DsEntryResult rejected={}; char e[256]={};
            CHECK(!ds_entry_run_three_constructors(&corrupt_image,&rejected,e,sizeof(e)) &&
                  rejected.stop==DS_ENTRY_ABI_MISMATCH && rejected.constructors_completed==2,
                  "corrupt third-constructor descriptor cannot fabricate a completed checkpoint");
            CHECK(!rejected.native_live_after_cleanup && !rejected.native_rwlocks_after_cleanup &&
                  !rejected.native_kernel_mutexes_after_cleanup,"failed multi-constructor run cleans every native registry");
            cleanup(corrupt_image);
        }
        CHECK(r.native_live_after_cleanup==0 && r.native_rwlocks_after_cleanup==0 && r.native_kernel_mutexes_after_cleanup==0,
              "exploration frontier cleans native resources without claiming completed boot");
        DsImage reference={};
        if(!ahead && !corrupt && load(reference)) {
            DsEntryResult expected={}; char expected_error[256]={};
            CHECK(ds_entry_run_second_constructor(&reference,&expected,expected_error,sizeof(expected_error)),
                  "legacy audited adapters retain their exact two-constructor checkpoint with annotations enabled");
            bool equal=r.function_count==expected.function_count &&
                std::memcmp(r.functions,expected.functions,expected.function_count*sizeof(uint32_t))==0 &&
                r.lr==expected.lr && r.stack_pointer==expected.stack_pointer && r.r3==expected.r3;
            for(size_t i=0;i<image.segment_count && i<reference.segment_count;++i) {
                const auto &a=image.segments[i]; const auto &b=reference.segments[i];
                if(!a.from_elf) continue;
                equal &= a.guest_address==b.guest_address && a.memory_size==b.memory_size;
                if(a.memory_size!=b.memory_size) continue;
                for(size_t j=0;j<a.memory_size;++j) {
                    const uint64_t address=a.guest_address+j;
                    if(address>=0x1D2B5C8 && address<0x1D2B5D0) continue; // Actual per-run hardware time.
                    equal &= a.data[j]==b.data[j];
                }
            }
            CHECK(equal,"automatic and manually audited dispatch produce identical ELF memory, ABI and function order except real clock time");
            cleanup(reference);
        }
        cleanup(image);
    }
    else if (argc == 3 && std::strncmp(argv[2], "--kernel-", 9) == 0) kernel_mutex_fault(argv[2]+9);
    else if (argc == 3 && (std::strcmp(argv[2], "--first-constructor-probe") == 0 || std::strcmp(argv[2], "--second-constructor-probe") == 0)) {
        DsImage image = {}; DsEntryResult r = {}; char error[256] = {};
        if (!load(image)) return 1;
        const bool second = std::strcmp(argv[2], "--second-constructor-probe") == 0;
        const bool ok = second ? ds_entry_run_second_constructor(&image,&r,error,sizeof(error)) : ds_entry_run_first_constructor(&image,&r,error,sizeof(error));
        std::printf("FIRST ok=%d stop=%d callee=%08x indirect=%08x descriptor=%08x lr=%llx sp=%llx r3=%llx count=%u accesses=%u error=%s\n",ok,r.stop,r.callee,r.indirect_code,r.indirect_descriptor,(unsigned long long)r.lr,(unsigned long long)r.stack_pointer,(unsigned long long)r.r3,r.function_count,r.trace_count,error);
        std::printf("node=%08x vector=%08x second=%08x complete=%d allocations=%u locks=%u/%u heavy=%u handle=%x output=%x attr=%x\n",r.node_address,r.node_vector_address,r.second_singleton_address,r.constructor_completed,r.allocation_calls,r.lock_calls,r.unlock_calls,r.kernel_mutex_calls,r.kernel_mutex_handle,r.kernel_mutex_output,r.kernel_mutex_attribute);
        for(unsigned i=137;i<r.function_count && i<DS_ENTRY_FUNCTION_CAPACITY;++i) std::printf("%u: %08x\n",i,r.functions[i]);
        CHECK(ok,"first constructor diagnostic reaches its selected checkpoint");
        cleanup(image);
    }
    else if (argc == 3 && std::strcmp(argv[2], "--registration-probe") == 0) registration_probe();
    else if (argc == 3 && std::strcmp(argv[2], "--constructor-timeout") == 0) constructor_sync_fault(false);
    else if (argc == 3 && std::strcmp(argv[2], "--constructor-lock-fault") == 0) constructor_sync_fault(true);
    else if (argc == 3 && std::strcmp(argv[2], "--rwlock-output") == 0) singleton_failure("output");
    else if (argc == 3 && std::strcmp(argv[2], "--rwlock-attribute") == 0) singleton_failure("attribute");
    else if (argc == 3 && std::strcmp(argv[2], "--rwlock-native-failure") == 0) singleton_failure("native");
    else if (argc == 3 && std::strcmp(argv[2], "--rwlock-shared") == 0) singleton_failure("shared");
    else if (argc == 3 && std::strcmp(argv[2], "--startup-writer") == 0) startup_fault("writer");
    else if (argc == 3 && std::strcmp(argv[2], "--startup-wait") == 0) startup_fault("wait");
    else if (argc == 3 && std::strcmp(argv[2], "--startup-fallback") == 0) startup_fault("fallback");
    else if (argc == 3 && std::strcmp(argv[2], "--startup-atomic") == 0) startup_fault("atomic");
    else {
        actual_startup();
        initialized_tls();
        crt_startup();
        memory_accounting();
        memory_startup();
        monotonic_clock();
        clock_startup();
        clock_faults();
        native_mutex_pool();
        constructor_startup();
        native_rwlock_pool();
        singleton_startup();
        singleton_dispatch_faults();
        manager_startup();
        manager_faults();
        completed_singleton();
        startup_dispatch_fault();
        initialized_registry();
        registry_dispatch_faults();
        completed_registration();
        registration_dispatch_faults();
        kernel_mutex_pool();
        completed_first_constructor();
        completed_second_constructor();
        readonly_data_fault();
        corrupted_descriptor_fault();
        invalid_inputs_and_budgets();
    }
    std::printf("Entry checkpoint tests: %u checks, %u failures%s\n", checks, failures,
                argc == 3 ? " (deliberately unsupported service fixture)" : " (pinned local lifted bodies)");
    return failures ? 1 : 0;
}
