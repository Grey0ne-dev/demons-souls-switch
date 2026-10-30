#ifndef DS_ENTRY_H
#define DS_ENTRY_H

#include "ds_image.h"
#include "ds_heap.h"
#include "ds_sync.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DS_ENTRY_FUNCTION_CAPACITY 2048

typedef enum {
    DS_ENTRY_NOT_RUN = 0,
    DS_ENTRY_TLS_BOUNDARY,
    DS_ENTRY_MEMORY_FAULT,
    DS_ENTRY_UNSUPPORTED_CALL,
    DS_ENTRY_RETURNED,
    DS_ENTRY_BAD_INPUT,
    DS_ENTRY_ABI_MISMATCH,
    DS_ENTRY_TLS_INITIALIZED,
    DS_ENTRY_MEMORY_QUERY_BOUNDARY,
    DS_ENTRY_TIME_BOUNDARY,
    DS_ENTRY_CONSTRUCTOR_BOUNDARY,
    DS_ENTRY_CLOCK_FAILURE,
    DS_ENTRY_SINGLETON_BOUNDARY,
    DS_ENTRY_SYNC_FAILURE,
    DS_ENTRY_RWLOCK_INITIALIZED,
    DS_ENTRY_MANAGER_POOL_BOUNDARY,
    DS_ENTRY_SINGLETON_READY,
    DS_ENTRY_EXECUTION_LIMIT,
    DS_ENTRY_REGISTRY_READY,
    DS_ENTRY_REGISTRATION_READY,
    DS_ENTRY_FIRST_CONSTRUCTOR_READY,
    DS_ENTRY_SECOND_CONSTRUCTOR_READY,
    DS_ENTRY_THIRD_CONSTRUCTOR_READY,
    DS_ENTRY_FOURTH_CONSTRUCTOR_READY,
    DS_ENTRY_SIXTH_CONSTRUCTOR_READY,
    DS_ENTRY_NINTH_CONSTRUCTOR_READY,
    DS_ENTRY_TENTH_CONSTRUCTOR_READY,
    DS_ENTRY_ELEVENTH_CONSTRUCTOR_READY,
    DS_ENTRY_TWELFTH_CONSTRUCTOR_READY,
    DS_ENTRY_RUNTIME_HEAP_BOUNDARY,
    DS_ENTRY_RUNTIME_ALLOCATED,
    DS_ENTRY_HEAP_FAILURE,
    DS_ENTRY_LOCALE_OBJECT_READY,
    DS_ENTRY_LOCALE_PUBLISHED,
    DS_ENTRY_STREAM_LOCALE_READY,
    DS_ENTRY_FIRST_FACET_READY,
    DS_ENTRY_FIRST_STREAM_READY,
    DS_ENTRY_NARROW_STREAMS_READY,
    DS_ENTRY_WIDE_STREAMS_READY
} DsEntryStop;

typedef struct {
    uint64_t address;
    uint64_t value;
    uint32_t size;
    bool write;
} DsEntryAccess;

