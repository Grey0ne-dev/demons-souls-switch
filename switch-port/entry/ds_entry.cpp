/* Small, real translated entry checkpoint over checked guest-memory arenas.
 * This is deliberately single-threaded with selectable pre/post TLS-service
 * checkpoints. It does not create a PS3 process, renderer, or scheduler.
 */
#include "ds_entry.h"
#include "ds_reference.h"
#include "ds_clock.h"
#include "ds_sync.h"
#include "ds_rwlock.h"
#include "ds_atomic.h"
#include "ds_kernel_mutex.h"
#include "ds_narrow_reference.h"
#include "ds_wide_reference.h"
#include "ppu_recomp.h"

#include <inttypes.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

void func_000124C0(ppu_context *ctx);
void func_007CF848(ppu_context *ctx);
void func_00A662C0(ppu_context *ctx);
void func_00A664E0(ppu_context *ctx);
void func_00AB5CC0(ppu_context *ctx);
void func_00A662C8(ppu_context *ctx);
void func_00B25E58(ppu_context *ctx);
void func_00B27658(ppu_context *ctx);
void func_007CEC40(ppu_context *ctx);
void func_007CEC48(ppu_context *ctx);
void func_007CEE00(ppu_context *ctx);
void func_007CEE18(ppu_context *ctx);
void func_007CEE30(ppu_context *ctx);
void func_007CEE48(ppu_context *ctx);
void func_00AE42C8(ppu_context *ctx);
void func_00B27730(ppu_context *ctx);
void func_00A8CA28(ppu_context *ctx);
void func_0066D688(ppu_context *ctx);
extern "C" void ds_entry_checkpoint_function(unsigned int address);
extern "C" bool ds_entry_call_lifted(unsigned int code, ppu_context *ctx);
extern "C" void ps3_indirect_call(ppu_context *ctx);

extern "C" {
void (*g_trampoline_fn)(void *) = nullptr;
}

namespace {
constexpr uint32_t kStackBase = 0x0FE00000u;
constexpr size_t kStackSize = 0x100000u;
constexpr uint32_t kStackTop = kStackBase + kStackSize;
constexpr uint32_t kTlsBase = 0x10F00000u;
constexpr size_t kTlsSize = 0x10000u;
constexpr uint32_t kTlsThreadPointer = kTlsBase + 0x7000u;
constexpr uint32_t kTlsNid = 0x744680A2u;
constexpr uint32_t kMainTlsBase = 0x0E000000u;
constexpr size_t kMainTlsSize = 0x9000u;
constexpr uint32_t kArgvBase = 0x10000000u;
constexpr size_t kArgvSize = 0x1000u;
constexpr char kBootPath[] = "/dev_bdvd/PS3_GAME/USRDIR/EBOOT.BIN";
constexpr uint32_t kMutexCreateNid = 0x2F85C0EFu;
constexpr uint32_t kTimeNid = 0x8461E528u;

/* Global storage is intentional. A longjmp leaves modified non-volatile local
 * automatic objects indeterminate. These objects and the jump buffer instead
 * live for the entire single-threaded invocation; no C++ destructors are crossed.
 * DS_FRAME uses the generated plain trace hook, not the desktop RAII shadow stack.
 */
bool busy;
bool initialize_tls_mode;
bool continue_crt_mode;
bool execute_memory_mode;
bool execute_clock_mode;
bool execute_constructor_mode;
bool execute_singleton_mode;
bool execute_manager_mode;
bool execute_allocator_mode;
bool execute_registry_mode;
bool execute_registration_mode;
bool complete_constructor_mode;
bool complete_second_constructor_mode;
bool explore_startup_mode;
bool runtime_heap_mode;
bool runtime_allocation_mode;
bool locale_object_mode;
bool locale_publish_mode;
bool stream_locale_mode;
bool first_facet_mode;
bool first_stream_mode;
bool narrow_streams_mode;
bool wide_streams_mode;
constexpr uint32_t kRuntimeSizes[]={40,4,52,40,4,4,24,8,4,40,4,4,4,40,4,40,4,40,4,4,32,8,4,4,4};
constexpr uint32_t kRuntimeCallers[]={0x16A051C,0x1686DFC,0x16892DC,0x16A051C,0x1686EF4,0x1685FEC,0x1688380,0x16884F0,0x1685FEC,0x16A051C,0x168704C,0x1685FEC,0x1685FEC,0x16A051C,0x168BD6C,0x16A051C,0x168BE38,0x16A051C,0x168BEFC,0x1685FEC,0x168E790,0x16884F0,0x1685FEC,0x1685FEC,0x1685FEC};
constexpr uint32_t kRuntimeStacks[]={0xFEFFB60,0xFEFFBF0,0xFEFFAA0,0xFEFFB60,0xFEFFBF0,0xFEFFA00,0xFEFFA90,0xFEFFA10,0xFEFFA00,0xFEFFB60,0xFEFFBF0,0xFEFFA00,0xFEFFB60,0xFEFFC10,0xFEFFCA0,0xFEFFC10,0xFEFFCA0,0xFEFFC10,0xFEFFCA0,0xFEFFAB0,0xFEFFB40,0xFEFFAC0,0xFEFFAB0,0xFEFFAB0,0xFEFFAB0};
DsHeap runtime_heap;
uint32_t pending_indirect_return_pc;
uint32_t exploration_constructor_limit;
jmp_buf checkpoint;
DsImage *active_image;
DsEntryResult *active_result;
char *active_error;
size_t active_error_capacity;
ppu_context context;

void snapshot()
{
    active_result->r3 = context.gpr[3];
    active_result->r4 = context.gpr[4];
    active_result->r5 = context.gpr[5];
    active_result->r6 = context.gpr[6];
    active_result->lr = context.lr;
    active_result->stack_pointer = context.gpr[1];
    active_result->thread_pointer = context.gpr[13];
    active_result->toc = context.gpr[2];
}

[[noreturn]] void stop(DsEntryStop reason, uint32_t callee)
{
    active_result->stop = reason;
    active_result->callee = callee;
    snapshot();
    longjmp(checkpoint, 1);
}

void access_trace(uint64_t address, uint32_t size, bool write, uint64_t value)
{
    ++active_result->trace_count;
    if (active_result->trace_count > 4000000) {
        if (active_error && active_error_capacity)
            snprintf(active_error, active_error_capacity, "Startup exceeded bounded guest access count");
        stop(DS_ENTRY_EXECUTION_LIMIT, active_result->callee);
    }
    if (active_result->trace_stored < 32) {
        DsEntryAccess &access = active_result->trace[active_result->trace_stored++];
        access.address = address;
        access.size = size;
        access.write = write;
        access.value = value;
    }
}

[[noreturn]] void memory_fault(uint64_t address, uint32_t size, bool write)
{
    active_result->memory_fault = true;
    active_result->fault_address = address;
    active_result->fault_size = size;
    active_result->fault_write = write;
    active_result->fault_required_flags = write ? DS_PF_W : DS_PF_R;
    if (active_error && active_error_capacity)
        snprintf(active_error, active_error_capacity,
                 "Checked guest %s%u fault at 0x%016" PRIx64 " in function 0x%08" PRIx32,
                 write ? "write" : "read", size * 8u, address, active_result->callee);
    stop(DS_ENTRY_MEMORY_FAULT, active_result->callee);
}

unsigned char *checked_address(uint64_t address, uint32_t size, bool write)
{
    void *host = address <= UINT32_MAX
                     ? ds_image_address(active_image, static_cast<uint32_t>(address), size,
                                        write ? DS_PF_W : DS_PF_R)
                     : nullptr;
    if (!host) memory_fault(address, size, write);
    return static_cast<unsigned char *>(host);
}

uint64_t read_value(uint64_t address, uint32_t size)
{
    /* Trace failures too; do not manufacture zeros for guest-null accesses. */
    if (address > UINT32_MAX ||
        !ds_image_address(active_image, static_cast<uint32_t>(address), size, DS_PF_R)) {
        access_trace(address, size, false, 0);
        memory_fault(address, size, false);
    }
    const unsigned char *host = checked_address(address, size, false);
    uint64_t value = 0;
    for (uint32_t i = 0; i < size; ++i) value = (value << 8) | host[i];
    access_trace(address, size, false, value);
    return value;
}

void write_value(uint64_t address, uint32_t size, uint64_t value)
{
    access_trace(address, size, true, value);
    unsigned char *host = checked_address(address, size, true);
    for (uint32_t i = 0; i < size; ++i)
        host[i] = static_cast<unsigned char>(value >> ((size - i - 1) * 8));
}

bool prepare_region(DsImage *image, uint32_t address, size_t size,
                    char *error, size_t error_capacity)
{
    /* Repeated checkpoint runs can reuse only a region this adapter owns by
     * exact address/size and anonymous metadata, never an overlapping ELF region.
     */
    for (size_t i = 0; i < image->segment_count; ++i) {
        DsSegment &region = image->segments[i];
        if (!region.from_elf && region.guest_address == address &&
            region.memory_size == size && (region.flags & (DS_PF_R | DS_PF_W)) == (DS_PF_R | DS_PF_W)) {
            memset(region.data, 0, size);
            return true;
        }
    }
    return ds_image_add_zero_region(image, address, size, DS_PF_R | DS_PF_W,
                                    error, error_capacity);
}

bool clear_active()
{
    const bool heap_ok=!runtime_heap.image || ds_heap_validate(&runtime_heap);
    active_result->runtime_heap=runtime_heap.stats;
    ds_heap_detach(&runtime_heap);
    active_result->runtime_heap_metadata_after_cleanup=runtime_heap.image!=nullptr;
    active_result->native_held_at_stop = ds_sync_held();
    active_result->native_rwlocks_held_at_stop = ds_rwlock_held();
    const bool sync_ok = ds_sync_reset();
    const bool rw_ok = ds_rwlock_reset();
    const bool kernel_ok = ds_kernel_mutex_reset();
    active_result->native_live_after_cleanup = ds_sync_live();
    active_result->native_rwlocks_after_cleanup = ds_rwlock_live();
    active_result->native_kernel_mutexes_after_cleanup = ds_kernel_mutex_live();
    if (!sync_ok || !rw_ok || !kernel_ok || !heap_ok) {
        active_result->stop = DS_ENTRY_SYNC_FAILURE;
        if (active_error && active_error_capacity)
            snprintf(active_error, active_error_capacity, "Native or heap metadata cleanup failed");
    }
    active_image = nullptr;
    active_result = nullptr;
    active_error = nullptr;
    active_error_capacity = 0;
    g_trampoline_fn = nullptr;
    pending_indirect_return_pc = 0;
    busy = false;
    return sync_ok && rw_ok && kernel_ok && heap_ok;
}
} // namespace

extern "C" uint8_t vm_read8(uint64_t address) { return static_cast<uint8_t>(read_value(address, 1)); }
extern "C" uint16_t vm_read16(uint64_t address) { return static_cast<uint16_t>(read_value(address, 2)); }
extern "C" uint32_t vm_read32(uint64_t address) { return static_cast<uint32_t>(read_value(address, 4)); }
extern "C" uint64_t vm_read64(uint64_t address) { return read_value(address, 8); }
extern "C" void vm_write8(uint64_t address, uint8_t value) { write_value(address, 1, value); }
extern "C" void vm_write16(uint64_t address, uint16_t value) { write_value(address, 2, value); }
extern "C" void vm_write32(uint64_t address, uint32_t value) { write_value(address, 4, value); }
extern "C" void vm_write64(uint64_t address, uint64_t value) { write_value(address, 8, value); }

extern "C" void ds_entry_trace_function(unsigned int address)
{
    if (address==0x168BC68 && !wide_streams_mode) ds_entry_checkpoint_function(address);
    if (address==0x1676088 && !locale_publish_mode)
        ds_entry_checkpoint_function(address);
    if (address==0x16896C0 && !locale_object_mode)
        ds_entry_checkpoint_function(address);
    if (address==0x168ABD0) {
        if (!runtime_allocation_mode) ds_entry_checkpoint_function(address);
        const uint32_t call=runtime_heap.stats.allocation_calls;
        if ((locale_publish_mode && !stream_locale_mode && call==3) || (stream_locale_mode && !first_facet_mode && call==6))
            ds_entry_checkpoint_function(address);
        if (first_stream_mode && !narrow_streams_mode && call==8) ds_entry_checkpoint_function(address);
        const uint32_t limit=wide_streams_mode?25u:narrow_streams_mode?13u:first_facet_mode?8u:stream_locale_mode?6u:locale_object_mode?3u:2u;
        if (call>=limit || context.gpr[3]!=kRuntimeSizes[call] ||
            context.lr!=kRuntimeCallers[call] || context.gpr[1]!=kRuntimeStacks[call] || context.gpr[2]!=0x19D3BAC)
            stop(DS_ENTRY_ABI_MISMATCH,address);
    }
    if (!(explore_startup_mode && active_result->constructors_completed)) {
    if (address == 0x00AE4D38 && execute_allocator_mode)
        ds_entry_checkpoint_function(address);
    if (address == 0x00AE4490 && execute_registry_mode)
        ds_entry_checkpoint_function(address);
    if (address == 0x00782468 && execute_registration_mode)
        ds_entry_checkpoint_function(address);
    if (address == 0x00AE7768 && execute_constructor_mode && !execute_singleton_mode)
        ds_entry_checkpoint_function(address);
    if (address == 0x00AE7768 && execute_singleton_mode) {
        active_result->singleton_started = true;
        if (complete_constructor_mode && active_result->singleton_address && context.gpr[3] != active_result->singleton_address)
            active_result->second_singleton_address = static_cast<uint32_t>(context.gpr[3]);
        else active_result->singleton_address = static_cast<uint32_t>(context.gpr[3]);
    }
    if (address == 0x00B25CB8 && execute_singleton_mode)
        active_result->manager_address = static_cast<uint32_t>(context.gpr[3]);
    if (address == 0x00B25B58 && execute_manager_mode) ds_entry_checkpoint_function(address);
    if (address == 0x00967AA0 && execute_allocator_mode) {
        static constexpr uint32_t state[][2] = {
            {0x1B91150,16}, {0x1B91154,5}, {0x1B91144,0x1AD6B78}, {0x1B91148,0x1AD6BF0},
            {0x1B9112C,0x1AD6BF0}, {0x1B91140,0x1B36B68}, {0x1B9115C,0x1B36B78},
            {0x1AD6BF0,0x1B91128}, {0x1AD6BF4,0x1B36B68}
        };
        for (const auto &entry : state) {
            uint32_t value = 0;
            if (!ds_image_read_be32(active_image, entry[0], &value) || value != entry[1])
                stop(DS_ENTRY_ABI_MISMATCH, address);
        }
        active_result->pool_initialized = true;
    }
    if (address == 0x00B26C18 && execute_allocator_mode) {
        if (!active_result->pool_initialized || context.gpr[3] != 0x1B91128 ||
            context.gpr[4] != 0x180 || context.gpr[5] != 4)
            stop(DS_ENTRY_ABI_MISMATCH, address);
        ++active_result->allocation_calls;
        active_result->allocation_size = static_cast<uint32_t>(context.gpr[4]);
    }
    if (address == 0x00B27658 && execute_registry_mode) {
        const bool node_vector = complete_constructor_mode && active_result->allocation_calls == 4;
        const bool callback = execute_registration_mode && active_result->allocation_calls == 2;
        if (!active_result->singleton_completed || context.gpr[3] != 0x1B91190 ||
            context.gpr[4] != (node_vector ? 0x10u : callback ? 0x1000u : 0x2000u) || context.gpr[5] != 4 ||
            (!callback && !node_vector && active_result->allocation_calls != 1))
            stop(DS_ENTRY_ABI_MISMATCH, address);
        ++active_result->allocation_calls;
        if (node_vector) { /* The later allocation is reported separately at return. */ }
        else if (callback) active_result->callback_allocation_size = static_cast<uint32_t>(context.gpr[4]);
        else active_result->registry_allocation_size = static_cast<uint32_t>(context.gpr[4]);
    }
    if (address == 0x00B27730 && complete_constructor_mode) {
        if (context.gpr[3] != 0x1B91190 || context.gpr[4] != 0x38 || active_result->allocation_calls != 3)
            stop(DS_ENTRY_ABI_MISMATCH, address);
        ++active_result->allocation_calls;
    }
    } else if (address == 0xB27658 || address == 0xB27730 || address == 0xB26C18) {
        ++active_result->allocation_calls;
    }
    active_result->callee = address;
    if (active_result->function_count < DS_ENTRY_FUNCTION_CAPACITY)
        active_result->functions[active_result->function_count] = address;
    ++active_result->function_count;
    if (active_result->function_count > DS_ENTRY_FUNCTION_CAPACITY) {
        if (active_error && active_error_capacity)
            snprintf(active_error, active_error_capacity, "Entry exceeded its bounded function trace");
        stop(DS_ENTRY_UNSUPPORTED_CALL, address);
    }
}

extern "C" void ds_entry_unsupported(unsigned int address)
{
    if (active_error && active_error_capacity)
        snprintf(active_error, active_error_capacity, "Unimplemented later game function 0x%08X reached", address);
    stop(DS_ENTRY_UNSUPPORTED_CALL, address);
}

