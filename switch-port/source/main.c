#include <switch.h>

#include "ds_image.h"
#include "ds_reference.h"
#ifdef DS_WITH_ENTRY
#include "ds_entry.h"
#endif

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define DS_MEMORY_LIMIT (48u * 1024u * 1024u)
static const char *image_path = "sdmc:/switch/demons-souls-loader/EBOOT.ELF";
static const char *log_path = "sdmc:/switch/demons-souls-loader/loader.log";
static unsigned char hash_buffer[65536];
static DsImage image;
static FILE *log_file;
static char last_error[256];
static bool file_verified, memory_verified;
static unsigned attempt;
#ifdef DS_WITH_ENTRY
static DsEntryResult entry_result;
static bool entry_reached;
static char entry_error[256];
#endif

static void log_line(const char *format, ...) __attribute__((format(printf, 1, 2)));
static void log_line(const char *format, ...)
{
    if (!log_file) return;
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fputc('\n', log_file);
    fflush(log_file);
}

static void log_hash(const char *name, const unsigned char hash[32])
{
    char text[65];
    for (size_t i = 0; i < 32; ++i) snprintf(text + i * 2, 3, "%02x", hash[i]);
    log_line("%s: %s", name, text);
}

static void progress(const char *text)
{
    printf("%s\n", text);
    consoleUpdate(NULL);
    log_line("%s", text);
}

static bool verify_file(void)
{
    FILE *input = fopen(image_path, "rb");
    if (!input) {
        snprintf(last_error, sizeof(last_error), "Cannot open EBOOT.ELF (errno %d)", errno);
        return false;
    }
    if (fseek(input, 0, SEEK_END) != 0) {
        snprintf(last_error, sizeof(last_error), "Cannot measure EBOOT.ELF");
        fclose(input);
        return false;
    }
    long size = ftell(input);
    if (size < 0 || (uint64_t)size != DS_REF_FILE_SIZE || fseek(input, 0, SEEK_SET) != 0) {
        snprintf(last_error, sizeof(last_error), "Wrong EBOOT.ELF size: %ld", size);
        fclose(input);
        return false;
    }
    Sha256Context context;
    sha256ContextCreate(&context);
    size_t count;
    while ((count = fread(hash_buffer, 1, sizeof(hash_buffer), input)) != 0)
        sha256ContextUpdate(&context, hash_buffer, count);
    bool read_ok = ferror(input) == 0;
    fclose(input);
    if (!read_ok) {
        snprintf(last_error, sizeof(last_error), "Read error while hashing EBOOT.ELF");
        return false;
    }
    unsigned char hash[32];
    sha256ContextGetHash(&context, hash);
    log_hash("EBOOT file SHA256", hash);
    if (memcmp(hash, DS_REF_FILE_SHA256, sizeof(hash)) != 0) {
        snprintf(last_error, sizeof(last_error), "EBOOT.ELF hash does not match BLUS30443 v1.00");
        return false;
    }
    return true;
}

static bool verify_memory(void)
{
    if (image.segment_count != DS_REF_SEGMENT_COUNT ||
        image.allocated_bytes != DS_REF_ALLOCATED_BYTES ||
        image.entry_descriptor != DS_REF_ENTRY_DESCRIPTOR ||
        image.entry_code != DS_REF_ENTRY_CODE || image.toc != DS_REF_TOC ||
        image.tls_address != DS_REF_TLS_ADDRESS ||
        image.tls_file_size != DS_REF_TLS_FILE_SIZE ||
        image.tls_memory_size != DS_REF_TLS_MEMORY_SIZE) {
        snprintf(last_error, sizeof(last_error), "Loaded ELF metadata differs from the reference");
        return false;
    }
    for (size_t i = 0; i < DS_REF_SEGMENT_COUNT; ++i) {
        const DsSegment *segment = &image.segments[i];
        const DsReferenceSegment *reference = &DS_REF_SEGMENTS[i];
        if (segment->guest_address != reference->guest_address || segment->flags != reference->flags ||
            segment->file_offset != reference->file_offset || segment->file_size != reference->file_size ||
            segment->memory_size != reference->memory_size || !segment->from_elf) {
            snprintf(last_error, sizeof(last_error), "Segment %zu metadata mismatch", i);
            return false;
        }
        unsigned char file_hash[32], memory_hash[32];
        sha256CalculateHash(file_hash, segment->data, segment->file_size);
        sha256CalculateHash(memory_hash, segment->data, segment->memory_size);
        log_line("Segment %zu guest=0x%08" PRIx32 " file=%zu backed=%zu flags=0x%08" PRIx32,
                 i, segment->guest_address, segment->file_size, segment->memory_size, segment->flags);
        log_hash("  loaded file bytes SHA256", file_hash);
        log_hash("  complete memory SHA256 (including BSS)", memory_hash);
        if (memcmp(file_hash, reference->file_sha256, 32) != 0 ||
            memcmp(memory_hash, reference->memory_sha256, 32) != 0) {
            snprintf(last_error, sizeof(last_error), "Segment %zu contents/BSS mismatch", i);
            return false;
        }
    }
    return true;
}