typedef struct {
    DsEntryStop stop;
    uint32_t boundary_nid;
    uint32_t callee; /* Most recent function entry, not an unwound call stack. */
    uint32_t trace_count; /* Checked guest memory accesses, including failures. */
    uint32_t trace_stored;
    DsEntryAccess trace[32];
    uint32_t function_count;
    uint32_t functions[DS_ENTRY_FUNCTION_CAPACITY];
    uint64_t r3, r4, r5, r6;
    uint64_t lr, stack_pointer;
    uint64_t thread_pointer;
    uint32_t tls_block;
    bool tls_initialized;
    bool mutex_initialized;
    uint32_t mutex_address;
    uint32_t mutex_attribute;
    uint32_t mutex_count;
    uint32_t mutex_addresses[DS_CRT_MUTEX_CAPACITY];
    uint32_t memory_total;
    uint32_t memory_available;
    uint32_t memory_output_address;
    uint32_t memory_query_count;
    size_t memory_backed_at_query;
    uint32_t syscall_number;
    uint32_t argv_address;
    uint32_t envp_address;
    uint64_t toc;
    uint32_t clock_calls;
    uint64_t clock_ticks, clock_frequency, clock_microseconds;
    uint32_t constructor_descriptor, constructor_target;
    uint32_t constructor_toc;
    bool constructor_started;
    bool constructor_completed;
    uint32_t thread_id_calls, lock_calls, unlock_calls;
    uint32_t last_locked_mutex;
    uint32_t native_held_at_stop, native_live_after_cleanup;
    bool singleton_started;
    uint32_t indirect_code, indirect_descriptor;
    uint32_t singleton_address, manager_address;
    uint32_t rwlock_output, rwlock_attribute;
    uint32_t rwlock_attribute_words[8];
    uint32_t rwlock_create_calls, rwlock_handle, native_rwlocks_after_cleanup;
    uint32_t pool_request_base, pool_request_size;
    bool manager_published;
    bool pool_initialized, singleton_completed;
    uint32_t allocation_calls, allocation_address, allocation_size;
    uint32_t atomic_attempts, atomic_successes, bootstrap_fallback_calls;
    uint32_t rwlock_write_locks, rwlock_write_unlocks, native_rwlocks_held_at_stop;
    bool registry_initialized;
    uint32_t registry_address, registry_payload, registry_entries;
    uint32_t registry_allocation_address, registry_allocation_size;
    bool callbacks_initialized, registration_completed;
    uint32_t callback_registry_address, callback_payload, callback_entries;
    uint32_t callback_allocation_address, callback_allocation_size;
    uint32_t second_singleton_address, node_address, node_vector_address;
    uint32_t kernel_mutex_calls, kernel_mutex_handle, kernel_mutex_output, kernel_mutex_attribute;
    uint32_t kernel_mutex_attribute_words[10], native_kernel_mutexes_after_cleanup;
    uint32_t next_constructor_descriptor, next_constructor_target;
    uint32_t constructors_completed;
    DsHeapStats runtime_heap;
    DsHeapStatus runtime_heap_status;
    uint32_t runtime_malloc_address,runtime_malloc_size;
    uint32_t runtime_last_malloc_address,runtime_last_malloc_size;
    bool runtime_heap_metadata_after_cleanup;
    bool memory_fault;
    uint64_t fault_address;
    uint32_t fault_size;
    bool fault_write;
    uint32_t fault_required_flags;
} DsEntryResult;

/* Run the verified BLUS30443 translated entry through its first import call.
 * The sys_initialize_tls implementation is NOT executed. This single-threaded
 * checkpoint leaves stack/TLS arenas in image until ds_image_destroy().
 * Caller must verify the loaded image against the reference hashes first.
 */
bool ds_entry_run(DsImage *image, DsEntryResult *result,
                  char *error, size_t error_capacity);

/* Execute the same real entry, implement the main-thread TLS service using
 * checked backing, then stop with its return registers set. No instruction
 * after the import executes. The original pre-service checkpoint remains available.
 */
bool ds_entry_run_tls(DsImage *image, DsEntryResult *result,
                      char *error, size_t error_capacity);

/* Continue CRT with argv/envp and import TOC preservation, create its first
 * guest mutex record, then stop before sys_memory_get_user_memory_size (352).
 * Lock/wait operations and the memory query itself are not implemented.
 */
bool ds_entry_run_crt(DsImage *image, DsEntryResult *result,
                      char *error, size_t error_capacity);

/* Execute syscall 352 against the configured guest budget, then continue CRT
 * mutex/table initialization to the first time import. The clock service,
 * memory allocation syscalls, lock/wait and guest threads remain unsupported.
 */
bool ds_entry_run_memory(DsImage *image, DsEntryResult *result,
                         char *error, size_t error_capacity);

/* Continue the real time service through the CRT timestamp store and constructor
 * enumeration. Validate, then stop before invoking the first static constructor.
 */
bool ds_entry_run_clock(DsImage *image, DsEntryResult *result,
                        char *error, size_t error_capacity);

/* Execute the first constructor prefix through destructor registration and
 * native uncontended lock/unlock, stopping before its singleton initializer.
 */
bool ds_entry_run_constructor(DsImage *image, DsEntryResult *result,
                              char *error, size_t error_capacity);

/* Enter singleton/manager constructors, serve private sys_rwlock_create with
 * a native object and guest handle, then stop before caller continuation.
 * Reader/writer acquisitions, IPC and waiter scheduling remain unsupported.
 */
bool ds_entry_run_singleton(DsImage *image, DsEntryResult *result,
                            char *error, size_t error_capacity);
/* Continue manager publication/unlock; validate its static pool handoff and
 * stop before allocator initialization. No guest allocation is served here.
 */
bool ds_entry_run_manager(DsImage *image, DsEntryResult *result,
                          char *error, size_t error_capacity);
/* Pool initialization, real allocation, range registration and singleton
 * completion; stops before the following substantial game initializer.
 */
bool ds_entry_run_startup(DsImage *image, DsEntryResult *result,
                          char *error, size_t error_capacity);
/* Initialize the first name registry, allocate storage and insert the real
 * singleton record; stop before the separate callback-registry initializer.
 */