extern "C" void ds_entry_checkpoint_function(unsigned int address)
{
    if (narrow_streams_mode && !wide_streams_mode && address==0x168BC68) {
        if (context.lr!=0x15165E8 || context.gpr[1]!=0xFEFFD90 || context.gpr[2]!=0x19D3BAC ||
            runtime_heap.stats.allocation_calls!=13 || runtime_heap.stats.live_allocations!=13 || ds_sync_held())
            stop(DS_ENTRY_ABI_MISMATCH,address);
        stop(DS_ENTRY_EXECUTION_LIMIT,address);
    }

    if (first_stream_mode && address==0x168ABD0) {
        if (context.gpr[3]!=4 || context.gpr[4]!=0x1CE6558 || context.gpr[5]!=1 ||
            context.gpr[6]!=UINT64_C(0x7F7F7F7F7F7F7F7F) || context.lr!=0x1685FEC ||
            context.gpr[1]!=0xFEFFA00 || context.gpr[2]!=0x19D3BAC ||
            runtime_heap.stats.allocation_calls!=8 || runtime_heap.stats.live_allocations!=8 || ds_sync_held())
            stop(DS_ENTRY_ABI_MISMATCH,address);
        stop(DS_ENTRY_EXECUTION_LIMIT,address);
    }
    if (stream_locale_mode && address==0x168ABD0) {
        if (context.gpr[3]!=24 || context.gpr[4]!=1 || context.gpr[5]!=0 || context.gpr[6]!=0x1CE6564 ||
            context.lr!=0x1688380 || context.gpr[1]!=0xFEFFA90 || context.gpr[2]!=0x19D3BAC ||
            runtime_heap.stats.allocation_calls!=6 || runtime_heap.stats.live_allocations!=6 || ds_sync_held()!=1)
            stop(DS_ENTRY_ABI_MISMATCH,address);
        stop(DS_ENTRY_EXECUTION_LIMIT,address);
    }
    if (locale_publish_mode && address==0x168ABD0) {
        if (context.gpr[3]!=40 || context.lr!=0x16A051C ||
            context.gpr[1]!=0xFEFFB60 || context.gpr[2]!=0x19D3BAC ||
            context.gpr[4]!=0x1AD58D0 || context.gpr[5]!=0x1CE6510 || context.gpr[6]!=0x1CE6500 ||
            runtime_heap.stats.allocation_calls!=3 || runtime_heap.stats.live_allocations!=3 || ds_sync_held())
            stop(DS_ENTRY_ABI_MISMATCH,address);
        stop(DS_ENTRY_EXECUTION_LIMIT,address);
    }
    if (locale_object_mode && address==0x1676088) {
        if (context.gpr[3]!=DS_HEAP_BASE+88 || context.lr!=0x16891DC ||
            context.gpr[1]!=0xFEFFA30 || context.gpr[2]!=0x19D3BAC ||
            context.gpr[4]!=0x17041B8 || context.gpr[5]!=0 || context.gpr[6]!=0x1AD8EE0 ||
            runtime_heap.stats.live_allocations!=3 || ds_sync_held()!=1)
            stop(DS_ENTRY_ABI_MISMATCH,address);
        stop(DS_ENTRY_EXECUTION_LIMIT,address);
    }
    if (runtime_allocation_mode && address==0x16896C0) {
        if (context.gpr[3]!=DS_HEAP_BASE+48 || context.lr!=0x1686E14 || context.gpr[1]!=0xFEFFBF0 ||
            context.gpr[2]!=0x19D3BAC || runtime_heap.stats.live_allocations!=2)
            stop(DS_ENTRY_ABI_MISMATCH,address);
        stop(DS_ENTRY_EXECUTION_LIMIT,address);
    }
    if (runtime_heap_mode && address == 0x0168ABD0) {
        if (active_result->constructors_completed != 12 || context.gpr[3] != 40 ||
            context.gpr[2] != 0x19D3BAC || context.lr != 0x16A051C ||
            context.gpr[1] != 0xFEFFB60)
            stop(DS_ENTRY_ABI_MISMATCH,address);
        stop(DS_ENTRY_EXECUTION_LIMIT,address);
    }
    if (execute_manager_mode && address == 0x00B25B58 && active_result->rwlock_create_calls == 1) {
        active_result->pool_request_base = static_cast<uint32_t>(context.gpr[4]);
        active_result->pool_request_size = static_cast<uint32_t>(context.gpr[5]);
        uint32_t published = 0;
        active_result->manager_published = ds_image_read_be32(active_image, 0x1B9111C, &published) &&
                                           published == active_result->manager_address;
        checked_address(context.gpr[4], 0x60000, true);
        if (!ds_image_address(active_image, static_cast<uint32_t>(context.gpr[4]), 0x60000, DS_PF_R))
            memory_fault(context.gpr[4], 0x60000, false);
        if (context.gpr[3] != active_result->manager_address || context.gpr[4] != 0x1AD6B78 ||
            context.gpr[5] != 0x60000 || !active_result->manager_published ||
            active_result->lock_calls != 2 || active_result->unlock_calls != 2 || ds_sync_held() != 0)
            stop(DS_ENTRY_ABI_MISMATCH, address);
        if (!execute_allocator_mode) stop(DS_ENTRY_MANAGER_POOL_BOUNDARY, address);
        return;
    }
    if (execute_allocator_mode && address == 0x00AE4D38) {
        uint32_t singleton = 0, interface = 0;
        active_result->singleton_completed = ds_image_read_be32(active_image, 0x1B4C6E0, &singleton) &&
            singleton == 0x1B4C6E8 && ds_image_read_be32(active_image, singleton + 12, &interface) && interface == 0x1B91190;
        if (!execute_registry_mode) stop(DS_ENTRY_SINGLETON_READY, address);
        if (!active_result->singleton_completed || context.gpr[3] != singleton ||
            context.lr != 0x7CF75C || context.gpr[1] != 0xFEFFD40 ||
            active_result->lock_calls != 3 || active_result->unlock_calls != 3 || ds_sync_held())
            stop(DS_ENTRY_ABI_MISMATCH, address);
        return;
    }
    if (execute_registry_mode && address == 0x00AE4490) {
        uint32_t begin = 0, end = 0;
        active_result->registry_address = 0x1B92A78;
        if (!ds_image_read_be32(active_image, 0x1B92A7C, &begin) ||
            !ds_image_read_be32(active_image, 0x1B92A80, &end) || end < begin || (end - begin) % 16)
            stop(DS_ENTRY_ABI_MISMATCH, address);
        active_result->registry_payload = begin;
        active_result->registry_entries = (end - begin) / 16;
        active_result->registry_initialized = true;
        if (!execute_registration_mode) stop(DS_ENTRY_REGISTRY_READY, address);
        if (active_result->registry_payload != 0x1AD6D90 || active_result->registry_entries != 1 ||
            context.lr != 0xAE50E0 || context.gpr[1] != 0xFEFFC30 ||
            active_result->lock_calls != 4 || active_result->unlock_calls != 4 || ds_sync_held())
            stop(DS_ENTRY_ABI_MISMATCH, address);
        return;
    }
    if (execute_registration_mode && address == 0x00782468) {
        uint32_t begin = 0, end = 0;
        active_result->callback_registry_address = 0x1B92A5C;
        if (!ds_image_read_be32(active_image, 0x1B92A60, &begin) ||
            !ds_image_read_be32(active_image, 0x1B92A64, &end) || end < begin || (end - begin) % 8)
            stop(DS_ENTRY_ABI_MISMATCH, address);
        active_result->callback_payload = begin;
        active_result->callback_entries = (end - begin) / 8;
        active_result->callbacks_initialized = true;
        active_result->registration_completed = true;
        if (!complete_constructor_mode) stop(DS_ENTRY_REGISTRATION_READY, address);
        if (context.lr != 0x7CF770 || context.gpr[1] != 0xFEFFD40 || active_result->callback_entries != 4)
            stop(DS_ENTRY_ABI_MISMATCH, address);
        return;
    }
    if (execute_constructor_mode && address == 0x00AE7768 && active_result->constructor_started &&
        active_result->thread_id_calls == 1 && active_result->lock_calls == 1 &&
        active_result->unlock_calls == 1 && active_result->mutex_count == 11)
        stop(DS_ENTRY_SINGLETON_BOUNDARY, address);
    ds_entry_unsupported(address);
}

extern "C" void ds_malloc(ppu_context *ctx)
{
    if (active_result->callee==0x167AC88 && runtime_allocation_mode) {
        const uint32_t call=runtime_heap.stats.allocation_calls;
        const bool first=call==0;
        if (ctx!=&context || ctx->gpr[2]!=0x19D3BAC || ctx->lr!=0x168AC18 ||
            call>=(wide_streams_mode?25u:narrow_streams_mode?13u:first_facet_mode?8u:stream_locale_mode?6u:locale_object_mode?3u:2u) || ctx->gpr[3]!=kRuntimeSizes[call])
            stop(DS_ENTRY_ABI_MISMATCH,active_result->callee);
        const uint64_t size=ctx->gpr[3];uint32_t guest=0;
        active_result->runtime_heap_status=ds_heap_alloc(&runtime_heap,size,16,&guest);
        ctx->gpr[3]=guest;
        if (active_result->runtime_heap_status!=DS_HEAP_OK) {
            if (active_error && active_error_capacity)
                snprintf(active_error,active_error_capacity,"Runtime guest heap allocation failed (status=%u)",
                         static_cast<unsigned>(active_result->runtime_heap_status));
            stop(DS_ENTRY_HEAP_FAILURE,active_result->callee);
        }
        if (first) {
            active_result->runtime_malloc_address=guest;
            active_result->runtime_malloc_size=static_cast<uint32_t>(size);
        }
        active_result->runtime_last_malloc_address=guest;
        active_result->runtime_last_malloc_size=static_cast<uint32_t>(size);
        return;
    }
    (void)ctx;
    ++active_result->bootstrap_fallback_calls;
    if (active_error && active_error_capacity)
        snprintf(active_error, active_error_capacity, "Unsupported preexisting NULL-pool bootstrap allocator fallback");
    stop(DS_ENTRY_UNSUPPORTED_CALL, active_result->callee);
}

extern "C" int ppu_stwcx32(uint64_t address, uint32_t expected, uint32_t desired)
{
    if (address & 3u) stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
    checked_address(address, 4, false);
    checked_address(address, 4, true);
    bool exchanged = false;
    if (!ds_image_compare_exchange_be32(active_image, address, expected, desired, &exchanged))
        stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
    ++active_result->atomic_attempts;
    if (exchanged) {
        ++active_result->atomic_successes;
        access_trace(address, 4, true, desired);
    } else {
        (void)vm_read32(address);
    }
    return exchanged ? 1 : 0;
}

extern "C" void ps3_hle_call(unsigned int nid, ppu_context *ctx)
{
    active_result->boundary_nid = nid;
    if (execute_constructor_mode && ctx == &context &&
        (nid == 0x350D454Eu || nid == 0x1573DC3Fu || nid == 0x1BC200F4u)) {
        if (ctx->thread_id != 1 || !active_result->constructor_started)
            stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
        checked_address(ctx->gpr[1] + 0x28, 8, true);
        if (nid == 0x350D454Eu) {
            checked_address(ctx->gpr[3], 8, true);
            vm_write64(ctx->gpr[3], ctx->thread_id);
            ++active_result->thread_id_calls;
        } else {
            uint32_t slot = 0;
            for (; slot < active_result->mutex_count; ++slot)
                if (active_result->mutex_addresses[slot] == ctx->gpr[3]) break;
            if (slot == active_result->mutex_count || ctx->gpr[3] > UINT32_MAX)
                stop(DS_ENTRY_SYNC_FAILURE, active_result->callee);
            checked_address(ctx->gpr[3], 24, true);
            const uint32_t owner = vm_read32(ctx->gpr[3]);
            const uint32_t recursion = vm_read32(ctx->gpr[3] + 12);
            bool ok;
            if (nid == 0x1573DC3Fu) {
                ok = ctx->gpr[4] == 0 && owner == 0 && recursion == 0 && ds_sync_acquire(slot, 1);
                if (ok) {
                    vm_write32(ctx->gpr[3], 1);
                    vm_write32(ctx->gpr[3] + 12, 1);
                    active_result->last_locked_mutex = static_cast<uint32_t>(ctx->gpr[3]);
                    ++active_result->lock_calls;
                }
            } else {
                ok = owner == 1 && recursion == 1 && ds_sync_release(slot, 1);
                if (ok) {
                    vm_write32(ctx->gpr[3], 0);
                    vm_write32(ctx->gpr[3] + 12, 0);
                    ++active_result->unlock_calls;
                }
            }
            if (!ok) {
                if (active_error && active_error_capacity)
                    snprintf(active_error, active_error_capacity, "Unsupported mutex ownership, contention or wait operation");
                stop(DS_ENTRY_SYNC_FAILURE, active_result->callee);
            }
        }
        vm_write64(ctx->gpr[1] + 0x28, ctx->gpr[2]);
        ctx->gpr[3] = 0;
        return;
    }
    if (execute_memory_mode && ctx == &context && nid == kTimeNid &&
        active_result->memory_query_count == 1 && active_result->mutex_count == 10) {
        if (execute_clock_mode) {
            if (ctx->lr != 0x12708 || active_result->clock_calls != 0)
                stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
            DsClockSample sample;
            if (!ds_clock_now(&sample)) {
                if (active_error && active_error_capacity)
                    snprintf(active_error, active_error_capacity, "Monotonic clock unavailable or outside supported range");
                stop(DS_ENTRY_CLOCK_FAILURE, active_result->callee);
            }
            vm_write64(ctx->gpr[1] + 0x28, ctx->gpr[2]);
            active_result->clock_ticks = sample.ticks;
            active_result->clock_frequency = sample.frequency;
            active_result->clock_microseconds = sample.microseconds;
            ++active_result->clock_calls;
            ctx->gpr[3] = sample.microseconds;
            return;
        }
        stop(DS_ENTRY_TIME_BOUNDARY, active_result->callee);
    }
    if (continue_crt_mode && ctx == &context && nid == kMutexCreateNid) {
        const bool allowed_lr = ctx->lr == 0x0167AA0Cu ||
            (execute_memory_mode && (ctx->lr == 0x01676380u || ctx->lr == 0x01676288u)) ||
            (execute_constructor_mode && ctx->lr == 0x00A66828u);
        if (!active_result->tls_initialized || active_result->mutex_count >= DS_CRT_MUTEX_CAPACITY ||
            (!execute_memory_mode && active_result->mutex_initialized) ||
            !allowed_lr || ctx->gpr[3] > UINT32_MAX || ctx->gpr[4] > UINT32_MAX)
            stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
        for (uint32_t i = 0; i < active_result->mutex_count; ++i)
            if (active_result->mutex_addresses[i] == ctx->gpr[3])
                stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
        /* This bounded slice creates unlocked guest records. No lock or
         * wait operation is accepted until real synchronization is implemented.
         * Validate both spans before changing any mutex bytes.
         */
        checked_address(ctx->gpr[3], 24, true);
        checked_address(ctx->gpr[4], 16, false);
        const uint32_t protocol = vm_read32(ctx->gpr[4]);
        if (execute_constructor_mode && !ds_sync_create(active_result->mutex_count))
            stop(DS_ENTRY_SYNC_FAILURE, active_result->callee);
        for (uint32_t i = 0; i < 24; i += 4)
            vm_write32(ctx->gpr[3] + i, i == 8 ? protocol : 0);
        active_result->mutex_initialized = true;
        active_result->mutex_address = static_cast<uint32_t>(ctx->gpr[3]);
        active_result->mutex_attribute = static_cast<uint32_t>(ctx->gpr[4]);
        active_result->mutex_addresses[active_result->mutex_count++] = active_result->mutex_address;
        vm_write64(ctx->gpr[1] + 0x28, ctx->gpr[2]);
        ctx->gpr[3] = 0;
        return;
    }
    if (ctx != &context || nid != kTlsNid) {
        if (active_error && active_error_capacity)
            snprintf(active_error, active_error_capacity, "Unsupported firmware boundary 0x%08X", nid);
        stop(DS_ENTRY_UNSUPPORTED_CALL, active_result->callee);
    }
    if (initialize_tls_mode) {
        /* Match the pinned desktop HLE's static-TLS layout and CELL_OK ABI,
         * but explicitly back and bounds-check every guest byte. This slice
         * accepts only the verified main-thread invocation, not arbitrary TLS.
         */
        if (ctx->gpr[3] != 1 || ctx->gpr[4] != active_image->tls_address ||
            ctx->gpr[5] != active_image->tls_file_size ||
            ctx->gpr[6] != active_image->tls_memory_size || ctx->lr != 0x12648u) {
            stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
        }
        if (!prepare_region(active_image, kMainTlsBase, kMainTlsSize,
                            active_error, active_error_capacity))
            stop(DS_ENTRY_BAD_INPUT, active_result->callee);
        for (uint32_t i = 0; i < active_image->tls_memory_size; ++i) {
            uint8_t value = i < active_image->tls_file_size
                ? vm_read8(active_image->tls_address + i) : 0;
            vm_write8(kMainTlsBase + i, value);
        }
        ctx->gpr[13] = kMainTlsBase + 0x7000u;
        ctx->gpr[3] = 0;
        active_result->tls_block = kMainTlsBase;
        active_result->tls_initialized = true;
        if (continue_crt_mode) {
            /* The glink import stub normally writes std r2,40(r1). The
             * unmodified lifted HLE stub omits it; match the pinned HLE
             * dispatcher's explicit restoration of this caller ABI slot.
             */
            vm_write64(ctx->gpr[1] + 0x28, ctx->gpr[2]);
            return;
        }
        stop(DS_ENTRY_TLS_INITIALIZED, active_result->callee);
    }
    stop(DS_ENTRY_TLS_BOUNDARY, active_result->callee);
}