static void draw_result(void)
{
    consoleClear();
    printf("Demon's Souls - Switch startup v0.27.0\n\n");
    printf("Input: %s v%s\n", DS_REF_TITLE_ID, DS_REF_APP_VERSION);
    printf("Executable SHA256: %s\n", file_verified ? "MATCHED" : "NOT VERIFIED");
    printf("Initial segment contents/BSS: %s\n", memory_verified ? "VERIFIED" : "NOT VERIFIED");
    if (memory_verified) {
        printf("ELF backing: %.2f MiB; current guest backing: %.2f MiB\n",
               (double)DS_REF_ALLOCATED_BYTES / (1024.0 * 1024.0),
               (double)image.allocated_bytes / (1024.0 * 1024.0));
        printf("ELF load segments: %u\n", DS_REF_SEGMENT_COUNT);
        printf("Entry descriptor: 0x%08" PRIx32 "\n", image.entry_descriptor);
        printf("Entry code:       0x%08" PRIx32 "\n", image.entry_code);
        printf("TOC register:     0x%08" PRIx32 "\n", image.toc);
        printf("TLS template: %zu bytes / %zu per-thread bytes\n",
               image.tls_file_size, image.tls_memory_size);
        printf("\nActual game image loaded and verified.\n");
#ifdef DS_WITH_ENTRY
        if (entry_reached) {
            printf("Translated startup checkpoint: REACHED\n");
            printf("Main-thread TLS and %" PRIu32 " CRT mutex records initialized.\n", entry_result.mutex_count);
            printf("TLS thread pointer: 0x%08" PRIx64 "\n", entry_result.thread_pointer);
            printf("argv/envp prepared; CRT argument conversion passed.\n");
            printf("Memory query: %" PRIu32 " MiB total / %" PRIu32 " MiB available.\n",
                   entry_result.memory_total >> 20, entry_result.memory_available >> 20);
            printf("Thirteen real constructors completed; destructor registered.\n");
            printf("384 KiB pool initialized; metadata objects constructed.\n");
            printf("Ten type names, forty keys and two methods registered.\n");
            printf("Constructor thirteen complete; runtime streams initialized.\n");
            printf("Twenty-five backed allocations; all eight streams registered.\n");
            printf("Locale name C and TLS/handle publication validated.\n");
            printf("Wide initialization complete; before constructor fourteen.\n");
        } else {
            printf("Translated startup stopped: %.110s\n", entry_error);
        }
#else
        printf("Translated entry has not been executed in this build.\n");
#endif
    } else {
        printf("\nLoad stopped: %.120s\n", last_error);
        printf("Expected file: /switch/demons-souls-loader/EBOOT.ELF\n");
    }
    printf("\nGame rendering and gameplay are not implemented.\n");
    printf("No launch-setting changes are required for this stage.\n\n");
    printf("Log: /switch/demons-souls-loader/loader.log%s\n",
           log_file ? "" : " (could not open)");
    printf("A: reload image    +: release memory and exit\n");
}