bool ds_entry_run_registry(DsImage *image, DsEntryResult *result,
                           char *error, size_t error_capacity);
/* Complete name/key registry insertion and sorting, return from registration,
 * and stop before the next singleton subtree. No registered key is invoked.
 */
bool ds_entry_run_registration(DsImage *image, DsEntryResult *result,
                               char *error, size_t error_capacity);
bool ds_entry_run_first_constructor(DsImage *image, DsEntryResult *result,
                                    char *error, size_t error_capacity);
bool ds_entry_run_second_constructor(DsImage *image, DsEntryResult *result,
                                     char *error, size_t error_capacity);
/* Development frontier: requires generated callsite annotations. Runs known
 * bodies until a strict unsupported boundary; no boot-success result is implied.
 */
bool ds_entry_run_exploration(DsImage *image, DsEntryResult *result,
                              char *error, size_t error_capacity, uint32_t constructor_limit);
bool ds_entry_run_three_constructors(DsImage *image, DsEntryResult *result,
                                     char *error, size_t error_capacity);
/* Complete the fourth constructor's logging metadata and two method records,
 * validating their pool objects and stopping before constructor five. */
bool ds_entry_run_four_constructors(DsImage *image, DsEntryResult *result,
                                  char *error, size_t error_capacity);
/* Register two additional metadata types, check all five name records and
 * twenty sorted opaque keys, and stop before constructor seven. */
bool ds_entry_run_six_constructors(DsImage *image, DsEntryResult *result,
                                 char *error, size_t error_capacity);
/* Complete nine constructor returns, validate eight names and all 32 opaque
 * keys, and stop before constructor ten without entering larger-sort paths. */
bool ds_entry_run_nine_constructors(DsImage *image, DsEntryResult *result,
                                  char *error, size_t error_capacity);
/* Complete FileCap metadata and the original >32-pair sort path, validating
 * all nine names and 36 opaque keys before stopping at constructor eleven. */
bool ds_entry_run_ten_constructors(DsImage *image, DsEntryResult *result,
                                 char *error, size_t error_capacity);
/* Complete eleven constructor returns and validate the static objects before
 * stopping at constructor twelve. No new native services are required. */
bool ds_entry_run_eleven_constructors(DsImage *image, DsEntryResult *result,
                                     char *error, size_t error_capacity);
/* Validate ten names, forty sorted keys and the draw-seed metadata, then stop
 * before the exact runtime-TOC constructor descriptor. */
bool ds_entry_run_twelve_constructors(DsImage *image, DsEntryResult *result,
                                     char *error, size_t error_capacity);
/* Start constructor thirteen's real locale initialization and stop before the
 * first 40-byte operator-new request. Thirteen has not returned at this stage. */
bool ds_entry_run_runtime_heap(DsImage *image, DsEntryResult *result,
                               char *error, size_t error_capacity);
/* Serve the real40-byte mutex allocation and following4-byte locale allocation,
 * then stop before the unreviewed locale consumer. */
bool ds_entry_run_runtime_allocated(DsImage *image, DsEntryResult *result,
                                    char *error, size_t error_capacity);
/* Initialize the real 52-byte locale object's scalar fields. Stop with the
 * locale lock held before its string constructor; TLS publication is pending. */
bool ds_entry_run_locale_object(DsImage *image, DsEntryResult *result,
                                char *error, size_t error_capacity);
/* Construct the original locale strings, publish real TLS/global/handle state,
 * and stop before the next runtime-mutex allocation. Constructor 13 is pending. */
bool ds_entry_run_locale_published(DsImage *image, DsEntryResult *result,
                                   char *error, size_t error_capacity);
/* Reuse the published locale in stream/base handles and serve three more
 * audited allocations. Stop before the first facet allocation with guard held. */
bool ds_entry_run_stream_locale(DsImage *image, DsEntryResult *result,
                                char *error, size_t error_capacity);
/* Construct and register the first facet and its cleanup node; stop at the
 * checked virtual-call boundary without invoking the target method. */
bool ds_entry_run_first_facet(DsImage *image, DsEntryResult *result,
                              char *error, size_t error_capacity);

/* Execute the reviewed facet virtual and register the first narrow stream. */
bool ds_entry_run_first_stream(DsImage *image, DsEntryResult *result,
                               char *error, size_t error_capacity);

bool ds_entry_run_narrow_streams(DsImage *, DsEntryResult *, char *, size_t);
bool ds_entry_run_wide_streams(DsImage *, DsEntryResult *, char *, size_t);

#ifdef __cplusplus
}
#endif
#endif