extern "C" void ps3_indirect_call(ppu_context *ctx)
{
    if (explore_startup_mode && ctx == &context) {
        const uint32_t return_pc = pending_indirect_return_pc;
        pending_indirect_return_pc = 0;
        /* Constructor thirteen belongs to the original runtime TOC. Its exact
         * OPD/code/TOC permits either the twelfth checkpoint's stop or the
         * reviewed runtime prefix, without arbitrary non-main-TOC dispatch. */
        const bool runtime_constructor = return_pc == 0x16B84F4 &&
            active_result->constructors_completed == 12 && ctx->gpr[11] == 0x19A3EA0 &&
            ctx->ctr == 0x15166F0;
        const uint32_t expected_toc = runtime_constructor ? 0x19D3BACu : active_image->toc;
        if (!return_pc || ctx->ctr > UINT32_MAX || ctx->gpr[2] != expected_toc)
            stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
        uint32_t instruction = 0;
        if (!ds_image_address(active_image, return_pc - 4, 4, DS_PF_X) ||
            !ds_image_read_be32(active_image, return_pc - 4, &instruction) || instruction != 0x4E800421 ||
            !ds_image_address(active_image, static_cast<uint32_t>(ctx->ctr), 4, DS_PF_X))
            stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
        uint32_t descriptor = 0;
        static constexpr unsigned descriptor_registers[] = {9,10,11,29};
        for (unsigned reg : descriptor_registers) {
            if (ctx->gpr[reg] > UINT32_MAX) continue;
            const uint32_t address = static_cast<uint32_t>(ctx->gpr[reg]);
            uint32_t code = 0, toc = 0;
            if (address <= UINT32_MAX - 4 && ds_image_read_be32(active_image, address, &code) &&
                ds_image_read_be32(active_image, address + 4, &toc) && code == ctx->ctr && toc == expected_toc) {
                descriptor = address;
                break;
            }
        }
        if (!descriptor) stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
        active_result->indirect_descriptor = descriptor;
        active_result->indirect_code = static_cast<uint32_t>(ctx->ctr);
        const bool constructor = return_pc == 0x16B84F4;
        if (constructor) {
            if (!active_result->constructors_completed) {
                active_result->constructor_started = true;
                active_result->constructor_descriptor = descriptor;
                active_result->constructor_target = static_cast<uint32_t>(ctx->ctr);
                active_result->constructor_toc = static_cast<uint32_t>(ctx->gpr[2]);
            } else {
                active_result->next_constructor_descriptor = descriptor;
                active_result->next_constructor_target = static_cast<uint32_t>(ctx->ctr);
            }
            if (exploration_constructor_limit && active_result->constructors_completed >= exploration_constructor_limit) {
                if (active_error && active_error_capacity)
                    snprintf(active_error, active_error_capacity, "Reached exploration constructor limit (%u)", exploration_constructor_limit);
                stop(DS_ENTRY_EXECUTION_LIMIT, static_cast<uint32_t>(ctx->ctr));
            }
        }
        if (ctx->ctr == 0xA662C8 && ctx->gpr[29] <= UINT32_MAX) {
            const uint32_t result = static_cast<uint32_t>(ctx->gpr[29]);
            if (return_pc == 0xB26CC4) active_result->allocation_address = result;
            if (return_pc == 0xB277D4 && active_result->allocation_calls == 4)
                active_result->node_address = result;
            if (return_pc == 0xB27708) {
                if (active_result->allocation_calls == 2) active_result->registry_allocation_address = result;
                if (active_result->allocation_calls == 3) active_result->callback_allocation_address = result;
                if (active_result->allocation_calls == 5) active_result->node_vector_address = result;
            }
        }
        const uint32_t target = static_cast<uint32_t>(ctx->ctr);
        ctx->lr = return_pc;
        if (first_facet_mode && return_pc==0x1688280) {
            if (descriptor!=0x1970348 || target!=0x9E96A0 || ctx->gpr[3]!=DS_HEAP_BASE+208 ||
                ctx->gpr[4]!=32 || ctx->gpr[1]!=0xFEFFA90 || (runtime_heap.stats.allocation_calls!=8 && (!narrow_streams_mode ||
                 (runtime_heap.stats.allocation_calls!=9 && runtime_heap.stats.allocation_calls!=12))) || ds_sync_held())
                stop(DS_ENTRY_ABI_MISMATCH,target);
            if (!first_stream_mode) stop(DS_ENTRY_EXECUTION_LIMIT,target);
        }
        if (narrow_streams_mode && return_pc==0x16872A0) {
            if (descriptor!=0x1970348 || target!=0x9E96A0 || ctx->gpr[3]!=DS_HEAP_BASE+208 ||
                ctx->gpr[4]!=32 || ctx->gpr[1]!=0xFEFFBF0 || runtime_heap.stats.allocation_calls!=13 || ds_sync_held())
                stop(DS_ENTRY_ABI_MISMATCH,target);
        }
        if (return_pc==0x168E690) {
            if (!wide_streams_mode || descriptor!=0x1967FD0 || target!=0xB17510 ||
                ctx->gpr[3]!=DS_HEAP_BASE+0x240 || ctx->gpr[4]!=32 || ctx->gpr[1]!=0xFEFFB40 ||
                runtime_heap.stats.allocation_calls<22 || runtime_heap.stats.allocation_calls>25 || ds_sync_held())
                stop(DS_ENTRY_ABI_MISMATCH,target);
        }
        if (!ds_entry_call_lifted(target, ctx)) {
            if (active_error && active_error_capacity)
                snprintf(active_error, active_error_capacity, "Unimplemented lifted indirect target 0x%08X", target);
            stop(DS_ENTRY_UNSUPPORTED_CALL, target);
        }
        while (g_trampoline_fn) {
            void (*next)(void *) = g_trampoline_fn;
            g_trampoline_fn = nullptr;
            reinterpret_cast<void (*)(ppu_context *)>(next)(ctx);
        }
        if (constructor) {
            ++active_result->constructors_completed;
            active_result->constructor_completed = true;
        }
        return;
    }
    if (execute_singleton_mode && ctx == &context) {
        active_result->indirect_code = static_cast<uint32_t>(ctx->ctr);
        if (complete_constructor_mode) {
            if (active_result->constructor_completed && ctx->ctr == 0x66D688) {
                active_result->next_constructor_descriptor = static_cast<uint32_t>(ctx->gpr[11]);
                active_result->next_constructor_target = static_cast<uint32_t>(ctx->ctr);
                if (ctx->gpr[11] != 0x1947390 || vm_read32(0x1947390) != ctx->ctr ||
                    vm_read32(0x1947394) != active_image->toc || ctx->gpr[2] != active_image->toc ||
                    ctx->gpr[1] != 0xFEFFE00 || ctx->lr != 0x16B84F4 ||
                    !ds_image_address(active_image, 0x66D688, 4, DS_PF_X))
                    stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
                if (!complete_second_constructor_mode) stop(DS_ENTRY_FIRST_CONSTRUCTOR_READY, 0x66D688);
                func_0066D688(ctx);
                while (g_trampoline_fn) {
                    void (*next)(void *) = g_trampoline_fn;
                    g_trampoline_fn = nullptr;
                    reinterpret_cast<void (*)(ppu_context *)>(next)(ctx);
                }
                active_result->constructors_completed = 2;
                return;
            }
            if (complete_second_constructor_mode && active_result->constructors_completed == 2 && ctx->ctr == 0x3F4FE0) {
                active_result->next_constructor_descriptor = static_cast<uint32_t>(ctx->gpr[11]);
                active_result->next_constructor_target = static_cast<uint32_t>(ctx->ctr);
                if (ctx->gpr[11] != 0x19359B0 || vm_read32(0x19359B0) != ctx->ctr ||
                    vm_read32(0x19359B4) != active_image->toc || ctx->gpr[2] != active_image->toc ||
                    ctx->gpr[1] != 0xFEFFE00 || ctx->lr != 0x16B84F4 ||
                    !ds_image_address(active_image, 0x3F4FE0, 4, DS_PF_X))
                    stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
                stop(DS_ENTRY_SECOND_CONSTRUCTOR_READY, 0x3F4FE0);
            }
            struct Method {
                uint32_t code, descriptor, reg, incoming_lr, return_lr, sp, r3;
                void (*function)(ppu_context *);
            };
            static const Method methods[] = {
                {0xB25E58,0x1968548,11,0x782518,0xAE77E8,0xFEFFBF0,0xFEFFC60,func_00B25E58},
                {0x7CEC40,0x1951D80,10,0x7CF7A8,0x7CF7D0,0xFEFFD40,0x1B4C6E8,func_007CEC40},
                {0x7CEC48,0x1951D88,10,0x7CF7DC,0x7CF804,0xFEFFD40,0x1B4C6E8,func_007CEC48},
                {0xAE42C8,0x1966640,29,0x7CF804,0x7CF82C,0xFEFFD40,0x1B4C6E8,func_00AE42C8},
                {0xB27730,0x1968628,11,0xAE3B6C,0xA9422C,0xFEFFB50,0x1B91190,func_00B27730},
                {0xA664E0,0x19629E8,10,0xA9422C,0xB2777C,0xFEFFAB0,0x1B91168,func_00A664E0},
                {0xA662C8,0x19629D8,10,0xB27794,0xB277D4,0xFEFFAB0,0x1B91168,func_00A662C8},
                {0xB25E58,0x1968548,9,0xAE3B6C,0xAE3BCC,0xFEFFBD0,0xFEFFC44,func_00B25E58},
                {0xB25E58,0x1968548,9,0xAE3BD8,0xAE3C3C,0xFEFFBD0,0xFEFFC40,func_00B25E58},
                {0xA8CA28,0x1963C90,10,0xAE3C94,0xA8CE50,0xFEFFB20,0x1AD9DE0,func_00A8CA28},
                {0xB27658,0x1968620,11,0xAE42F8,0xAE4358,0xFEFFCA0,0x1B91190,func_00B27658},
                {0xA664E0,0x19629E8,11,0xAE4358,0xB276AC,0xFEFFBF0,0x1B91168,func_00A664E0},
                {0xA662C8,0x19629D8,10,0xB276C8,0xB27708,0xFEFFBF0,0x1B91168,func_00A662C8}
            };
            for (const auto &method : methods) {
                if (ctx->ctr != method.code || ctx->lr != method.incoming_lr || ctx->gpr[1] != method.sp) continue;
                active_result->indirect_descriptor = static_cast<uint32_t>(ctx->gpr[method.reg]);
                if (ctx->gpr[method.reg] != method.descriptor || vm_read32(method.descriptor) != method.code ||
                    vm_read32(method.descriptor + 4) != active_image->toc || ctx->gpr[2] != active_image->toc ||
                    ctx->gpr[3] != method.r3 || !ds_image_address(active_image, method.code, 4, DS_PF_X))
                    stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
                if (method.code == 0xB25E58 && (ctx->gpr[4] != 0x1B91190 || ctx->gpr[5] != 0))
                    stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
                if (method.code == 0xA664E0 && ctx->gpr[4] != 0)
                    stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
                if (method.code == 0xB27730 && ctx->gpr[4] != 0x38)
                    stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
                if (method.code == 0xB27658 && (ctx->gpr[4] != 0x10 || ctx->gpr[5] != 4))
                    stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
                if (method.code == 0xA662C8) {
                    if (ctx->gpr[29] > UINT32_MAX) stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
                    const bool node = method.return_lr == 0xB277D4;
                    checked_address(ctx->gpr[29], node ? 0x38 : 0x10, true);
                    if (node) active_result->node_address = static_cast<uint32_t>(ctx->gpr[29]);
                    else active_result->node_vector_address = static_cast<uint32_t>(ctx->gpr[29]);
                }
                ctx->lr = method.return_lr;
                method.function(ctx);
                return;
            }
        }
        if (execute_manager_mode && ctx->ctr == 0x00A662C8) {
            const uint32_t descriptor = 0x19629D8;
            const bool allocation_unlock = execute_allocator_mode && ctx->lr == 0xB26C84;
            const bool registry_unlock = execute_registry_mode && ctx->lr == 0xB276C8 &&
                ctx->gpr[3] == 0x1B91168 && ctx->gpr[1] == 0xFEFFB80;
            const uint64_t actual_descriptor = ctx->gpr[(allocation_unlock || registry_unlock) ? 10 : 11];
            active_result->indirect_descriptor = static_cast<uint32_t>(actual_descriptor);
            if (actual_descriptor != descriptor || vm_read32(descriptor) != ctx->ctr ||
                vm_read32(descriptor + 4) != active_image->toc || ctx->gpr[2] != active_image->toc ||
                (!allocation_unlock && !registry_unlock && ctx->lr != 0xA35B7C) ||
                !ds_image_address(active_image, 0xA662C8, 4, DS_PF_X))
                stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
            if (allocation_unlock) {
                if (ctx->gpr[29] > UINT32_MAX) stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
                active_result->allocation_address = static_cast<uint32_t>(ctx->gpr[29]);
                checked_address(ctx->gpr[29], active_result->allocation_size, true);
            }
            if (registry_unlock) {
                if (ctx->gpr[29] > UINT32_MAX) stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
                const bool callback = execute_registration_mode && active_result->allocation_calls == 3;
                if (callback) active_result->callback_allocation_address = static_cast<uint32_t>(ctx->gpr[29]);
                else active_result->registry_allocation_address = static_cast<uint32_t>(ctx->gpr[29]);
                checked_address(ctx->gpr[29], callback ? active_result->callback_allocation_size : active_result->registry_allocation_size, true);
            }
            ctx->lr = registry_unlock ? 0xB27708 : allocation_unlock ? 0xB26CC4 : 0xA35A48;
            func_00A662C8(ctx);
            return;
        }
        if (ctx->ctr == 0x00A664E0) {
            const uint32_t descriptor = 0x019629E8;
            const bool registry_lock = execute_registry_mode &&
                (ctx->lr == 0xAE4E18 || (execute_registration_mode && ctx->lr == 0xAE514C)) &&
                ctx->gpr[3] == 0x1B91168 && ctx->gpr[1] == 0xFEFFB80;
            const uint64_t actual_descriptor = ctx->gpr[registry_lock ? 11 : 10];
            active_result->indirect_descriptor = static_cast<uint32_t>(actual_descriptor);
            if (actual_descriptor != descriptor || vm_read32(descriptor) != ctx->ctr ||
                vm_read32(descriptor + 4) != active_image->toc || ctx->gpr[2] != active_image->toc ||
                !ds_image_address(active_image, static_cast<uint32_t>(ctx->ctr), 4, DS_PF_X) ||
                (ctx->lr != 0x00A359E8 && !(execute_allocator_mode && ctx->lr == 0xA358B8) && !registry_lock))
                stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
            /* Audited bctrl sites in func_00A35780. */
            ctx->lr = registry_lock ? 0xB276AC : ctx->lr == 0xA358B8 ? 0xB26C68 : 0x00A35A18;
            func_00A664E0(ctx);
            return;
        }
        if (execute_allocator_mode && ctx->ctr == 0xB25E58) {
            const uint32_t descriptor = 0x1968548;
            const bool registry_status = execute_registry_mode &&
                (ctx->lr == 0xAE227C || (execute_registration_mode && ctx->lr == 0xAE45C4)) &&
                ctx->gpr[3] == 0xFEFFBF0 && ctx->gpr[1] == 0xFEFFB80;
            const uint64_t actual_descriptor = ctx->gpr[registry_status ? 9 : 11];
            active_result->indirect_descriptor = static_cast<uint32_t>(actual_descriptor);
            if (actual_descriptor != descriptor || vm_read32(descriptor) != ctx->ctr ||
                vm_read32(descriptor + 4) != active_image->toc || ctx->gpr[2] != active_image->toc ||
                ctx->gpr[4] != 0x1B91190 || ctx->gpr[5] != 0 || (!registry_status && ctx->lr != 0xAE7854) ||
                !ds_image_address(active_image, 0xB25E58, 4, DS_PF_X))
                stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
            ctx->lr = registry_status ? (ctx->lr == 0xAE45C4 ? 0xAE4524 : 0xAE21DC) : 0xAE77E8;
            func_00B25E58(ctx);
            return;
        }
        if (execute_registry_mode && (ctx->ctr == 0x7CEC40 || ctx->ctr == 0x7CEC48)) {
            const bool name = ctx->ctr == 0x7CEC40;
            const uint32_t descriptor = name ? 0x1951D80 : 0x1951D88;
            active_result->indirect_descriptor = static_cast<uint32_t>(ctx->gpr[11]);
            if (ctx->gpr[11] != descriptor || vm_read32(descriptor) != ctx->ctr ||
                vm_read32(descriptor + 4) != active_image->toc || ctx->gpr[2] != active_image->toc ||
                ctx->gpr[3] != 0x1B4C6E8 || ctx->gpr[1] != 0xFEFFBA0 ||
                ctx->lr != (name ? 0xAE4DB4 : 0xADEBF8) ||
                !ds_image_address(active_image, static_cast<uint32_t>(ctx->ctr), 4, DS_PF_X))
                stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
            ctx->lr = name ? 0xADEBF8 : 0xADEC20;
            if (name) func_007CEC40(ctx);
            else func_007CEC48(ctx);
            return;
        }
        if (execute_registry_mode && ctx->ctr == 0xB27658) {
            const uint32_t descriptor = 0x1968620;
            const bool callback = execute_registration_mode && ctx->lr == 0xAE50E0 && ctx->gpr[4] == 0x1000;
            active_result->indirect_descriptor = static_cast<uint32_t>(ctx->gpr[11]);
            if (ctx->gpr[11] != descriptor || vm_read32(descriptor) != ctx->ctr ||
                vm_read32(descriptor + 4) != active_image->toc || ctx->gpr[2] != active_image->toc ||
                ctx->gpr[3] != 0x1B91190 || (!callback && ctx->gpr[4] != 0x2000) || ctx->gpr[5] != 4 ||
                ctx->gpr[1] != 0xFEFFC30 || (!callback && ctx->lr != 0xAE4DB4) ||
                !ds_image_address(active_image, 0xB27658, 4, DS_PF_X))
                stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
            ctx->lr = callback ? 0xAE514C : 0xAE4E18;
            func_00B27658(ctx);
            return;
        }
        if (execute_registration_mode && ctx->ctr >= 0x7CEE00 && ctx->ctr <= 0x7CEE48) {
            static constexpr uint32_t methods[][4] = {
                {0x7CEE00,0x1951DC8,0xAE514C,0xAE522C},
                {0x7CEE18,0x1951DD0,0xAE522C,0xAE525C},
                {0x7CEE30,0x1951DD8,0xAE525C,0xAE528C},
                {0x7CEE48,0x1951DE0,0xAE528C,0xAE52BC}
            };
            for (const auto &method : methods) {
                if (ctx->ctr != method[0]) continue;
                active_result->indirect_descriptor = static_cast<uint32_t>(ctx->gpr[9]);
                if (ctx->gpr[9] != method[1] || vm_read32(method[1]) != ctx->ctr ||
                    vm_read32(method[1] + 4) != active_image->toc || ctx->gpr[2] != active_image->toc ||
                    ctx->gpr[3] != 0x1B4C6E8 || ctx->gpr[1] != 0xFEFFC30 || ctx->lr != method[2] ||
                    !ds_image_address(active_image, method[0], 4, DS_PF_X))
                    stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
                ctx->lr = method[3];
                if (method[0] == 0x7CEE00) func_007CEE00(ctx);
                else if (method[0] == 0x7CEE18) func_007CEE18(ctx);
                else if (method[0] == 0x7CEE30) func_007CEE30(ctx);
                else func_007CEE48(ctx);
                return;
            }
            stop(DS_ENTRY_UNSUPPORTED_CALL, active_result->callee);
        }
        if (ctx->ctr == 0x00AB5CC0 && active_result->callee == 0x00AB6378) {
            const uint32_t descriptor = 0x01964BF0;
            active_result->indirect_descriptor = static_cast<uint32_t>(ctx->gpr[11]);
            if (ctx->gpr[11] != descriptor || vm_read32(descriptor) != ctx->ctr ||
                vm_read32(descriptor + 4) != active_image->toc || ctx->gpr[2] != active_image->toc ||
                !ds_image_address(active_image, 0x00AB5CC0, 4, DS_PF_X))
                stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
            ctx->lr = 0x00AB63B8;
            func_00AB5CC0(ctx);
            return;
        }
    }
    if (execute_constructor_mode && ctx == &context && active_result->constructor_started &&
        active_result->callee == 0x00A66790) {
        if (ctx->gpr[11] != 0x019629D0 || vm_read32(ctx->gpr[11]) != 0x00A662C0 ||
            vm_read32(ctx->gpr[11] + 4) != active_image->toc || ctx->ctr != 0x00A662C0 ||
            ctx->gpr[2] != active_image->toc ||
            !ds_image_address(active_image, 0x00A662C0, 4, DS_PF_X))
            stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
        ctx->lr = 0x00A667D4; /* Original bctrl at 0x00A667D0. */
        func_00A662C0(ctx);
        return;
    }
    /* Only the first constructor and one audited virtual method are supported,
     * not arbitrary indirect dispatch. The original walker already read the
     * OPD into CTR/r2. Validate those against the descriptor and actual access
     * permissions before reporting the deliberate pre-invocation boundary.
     */
    if (!execute_clock_mode || ctx != &context || active_result->clock_calls != 1 ||
        active_result->callee != 0x016B84B0 || ctx->gpr[11] > UINT32_MAX)
        stop(DS_ENTRY_UNSUPPORTED_CALL, active_result->callee);
    const uint32_t descriptor = static_cast<uint32_t>(ctx->gpr[11]);
    const uint32_t code = vm_read32(descriptor);
    const uint32_t toc = vm_read32(static_cast<uint64_t>(descriptor) + 4);
    active_result->constructor_descriptor = descriptor;
    active_result->constructor_target = code;
    active_result->constructor_toc = toc;
    if (ctx->ctr != code || ctx->gpr[2] != toc || descriptor != 0x01951EA8 ||
        code != 0x007CF848 || toc != active_image->toc)
        stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
    if (!ds_image_address(active_image, code, 4, DS_PF_X)) {
        if (active_error && active_error_capacity)
            snprintf(active_error, active_error_capacity, "Constructor target has no executable backing");
        stop(DS_ENTRY_ABI_MISMATCH, active_result->callee);
    }
    if (!execute_constructor_mode)
        stop(DS_ENTRY_CONSTRUCTOR_BOUNDARY, active_result->callee);
    if (active_result->constructor_started) stop(DS_ENTRY_UNSUPPORTED_CALL, active_result->callee);
    active_result->constructor_started = true;
    ctx->lr = 0x016B84F4; /* Original bctrl at 0x016B84F0, omitted by the lift. */
    func_007CF848(ctx);
    while (g_trampoline_fn) {
        void (*next)(void *) = g_trampoline_fn;
        g_trampoline_fn = nullptr;
        reinterpret_cast<void (*)(ppu_context *)>(next)(ctx);
    }
    active_result->constructor_completed = true;
    active_result->constructors_completed = 1;
}