static void perform_load(void)
{
    ds_image_destroy(&image);
    file_verified = memory_verified = false;
    last_error[0] = '\0';
#ifdef DS_WITH_ENTRY
    memset(&entry_result, 0, sizeof(entry_result));
    entry_reached = false;
    entry_error[0] = '\0';
#endif
    ++attempt;
    consoleClear();
    printf("Demon's Souls - Switch startup v0.27.0\n\n");
    progress("Checking the real game executable...");
    log_line("Attempt %u memory_limit=%u", attempt, DS_MEMORY_LIMIT);
    file_verified = verify_file();
    if (file_verified) {
        progress("Executable matched. Loading backed guest segments...");
        if (ds_image_load_file(&image, image_path, DS_MEMORY_LIMIT, last_error, sizeof(last_error))) {
            progress("Checking copied data, zeroed BSS and entry descriptor...");
            memory_verified = verify_memory();
        }
    }
    if (!memory_verified) {
        log_line("LOAD FAILED: %s", last_error);
        ds_image_destroy(&image);
    } else {
        log_line("IMAGE VERIFIED: descriptor=0x%08" PRIx32 " code=0x%08" PRIx32
                 " toc=0x%08" PRIx32 " backed=%zu", image.entry_descriptor,
                 image.entry_code, image.toc, image.allocated_bytes);
#ifdef DS_WITH_ENTRY
        progress("Completing wide-stream initialization and constructor thirteen...");
        entry_reached = ds_entry_run_wide_streams(&image, &entry_result, entry_error, sizeof(entry_error));
        log_line("TLS initialized=%d block=0x%08" PRIx32 " r13=0x%08" PRIx64,
                 entry_result.tls_initialized, entry_result.tls_block, entry_result.thread_pointer);
        log_line("CRT mutex initialized=%d address=0x%08" PRIx32 " attribute=0x%08" PRIx32,
                 entry_result.mutex_initialized, entry_result.mutex_address, entry_result.mutex_attribute);
        log_line("CRT argv=0x%08" PRIx32 " envp=0x%08" PRIx32 " toc=0x%08" PRIx64
                 " last_syscall=%" PRIu32,
                 entry_result.argv_address, entry_result.envp_address, entry_result.toc, entry_result.syscall_number);
        log_line("MEMORY QUERY: completed=%" PRIu32 " output=0x%08" PRIx32
                 " total=%" PRIu32 " available=%" PRIu32 " backed=%zu cap=%zu",
                 entry_result.memory_query_count, entry_result.memory_output_address,
                 entry_result.memory_total, entry_result.memory_available,
                 entry_result.memory_backed_at_query, image.memory_limit);
        log_line("CRT mutex records=%" PRIu32 "", entry_result.mutex_count);
        for (uint32_t i = 0; i < entry_result.mutex_count && i < DS_CRT_MUTEX_CAPACITY; ++i)
            log_line("  mutex[%" PRIu32 "]=0x%08" PRIx32, i, entry_result.mutex_addresses[i]);
        log_line("CLOCK: calls=%" PRIu32 " ticks=%" PRIu64 " frequency=%" PRIu64 " microseconds=%" PRIu64,
                 entry_result.clock_calls, entry_result.clock_ticks, entry_result.clock_frequency,
                 entry_result.clock_microseconds);
        log_line("CONSTRUCTOR: descriptor=0x%08" PRIx32 " target=0x%08" PRIx32 " toc=0x%08" PRIx32
                 " started=%d completed=%d", entry_result.constructor_descriptor, entry_result.constructor_target,
                 entry_result.constructor_toc, entry_result.constructor_started, entry_result.constructor_completed);
        log_line("SYNC: thread_id_calls=%" PRIu32 " locks=%" PRIu32 " unlocks=%" PRIu32
                 " last_mutex=0x%08" PRIx32 " held_at_stop=%" PRIu32 " native_live_after_cleanup=%" PRIu32,
                 entry_result.thread_id_calls, entry_result.lock_calls, entry_result.unlock_calls,
                 entry_result.last_locked_mutex, entry_result.native_held_at_stop, entry_result.native_live_after_cleanup);
        uint32_t atexit_free = 0, atexit_callback = 0;
        if (ds_image_read_be32(&image, 0x1AD5630, &atexit_free) &&
            ds_image_read_be32(&image, 0x1CE58E8, &atexit_callback))
            log_line("ATEXIT: free_slots=%" PRIu32 " callback=0x%08" PRIx32, atexit_free, atexit_callback);
        log_line("SINGLETON: started=%d completed=%d address=0x%08" PRIx32 " manager=0x%08" PRIx32,
                 entry_result.singleton_started, entry_result.singleton_completed,
                 entry_result.singleton_address, entry_result.manager_address);
        log_line("RWLOCK: calls=%" PRIu32 " handle=%" PRIu32 " output=0x%08" PRIx32
                 " attribute=0x%08" PRIx32 " protocol=%" PRIu32 " pshared=0x%08" PRIx32
                 " native_live_after_cleanup=%" PRIu32,
                 entry_result.rwlock_create_calls, entry_result.rwlock_handle, entry_result.rwlock_output,
                 entry_result.rwlock_attribute, entry_result.rwlock_attribute_words[0],
                 entry_result.rwlock_attribute_words[1], entry_result.native_rwlocks_after_cleanup);
        log_line("RWLOCK WRITER: locks=%" PRIu32 " unlocks=%" PRIu32 " held_at_stop=%" PRIu32,
                 entry_result.rwlock_write_locks, entry_result.rwlock_write_unlocks,
                 entry_result.native_rwlocks_held_at_stop);
        log_line("MANAGER: published=%d pool=0x%08" PRIx32 " pool_bytes=%" PRIu32 " allocator_setup_executed=%d",
                 entry_result.manager_published, entry_result.pool_request_base, entry_result.pool_request_size,
                 entry_result.pool_initialized);
        log_line("ALLOCATION: calls=%" PRIu32 " address=0x%08" PRIx32 " bytes=%" PRIu32
                 " bootstrap_fallback_calls=%" PRIu32,
                 entry_result.allocation_calls, entry_result.allocation_address, entry_result.allocation_size,
                 entry_result.bootstrap_fallback_calls);
        log_line("ATOMIC: attempts=%" PRIu32 " successes=%" PRIu32,
                 entry_result.atomic_attempts, entry_result.atomic_successes);
        log_line("REGISTRY: initialized=%d address=0x%08" PRIx32 " payload=0x%08" PRIx32
                 " entries=%" PRIu32 " allocation=0x%08" PRIx32 " allocation_bytes=%" PRIu32,
                 entry_result.registry_initialized, entry_result.registry_address, entry_result.registry_payload,
                 entry_result.registry_entries, entry_result.registry_allocation_address,
                 entry_result.registry_allocation_size);
        log_line("REGISTRATION: completed=%d key_registry=0x%08" PRIx32 " payload=0x%08" PRIx32
                 " entries=%" PRIu32 " allocation_bytes=%" PRIu32,
                 entry_result.registration_completed, entry_result.callback_registry_address,
                 entry_result.callback_payload, entry_result.callback_entries, entry_result.callback_allocation_size);
        log_line("KERNEL MUTEX: calls=%" PRIu32 " handle=0x%08" PRIx32 " output=0x%08" PRIx32
                 " attribute=0x%08" PRIx32 " protocol=%" PRIu32 " recursive=0x%08" PRIx32
                 " pshared=0x%08" PRIx32 " adaptive=0x%08" PRIx32 " native_live_after_cleanup=%" PRIu32,
                 entry_result.kernel_mutex_calls, entry_result.kernel_mutex_handle,
                 entry_result.kernel_mutex_output, entry_result.kernel_mutex_attribute,
                 entry_result.kernel_mutex_attribute_words[0], entry_result.kernel_mutex_attribute_words[1],
                 entry_result.kernel_mutex_attribute_words[2], entry_result.kernel_mutex_attribute_words[3],
                 entry_result.native_kernel_mutexes_after_cleanup);
        log_line("NODE: address=0x%08" PRIx32 " vector=0x%08" PRIx32 " second_singleton=0x%08" PRIx32,
                 entry_result.node_address, entry_result.node_vector_address, entry_result.second_singleton_address);
        log_line("CONSTRUCTORS: completed=%" PRIu32, entry_result.constructors_completed);
        log_line("NEXT CONSTRUCTOR: descriptor=0x%08" PRIx32 " target=0x%08" PRIx32,
                 entry_result.next_constructor_descriptor, entry_result.next_constructor_target);
        uint32_t locale_count=0, locale_vtable=0, locale_mutex=0;
        const bool locale_state_read = ds_image_read_be32(&image,0x1AD6020,&locale_count) &&
            ds_image_read_be32(&image,0x1CE64E8,&locale_vtable) &&
            ds_image_read_be32(&image,0x1CE64EC,&locale_mutex);
        log_line("RUNTIME HEAP: state_read=%d locale_count=%" PRIu32
                 " locale_vtable=0x%08" PRIx32 " mutex_pointer=0x%08" PRIx32
                 " first_guest=0x%08" PRIx32 " first_bytes=%" PRIu32
                 " last_guest=0x%08" PRIx32 " last_bytes=%" PRIu32
                 " allocations=%" PRIu32 " frees=%" PRIu32 " failures=%" PRIu32
                 " live_allocations=%" PRIu32 " requested_bytes=%zu live_bytes=%zu peak_bytes=%zu"
                 " backed_bytes=%zu arenas=%" PRIu32 " metadata_after_cleanup=%d status=%d"
                 " image_backed_bytes=%zu image_limit_bytes=%zu",
                 locale_state_read,locale_count,locale_vtable,locale_mutex,
                 entry_result.runtime_malloc_address,entry_result.runtime_malloc_size,
                 entry_result.runtime_last_malloc_address,entry_result.runtime_last_malloc_size,
                 entry_result.runtime_heap.allocation_calls,entry_result.runtime_heap.free_calls,
                 entry_result.runtime_heap.failed_allocations,entry_result.runtime_heap.live_allocations,
                 entry_result.runtime_heap.requested_bytes,entry_result.runtime_heap.live_bytes,
                 entry_result.runtime_heap.peak_live_bytes,entry_result.runtime_heap.backed_bytes,
                 entry_result.runtime_heap.arena_count,entry_result.runtime_heap_metadata_after_cleanup,
                 (int)entry_result.runtime_heap_status,image.allocated_bytes,image.memory_limit);
        uint32_t object_vtable=0,object_refs=0,string_size=0,string_capacity=0,tls_locale=0,public_locale=0;
        const uint32_t object=DS_HEAP_BASE+64;
        const bool object_read=ds_image_read_be32(&image,object,&object_vtable) &&
            ds_image_read_be32(&image,object+4,&object_refs) &&
            ds_image_read_be32(&image,object+44,&string_size) &&
            ds_image_read_be32(&image,object+48,&string_capacity) &&
            ds_image_read_be32(&image,0xE000430,&tls_locale) &&
            ds_image_read_be32(&image,DS_HEAP_BASE+48,&public_locale);
        log_line("LOCALE OBJECT: state_read=%d address=0x%08" PRIx32 " vtable=0x%08" PRIx32
                 " references=%" PRIu32 " string_size=%" PRIu32 " string_capacity=%" PRIu32
                 " tls_pointer=0x%08" PRIx32 " public_handle=0x%08" PRIx32,
                 object_read,object,object_vtable,object_refs,string_size,string_capacity,tls_locale,public_locale);
        uint32_t name_word=0,locale_flags=0,first_global=0,second_global=0,next_mutex_vtable=0,next_mutex_pointer=0;
        const bool publication_read=ds_image_read_be32(&image,object+28,&name_word) &&
            ds_image_read_be32(&image,object+16,&locale_flags) &&
            ds_image_read_be32(&image,0x1CE672C,&first_global) &&
            ds_image_read_be32(&image,0x1CE6730,&second_global) &&
            ds_image_read_be32(&image,0x1CE6558,&next_mutex_vtable) &&
            ds_image_read_be32(&image,0x1CE655C,&next_mutex_pointer);
        log_line("LOCALE PUBLICATION: state_read=%d name_word=0x%08" PRIx32 " flags=%" PRIu32
                 " first_global=0x%08" PRIx32 " second_global=0x%08" PRIx32
                 " next_mutex_vtable=0x%08" PRIx32 " next_mutex_pointer=0x%08" PRIx32,
                 publication_read,name_word,locale_flags,first_global,second_global,next_mutex_vtable,next_mutex_pointer);
        uint32_t stream_vtable=0,stream_state=0,stream_mask=0,stream_flags=0,stream_precision=0;
        uint32_t stream_handle=0,base_handle=0,tied_buffer=0,stream_locale=0,base_locale=0,facet_id=0,facet_counter=0;
        const bool stream_read=ds_image_read_be32(&image,0x1CE65D0,&stream_vtable) &&
            ds_image_read_be32(&image,0x1CE65D8,&stream_state) && ds_image_read_be32(&image,0x1CE65DC,&stream_mask) &&
            ds_image_read_be32(&image,0x1CE65E0,&stream_flags) && ds_image_read_be32(&image,0x1CE65E4,&stream_precision) &&
            ds_image_read_be32(&image,0x1CE6590,&stream_handle) && ds_image_read_be32(&image,0x1CE65F4,&base_handle) &&
            ds_image_read_be32(&image,0x1CE65F8,&tied_buffer) && ds_image_read_be32(&image,DS_HEAP_BASE+176,&stream_locale) &&
            ds_image_read_be32(&image,DS_HEAP_BASE+192,&base_locale) && ds_image_read_be32(&image,0x1CE6734,&facet_id) &&
            ds_image_read_be32(&image,0x1CE6724,&facet_counter);
        log_line("STREAM LOCALE: state_read=%d address=0x01ce65d0 vtable=0x%08" PRIx32
                 " state=%" PRIu32 " exception_mask=%" PRIu32 " flags=%" PRIu32 " precision=%" PRIu32
                 " stream_handle=0x%08" PRIx32 " base_handle=0x%08" PRIx32 " tied_buffer=0x%08" PRIx32
                 " stream_locale=0x%08" PRIx32 " base_locale=0x%08" PRIx32 " facet_id=%" PRIu32 " facet_counter=%" PRIu32,
                 stream_read,stream_vtable,stream_state,stream_mask,stream_flags,stream_precision,stream_handle,base_handle,
                 tied_buffer,stream_locale,base_locale,facet_id,facet_counter);
        uint32_t facet_vtable=0,facet_refs=0,facet_table=0,lower_table=0,upper_table=0,facet_cache=0;
        uint32_t cleanup_head=0,cleanup_next=0,cleanup_facet=0,cleanup_callback=0;
        const uint32_t facet=DS_HEAP_BASE+208;
        const bool facet_read=ds_image_read_be32(&image,facet,&facet_vtable) && ds_image_read_be32(&image,facet+4,&facet_refs) &&
            ds_image_read_be32(&image,facet+8,&facet_table) && ds_image_read_be32(&image,facet+12,&lower_table) &&
            ds_image_read_be32(&image,facet+16,&upper_table) && ds_image_read_be32(&image,0x1B50430,&facet_cache) &&
            ds_image_read_be32(&image,0x1CE6720,&cleanup_head) && ds_image_read_be32(&image,DS_HEAP_BASE+240,&cleanup_next) &&
            ds_image_read_be32(&image,DS_HEAP_BASE+244,&cleanup_facet) && ds_image_read_be32(&image,0x1CE58E4,&cleanup_callback);
        log_line("FIRST FACET: state_read=%d address=0x%08" PRIx32 " vtable=0x%08" PRIx32 " references=%" PRIu32
                 " table=0x%08" PRIx32 " lower_table=0x%08" PRIx32 " upper_table=0x%08" PRIx32
                 " cache=0x%08" PRIx32 " cleanup_head=0x%08" PRIx32 " cleanup_next=0x%08" PRIx32
                 " cleanup_node=0x600000f0 cleanup_facet=0x%08" PRIx32 " cleanup_callback=0x%08" PRIx32,
                 facet_read,facet,facet_vtable,facet_refs,facet_table,lower_table,upper_table,facet_cache,cleanup_head,
                 cleanup_next,cleanup_facet,cleanup_callback);
        uint32_t stream_registration=0,stream_fill=0,stream_references=0,stream_slot=0;
        uint32_t next_vtable=0,next_base_vtable=0;
        const uint8_t *fill_address=ds_image_address(&image,0x1CE6600,1,DS_PF_R);
        const uint8_t *references_address=ds_image_address(&image,0x1CE6489,1,DS_PF_R);
        const bool first_stream_read=fill_address && references_address &&
            ds_image_read_be32(&image,0x1CE65D4,&stream_registration) &&
            ds_image_read_be32(&image,0x1CE6498,&stream_slot) &&
            ds_image_read_be32(&image,0x1CE6604,&next_vtable) &&
            ds_image_read_be32(&image,0x1CE6608,&next_base_vtable);
        if (first_stream_read) { stream_fill=*fill_address;stream_references=*references_address; }
        log_line("FIRST STREAM: state_read=%d address=0x01ce65d0 registration=%" PRIu32
                 " fill=%" PRIu32 " registry=0x01ce6488 references=%" PRIu32
                 " slot=0x%08" PRIx32 " next_vtable=0x%08" PRIx32 " next_base_vtable=0x%08" PRIx32,
                 first_stream_read,stream_registration,stream_fill,stream_references,stream_slot,next_vtable,next_base_vtable);
        uint32_t narrow_slots[4]={0},narrow_indices[4]={0},narrow_refs[2]={0};
        uint32_t third_flags=0,third_handle=0,final_tie=0;
        const uint32_t narrow_streams[4]={0x1CE65D0,0x1CE6608,0x1CE66B4,0x1CE66EC};
        bool narrow_read=ds_image_read_be32(&image,0x1CE6488,&narrow_refs[0]) &&
            ds_image_read_be32(&image,0x1CE648C,&narrow_refs[1]) &&
            ds_image_read_be32(&image,0x1CE66C4,&third_flags) &&
            ds_image_read_be32(&image,0x1CE6678,&third_handle) &&
            ds_image_read_be32(&image,0x1CE6718,&final_tie);
        for (unsigned i=0;i<4;++i) {
            narrow_read &= ds_image_read_be32(&image,0x1CE6498+4*i,&narrow_slots[i]);
            narrow_read &= ds_image_read_be32(&image,narrow_streams[i]+4,&narrow_indices[i]);
        }
        log_line("NARROW STREAMS: state_read=%d slot1=0x%08" PRIx32 " slot2=0x%08" PRIx32
                 " slot3=0x%08" PRIx32 " slot4=0x%08" PRIx32
                 " index1=%" PRIu32 " index2=%" PRIu32 " index3=%" PRIu32 " index4=%" PRIu32
                 " references0=0x%08" PRIx32 " references1=0x%08" PRIx32
                 " third_flags=0x%08" PRIx32 " third_handle=0x%08" PRIx32 " final_tie=0x%08" PRIx32,
                 narrow_read,narrow_slots[0],narrow_slots[1],narrow_slots[2],narrow_slots[3],
                 narrow_indices[0],narrow_indices[1],narrow_indices[2],narrow_indices[3],narrow_refs[0],narrow_refs[1],
                 third_flags,third_handle,final_tie);
        uint32_t wide_slots[4]={0},wide_indices[4]={0},wide_fill[4]={0},wide_refs[3]={0};
        uint32_t wide_cache=0,wide_table=0,wide_state=0,wide_cleanup=0,wide_next=0,wide_node_facet=0;
        const uint32_t wide_streams[4]={0x1CE68A8,0x1CE68E0,0x1CE6918,0x1CE6950};
        bool wide_read=ds_image_read_be32(&image,0x1B93E84,&wide_cache) &&
            ds_image_read_be32(&image,0x60000258,&wide_table) && ds_image_read_be32(&image,0x6000025C,&wide_state) &&
            ds_image_read_be32(&image,0x1CE6720,&wide_cleanup) && ds_image_read_be32(&image,0x60000260,&wide_next) &&
            ds_image_read_be32(&image,0x60000264,&wide_node_facet);
        for(unsigned i=0;i<4;++i){
            uint32_t fill_word=0;
            wide_read &= ds_image_read_be32(&image,0x1CE64A8+4*i,&wide_slots[i]);
            wide_read &= ds_image_read_be32(&image,wide_streams[i]+4,&wide_indices[i]);
            wide_read &= ds_image_read_be32(&image,wide_streams[i]+0x30,&fill_word);
            wide_fill[i]=fill_word>>16;
        }
        for(unsigned i=0;i<3;++i)wide_read &= ds_image_read_be32(&image,0x1CE6488+4*i,&wide_refs[i]);
        log_line("WIDE STREAMS: state_read=%d slot5=0x%08" PRIx32 " slot6=0x%08" PRIx32
                 " slot7=0x%08" PRIx32 " slot8=0x%08" PRIx32
                 " index5=%" PRIu32 " index6=%" PRIu32 " index7=%" PRIu32 " index8=%" PRIu32
                 " fill5=%" PRIu32 " fill6=%" PRIu32 " fill7=%" PRIu32 " fill8=%" PRIu32
                 " references0=0x%08" PRIx32 " references1=0x%08" PRIx32 " references2=0x%08" PRIx32
                 " facet=0x%08" PRIx32 " conversion_table=0x%08" PRIx32 " conversion_state=0x%08" PRIx32
                 " cleanup_head=0x%08" PRIx32 " cleanup_next=0x%08" PRIx32 " cleanup_facet=0x%08" PRIx32,
                 wide_read,wide_slots[0],wide_slots[1],wide_slots[2],wide_slots[3],
                 wide_indices[0],wide_indices[1],wide_indices[2],wide_indices[3],wide_fill[0],wide_fill[1],wide_fill[2],wide_fill[3],
                 wide_refs[0],wide_refs[1],wide_refs[2],wide_cache,wide_table,wide_state,wide_cleanup,wide_next,wide_node_facet);
        uint32_t narrow_count=0,wide_count=0;
        uint64_t runtime_counts[5]={0};
        bool initializer_read=ds_image_read_be32(&image,0x1AD6020,&narrow_count) &&
            ds_image_read_be32(&image,0x1AD6088,&wide_count);
        for(unsigned i=0;i<5;++i)initializer_read &= ds_image_read_be64(&image,0x1AD61B0+8*i,&runtime_counts[i]);
        log_line("RUNTIME INITIALIZERS: state_read=%d narrow=%" PRIu32 " wide=%" PRIu32
                 " counter0=%" PRIu64 " counter1=%" PRIu64 " counter2=%" PRIu64
                 " counter3=%" PRIu64 " counter4=%" PRIu64,
                 initializer_read,narrow_count,wide_count,runtime_counts[0],runtime_counts[1],runtime_counts[2],runtime_counts[3],runtime_counts[4]);
        log_line("ENTRY RESULT: reached=%d stop=%d nid=0x%08" PRIx32 " callee=0x%08" PRIx32
                 " functions=%" PRIu32 " accesses=%" PRIu32 " error=%s", entry_reached,
                 (int)entry_result.stop, entry_result.boundary_nid, entry_result.callee,
                 entry_result.function_count, entry_result.trace_count, entry_error);
        log_line("ENTRY ABI: r3=0x%" PRIx64 " r4=0x%" PRIx64 " r5=0x%" PRIx64
                 " r6=0x%" PRIx64 " lr=0x%" PRIx64 " sp=0x%" PRIx64,
                 entry_result.r3, entry_result.r4, entry_result.r5, entry_result.r6,
                 entry_result.lr, entry_result.stack_pointer);
        for (uint32_t i = 0; i < entry_result.function_count && i < DS_ENTRY_FUNCTION_CAPACITY; ++i)
            log_line("  guest function[%" PRIu32 "]=0x%08" PRIx32, i, entry_result.functions[i]);
        if (entry_result.memory_fault)
            log_line("  memory fault address=0x%" PRIx64 " size=%" PRIu32
                     " write=%d required_flags=0x%" PRIx32, entry_result.fault_address,
                     entry_result.fault_size, entry_result.fault_write, entry_result.fault_required_flags);
        for (uint32_t i = 0; i < entry_result.trace_stored && i < 32; ++i) {
            const DsEntryAccess *access = &entry_result.trace[i];
            log_line("  %s size=%" PRIu32 " address=0x%" PRIx64 " value=0x%" PRIx64,
                     access->write ? "write" : "read", access->size, access->address, access->value);
        }
#endif
    }
    draw_result();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    mkdir("sdmc:/switch/demons-souls-loader", 0777);
    log_file = fopen(log_path, "w");
    log_line("Demon's Souls Switch startup v0.27.0; no gameplay implementation");
    log_line("HOS=0x%08" PRIx32 " Atmosphere=%d loader_applet_type=%d",
             (uint32_t)hosversionGet(), hosversionIsAtmosphere(), (int)appletGetAppletType());
    u64 total = 0;
    Result info_rc = svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    log_line("Process allowance rc=0x%08" PRIx32 " bytes=%" PRIu64,
             (uint32_t)info_rc, (uint64_t)total);
    perform_load();
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 pressed = padGetButtonsDown(&pad);
        if (pressed & HidNpadButton_Plus) break;
        if (pressed & HidNpadButton_A) perform_load();
        consoleUpdate(NULL);
    }
    ds_image_destroy(&image);
    log_line("Cleanup complete: backed bytes=%zu", image.allocated_bytes);
    if (log_file) fclose(log_file);
    consoleExit(NULL);
    return 0;
}