extern "C" void ds_entry_indirect_callsite(ppu_context *ctx, unsigned int return_pc)
{
    pending_indirect_return_pc = return_pc;
    ps3_indirect_call(ctx);
}

extern "C" void lv2_syscall(ppu_context *ctx)
{
    if (ctx == &context && ctx->gpr[11] <= UINT32_MAX)
        active_result->syscall_number = static_cast<uint32_t>(ctx->gpr[11]);
    if (complete_constructor_mode && ctx == &context && ctx->gpr[11] == 100) {
        checked_address(ctx->gpr[3], 4, true);
        checked_address(ctx->gpr[4], 40, false);
        active_result->kernel_mutex_output = static_cast<uint32_t>(ctx->gpr[3]);
        active_result->kernel_mutex_attribute = static_cast<uint32_t>(ctx->gpr[4]);
        for (unsigned i = 0; i < 10; ++i)
            active_result->kernel_mutex_attribute_words[i] = vm_read32(ctx->gpr[4] + i * 4);
        const uint32_t *attr = active_result->kernel_mutex_attribute_words;
        // Padding and seven trailing name bytes are not initialized by guest code.
        if (attr[0] != 2 || attr[1] != 0x10 || attr[2] != 0x200 || attr[3] != 0x2000 ||
            attr[4] || attr[5] || attr[6] || vm_read8(ctx->gpr[4] + 32) ||
            (!explore_startup_mode && active_result->kernel_mutex_calls))
            stop(DS_ENTRY_UNSUPPORTED_CALL, active_result->callee);
        uint32_t handle = 0;
        if (!ds_kernel_mutex_create(&handle)) stop(DS_ENTRY_SYNC_FAILURE, active_result->callee);
        vm_write32(ctx->gpr[3], handle);
        active_result->kernel_mutex_handle = handle;
        ++active_result->kernel_mutex_calls;
        ctx->gpr[3] = 0;
        return;
    }
    if (execute_allocator_mode && ctx == &context && (ctx->gpr[11] == 125 || ctx->gpr[11] == 127)) {
        const bool acquire = ctx->gpr[11] == 125;
        if (ctx->gpr[3] > UINT32_MAX || ctx->thread_id != 1 ||
            (acquire && ctx->gpr[4] != 0) ||
            !(acquire ? ds_rwlock_write_acquire(static_cast<uint32_t>(ctx->gpr[3]), 1)
                      : ds_rwlock_write_release(static_cast<uint32_t>(ctx->gpr[3]), 1))) {
            if (active_error && active_error_capacity)
                snprintf(active_error, active_error_capacity, "Unsupported reader/writer ownership, contention or wait");
            stop(DS_ENTRY_SYNC_FAILURE, active_result->callee);
        }
        if (acquire) ++active_result->rwlock_write_locks;
        else ++active_result->rwlock_write_unlocks;
        ctx->gpr[3] = 0;
        return;
    }
    if (execute_singleton_mode && ctx == &context && ctx->gpr[11] == 120 &&
        active_result->singleton_started && active_result->lock_calls == 2 && active_result->unlock_calls == 1) {
        checked_address(ctx->gpr[3], 4, true);
        checked_address(ctx->gpr[4], 32, false);
        active_result->rwlock_output = static_cast<uint32_t>(ctx->gpr[3]);
        active_result->rwlock_attribute = static_cast<uint32_t>(ctx->gpr[4]);
        for (unsigned i = 0; i < 8; ++i)
            active_result->rwlock_attribute_words[i] = vm_read32(ctx->gpr[4] + 4u * i);
        /* sys_rwlock_attribute_t: protocol, pshared, ipc_key, flags, pad, name.
         * Accept this verified startup's private, priority-protocol creation.
         * Only uncontended writer operations are implemented; no waiter policy.
         */
        const uint32_t *attr = active_result->rwlock_attribute_words;
        if (attr[0] != 2 || attr[1] != 0x200 || attr[2] || attr[3] || attr[4] ||
            active_result->rwlock_create_calls != 0)
            stop(DS_ENTRY_UNSUPPORTED_CALL, active_result->callee);
        uint32_t handle = 0;
        if (!ds_rwlock_create(&handle)) {
            if (active_error && active_error_capacity)
                snprintf(active_error, active_error_capacity, "Native reader/writer lock creation failed");
            stop(DS_ENTRY_SYNC_FAILURE, active_result->callee);
        }
        vm_write32(ctx->gpr[3], handle);
        active_result->rwlock_handle = handle;
        ++active_result->rwlock_create_calls;
        ctx->gpr[3] = 0;
        if (!execute_manager_mode) stop(DS_ENTRY_RWLOCK_INITIALIZED, active_result->callee);
        return;
    }
    if (ctx != &context || !continue_crt_mode || ctx->gpr[11] != 352 ||
        !active_result->tls_initialized || !active_result->mutex_initialized) {
        if (active_error && active_error_capacity)
            snprintf(active_error, active_error_capacity, "Unsupported LV2 syscall %llu",
                     static_cast<unsigned long long>(ctx->gpr[11]));
        stop(DS_ENTRY_UNSUPPORTED_CALL, active_result->callee);
    }
    if (execute_memory_mode) {
        if (active_result->memory_query_count != 0)
            stop(DS_ENTRY_UNSUPPORTED_CALL, active_result->callee);
        /* Validate the whole output before either write. The budget counts
         * ELF, stacks, TLS, argv and any other arenas, including test arenas.
         * No backing is allocated by this query, and no desktop RAM total is
         * exposed as if it were available to this bounded process.
         */
        checked_address(ctx->gpr[3], 8, true);
        uint32_t total = 0, available = 0;
        if (!ds_image_memory_info(active_image, &total, &available))
            stop(DS_ENTRY_BAD_INPUT, active_result->callee);
        vm_write32(ctx->gpr[3], total);
        vm_write32(ctx->gpr[3] + 4, available);
        active_result->memory_total = total;
        active_result->memory_available = available;
        active_result->memory_output_address = static_cast<uint32_t>(ctx->gpr[3]);
        active_result->memory_backed_at_query = active_image->allocated_bytes;
        ++active_result->memory_query_count;
        ctx->gpr[3] = 0;
        return;
    }
    stop(DS_ENTRY_MEMORY_QUERY_BOUNDARY, active_result->callee);
}

static bool run_entry(DsImage *image, DsEntryResult *result,
                      char *error, size_t error_capacity, bool initialize_tls,
                      bool continue_crt = false, bool execute_memory = false, bool execute_clock = false,
                      bool execute_constructor = false, bool execute_singleton = false, bool execute_manager = false,
                      bool execute_allocator = false, bool execute_registry = false, bool execute_registration = false,
                      bool complete_constructor = false, bool complete_second_constructor = false,
                      bool explore_startup = false, uint32_t constructor_limit = 0,
                      bool runtime_heap_stop = false, bool allocate_runtime = false, bool locale_object = false,
                      bool locale_publish = false, bool stream_locale = false, bool first_facet = false, bool first_stream = false, bool narrow_streams = false, bool wide_streams = false)
{
    if (error && error_capacity) error[0] = '\0';
    if (!result) {
        if (error && error_capacity) snprintf(error, error_capacity, "Missing entry result");
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->stop = DS_ENTRY_BAD_INPUT;
    if (busy || runtime_heap.image || ds_sync_live() || ds_rwlock_live() || ds_kernel_mutex_live() || !image || image->entry_descriptor != 0x01916138u ||
        image->entry_code != 0x000124C0u || image->toc != 0x019B5040u ||
        image->tls_address != 0x019D8C68u || image->tls_file_size != 4 ||
        image->tls_memory_size != 1076) {
        if (error && error_capacity)
            snprintf(error, error_capacity, "Entry checkpoint requires the verified BLUS30443 image and an idle adapter");
        return false;
    }
    if (execute_constructor && image->constructor_execution_attempted) {
        if (error && error_capacity)
            snprintf(error, error_capacity, "Constructor execution requires a freshly reloaded image");
        return false;
    }
    const void *tls_template = ds_image_address(image, image->tls_address,
                                                image->tls_file_size, DS_PF_R);
    if (!tls_template || !prepare_region(image, kStackBase, kStackSize, error, error_capacity) ||
        !prepare_region(image, kTlsBase, kTlsSize, error, error_capacity)) return false;
    void *tls_copy = ds_image_address(image, kTlsBase, kTlsSize, DS_PF_R | DS_PF_W);
    if (!tls_copy) {
        if (error && error_capacity) snprintf(error, error_capacity, "TLS arena is not backed");
        return false;
    }
    memcpy(tls_copy, tls_template, image->tls_file_size);

    if (continue_crt) {
        if (!prepare_region(image, kArgvBase, kArgvSize, error, error_capacity)) return false;
        /* 64-bit big-endian argv[0], null argv[1], null envp[0]. CRT then
         * compacts these to 32-bit slots in place, including the terminators.
         * Keep this writable storage outside both verified ELF segments.
         */
        if (!ds_image_write_be32(image, kArgvBase + 4, kArgvBase + 0x20u)) return false;
        void *boot = ds_image_address(image, kArgvBase + 0x20u, sizeof(kBootPath), DS_PF_W);
        if (!boot) return false;
        memcpy(boot, kBootPath, sizeof(kBootPath));
        result->argv_address = kArgvBase;
        result->envp_address = kArgvBase + 16;
    }

    if (allocate_runtime && !ds_heap_init(&runtime_heap,image,DS_HEAP_DEFAULT_LIMIT)) {
        if (error && error_capacity) snprintf(error,error_capacity,"Could not initialize bounded runtime heap");
        return false;
    }
    memset(&context, 0, sizeof(context));
    context.gpr[1] = kStackTop;
    context.gpr[2] = image->toc;
    context.gpr[3] = 1;           /* argc; argv is not dereferenced before this boundary. */
    context.gpr[4] = 0x00B00000u; /* Desktop process-entry register, no RO image mutation. */
    if (continue_crt) {
        context.gpr[4] = kArgvBase;
        context.gpr[5] = kArgvBase + 16;
        context.gpr[6] = 0;
    }
    context.gpr[7] = 1;
    context.gpr[8] = image->tls_address;
    context.gpr[9] = image->tls_file_size;
    context.gpr[10] = image->tls_memory_size;
    context.gpr[12] = 0x100000u;
    context.gpr[13] = kTlsThreadPointer;
    context.thread_id = 1;
    active_image = image;
    active_result = result;
    active_error = error;
    active_error_capacity = error_capacity;
    busy = true;
    initialize_tls_mode = initialize_tls;
    continue_crt_mode = continue_crt;
    execute_memory_mode = execute_memory;
    execute_clock_mode = execute_clock;
    execute_constructor_mode = execute_constructor;
    execute_singleton_mode = execute_singleton;
    execute_manager_mode = execute_manager;
    execute_allocator_mode = execute_allocator;
    execute_registry_mode = execute_registry;
    execute_registration_mode = execute_registration;
    complete_constructor_mode = complete_constructor;
    complete_second_constructor_mode = complete_second_constructor;
    explore_startup_mode = explore_startup;
    exploration_constructor_limit = constructor_limit;
    runtime_heap_mode = runtime_heap_stop;
    runtime_allocation_mode = allocate_runtime;
    locale_object_mode = locale_object;
    locale_publish_mode = locale_publish;
    stream_locale_mode = stream_locale;
    first_facet_mode = first_facet;
    first_stream_mode = first_stream;
    narrow_streams_mode = narrow_streams;
    wide_streams_mode = wide_streams;
    pending_indirect_return_pc = 0;
    if (execute_constructor) image->constructor_execution_attempted = true;
    g_trampoline_fn = nullptr;

    if (setjmp(checkpoint) == 0) {
        func_000124C0(&context);
        while (g_trampoline_fn) {
            void (*next)(void *) = g_trampoline_fn;
            g_trampoline_fn = nullptr;
            reinterpret_cast<void (*)(ppu_context *)>(next)(&context);
        }
        stop(DS_ENTRY_RETURNED, result->callee);
    }
    bool constructor_prefix_matches = false, manager_prefix_matches = false, startup_prefix_matches = false;
    bool registry_prefix_matches = false, registry_state_matches = false, registration_prefix_matches = false;
    bool matched = result->stop == (initialize_tls ? DS_ENTRY_TLS_INITIALIZED : DS_ENTRY_TLS_BOUNDARY) &&
                   result->boundary_nid == kTlsNid &&
                   result->callee == 0x016BB3E4u && result->r3 == (initialize_tls ? 0u : 1u) &&
                   (!initialize_tls || (result->tls_initialized && result->tls_block == kMainTlsBase &&
                                        result->thread_pointer == kMainTlsBase + 0x7000u)) &&
                   result->r4 == image->tls_address && result->r5 == image->tls_file_size &&
                   result->r6 == image->tls_memory_size && result->lr == 0x00012648u &&
                   result->stack_pointer == kStackTop - 0x110u &&
                   result->function_count == 3 && result->functions[0] == 0x000124C0u &&
                   result->functions[1] == 0x000125F0u && result->functions[2] == 0x016BB3E4u;
    if (continue_crt) {
        static constexpr uint32_t sequence[] = {0x124C0, 0x125F0, 0x16BB3E4, 0x11EA0,
                                                0x167ACE0, 0x167A9C8, 0x16BB2C4};
        matched = result->stop == DS_ENTRY_MEMORY_QUERY_BOUNDARY && result->syscall_number == 352 &&
                  result->tls_initialized && result->mutex_initialized &&
                  result->thread_pointer == kMainTlsBase + 0x7000 &&
                  result->callee == 0x16BB2C4 && result->boundary_nid == kMutexCreateNid &&
                  result->lr == 0x167AA0C && result->stack_pointer == kStackTop - 0x270 &&
                  result->r3 == result->stack_pointer + 0x74 &&
                  result->toc == image->toc + 0x20000 - 5268 &&
                  result->function_count == sizeof(sequence) / sizeof(sequence[0]) &&
                  memcmp(result->functions, sequence, sizeof(sequence)) == 0;
    }
    if (execute_memory) {
        static constexpr uint32_t sequence[] = {
            0x124C0, 0x125F0, 0x16BB3E4, 0x11EA0, 0x167ACE0, 0x167A9C8, 0x16BB2C4,
            0x10D60, 0x1676368, 0x16BB2C4, 0x120C0, 0x166FA10,
            0x1676258, 0x16BB2C4, 0x1676258, 0x16BB2C4, 0x1676258, 0x16BB2C4,
            0x1676258, 0x16BB2C4, 0x1676258, 0x16BB2C4, 0x1676258, 0x16BB2C4,
            0x1676258, 0x16BB2C4, 0x1676258, 0x16BB2C4, 0x10D50, 0x1675CB0, 0x16BB404
        };
        matched = result->stop == DS_ENTRY_TIME_BOUNDARY && result->syscall_number == 352 &&
                  result->memory_query_count == 1 && result->mutex_count == 10 &&
                  result->tls_initialized && result->thread_pointer == kMainTlsBase + 0x7000 &&
                  result->callee == 0x16BB404 && result->boundary_nid == kTimeNid &&
                  result->lr == 0x12708 && result->stack_pointer == kStackTop - 0x110 &&
                  result->toc == image->toc && result->function_count == sizeof(sequence) / sizeof(sequence[0]) &&
                  memcmp(result->functions, sequence, sizeof(sequence)) == 0;
        if (execute_clock) {
            static constexpr uint32_t suffix[] = {0x10210, 0x12878, 0x10200, 0x16B84B0};
            uint64_t timestamp = 0;
            matched = result->stop == DS_ENTRY_CONSTRUCTOR_BOUNDARY && result->clock_calls == 1 &&
                result->constructor_descriptor == 0x1951EA8 && result->constructor_target == 0x7CF848 &&
                result->constructor_toc == image->toc && result->toc == image->toc &&
                result->lr == 0x10228 && result->stack_pointer == kStackTop - 0x200 &&
                result->memory_query_count == 1 && result->mutex_count == 10 && result->tls_initialized &&
                result->thread_pointer == kMainTlsBase + 0x7000 && result->callee == 0x16B84B0 &&
                result->function_count == 35 && memcmp(result->functions, sequence, sizeof(sequence)) == 0 &&
                memcmp(result->functions + 31, suffix, sizeof(suffix)) == 0 &&
                ds_image_read_be64(image, 0x1D2B5C8, &timestamp) && timestamp == result->clock_microseconds;
            if (execute_constructor) {
                static constexpr uint32_t constructor_suffix[] = {
                    0x7CF848, 0x7CF5D0, 0xDBA100, 0x7CEE78, 0xB0AF28, 0x121B0, 0x1664CD8,
                    0x166F990, 0x1676150, 0x16BB2E4, 0x16BB244, 0x166F910, 0x1676108, 0x16BB264,
                    0xA668B0, 0xA66790, 0xA662C0, 0x16BB2C4
                };
                constructor_prefix_matches = memcmp(result->functions, sequence, sizeof(sequence)) == 0 &&
                    memcmp(result->functions + 31, suffix, sizeof(suffix)) == 0 &&
                    memcmp(result->functions + 35, constructor_suffix, sizeof(constructor_suffix)) == 0;
                uint32_t free_slots = 0, callback = 0;
                matched = result->stop == DS_ENTRY_SINGLETON_BOUNDARY && result->callee == 0xAE7768 &&
                    result->constructor_started && !result->constructor_completed && result->clock_calls == 1 &&
                    result->constructor_target == 0x7CF848 && result->constructor_toc == image->toc &&
                    result->thread_id_calls == 1 && result->lock_calls == 1 && result->unlock_calls == 1 &&
                    result->mutex_count == 11 && ds_sync_held() == 0 && result->function_count == 53 &&
                    memcmp(result->functions, sequence, sizeof(sequence)) == 0 &&
                    memcmp(result->functions + 31, suffix, sizeof(suffix)) == 0 &&
                    memcmp(result->functions + 35, constructor_suffix, sizeof(constructor_suffix)) == 0 &&
                    result->lr == 0x7CEF28 && result->stack_pointer == kStackTop - 0x360 &&
                    result->toc == image->toc && result->memory_query_count == 1 &&
                    ds_image_read_be32(image, 0x1AD5630, &free_slots) && free_slots == 79 &&
                    ds_image_read_be32(image, 0x1CE58E8, &callback) && callback == 0x1967920;
            }
        }
    }
    if (execute_singleton) {
        static constexpr uint32_t singleton_suffix[] = {
            0xAE7768, 0xA35DB0, 0xA35780, 0xB0AF28, 0xA664E0, 0x16BB244, 0xB25CB8,
            0xAC7578, 0xA662A0, 0xB25A60, 0xA34BB8, 0xA34C48, 0xAB64E8, 0xAB6378, 0xAB5CC0
        };
        uint32_t handle_word = 0, atexit_free = 0, atexit_callback = 0;
        matched = result->stop == DS_ENTRY_RWLOCK_INITIALIZED && result->syscall_number == 120 &&
            constructor_prefix_matches && memcmp(result->functions + 53, singleton_suffix, sizeof(singleton_suffix)) == 0 &&
            result->singleton_started && result->singleton_address == 0x1B4C6E8 &&
            result->manager_address == 0x1B91128 && result->rwlock_output == 0x1B911AC &&
            result->rwlock_attribute_words[0] == 2 && result->rwlock_attribute_words[1] == 0x200 &&
            result->constructor_started && !result->constructor_completed && result->function_count == 68 &&
            result->memory_query_count == 1 && result->clock_calls == 1 && result->tls_initialized &&
            result->thread_pointer == kMainTlsBase + 0x7000 && result->thread_id_calls == 1 &&
            result->lock_calls == 2 && result->unlock_calls == 1 && result->mutex_count == 11 &&
            ds_sync_held() == 1 && result->toc == image->toc && result->r3 == 0 &&
            result->rwlock_create_calls == 1 && ds_rwlock_contains(result->rwlock_handle) &&
            result->stack_pointer == kStackTop - 0x600 && result->lr == 0xAB63B8 &&
            ds_image_read_be32(image, result->rwlock_output, &handle_word) && handle_word == result->rwlock_handle &&
            ds_image_read_be32(image, 0x1AD5630, &atexit_free) && atexit_free == 79 &&
            ds_image_read_be32(image, 0x1CE58E8, &atexit_callback) && atexit_callback == 0x1967920;
        if (execute_manager) {
            static constexpr uint32_t suffix[] = {0x967998, 0xA662C8, 0x16BB264, 0x2F0B8};
            uint32_t handle = 0;
            manager_prefix_matches = constructor_prefix_matches &&
                memcmp(result->functions + 53, singleton_suffix, sizeof(singleton_suffix)) == 0 &&
                memcmp(result->functions + 68, suffix, sizeof(suffix)) == 0;
            matched = result->stop == DS_ENTRY_MANAGER_POOL_BOUNDARY && result->callee == 0xB25B58 &&
                result->function_count == 72 && constructor_prefix_matches &&
                memcmp(result->functions + 53, singleton_suffix, sizeof(singleton_suffix)) == 0 &&
                memcmp(result->functions + 68, suffix, sizeof(suffix)) == 0 &&
                result->manager_published && result->pool_request_base == 0x1AD6B78 && result->pool_request_size == 0x60000 &&
                result->lock_calls == 2 && result->unlock_calls == 2 && ds_sync_held() == 0 &&
                result->rwlock_create_calls == 1 && ds_rwlock_contains(result->rwlock_handle) &&
                ds_image_read_be32(image, result->rwlock_output, &handle) && handle == result->rwlock_handle &&
                result->constructor_started && !result->constructor_completed && result->toc == image->toc &&
                result->lr == 0xA3585C && result->stack_pointer == kStackTop - 0x550;
        }
    }
    if (execute_allocator) {
        static constexpr uint32_t suffix[] = {
            0xB25B58,0xA66790,0xA662C0,0x16BB2C4,0xAC7D00,0xAC7AA0,0x967AA0,
            0xB26C18,0xA664E0,0x16BB244,0xAC8578,0xAC8078,0xA662C8,0x16BB264,
            0xC0F580,0xB25978,0xA35210,0xAB6160,0xAB5F90,0xB25978,0xB25E58,0xA34BB8,0xA34C48
        };
        uint32_t interface = 0, begin = 0, end = 0, tuple = 0;
        startup_prefix_matches = manager_prefix_matches && memcmp(result->functions + 72, suffix, sizeof(suffix)) == 0;
        /* Verify the resulting pool split, range vector and completed object
         * on the device too; these reads do not add guest instructions/traces.
         */
        static constexpr uint32_t final_words[][2] = {
            {0x1AD6BF0,0x1B91128},{0x1AD6BF4,0x1AD6D80},{0x1AD6BF8,0},
            {0x1AD6D80,0x1AD6BF0},{0x1AD6D84,0x1B36B68},{0x1B36B68,0x1AD6D80},
            {0x1AD6BE8,0x1AD6D88},{0x1AD6BEC,0x1AD6D88},
            {0x1B91160,0},{0x1B91164,0x5FDE8},{0x1B911B0,0x1000},
            {0x1B9111C,0x1B91128},{0x1B911AC,1},{0x1B911A4,0x1AD6D80},
            {0x1AD6C04,0x1AD6B78},{0x1AD6C08,0x1B36B78},
            {0x1B4C6E0,0x1B4C6E8},{0x1B4C6E8,0x186A628},{0x1B4C6EC,0},{0x1B4C6F0,0},
            {0x1B4C6F8,0},{0x1B4C6FC,0},{0x1B4C700,0},
            {0x1B4C704,0x16F8410},{0x1B4C708,0x16F8320},{0xE000010,0xD1}
        };
        bool state_matches = true;
        uint32_t word = 0;
        for (const auto &expected : final_words)
            state_matches &= ds_image_read_be32(image, expected[0], &word) && word == expected[1];
        matched = result->stop == DS_ENTRY_SINGLETON_READY && result->singleton_completed &&
            state_matches && manager_prefix_matches && result->function_count == 95 &&
            memcmp(result->functions + 72, suffix, sizeof(suffix)) == 0 &&
            result->pool_initialized && result->allocation_calls == 1 && result->allocation_address == 0x1AD6C00 &&
            result->allocation_size == 0x180 && result->atomic_successes == 1 && result->bootstrap_fallback_calls == 0 &&
            result->rwlock_write_locks == 1 && result->rwlock_write_unlocks == 1 && ds_rwlock_held() == 0 &&
            result->lock_calls == 3 && result->unlock_calls == 3 && ds_sync_held() == 0 &&
            result->mutex_count == 12 && result->rwlock_create_calls == 1 && ds_rwlock_contains(result->rwlock_handle) &&
            result->memory_query_count == 1 && result->clock_calls == 1 && !result->constructor_completed &&
            result->toc == image->toc && result->thread_pointer == kMainTlsBase + 0x7000 &&
            ds_image_read_be32(image, 0x1B4C6F4, &interface) && interface == 0x1B91190 &&
            ds_image_read_be32(image, 0x1B9119C, &begin) && begin == result->allocation_address &&
            ds_image_read_be32(image, 0x1B911A0, &end) && end == begin + 12 &&
            ds_image_read_be32(image, begin, &tuple) && tuple == interface &&
            result->lr == 0x7CF75C && result->stack_pointer == kStackTop - 0x2C0 && result->r3 == 0x1B4C6E8;
    }
    if (execute_registry) {
        static constexpr uint32_t suffix[] = {
            0xAE4D38,0xAE2148,0xB0AF28,0xB25E58,0xA34BB8,0xA34C48,
            0xADEBA0,0x7CEC40,0x7CEC48,0x10BA0,0x16693E0,0xB27658,
            0xA664E0,0x16BB244,0xAC8578,0xAC8078,0xA662C8,0x16BB264,0xADCAB0,0xADCAB0
        };
        static constexpr uint32_t final_words[][2] = {
            {0x1AD6BF0,0x1B91128},{0x1AD6BF4,0x1AD6D80},{0x1AD6BF8,0},
            {0x1AD6D80,0x1AD6BF0},{0x1AD6D84,0x1AD8D90},{0x1AD6D88,0},
            {0x1AD8D90,0x1AD6D80},{0x1AD8D94,0x1B36B68},{0x1B36B68,0x1AD8D90},
            {0x1AD6BE8,0x1AD8D98},{0x1AD6BEC,0x1AD8D98},
            {0x1AD8D98,0x1AD6BE8},{0x1AD8D9C,0x1AD6BE8},
            {0x1B91160,0},{0x1B91164,0x5DDD8},{0x1B9114C,14},{0x1B911B0,0x1000},
            {0x1B9111C,0x1B91128},{0x1B911AC,1},
            {0x1B9119C,0x1AD6C00},{0x1B911A0,0x1AD6C0C},{0x1B911A4,0x1AD6D80},
            {0x1AD6C00,0x1B91190},{0x1AD6C04,0x1AD6B78},{0x1AD6C08,0x1B36B78},
            {0x1B4C6E0,0x1B4C6E8},{0x1B4C6E8,0x186A628},{0x1B4C6EC,0},{0x1B4C6F0,0},
            {0x1B4C6F4,0x1B91190},{0x1B4C6F8,0},{0x1B4C6FC,0},{0x1B4C700,0},
            {0x1B4C704,0x16F8410},{0x1B4C708,0x16F8320},{0xE000010,0xD1},
            {0x1B92A70,0x1B92A75},{0x1B92A78,0x1B91190},
            {0x1B92A7C,0x1AD6D90},{0x1B92A80,0x1AD6DA0},{0x1B92A84,0x1AD8D90},
            {0x1AD6D90,0x1B4C6E8},{0x1AD6D94,0x16F8410},{0x1AD6D98,0x16F8320},{0x1AD6D9C,17}
        };
        bool state_matches = true;
        uint32_t word = 0;
        for (const auto &expected : final_words) {
            // A third allocation changes these free-list/accounting words;
            // its final values are checked by the registration mode below.
            if (execute_registration && (expected[0] == 0x1AD8D94 || expected[0] == 0x1B36B68 ||
                expected[0] == 0x1AD6BE8 || expected[0] == 0x1AD6BEC || expected[0] == 0x1AD8D98 ||
                expected[0] == 0x1AD8D9C || expected[0] == 0x1B91164)) continue;
            if (complete_constructor && (expected[0] == 0x1B4C6EC || expected[0] == 0x1B4C6F0)) continue;
            state_matches &= ds_image_read_be32(image, expected[0], &word) && word == expected[1];
        }
        const auto *initialized = static_cast<const unsigned char *>(ds_image_address(image, 0x1B92A88, 1, DS_PF_R));
        const auto *guard = static_cast<const unsigned char *>(ds_image_address(image, 0x1B925B0, 1, DS_PF_R));
        registry_prefix_matches = startup_prefix_matches && memcmp(result->functions + 95, suffix, sizeof(suffix)) == 0;
        registry_state_matches = state_matches && initialized && *initialized == 1 && guard && *guard == 1;
        matched = result->stop == DS_ENTRY_REGISTRY_READY && result->callee == 0xAE4490 &&
            startup_prefix_matches && result->function_count == 115 &&
            memcmp(result->functions + 95, suffix, sizeof(suffix)) == 0 &&
            state_matches && initialized && *initialized == 1 && guard && *guard == 1 &&
            result->registry_initialized && result->registry_address == 0x1B92A78 &&
            result->registry_payload == 0x1AD6D90 && result->registry_entries == 1 &&
            result->registry_allocation_address == result->registry_payload && result->registry_allocation_size == 0x2000 &&
            result->pool_initialized && result->singleton_completed && result->allocation_calls == 2 &&
            result->allocation_address == 0x1AD6C00 && result->allocation_size == 0x180 &&
            result->atomic_successes == 1 && result->bootstrap_fallback_calls == 0 &&
            result->lock_calls == 4 && result->unlock_calls == 4 && ds_sync_held() == 0 &&
            result->rwlock_write_locks == 1 && result->rwlock_write_unlocks == 1 && ds_rwlock_held() == 0 &&
            result->mutex_count == 12 && result->rwlock_create_calls == 1 && ds_rwlock_contains(result->rwlock_handle) &&
            result->memory_query_count == 1 && result->clock_calls == 1 && !result->constructor_completed &&
            result->toc == image->toc && result->thread_pointer == kMainTlsBase + 0x7000 &&
            result->lr == 0xAE50E0 && result->stack_pointer == 0xFEFFC30;
    }
    if (execute_registration) {
        static constexpr uint32_t suffix[] = {
            0xAE4490,0xB0AF28,0xB25E58,0xA34BB8,0xA34C48,0xB27658,0xA664E0,0x16BB244,
            0xAC8578,0xAC8078,0xA662C8,0x16BB264,0x7CEE00,0x7CEE18,0x7CEE30,0x7CEE48,
            0xAE47A0,0xAE47A0,0xAE47A0,0xAE47A0,0xAE0BE8,0x7CEE78
        };
        static constexpr uint32_t final_words[][2] = {
            {0x1AD8D94,0x1AD9DA0},{0x1AD8D98,0},
            {0x1AD9DA0,0x1AD8D90},{0x1AD9DA4,0x1B36B68},{0x1B36B68,0x1AD9DA0},
            {0x1AD6BE8,0x1AD9DA8},{0x1AD6BEC,0x1AD9DA8},
            {0x1AD9DA8,0x1AD6BE8},{0x1AD9DAC,0x1AD6BE8},{0x1B91164,0x5CDC8},
            {0x1B92A54,0x1B92A59},{0x1B92A5C,0x1B91190},
            {0x1B92A60,0x1AD8DA0},{0x1B92A64,0x1AD8DC0},{0x1B92A68,0x1AD9DA0},
            {0x1AD8DA0,0x1B4C70C},{0x1AD8DA4,0x1B4C6E8},
            {0x1AD8DA8,0x1B4C70D},{0x1AD8DAC,0x1B4C6E8},
            {0x1AD8DB0,0x1B4C70E},{0x1AD8DB4,0x1B4C6E8},
            {0x1AD8DB8,0x1B4C70F},{0x1AD8DBC,0x1B4C6E8}
        };
        bool state_matches = registry_state_matches;
        registration_prefix_matches = registry_prefix_matches && memcmp(result->functions + 115, suffix, sizeof(suffix)) == 0;
        uint32_t word = 0;
        for (const auto &expected : final_words)
            state_matches &= ds_image_read_be32(image, expected[0], &word) && word == expected[1];
        const auto *initialized = static_cast<const unsigned char *>(ds_image_address(image, 0x1B92A6C, 1, DS_PF_R));
        const auto *guard = static_cast<const unsigned char *>(ds_image_address(image, 0x1B925B8, 1, DS_PF_R));
        matched = result->stop == DS_ENTRY_REGISTRATION_READY && result->callee == 0x782468 &&
            registry_prefix_matches && result->function_count == 137 &&
            memcmp(result->functions + 115, suffix, sizeof(suffix)) == 0 &&
            state_matches && initialized && *initialized == 1 && guard && *guard == 1 &&
            result->registration_completed && result->callbacks_initialized &&
            result->callback_registry_address == 0x1B92A5C && result->callback_payload == 0x1AD8DA0 &&
            result->callback_entries == 4 && result->callback_allocation_address == result->callback_payload &&
            result->callback_allocation_size == 0x1000 && result->registry_initialized &&
            result->registry_entries == 1 && result->registry_allocation_address == 0x1AD6D90 &&
            result->registry_allocation_size == 0x2000 && result->pool_initialized && result->singleton_completed &&
            result->allocation_calls == 3 && result->allocation_address == 0x1AD6C00 && result->allocation_size == 0x180 &&
            result->atomic_successes == 1 && result->bootstrap_fallback_calls == 0 &&
            result->lock_calls == 5 && result->unlock_calls == 5 && ds_sync_held() == 0 &&
            result->rwlock_write_locks == 1 && result->rwlock_write_unlocks == 1 && ds_rwlock_held() == 0 &&
            result->mutex_count == 12 && result->rwlock_create_calls == 1 && ds_rwlock_contains(result->rwlock_handle) &&
            result->memory_query_count == 1 && result->clock_calls == 1 && !result->constructor_completed &&
            result->toc == image->toc && result->thread_pointer == kMainTlsBase + 0x7000 &&
            result->lr == 0x7CF770 && result->stack_pointer == 0xFEFFD40 && result->r3 == 0x1B4C6E8;
    }
    if (complete_constructor) {
        static constexpr uint32_t suffix[] = {
            0x782468,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xADE478,
            0x7CEE78,0x7CEE78,0x7CEC40,0x7CEE78,0x7CEC48,0xAE42C8,0xAE3AC8,
            0xA941F8,0xB27730,0xA664E0,0x16BB244,0xAC85B8,0xAC8078,0xA662C8,0x16BB264,
            0xB25E58,0xA34BB8,0xA34C48,0xB25E58,0xA34BB8,0xA34C48,0xA8CFA8,0xA8CE10,
            0xA8CA28,0xB27658,0xA664E0,0x16BB244,0xAC8578,0xAC8078,0xA662C8,0x16BB264,0xAE3D68
        };
        static constexpr uint32_t final_words[][2] = {
            {0x1B4C6EC,0x1B4BB0C},{0x1B4C6F0,0x1AD9DB0},
            {0x1B4BB04,0x1B4BB0C},{0x1B4BB0C,0x18682C0},{0x1B4BB10,0},{0x1B4BB14,0},
            {0x1B4BB18,0x1B91190},{0x1B4BB1C,0},{0x1B4BB20,0},{0x1B4BB24,0},
            {0x1B4BB28,0},{0x1B4BB2C,0},
            {0x1AD9DB0,0x1B4C6E8},{0x1AD9DB4,0x16F8410},{0x1AD9DB8,0x16F8320},
            {0x1AD9DBC,0x1B91190},{0x1AD9DC0,0x1AD9E00},{0x1AD9DC4,0x1AD9E04},{0x1AD9DC8,0x1AD9E10},
            {0x1AD9DCC,0x1B91190},{0x1AD9DD0,0},{0x1AD9DD4,0},{0x1AD9DD8,0},{0x1AD9DDC,0},
            {0x1AD9DE0,0x1874B50},{0x1AD9DE4,0x1001},
            {0x1AD9E00,0x1B4C6DC},{0x1B4C6DC,0x186A668},
            {0x1AD9DA0,0x1AD8D90},{0x1AD9DA4,0x1AD9DF0},{0x1AD9DA8,0},
            {0x1AD9DF0,0x1AD9DA0},{0x1AD9DF4,0x1AD9E10},{0x1AD9DF8,0},
            {0x1AD9E10,0x1AD9DF0},{0x1AD9E14,0x1B36B68},{0x1B36B68,0x1AD9E10},
            {0x1AD6BE8,0x1AD9E18},{0x1AD6BEC,0x1AD9E18},
            {0x1AD9E18,0x1AD6BE8},{0x1AD9E1C,0x1AD6BE8},{0x1B91164,0x5CD58},
            {0x1B92A54,0x1B92A59},{0x1B92A5C,0x1B91190},
            {0x1B92A60,0x1AD8DA0},{0x1B92A64,0x1AD8DC0},{0x1B92A68,0x1AD9DA0},
            {0x1AD8DA0,0x1B4C70C},{0x1AD8DA4,0x1B4C6E8},{0x1AD8DA8,0x1B4C70D},{0x1AD8DAC,0x1B4C6E8},
            {0x1AD8DB0,0x1B4C70E},{0x1AD8DB4,0x1B4C6E8},{0x1AD8DB8,0x1B4C70F},{0x1AD8DBC,0x1B4C6E8}
        };
        bool state_matches = registry_state_matches;
        uint32_t word = 0;
        for (const auto &expected : final_words)
            state_matches &= ds_image_read_be32(image, expected[0], &word) && word == expected[1];
        bool second_state = true;
        if (complete_second_constructor) {
            static constexpr uint32_t words[][2] = {
                {0x1B49B98,0x1852728},{0x1B49B9C,0},{0x1B49BA0,0x1852738},{0x1B49BA4,0},
                {0x1B49BA8,0x1852748},{0x1B49BAC,0},
                {0x1B49BB0,0x1852728},{0x1B49BB4,0},{0x1B49BB8,0x1852738},{0x1B49BBC,0}
            };
            for (const auto &expected : words)
                second_state &= ds_image_read_be32(image, expected[0], &word) && word == expected[1];
        }
        static constexpr uint32_t second_suffix[] = {0x66D688,0x66D590,0xDBA100};
        matched = result->stop == (complete_second_constructor ? DS_ENTRY_SECOND_CONSTRUCTOR_READY : DS_ENTRY_FIRST_CONSTRUCTOR_READY) &&
            result->constructor_completed && result->constructors_completed == (complete_second_constructor ? 2u : 1u) &&
            registration_prefix_matches && result->function_count == (complete_second_constructor ? 179u : 176u) &&
            second_state && (!complete_second_constructor || memcmp(result->functions + 176, second_suffix, sizeof(second_suffix)) == 0) &&
            memcmp(result->functions + 137, suffix, sizeof(suffix)) == 0 && state_matches &&
            result->second_singleton_address == 0x1B4BB0C && result->node_address == 0x1AD9DB0 &&
            result->node_vector_address == 0x1AD9E00 && result->allocation_calls == 5 &&
            result->kernel_mutex_calls == 1 && result->kernel_mutex_handle == 0x1001 &&
            ds_kernel_mutex_contains(result->kernel_mutex_handle) &&
            result->kernel_mutex_output == 0x1AD9DE4 && result->kernel_mutex_attribute == 0xFEFFB90 &&
            result->lock_calls == 7 && result->unlock_calls == 7 && !ds_sync_held() &&
            result->rwlock_write_locks == 1 && result->rwlock_write_unlocks == 1 && !ds_rwlock_held() &&
            result->mutex_count == 12 && result->rwlock_create_calls == 1 && result->atomic_successes == 1 &&
            !result->bootstrap_fallback_calls && result->toc == image->toc &&
            result->lr == 0x16B84F4 && result->stack_pointer == 0xFEFFE00 &&
            result->next_constructor_descriptor == (complete_second_constructor ? 0x19359B0u : 0x1947390u) &&
            result->next_constructor_target == (complete_second_constructor ? 0x3F4FE0u : 0x66D688u);
    }
    if (!matched && (result->stop == DS_ENTRY_TLS_BOUNDARY || result->stop == DS_ENTRY_TLS_INITIALIZED ||
                     result->stop == DS_ENTRY_MEMORY_QUERY_BOUNDARY || result->stop == DS_ENTRY_TIME_BOUNDARY ||
                     result->stop == DS_ENTRY_CONSTRUCTOR_BOUNDARY || result->stop == DS_ENTRY_SINGLETON_BOUNDARY ||
                     result->stop == DS_ENTRY_RWLOCK_INITIALIZED || result->stop == DS_ENTRY_MANAGER_POOL_BOUNDARY ||
                     result->stop == DS_ENTRY_SINGLETON_READY || result->stop == DS_ENTRY_REGISTRY_READY ||
                     result->stop == DS_ENTRY_REGISTRATION_READY || result->stop == DS_ENTRY_FIRST_CONSTRUCTOR_READY ||
                     result->stop == DS_ENTRY_SECOND_CONSTRUCTOR_READY))
        result->stop = DS_ENTRY_ABI_MISMATCH;
    if (!matched && error && error_capacity && !error[0])
        snprintf(error, error_capacity, "Entry stopped outside the selected startup checkpoint (stop=%d)",
                 static_cast<int>(result->stop));
    if (!clear_active()) matched = false;
    return matched;
}

extern "C" bool ds_entry_run_exploration(DsImage *image, DsEntryResult *result,
                                        char *error, size_t error_capacity, uint32_t constructor_limit)
{
    return run_entry(image, result, error, error_capacity, true, true, true, true, true, true, true, true, true, true, true, true, true, constructor_limit);
}

static bool run_metadata_constructors(DsImage *image, DsEntryResult *result,
                                      char *error, size_t error_capacity, uint32_t constructor_count,
                                      bool runtime_heap = false, bool allocated = false, bool locale_object = false,
                                      bool locale_published = false, bool stream_locale = false, bool first_facet = false, bool first_stream = false, bool narrow_streams = false, bool wide_streams = false)
{
    struct Profile {
        uint32_t count, functions, accesses, target, descriptor, names, keys;
        uint32_t allocations, kernel_creates, kernel_handle, locks, free_bytes;
        DsEntryStop ready;
    };
    static constexpr Profile profiles[] = {
        {3,213,4141,0x9E54C8,0x195FFE0,2,8,5,1,0x1001,7,0x5CD58,DS_ENTRY_THIRD_CONSTRUCTOR_READY},
        {4,354,5937,0x130D38,0x1923818,3,12,12,4,0x1004,14,0x5CBB8,DS_ENTRY_FOURTH_CONSTRUCTOR_READY},
        {6,424,7407,0xBF6E0,0x1920778,5,20,12,4,0x1004,14,0x5CBB8,DS_ENTRY_SIXTH_CONSTRUCTOR_READY},
        {9,561,10052,0x66A4A8,0x1947270,8,32,14,5,0x1005,16,0x5CB48,DS_ENTRY_NINTH_CONSTRUCTOR_READY},
        {10,600,11007,0x5FD720,0x1943CD0,9,36,14,5,0x1005,16,0x5CB48,DS_ENTRY_TENTH_CONSTRUCTOR_READY},
        {11,603,11044,0x3F46B0,0x1935910,9,36,14,5,0x1005,16,0x5CB48,DS_ENTRY_ELEVENTH_CONSTRUCTOR_READY},
        {12,641,12134,0x15166F0,0x19A3EA0,10,40,14,5,0x1005,16,0x5CB48,DS_ENTRY_TWELFTH_CONSTRUCTOR_READY}
    };
    const Profile *profile = nullptr;
    for (const auto &candidate : profiles)
        if (candidate.count == constructor_count) profile = &candidate;
    static constexpr Profile heap_profile = {
        12,652,12206,0x15166F0,0x19A3EA0,10,40,14,5,0x1005,17,0x5CB48,DS_ENTRY_RUNTIME_HEAP_BOUNDARY
    };
    if (runtime_heap && constructor_count == 12) profile = &heap_profile;
    static constexpr Profile allocated_profile = {
        12,658,12247,0x15166F0,0x19A3EA0,10,40,14,5,0x1005,17,0x5CB48,DS_ENTRY_RUNTIME_ALLOCATED
    };
    if (allocated && constructor_count==12) profile=&allocated_profile;
    static constexpr Profile locale_profile = {
        12,669,12314,0x15166F0,0x19A3EA0,10,40,14,5,0x1005,18,0x5CB48,DS_ENTRY_LOCALE_OBJECT_READY
    };
    if (locale_object && constructor_count==12) profile=&locale_profile;
    static constexpr Profile published_profile = {
        12,697,12492,0x15166F0,0x19A3EA0,10,40,14,5,0x1005,19,0x5CB48,DS_ENTRY_LOCALE_PUBLISHED
    };
    if (locale_published && constructor_count==12) profile=&published_profile;
    static constexpr Profile stream_profile = {
        12,747,12849,0x15166F0,0x19A3EA0,10,40,14,5,0x1005,23,0x5CB48,DS_ENTRY_STREAM_LOCALE_READY
    };
    if (stream_locale && constructor_count==12) profile=&stream_profile;
    static constexpr Profile facet_profile = {
        12,799,13238,0x15166F0,0x19A3EA0,10,40,14,5,0x1005,25,0x5CB48,DS_ENTRY_FIRST_FACET_READY
    };
    if (first_facet && constructor_count==12) profile=&facet_profile;
    static constexpr Profile first_stream_profile = {
        12,810,13312,0x15166F0,0x19A3EA0,10,40,14,5,0x1005,26,0x5CB48,DS_ENTRY_FIRST_STREAM_READY
    };
    if (first_stream && constructor_count==12) profile=&first_stream_profile;
    static constexpr Profile narrow_profile = {
        12,966,14378,0x15166F0,0x19A3EA0,10,40,14,5,0x1005,42,0x5CB48,DS_ENTRY_NARROW_STREAMS_READY
    };
    if (narrow_streams && constructor_count==12) profile=&narrow_profile;
    static constexpr Profile wide_profile = {
        13,1265,16590,0x745248,0x194CF48,10,40,14,5,0x1005,66,0x5CB48,DS_ENTRY_WIDE_STREAMS_READY
    };
    if (wide_streams && constructor_count==12) profile=&wide_profile;


    if (!profile) {
        if (result) { memset(result,0,sizeof(*result)); result->stop = DS_ENTRY_BAD_INPUT; }
        if (error && error_capacity) snprintf(error,error_capacity,"Unknown metadata checkpoint");
        return false;
    }
    const bool fourth = constructor_count >= 4;
    const bool sixth = constructor_count >= 6;
    const bool ninth = constructor_count >= 9;
    const bool tenth = constructor_count >= 10;
    const bool twelfth = constructor_count >= 12;
    const uint32_t stop_toc = !wide_streams && twelfth && (!first_facet || first_stream) ? 0x19D3BACu : image ? image->toc : 0u;
    const uint32_t name_count = profile->names, key_count = profile->keys;
    const bool held_guard=(locale_object && !locale_published) || (stream_locale && !first_facet);
    const bool unexpected_success = (runtime_heap || allocated)
        ? run_entry(image,result,error,error_capacity,true,true,true,true,true,true,true,true,true,true,true,true,true,wide_streams?13u:0u,runtime_heap,allocated,locale_object,locale_published,stream_locale,first_facet,first_stream,narrow_streams,wide_streams)
        : ds_entry_run_exploration(image, result, error, error_capacity, constructor_count);
    if (unexpected_success || !image || !result || result->stop != DS_ENTRY_EXECUTION_LIMIT) return false;
    if (result->constructors_completed != profile->count || result->function_count != profile->functions ||
        result->callee != (wide_streams ? 0x745248u : narrow_streams ? 0x168BC68u : first_stream ? 0x168ABD0u : first_facet ? 0x9E96A0u : locale_published ? 0x168ABD0u : locale_object ? 0x1676088u : allocated ? 0x16896C0u : runtime_heap ? 0x168ABD0u : profile->target) ||
        result->next_constructor_descriptor != profile->descriptor || result->next_constructor_target != profile->target ||
        result->stack_pointer != (wide_streams ? 0xFEFFE00u : narrow_streams ? 0xFEFFD90u : first_stream ? 0xFEFFA00u : stream_locale ? 0xFEFFA90u : locale_published ? 0xFEFFB60u : locale_object ? 0xFEFFA30u : allocated ? 0xFEFFBF0u : runtime_heap ? 0xFEFFB60u : 0xFEFFE00u) ||
        result->lr != (wide_streams ? 0x16B84F4u : narrow_streams ? 0x15165E8u : first_stream ? 0x1685FECu : first_facet ? 0x1688280u : stream_locale ? 0x1688380u : locale_published ? 0x16A051Cu : locale_object ? 0x16891DCu : allocated ? 0x1686E14u : runtime_heap ? 0x16A051Cu : 0x16B84F4u) || result->toc != stop_toc ||
        result->allocation_calls != profile->allocations || result->kernel_mutex_calls != profile->kernel_creates ||
        result->kernel_mutex_handle != profile->kernel_handle || result->lock_calls != profile->locks ||
        result->unlock_calls != profile->locks-(held_guard?1u:0u) ||
        result->native_held_at_stop != (held_guard?1u:0u) ||
        result->rwlock_write_locks != 1 || result->rwlock_write_unlocks != 1 ||
        result->atomic_successes != 1 || result->bootstrap_fallback_calls || result->native_live_after_cleanup ||
        result->native_rwlocks_after_cleanup || result->native_kernel_mutexes_after_cleanup) {
        result->stop = DS_ENTRY_ABI_MISMATCH;
        if (error && error_capacity) snprintf(error,error_capacity,"Metadata checkpoint trace, ABI or resource mismatch");
        return false;
    }
    static constexpr uint32_t suffix[] = {
        0x3F4FE0,0x3F4E10,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,
        0xAE4D38,0xAE2148,0xADEBA0,0x3F4748,0x3F4750,0x10BA0,0x16693E0,
        0xADCAD8,0xADCAB0,0xADCAB0,0xAE4490,0x3F4768,0x3F4780,0x3F4798,0x3F47B0,
        0xAE47A0,0xAE47A0,0xAE47A0,0xAE47A0,0xAE0BE8,0xCF620,0xB0AF28,0xAE7768,
        0xB25E58,0xA34BB8,0xA34C48,0xADE478
    };
    static constexpr uint32_t words[][2] = {
        {0x1B4C6EC,0x1B4BB0C},{0x1B4C6F0,0x1AD9DB0},{0x1AD9DE4,0x1001},
        {0x1AD9DC0,0x1AD9E00},{0x1AD9DC4,0x1AD9E04},{0x1AD9DC8,0x1AD9E10},
        {0x1AD9E00,0x1B4C6DC},
        {0x1B428A8,0x1B428B0},{0x1B428B0,0x185D6A8},{0x1B428B4,0x1B37D28},{0x1B428B8,0},
        {0x1B428BC,0x1B91190},{0x1B428CC,0x16DBDA0},{0x1B428D0,0x16DBD70},
        {0x1B37D20,0x1B37D28},{0x1B37D28,0x1855548},
        {0x1B92A7C,0x1AD6D90},{0x1B92A84,0x1AD8D90},
        {0x1B92A60,0x1AD8DA0},{0x1B92A68,0x1AD9DA0},
    };
    bool matched = result->trace_count == profile->accesses &&
                   memcmp(result->functions + 179, suffix, sizeof(suffix)) == 0;
    uint32_t bad_address = 0, bad_actual = 0, bad_expected = 0;
    const auto word_matches = [&](uint32_t address, uint32_t expected) {
        uint32_t actual = 0;
        if (ds_image_read_be32(image, address, &actual) && actual == expected) return true;
        if (!bad_address) { bad_address = address; bad_actual = actual; bad_expected = expected; }
        return false;
    };
    for (const auto &expected : words)
        matched &= word_matches(expected[0], expected[1]);
    const uint32_t counts[][2] = {
        {0x1B91164,profile->free_bytes},
        {0x1B92A80,0x1AD6D90u + name_count * 16},
        {0x1B92A64,0x1AD8DA0u + key_count * 8}
    };
    for (const auto &expected : counts)
        matched &= word_matches(expected[0], expected[1]);
    // Complete records in length order, retaining insertion order for ties.
    // Later constructors insert earlier in the buffer; compare every field.
    static constexpr uint32_t names[][5] = {
        {8,0x1B482C8,0x16EB678,0x16EB668,6},
        {10,0x1B49B70,0x16ED920,0x16ED888,7},
        {5,0x1B385D0,0x16C6EB0,0x16C6E98,11},
        {7,0x1B37AA8,0x16C39F0,0x16C39D0,13},
        {1,0x1B4C6E8,0x16F8410,0x16F8320,17},
        {4,0x1B4F8BC,0x1702660,0x1702608,17},
        {9,0x1B4E560,0x16FCB10,0x16FCA70,18},
        {3,0x1B428B0,0x16DBDA0,0x16DBD70,23},
        {12,0x1B42878,0x16DBD50,0x16DBD18,26},
        {6,0x1B36EB0,0x16C21C8,0x16C2180,33}
    };
    uint32_t name_index = 0;
    for (const auto &name : names) {
        if (name[0] > constructor_count) continue;
        for (uint32_t field = 0; field < 4; ++field)
            matched &= word_matches(0x1AD6D90 + name_index * 16 + field * 4, name[field+1]);
        ++name_index;
    }
    // Each type supplies four opaque BSS keys. This table is already in the
    // required ascending order; these addresses are data, not callable code.
    static constexpr uint32_t key_groups[][3] = {
        {6,0x1B36ED4,0x1B36EB0},{7,0x1B37ACC,0x1B37AA8},
        {5,0x1B385F4,0x1B385D0},{12,0x1B4289C,0x1B42878},{3,0x1B428D4,0x1B428B0},
        {8,0x1B482EC,0x1B482C8},{10,0x1B49B94,0x1B49B70},
        {1,0x1B4C70C,0x1B4C6E8},
        {9,0x1B4E584,0x1B4E560},{4,0x1B4F8E0,0x1B4F8BC}
    };
    uint32_t key_index = 0;
    for (const auto &group : key_groups) {
        if (group[0] > constructor_count) continue;
        for (uint32_t offset = 0; offset < 4; ++offset, ++key_index) {
            matched &= word_matches(0x1AD8DA0 + key_index * 8, group[1] + offset);
            matched &= word_matches(0x1AD8DA4 + key_index * 8, group[2]);
        }
    }
    matched &= name_index == name_count && key_index == key_count;
    static constexpr uint32_t fourth_suffix[] = {
        0x9E54C8,0x9E4F30,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xAE4D38,0xAE2148,0xADEBA0,
        0x9E3C00,0x9E3C08,0x10BA0,0x16693E0,0xADCAD8,0xADCAD8,0xADCAB0,0xADCAB0,0xAE4490,0x9E3E50,
        0x9E3E68,0x9E3E80,0x9E3E98,0xAE47A0,0xAE47A0,0xAE47A0,0xAE47A0,0xAE0BE8,0x95A070,0xB0AF28,
        0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xADE478,0x9E3C00,0x9E3C08,0xAE42C8,0xAE3AC8,0xA941F8,
        0xB27730,0xA664E0,0x16BB244,0xAC85B8,0xAC8078,0xA662C8,0x16BB264,0xB25E58,0xA34BB8,0xA34C48,
        0xB25E58,0xA34BB8,0xA34C48,0xA8CFA8,0xA8CE10,0xA8CA28,0xB27658,0xA664E0,0x16BB244,0xAC8578,
        0xAC8078,0xA662C8,0x16BB264,0xAE3D68,0xAE60A0,0x10BA0,0x16693E0,0xAE1408,0xB27658,0xA664E0,
        0x16BB244,0xAC8578,0xAC8078,0xA662C8,0x16BB264,0xA941F8,0xB27730,0xA664E0,0x16BB244,0xAC85B8,
        0xAC8078,0xA662C8,0x16BB264,0xB25E58,0xA34BB8,0xA34C48,0xB25E58,0xA34BB8,0xA34C48,0xA8CFA8,
        0xA8CE10,0xA8CA28,0xADF018,0x10BA0,0x16693E0,0xADCB28,0xADCB28,0xB27658,0xA664E0,0x16BB244,
        0xAC8578,0xAC8078,0xA662C8,0x16BB264,0xAE5B40,0xAE60A0,0x10BA0,0x16693E0,0x1675F30,0xAE1408,
        0xA941F8,0xB27730,0xA664E0,0x16BB244,0xAC85B8,0xAC8078,0xA662C8,0x16BB264,0xB25E58,0xA34BB8,
        0xA34C48,0xB25E58,0xA34BB8,0xA34C48,0xA8CFA8,0xA8CE10,0xA8CA28,0xADF018,0x10BA0,0x16693E0,
        0xADCB50,0xADCB28,0xADCB28,0xB27658,0xA664E0,0x16BB244,0xAC8578,0xAC8078,0xA662C8,0x16BB264,
        0xAE5B40,
    };
    if (fourth) {
        matched &= memcmp(result->functions + 213, fourth_suffix, sizeof(fourth_suffix)) == 0;
        static constexpr uint32_t fourth_words[][2] = {
            {0x1B4F8B4,0x1B4F8BC},{0x1B4F8BC,0x1872B68},{0x1B4F8C0,0x1B4F634},
            {0x1B4F634,0x1871978},{0x1B4F8C4,0x1AD9E20},{0x1B4F8C8,0x1B91190},
            {0x1B4F8CC,0x1AD9E90},{0x1B4F8D0,0x1AD9EB0},{0x1B4F8D4,0x1AD9ED0},
            {0x1B4F8D8,0x1702660},{0x1B4F8DC,0x1702608},
            {0x1B4F898,0x1872BA8},{0x1B4F89C,0x1872BD0},{0x1B4F8A0,0x195FF58},{0x1B4F8A4,0},
            {0x1B4F8A8,0x1872BF8},{0x1B4F8AC,0x195FF50},{0x1B4F8B0,0},
            {0x1AD9E90,0x1AD9EE0},{0x1AD9E94,0x1702678},{0x1AD9E98,0x1702630},{0x1AD9E9C,10},
            {0x1AD9EA0,0x1AD9F50},{0x1AD9EA4,0x1702688},{0x1AD9EA8,0x1702648},{0x1AD9EAC,10},
            {0x1AD9FB0,0x1AD9F90},{0x1AD9FB4,0x1B36B68},
            {0x1AD9FB8,0x1AD6BE8},{0x1AD9FBC,0x1AD6BE8}
        };
        for (const auto &expected : fourth_words) {
            if (ninth && expected[0] >= 0x1AD9FB0 && expected[0] < 0x1AD9FC0) continue;
            matched &= word_matches(expected[0], expected[1]);
        }
        // Actual node constructors create an owner/name/signature tuple, a
        // one-pointer vector and a recursive mutex. These are data registrations;
        // the registered SetChannel/GetChannel functions are not executed here.
        static constexpr uint32_t nodes[][6] = {
            {0x1AD9E20,0x1702660,0x1702608,0x1AD9E70,0x1B4F898,0x1002},
            {0x1AD9EE0,0x1702678,0x1702630,0x1AD9F30,0x1B4F89C,0x1003},
            {0x1AD9F50,0x1702688,0x1702648,0x1AD9FA0,0x1B4F8A8,0x1004}
        };
        for (const auto &node : nodes) {
            const uint32_t fields[][2] = {
                {0,0x1B4F8BC},{4,node[1]},{8,node[2]},{12,0x1B91190},
                {16,node[3]},{20,node[3]+4},{24,node[3]+16},{28,0x1B91190},
                {32,0},{36,0},{40,0},{44,0},{48,0x1874B50},{52,node[5]}
            };
            for (const auto &field : fields)
                matched &= word_matches(node[0]+field[0], field[1]);
            matched &= word_matches(node[3], node[4]);
        }
    }
    static constexpr uint32_t sixth_suffix[] = {
        0x130D38,0x130B68,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xAE4D38,0xAE2148,0xADEBA0,
        0x130430,0x130438,0x10BA0,0x16693E0,0xADCAD8,0xADCAD8,0xADCAB0,0xADCAB0,0xAE4490,0x130450,
        0x130468,0x130480,0x130498,0xAE47A0,0xAE47A0,0xAE47A0,0xAE47A0,0xAE0BE8,0x12E718,0xB0AF28,
        0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xADE478,0x6B9E8,0x6B818,0xB0AF28,0xAE7768,0xB25E58,
        0xA34BB8,0xA34C48,0xAE4D38,0xAE2148,0xADEBA0,0x6B270,0x6B278,0x10BA0,0x16693E0,0xADCAD8,
        0xADCAD8,0xADCAB0,0xADCAB0,0xAE4490,0x6B290,0x6B2A8,0x6B2C0,0x6B2D8,0xAE47A0,0xAE47A0,
        0xAE47A0,0xAE47A0,0xAE0BE8,0x65EA0,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xADE478,
    };
    if (sixth) {
        matched &= memcmp(result->functions + 354, sixth_suffix, sizeof(sixth_suffix)) == 0;
        static constexpr uint32_t new_types[][6] = {
            {0x1B385C8,0x1B385D0,0x1856410,0x1B38560,0x16C6EB0,0x16C6E98},
            {0x1B36EA8,0x1B36EB0,0x18538E8,0x1B36D60,0x16C21C8,0x16C2180}
        };
        for (const auto &type : new_types) {
            matched &= word_matches(type[0], type[1]);
            const uint32_t fields[] = {type[2],type[3],0,0x1B91190,0,0,0,type[4],type[5]};
            for (uint32_t i = 0; i < sizeof(fields)/sizeof(fields[0]); ++i)
                matched &= word_matches(type[1]+i*4, fields[i]);
        }
        matched &= word_matches(0x1B38558,0x1B38560) && word_matches(0x1B38560,0x1856350) &&
                   word_matches(0x1B36D58,0x1B36D60) && word_matches(0x1B36D60,0x1853660);
    }
    static constexpr uint32_t ninth_suffix[] = {
        0xBF6E0,0xBF500,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xAE4D38,0xAE2148,0xADEBA0,
        0xBE9D0,0xBE9D8,0x10BA0,0x16693E0,0xADCAD8,0xADCAD8,0xADCAD8,0xADCAB0,0xADCAB0,0xAE4490,
        0xBE9F0,0xBEA08,0xBEA20,0xBEA38,0xAE47A0,0xAE47A0,0xAE47A0,0xAE47A0,0xAE0BE8,0xCFCDB0,
        0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xADE478,0x5A9DC0,0x5A9BF0,0xB0AF28,0xAE7768,
        0xB25E58,0xA34BB8,0xA34C48,0xAE4D38,0xAE2148,0xADEBA0,0x5A97B0,0x5A97B8,0x10BA0,0x16693E0,
        0xADCAD8,0xADCAD8,0xADCAD8,0xADCAB0,0xADCAB0,0xAE4490,0x5A97D0,0x5A97E8,0x5A9800,0x5A9818,
        0xAE47A0,0xAE47A0,0xAE47A0,0xAE47A0,0xAE0BE8,0x582130,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,
        0xA34C48,0xADE478,0x8B89D0,0x8B8600,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xAE4D38,
        0xAE2148,0xADEBA0,0x8B7B68,0x8B7B70,0x10BA0,0x16693E0,0xADCAD8,0xADCAD8,0xADCAD8,0xADCAB0,
        0xADCAB0,0xAE4490,0x8B7CB8,0x8B7CD0,0x8B7CE8,0x8B7D00,0xAE47A0,0xAE47A0,0xAE47A0,0xAE47A0,
        0xAE0BE8,0x71AE20,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xADE478,0x8B7B68,0x8B7B70,
        0xAE42C8,0xAE3AC8,0xA941F8,0xB27730,0xA664E0,0x16BB244,0xAC85B8,0xAC8078,0xA662C8,0x16BB264,
        0xB25E58,0xA34BB8,0xA34C48,0xB25E58,0xA34BB8,0xA34C48,0xA8CFA8,0xA8CE10,0xA8CA28,0xB27658,
        0xA664E0,0x16BB244,0xAC8578,0xAC8078,0xA662C8,0x16BB264,0xAE3D68,
    };
    if (ninth) {
        matched &= memcmp(result->functions + 424, ninth_suffix, sizeof(ninth_suffix)) == 0;
        static constexpr uint32_t types[][7] = {
            {0x1B37AA0,0x1B37AA8,0x1855050,0x1BB6568,0x16C39F0,0x16C39D0,0},
            {0x1B482C0,0x1B482C8,0x1861148,0x1B48068,0x16EB678,0x16EB668,0},
            {0x1B4E558,0x1B4E560,0x186F4A8,0x1B4AD9C,0x16FCB10,0x16FCA70,0x1AD9FC0}
        };
        for (const auto &type : types) {
            matched &= word_matches(type[0], type[1]);
            const uint32_t fields[] = {type[2],type[3],type[6],0x1B91190,0,0,0,type[4],type[5]};
            for (uint32_t i = 0; i < sizeof(fields)/sizeof(fields[0]); ++i)
                matched &= word_matches(type[1]+i*4, fields[i]);
        }
        static constexpr uint32_t extra_words[][2] = {
            {0x1BB6560,0x1BB6568},{0x1BB6568,0x187B7B0},
            {0x1B48060,0x1B48068},{0x1B48068,0x1860B88},
            {0x1B4AD94,0x1B4AD9C},{0x1B4AD9C,0x1865D80},
            {0x1AD61A8,0},{0x1AD61AC,1},{0x1B4E550,0xFFFFFFFF},{0x1B4E554,0x186F4E8},
            {0x1AD9FC0,0x1B4E560},{0x1AD9FC4,0x16FCB10},{0x1AD9FC8,0x16FCA70},
            {0x1AD9FCC,0x1B91190},{0x1AD9FD0,0x1ADA010},{0x1AD9FD4,0x1ADA014},
            {0x1AD9FD8,0x1ADA020},{0x1AD9FDC,0x1B91190},
            {0x1AD9FE0,0},{0x1AD9FE4,0},{0x1AD9FE8,0},{0x1AD9FEC,0},
            {0x1AD9FF0,0x1874B50},{0x1AD9FF4,0x1005},{0x1ADA010,0x1B4E554},
            {0x1ADA020,0x1ADA000},{0x1ADA024,0x1B36B68},
            {0x1ADA028,0x1AD6BE8},{0x1ADA02C,0x1AD6BE8}
        };
        for (const auto &expected : extra_words)
            matched &= word_matches(expected[0], expected[1]);
    }
    static constexpr uint32_t tenth_suffix[] = {
        0x66A4A8,0x66A2D8,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xAE4D38,0xAE2148,0xADEBA0,
        0x669B88,0x669B90,0x10BA0,0x16693E0,0xADCAD8,0xADCAD8,0xADCAD8,0xADCAD8,0xADCAB0,0xADCAB0,
        0xAE4490,0x669BA8,0x669BC0,0x669BD8,0x669BF0,0xAE47A0,0xAE47A0,0xAE47A0,0xAE47A0,0xAE0BE8,
        0xADF198,0xAE0BE8,0x5720A8,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xADE478,
    };
    if (tenth) {
        matched &= memcmp(result->functions + 561,tenth_suffix,sizeof(tenth_suffix)) == 0;
        static constexpr uint32_t fields[][2] = {
            {0x1B49B68,0x1B49B70},{0x1B49B70,0x1864700},{0x1B49B74,0x1B47D88},
            {0x1B49B78,0},{0x1B49B7C,0x1B91190},{0x1B49B80,0},{0x1B49B84,0},{0x1B49B88,0},
            {0x1B49B8C,0x16ED920},{0x1B49B90,0x16ED888},
            {0x1B47D80,0x1B47D88},{0x1B47D88,0x18606E0}
        };
        for (const auto &field : fields) matched &= word_matches(field[0],field[1]);
    }
    if (constructor_count >= 11) {
        static constexpr uint32_t eleventh_suffix[] = {0x5FD720,0x5FD628,0xDBA100};
        matched &= memcmp(result->functions + 600,eleventh_suffix,sizeof(eleventh_suffix)) == 0;
        if (!twelfth) matched &= result->r3 == 0x1B48C64 && result->r4 == 0xFEFFDE8 &&
                                 result->r5 == 0xFEFFDE0 && result->r6 == 0;
        static constexpr uint32_t fields[][2] = {
            {0x1B48C44,0x1852728},{0x1B48C48,0},{0x1B48C4C,0x1852738},{0x1B48C50,0},
            {0x1B48C54,0x1852748},{0x1B48C58,0},{0x1B48C5C,0x1852728},{0x1B48C60,0},
            {0x1B48C64,0x1852738},{0x1B48C68,0}
        };
        for (const auto &field : fields) matched &= word_matches(field[0],field[1]);
    }
    if (twelfth) {
        static constexpr uint32_t suffix[] = {
            0x3F46B0,0x3F44E0,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,
            0xAE4D38,0xAE2148,0xADEBA0,0x3F3F10,0x3F3F18,0x10BA0,0x16693E0,
            0xADCAD8,0xADCAD8,0xADCAD8,0xADCAB0,0xADCAB0,0xAE4490,
            0x3F3F30,0x3F3F48,0x3F3F60,0x3F3F78,
            0xAE47A0,0xAE47A0,0xAE47A0,0xAE47A0,0xAE0BE8,0xADF198,0xAE0BE8,
            0xCE3DF0,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xADE478
        };
        matched &= memcmp(result->functions + 603,suffix,sizeof(suffix)) == 0;
        if (!runtime_heap && !allocated) matched &= result->r3 == 0x1B42878 && result->r4 == 0x1BB6060 &&
                                     result->r5 == 0 && result->r6 == 0x1AD8EE0;
        static constexpr uint32_t fields[][2] = {
            {0x1B42868,0x01000000},{0x1B4286C,0},{0x1B42870,0x1B42878},{0x1B42874,0},
            {0x1B42878,0x185D620},{0x1B4287C,0x1BB6060},{0x1B42880,0},{0x1B42884,0x1B91190},
            {0x1B42888,0},{0x1B4288C,0},{0x1B42890,0},{0x1B42894,0x16DBD50},
            {0x1B42898,0x16DBD18},{0x1B4289C,0},
            {0x1BB6050,0x01000000},{0x1BB6054,0},{0x1BB6058,0x1BB6060},{0x1BB605C,0},
            {0x1BB6060,0x187AF88},{0x1BB6064,0},{0x1BB6068,0},{0x1BB606C,0x1B91190},
            {0x1BB6070,0},{0x1BB6074,0},{0x1BB6078,0},{0x1BB607C,0},
            {0x1BB6080,0},{0x1BB6084,0}
        };
        for (const auto &field : fields) matched &= word_matches(field[0],field[1]);
    }
    if (runtime_heap || allocated) {
        static constexpr uint32_t suffix[] = {
            0x15166F0,0x1516590,0x1686CE8,0x166F990,0x1676150,0x16BB2E4,
            0x16BB244,0x166F910,0x1676108,0x16BB264,0x16A0500
        };
        matched &= memcmp(result->functions+641,suffix,sizeof(suffix)) == 0;
        if (!allocated) matched &= result->r3 == 40 && result->r4 == 0 && result->r5 == 0 && result->r6 == 0x1AD8EE0;
        static constexpr uint32_t fields[][2] = {
            {0x1AD6020,1},
            {0x1AD6088,0xFFFFFFFF},
            {0x1AD61B0,0},{0x1AD61B4,0},{0x1AD61B8,0},{0x1AD61BC,0},
            {0x1AD61C0,0},{0x1AD61C4,0},{0x1AD61C8,0},{0x1AD61CC,0},
            {0x1AD61D0,0},{0x1AD61D4,0}
        };
        for (const auto &field : fields) {
            const uint32_t value=wide_streams && field[0]==0x1AD6088?1u:
                wide_streams && field[0]>=0x1AD61B0 && field[0]<=0x1AD61D4 && (field[0]&7)==4?1u:field[1];
            matched &= word_matches(field[0],value);
        }
        matched &= word_matches(0x1CE64E8,locale_published?0x18C1B58u:0x18C1BF0u);
        matched &= word_matches(0x1CE64EC,allocated?DS_HEAP_BASE:0u);
    }
    if (allocated) {
        static constexpr uint32_t suffix[] = {0x168ABD0,0x167AC88,0x1676258,0x16BB2C4,0x168ABD0,0x167AC88};
        matched &= memcmp(result->functions+652,suffix,sizeof(suffix))==0;
        if (!locale_object) matched &= result->r3==DS_HEAP_BASE+48 && result->r4==0x176F780 && result->r5==0 && result->r6==0x1AD8EE0;
        matched &= result->mutex_count==(wide_streams?18u:narrow_streams?15u:stream_locale?14u:13u) && result->mutex_addresses[12]==DS_HEAP_BASE+16 &&
            (!stream_locale || result->mutex_addresses[13]==DS_HEAP_BASE+144) &&
            result->thread_id_calls==(wide_streams?58u:narrow_streams?31u:first_stream?15u:first_facet?14u:stream_locale?10u:locale_published?5u:locale_object?3u:2u) &&
            result->runtime_malloc_address==DS_HEAP_BASE && result->runtime_malloc_size==40 &&
            result->runtime_last_malloc_address==DS_HEAP_BASE+(wide_streams?656u:narrow_streams?352u:first_facet?240u:stream_locale?192u:locale_object?64u:48u) &&
            result->runtime_last_malloc_size==(narrow_streams?4u:first_facet?8u:stream_locale?4u:locale_object?52u:4u) &&
            result->runtime_heap_status==DS_HEAP_OK && !result->runtime_heap_metadata_after_cleanup &&
            result->runtime_heap.allocation_calls==(wide_streams?25u:narrow_streams?13u:first_facet?8u:stream_locale?6u:locale_object?3u:2u) && !result->runtime_heap.free_calls && !result->runtime_heap.failed_allocations &&
            result->runtime_heap.live_allocations==(wide_streams?25u:narrow_streams?13u:first_facet?8u:stream_locale?6u:locale_object?3u:2u) &&
            result->runtime_heap.live_bytes==(wide_streams?672u:narrow_streams?368u:first_facet?256u:stream_locale?208u:locale_object?128u:64u) &&
            result->runtime_heap.requested_bytes==(wide_streams?420u:narrow_streams?232u:first_facet?176u:stream_locale?144u:locale_object?96u:44u) &&
            result->runtime_heap.peak_live_bytes==(wide_streams?672u:narrow_streams?368u:first_facet?256u:stream_locale?208u:locale_object?128u:64u) &&
            result->runtime_heap.backed_bytes==65536 && result->runtime_heap.arena_count==1 &&
            image->allocated_bytes==DS_REF_ALLOCATED_BYTES+0x11A000+65536 && image->segment_count==7 &&
            ds_image_address(image,DS_HEAP_BASE,65536,DS_PF_R|DS_PF_W) &&
            !ds_image_address(image,DS_HEAP_BASE,1,DS_PF_X);
        for (uint32_t offset=0;offset<64;offset+=4)
            matched &= word_matches(DS_HEAP_BASE+offset,offset==24?2u:locale_published && offset==48?DS_HEAP_BASE+64:0u);
    }
    if (locale_object) {
        static constexpr uint32_t suffix[]={
            0x16896C0,0x16891F0,0x16B7F30,0x166F990,0x1676150,0x16BB2E4,
            0x16BB244,0x16B7F30,0x168ABD0,0x167AC88,0x1689180
        };
        matched &= memcmp(result->functions+658,suffix,sizeof(suffix))==0;
        if (!locale_published) matched &= result->r3==DS_HEAP_BASE+88 && result->r4==0x17041B8 &&
            result->r5==0 && result->r6==0x1AD8EE0;
        matched &= result->thread_pointer==0xE007000;
        for (uint32_t offset=64;offset<128;offset+=4) {
            const uint32_t value=offset==64?0x18C7DE0u:offset==68?(wide_streams?16u:narrow_streams?9u:first_facet?5u:stream_locale?6u:locale_published?3u:1u):
                locale_published && offset==80?0x3Fu:locale_published && offset==92?0x43000000u:
                locale_published && offset==108?1u:offset==112?15u:0u;
            matched &= word_matches(DS_HEAP_BASE+offset,value);
        }
        const uint32_t fields[][2]={
            {0xE000430,locale_published?DS_HEAP_BASE+64:0u},{0x1CE5908,held_guard?1u:0u},
            {0x1CE5910,0},{0x1CE5914,1},{0x1CE5918,held_guard?1u:0u},{0x1CE5924,held_guard?1u:0u}
        };
        for (const auto &field:fields) matched &= word_matches(field[0],field[1]);
    }
    if (locale_published) {
        static constexpr uint32_t suffix[]={
            0x1676088,0xF17448,0x10BA0,0x16693E0,0xF172D0,0x179118,0x11720,0x1666594,
            0x16B7F40,0x16693E0,0x1666594,0x166F990,0x1676150,0x16BB2E4,0x166F910,
            0x1676108,0x166F910,0x1676108,0x16BB264,0x16B7F30,0x166F990,0x1676150,
            0x16BB2E4,0x16BB244,0x166F910,0x1676108,0x16BB264,0x16A0500
        };
        matched &= memcmp(result->functions+669,suffix,sizeof(suffix))==0;
        if (!stream_locale) matched &= result->r3==40 && result->r4==0x1AD58D0 && result->r5==0x1CE6510 && result->r6==0x1CE6500;
        // Original narrow-locale aggregate initialization has returned; the
        // following embedded runtime mutex has only received its base vtable.
        static constexpr uint32_t aggregate[]={
            0x18C1B58,DS_HEAP_BASE,0,0,0x1AD58C0,0x1AD58C0,0,0,
            0x1AD58C8,0x1AD58C8,0,0,0x1AD58CC,0x1AD58D0,DS_HEAP_BASE+48,0,
            0,0,0,0,0x1AD5900,0x1AD5910,0,0,0,0,0,0x1AD58B8,0x18C1BF0,0
        };
        for (uint32_t i=0;i<sizeof(aggregate)/sizeof(aggregate[0]);++i)
            matched &= word_matches(0x1CE64E8+i*4,stream_locale && i==28?0x18C1B58u:stream_locale && i==29?DS_HEAP_BASE+128:aggregate[i]);
        matched &= word_matches(0x1CE672C,DS_HEAP_BASE+64) && word_matches(0x1CE6730,DS_HEAP_BASE+64);
    }
    if (stream_locale) {
        static constexpr uint32_t suffix[]={
            0x168ABD0,0x167AC88,0x1676258,0x16BB2C4,0x168ABD0,0x167AC88,
            0x16896C0,0x16891F0,0x16B7F30,0x16B7F30,0x166F990,0x1676150,0x16BB2E4,
            0x16BB244,0x166F910,0x1676108,0x16BB264,0x1688138,0x1685FC8,0x168ABD0,
            0x167AC88,0x16896C0,0x16891F0,0x16B7F30,0x16B7F30,0x166F990,0x1676150,
            0x16BB2E4,0x16BB244,0x166F910,0x1676108,0x16BB264,0x1685248,0x166F990,
            0x1676150,0x16BB2E4,0x16BB244,0x166F910,0x1676108,0x16BB264,0x166F990,
            0x1676150,0x16BB2E4,0x16BB244,0x166F990,0x1676150,0x16BB2E4,0x166F910,
            0x1676108,0x1688538
        };
        matched &= memcmp(result->functions+697,suffix,sizeof(suffix))==0;
        if (!first_facet) matched &= result->r3==24 && result->r4==1 && result->r5==0 && result->r6==0x1CE6564;
        for (uint32_t offset=128;offset<208;offset+=4) {
            const uint32_t expected=offset==152?2u:(offset==176 || offset==192)?DS_HEAP_BASE+64:0u;
            matched &= word_matches(DS_HEAP_BASE+offset,expected);
        }
        static constexpr uint32_t aggregate[]={
            0x18C1B58,DS_HEAP_BASE+128,0,0,0x1AD5928,0x1AD5928,0,0,
            0x1AD5930,0x1AD5930,0,0,0x1AD5934,0x1AD5938,DS_HEAP_BASE+176,0,
            0,0,0,0,0x1AD5968,0x1AD5978,0,0,0,0,0,0x1AD5920,0x18C1C9C,0,
            0x18C1CB0,0,0,0,0x201,6,0,0,0,DS_HEAP_BASE+192,0x1CE64E8,0,0
        };
        for (uint32_t i=0;i<sizeof(aggregate)/sizeof(aggregate[0]);++i)
            matched &= word_matches(0x1CE6558+i*4,narrow_streams && i==41?0x1CE6604u:first_stream && i==31?1u:first_stream && i==42?0x20000000u:aggregate[i]);
        matched &= word_matches(0x1CE6724,1) && word_matches(0x1CE6734,wide_streams?2u:1u) && word_matches(0x1CE6728,0);
    }
    if (first_facet) {
        static constexpr uint32_t suffix[]={
            0x168ABD0,0x167AC88,0x168A6B0,0x166F990,0x1676150,0x16BB2E4,0x167AED0,
            0x1666594,0x1668208,0x1676088,0xF17448,0x10BA0,0x16693E0,0xF172D0,
            0x179118,0x11720,0x1666594,0x1668208,0x1668E50,0x16693E0,0x1666594,
            0x16888D8,0x1668208,0x1668E50,0x166F910,0x1676108,0x166F990,0x1676150,
            0x16BB2E4,0x166F910,0x1676108,0x16884C0,0x1664CD8,0x166F990,0x1676150,
            0x16BB2E4,0x16BB244,0x166F910,0x1676108,0x16BB264,0x168ABD0,0x167AC88,
            0x166F910,0x1676108,0x16BB264,0x166F990,0x1676150,0x16BB2E4,0x16BB244,
            0x166F910,0x1676108,0x16BB264
        };
        matched &= memcmp(result->functions+747,suffix,sizeof(suffix))==0;
        if (!first_stream) matched &= result->r3==DS_HEAP_BASE+208 && result->r4==32 && result->r5==1 && result->r6==UINT64_C(0x7F7F7F7F7F7F7F7F) &&
            result->boundary_nid==0x1BC200F4 && result->indirect_descriptor==0x1970348 && result->indirect_code==0x9E96A0;
        static constexpr uint32_t heap_tail[]={0x1872C50,1,0x176E82A,0x176EFF4,0x176F1F6,0,0,0,0,DS_HEAP_BASE+208,0,0};
        for (uint32_t i=0;i<sizeof(heap_tail)/sizeof(heap_tail[0]);++i)
            matched &= word_matches(DS_HEAP_BASE+208+i*4,heap_tail[i]);
        static constexpr uint32_t snapshot[]={0x176F838,0x176F840,0x176F840,0x176F840,0x176F898,0x176F898,0x176F898,
            0x176F920,0x176F920,0x176F920,0x176F920,0x176F920,0x176F920,0x176F920,0x176F920,0x176F920,0x176F920,
            0x176F950,0x176F958,0x176F960,0x176F968};
        for (uint32_t i=0;!narrow_streams && i<sizeof(snapshot)/sizeof(snapshot[0]);++i)
            matched &= word_matches(0xFEFFB08+i*4,snapshot[i]);
        static constexpr uint32_t fields[][2]={{0x1AD5630,78},{0x1CE58E4,0x19AC6D0},{0x1CE58E8,0x1967920},
            {0x1B50430,DS_HEAP_BASE+208},{0x1CE6720,DS_HEAP_BASE+240},
            {0xFEFFB04,0},{0xFEFFB70,0},{0xFEFFB74,15},{0xFEFFB8C,0},{0xFEFFB90,15}};
        for (const auto &field:fields) if (!narrow_streams || field[0]<0xFE00000)
            matched &= word_matches(field[0],wide_streams && field[0]==0x1CE6720?DS_HEAP_BASE+0x260:field[1]);
        const uint8_t *first=static_cast<const uint8_t*>(ds_image_address(image,0xFEFFB60,1,DS_PF_R));
        const uint8_t *second=static_cast<const uint8_t*>(ds_image_address(image,0xFEFFB7C,1,DS_PF_R));
        if (!narrow_streams) matched &= first && !*first && second && !*second;
    }
    if (first_stream) {
        static constexpr uint32_t suffix[]={0x9E96A0,0x1684D70,0x166F990,0x1676150,0x16BB2E4,
            0x16BB244,0x166F910,0x1676108,0x16BB264,0x1688138,0x1685FC8};
        matched &= memcmp(result->functions+799,suffix,sizeof(suffix))==0;
        if (!narrow_streams) matched &= result->r3==4 && result->r4==0x1CE6558 && result->r5==1 &&
            result->r6==UINT64_C(0x7F7F7F7F7F7F7F7F) && result->boundary_nid==0x1BC200F4 &&
            result->indirect_descriptor==0x1970348 && result->indirect_code==0x9E96A0;
        // Reference bytes, all nine slot words, lazy state and padding are checked.
        for (uint32_t offset=0;!narrow_streams && offset<0x60;offset+=4)
            matched &= word_matches(0x1CE6488+offset,offset==0?0x10000u:offset==16?0x1CE65D0u:0u);
        matched &= word_matches(0x1CE6604,0x18C1B1C) && word_matches(0x1CE6608,0x18C1B30);
    }
    if (narrow_streams) {
        matched &= memcmp(result->functions+810,kNarrowTraceSuffix,sizeof(kNarrowTraceSuffix))==0;
        if (!wide_streams) matched &= result->r3==0x1C5BE41 && !result->r4 && !result->r5 && result->r6==0x1CE6658 &&
            result->boundary_nid==0x1BC200F4 && result->indirect_descriptor==0x1970348 && result->indirect_code==0x9E96A0 &&
            result->mutex_addresses[14]==DS_HEAP_BASE+0x120;
        for (uint32_t i=0;!wide_streams && i<sizeof(kNarrowRegistry)/sizeof(*kNarrowRegistry);++i)
            matched &= word_matches(0x1CE6488+4*i,kNarrowRegistry[i]);
        for (uint32_t i=0;i<sizeof(kNarrowAggregate)/sizeof(*kNarrowAggregate);++i)
            matched &= word_matches(0x1CE64E8+4*i,kNarrowAggregate[i]);
        for (uint32_t i=0;i<sizeof(kNarrowHeap)/sizeof(*kNarrowHeap);++i)
            matched &= word_matches(DS_HEAP_BASE+4*i,wide_streams && i==0x44/4?16u:kNarrowHeap[i]);
    }
    if (wide_streams) {
        matched &= memcmp(result->functions+966,kWideTraceSuffix,sizeof(kWideTraceSuffix))==0 &&
            !result->r3 && !result->r4 && result->r5==1 && result->r6==0xFEFFB24 &&
            result->boundary_nid==0x1BC200F4 && result->indirect_descriptor==0x194CF48 && result->indirect_code==0x745248 &&
            result->mutex_addresses[14]==DS_HEAP_BASE+0x120 && result->mutex_addresses[15]==DS_HEAP_BASE+0x180 &&
            result->mutex_addresses[16]==DS_HEAP_BASE+0x1C0 && result->mutex_addresses[17]==DS_HEAP_BASE+0x200;
        for (uint32_t i=0;i<sizeof(kWideRegistry)/sizeof(*kWideRegistry);++i) matched &= word_matches(0x1CE6488+4*i,kWideRegistry[i]);
        for (uint32_t i=0;i<sizeof(kWideAggregate)/sizeof(*kWideAggregate);++i) matched &= word_matches(0x1CE6750+4*i,kWideAggregate[i]);
        for (uint32_t i=0;i<sizeof(kWideHeap)/sizeof(*kWideHeap);++i) matched &= word_matches(DS_HEAP_BASE+4*i,kWideHeap[i]);
        for (uint32_t i=0;i<sizeof(kWideGlobals)/sizeof(*kWideGlobals);++i) matched &= word_matches(0x1AD61B0+4*i,kWideGlobals[i]);
        for (uint32_t a=0xE000300;a<0xE0003E0;a+=4) matched &= word_matches(a,0);
        matched &= word_matches(0x1B93E84,DS_HEAP_BASE+0x240) && word_matches(0x1CE6994,2);
    }
    if (!matched) {
        result->stop = DS_ENTRY_ABI_MISMATCH;
        if (error && error_capacity) {
            if (bad_address) snprintf(error, error_capacity,
                "Constructor checkpoint at 0x%08X: got 0x%08X, expected 0x%08X", bad_address, bad_actual, bad_expected);
            else snprintf(error, error_capacity, "Constructor checkpoint function trace mismatch");
        }
        return false;
    }
    result->stop = profile->ready;
    result->registry_entries = name_count;
    result->callback_entries = key_count;
    if (error && error_capacity) error[0] = '\0';
    return true;
}

extern "C" bool ds_entry_run_three_constructors(DsImage *image, DsEntryResult *result,
                                               char *error, size_t error_capacity)
{
    return run_metadata_constructors(image, result, error, error_capacity, 3);
}

extern "C" bool ds_entry_run_four_constructors(DsImage *image, DsEntryResult *result,
                                              char *error, size_t error_capacity)
{
    return run_metadata_constructors(image, result, error, error_capacity, 4);
}

extern "C" bool ds_entry_run_six_constructors(DsImage *image, DsEntryResult *result,
                                             char *error, size_t error_capacity)
{
    return run_metadata_constructors(image, result, error, error_capacity, 6);
}

extern "C" bool ds_entry_run_nine_constructors(DsImage *image, DsEntryResult *result,
                                              char *error, size_t error_capacity)
{
    return run_metadata_constructors(image, result, error, error_capacity, 9);
}

extern "C" bool ds_entry_run_ten_constructors(DsImage *image, DsEntryResult *result,
                                             char *error, size_t error_capacity)
{
    return run_metadata_constructors(image, result, error, error_capacity, 10);
}

extern "C" bool ds_entry_run_eleven_constructors(DsImage *image, DsEntryResult *result,
                                                char *error, size_t error_capacity)
{
    return run_metadata_constructors(image, result, error, error_capacity, 11);
}

extern "C" bool ds_entry_run_twelve_constructors(DsImage *image, DsEntryResult *result,
                                                char *error, size_t error_capacity)
{
    return run_metadata_constructors(image, result, error, error_capacity, 12);
}

extern "C" bool ds_entry_run_runtime_heap(DsImage *image, DsEntryResult *result,
                                          char *error, size_t error_capacity)
{
    return run_metadata_constructors(image,result,error,error_capacity,12,true);
}

extern "C" bool ds_entry_run_runtime_allocated(DsImage *image, DsEntryResult *result,
                                               char *error,size_t error_capacity)
{
    return run_metadata_constructors(image,result,error,error_capacity,12,false,true);
}

extern "C" bool ds_entry_run_locale_object(DsImage *image, DsEntryResult *result,
                                          char *error,size_t error_capacity)
{
    return run_metadata_constructors(image,result,error,error_capacity,12,false,true,true);
}

extern "C" bool ds_entry_run_locale_published(DsImage *image, DsEntryResult *result,
                                             char *error,size_t error_capacity)
{
    return run_metadata_constructors(image,result,error,error_capacity,12,false,true,true,true);
}

extern "C" bool ds_entry_run_stream_locale(DsImage *image, DsEntryResult *result,
                                          char *error,size_t error_capacity)
{
    return run_metadata_constructors(image,result,error,error_capacity,12,false,true,true,true,true);
}

extern "C" bool ds_entry_run_first_facet(DsImage *image, DsEntryResult *result,
                                        char *error,size_t error_capacity)
{
    return run_metadata_constructors(image,result,error,error_capacity,12,false,true,true,true,true,true);
}

extern "C" bool ds_entry_run_first_stream(DsImage *image, DsEntryResult *result,
                                         char *error,size_t error_capacity)
{
    return run_metadata_constructors(image,result,error,error_capacity,12,false,true,true,true,true,true,true);
}

extern "C" bool ds_entry_run_second_constructor(DsImage *image, DsEntryResult *result,
                                               char *error, size_t error_capacity)
{
    return run_entry(image, result, error, error_capacity, true, true, true, true, true, true, true, true, true, true, true, true);
}

extern "C" bool ds_entry_run_first_constructor(DsImage *image, DsEntryResult *result,
                                              char *error, size_t error_capacity)
{
    return run_entry(image, result, error, error_capacity, true, true, true, true, true, true, true, true, true, true, true);
}

extern "C" bool ds_entry_run_registration(DsImage *image, DsEntryResult *result,
                                          char *error, size_t error_capacity)
{
    return run_entry(image, result, error, error_capacity, true, true, true, true, true, true, true, true, true, true);
}

extern "C" bool ds_entry_run_registry(DsImage *image, DsEntryResult *result,
                                      char *error, size_t error_capacity)
{
    return run_entry(image, result, error, error_capacity, true, true, true, true, true, true, true, true, true);
}

extern "C" bool ds_entry_run_startup(DsImage *image, DsEntryResult *result,
                                     char *error, size_t error_capacity)
{
    return run_entry(image, result, error, error_capacity, true, true, true, true, true, true, true, true);
}

extern "C" bool ds_entry_run_manager(DsImage *image, DsEntryResult *result,
                                     char *error, size_t error_capacity)
{
    return run_entry(image, result, error, error_capacity, true, true, true, true, true, true, true);
}

extern "C" bool ds_entry_run_singleton(DsImage *image, DsEntryResult *result,
                                       char *error, size_t error_capacity)
{
    return run_entry(image, result, error, error_capacity, true, true, true, true, true, true);
}

extern "C" bool ds_entry_run_constructor(DsImage *image, DsEntryResult *result,
                                          char *error, size_t error_capacity)
{
    return run_entry(image, result, error, error_capacity, true, true, true, true, true);
}

extern "C" bool ds_entry_run_clock(DsImage *image, DsEntryResult *result,
                                    char *error, size_t error_capacity)
{
    return run_entry(image, result, error, error_capacity, true, true, true, true);
}

extern "C" bool ds_entry_run_memory(DsImage *image, DsEntryResult *result,
                                     char *error, size_t error_capacity)
{
    return run_entry(image, result, error, error_capacity, true, true, true);
}

extern "C" bool ds_entry_run_crt(DsImage *image, DsEntryResult *result,
                                  char *error, size_t error_capacity)
{
    return run_entry(image, result, error, error_capacity, true, true);
}

extern "C" bool ds_entry_run(DsImage *image, DsEntryResult *result,
                              char *error, size_t error_capacity)
{
    return run_entry(image, result, error, error_capacity, false);
}

extern "C" bool ds_entry_run_tls(DsImage *image, DsEntryResult *result,
                                  char *error, size_t error_capacity)
{
    return run_entry(image, result, error, error_capacity, true);
}

extern "C" bool ds_entry_run_narrow_streams(DsImage *image, DsEntryResult *result, char *error,size_t capacity)
{
    return run_metadata_constructors(image,result,error,capacity,12,false,true,true,true,true,true,true,true);
}

extern "C" bool ds_entry_run_wide_streams(DsImage *image,DsEntryResult *result,char *error,size_t capacity)
{
    return run_metadata_constructors(image,result,error,capacity,12,false,true,true,true,true,true,true,true,true);
}
