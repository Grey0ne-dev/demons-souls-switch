#include "ds_image.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DS_ELF_HEADER_SIZE 64u
#define DS_ELF_PROGRAM_HEADER_SIZE 56u
#define DS_GUEST_LIMIT UINT64_C(0x100000000)

bool ds_image_memory_info(const DsImage *image, uint32_t *total, uint32_t *available)
{
    if (!image || !total || !available || image->allocated_bytes > image->memory_limit)
        return false;
    uint64_t limit = image->memory_limit;
    if (limit > UINT32_MAX) limit = UINT32_MAX;
    limit &= ~UINT64_C(0xFFFFF);
    uint64_t remaining = image->allocated_bytes < limit ? limit - image->allocated_bytes : 0;
    *total = (uint32_t)limit;
    *available = (uint32_t)(remaining & ~UINT64_C(0xFFFFF));
    return true;
}

static bool fail(char *error, size_t capacity, const char *format, ...)
{
    if (error && capacity) {
        va_list args;
        va_start(args, format);
        vsnprintf(error, capacity, format, args);
        va_end(args);
    }
    return false;
}

static void clear_error(char *error, size_t capacity)
{
    if (error && capacity) error[0] = '\0';
}

static uint16_t be16(const unsigned char *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t be64(const unsigned char *p)
{
    return ((uint64_t)be32(p) << 32) | be32(p + 4);
}

static bool guest_range(uint64_t address, uint64_t size)
{
    return address < DS_GUEST_LIMIT && size <= DS_GUEST_LIMIT - address;
}

static bool file_range(uint64_t offset, uint64_t size, uint64_t file_size)
{
    return offset <= file_size && size <= file_size - offset;
}

static bool read_at(FILE *file, uint64_t offset, void *data, size_t size,
                    char *error, size_t capacity)
{
    if (offset > (uint64_t)LONG_MAX || fseek(file, (long)offset, SEEK_SET) != 0)
        return fail(error, capacity, "Could not seek to file offset 0x%llx",
                    (unsigned long long)offset);
    if (size && fread(data, 1, size, file) != size)
        return fail(error, capacity, "Input is truncated or unreadable at file offset 0x%llx",
                    (unsigned long long)offset);
    return true;
}

static const DsSegment *find_segment(const DsImage *image, uint32_t address,
                                     size_t size, uint32_t required_flags,
                                     bool require_file_backing)
{
    if (!image || !size || image->segment_count > DS_MAX_SEGMENTS ||
        !guest_range(address, (uint64_t)size)) return NULL;
    for (size_t i = 0; i < image->segment_count; ++i) {
        const DsSegment *segment = &image->segments[i];
        if (!segment->data || address < segment->guest_address ||
            (segment->flags & required_flags) != required_flags ||
            (require_file_backing && !segment->from_elf)) continue;
        uint64_t delta = (uint64_t)address - segment->guest_address;
        uint64_t limit = require_file_backing ? segment->file_size : segment->memory_size;
        if (delta <= limit && (uint64_t)size <= limit - delta) return segment;
    }
    return NULL;
}

void *ds_image_address(const DsImage *image, uint32_t guest_address,
                       size_t size, uint32_t required_flags)
{
    const DsSegment *segment = find_segment(image, guest_address, size, required_flags, false);
    if (!segment) return NULL;
    return segment->data + ((uint64_t)guest_address - segment->guest_address);
}

void ds_image_destroy(DsImage *image)
{
    if (!image) return;
    for (size_t i = 0; i < image->segment_count && i < DS_MAX_SEGMENTS; ++i)
        free(image->segments[i].data);
    memset(image, 0, sizeof(*image));
}

bool ds_image_add_zero_region(DsImage *image, uint32_t guest_address, size_t size,
                              uint32_t flags, char *error, size_t error_capacity)
{
    clear_error(error, error_capacity);
    if (!image) return fail(error, error_capacity, "No image supplied");
    if (!size || !guest_range(guest_address, (uint64_t)size))
        return fail(error, error_capacity, "Invalid 32-bit guest-memory range");
    if (image->segment_count >= DS_MAX_SEGMENTS)
        return fail(error, error_capacity, "Too many guest-memory regions");
    if (image->allocated_bytes > image->memory_limit ||
        size > image->memory_limit - image->allocated_bytes)
        return fail(error, error_capacity, "Guest-memory budget exceeded");
    uint64_t end = (uint64_t)guest_address + size;
    for (size_t i = 0; i < image->segment_count; ++i) {
        const DsSegment *other = &image->segments[i];
        uint64_t other_end = (uint64_t)other->guest_address + other->memory_size;
        if ((uint64_t)guest_address < other_end && (uint64_t)other->guest_address < end)
            return fail(error, error_capacity, "Overlapping guest-memory regions");
    }
    unsigned char *data = calloc(1, size);
    if (!data) return fail(error, error_capacity, "Could not allocate %zu guest-memory bytes", size);
    DsSegment *segment = &image->segments[image->segment_count++];
    memset(segment, 0, sizeof(*segment));
    segment->guest_address = guest_address;
    segment->flags = flags;
    segment->memory_size = size;
    segment->file_offset = UINT64_MAX;
    segment->data = data;
    image->allocated_bytes += size;
    return true;
}

bool ds_image_remove_zero_region(DsImage *image, uint32_t address, size_t size,
                                 const void *expected_backing, char *error, size_t capacity)
{
    clear_error(error,capacity);
    if (!image || !expected_backing || !size || image->segment_count > DS_MAX_SEGMENTS ||
        !guest_range(address,size)) return fail(error,capacity,"Invalid owned-region release");
    for (size_t i=0;i<image->segment_count;++i) {
        DsSegment *segment=&image->segments[i];
        if (segment->guest_address!=address || segment->memory_size!=size) continue;
        if (segment->from_elf || segment->file_size || segment->file_offset!=UINT64_MAX ||
            segment->data!=expected_backing || image->allocated_bytes<size)
            return fail(error,capacity,"Region release does not match owned anonymous backing");
        free(segment->data);
        memmove(segment,segment+1,(image->segment_count-i-1)*sizeof(*segment));
        --image->segment_count;
        memset(&image->segments[image->segment_count],0,sizeof(*segment));
        image->allocated_bytes-=size;
        return true;
    }
    return fail(error,capacity,"Owned region is not present");
}

bool ds_image_read_be32(const DsImage *image, uint32_t address, uint32_t *value)
{
    if (!value) return false;
    const unsigned char *data = ds_image_address(image, address, 4, DS_PF_R);
    if (!data) return false;
    *value = be32(data);
    return true;
}

bool ds_image_read_be64(const DsImage *image, uint32_t address, uint64_t *value)
{
    if (!value) return false;
    const unsigned char *data = ds_image_address(image, address, 8, DS_PF_R);
    if (!data) return false;
    *value = be64(data);
    return true;
}

bool ds_image_write_be32(DsImage *image, uint32_t address, uint32_t value)
{
    unsigned char *data = ds_image_address(image, address, 4, DS_PF_W);
    if (!data) return false;
    data[0] = (unsigned char)(value >> 24);
    data[1] = (unsigned char)(value >> 16);
    data[2] = (unsigned char)(value >> 8);
    data[3] = (unsigned char)value;
    return true;
}

static bool valid_alignment(uint64_t alignment, uint64_t address, uint64_t offset)
{
    return alignment <= 1 ||
           ((alignment & (alignment - 1)) == 0 &&
            (address & (alignment - 1)) == (offset & (alignment - 1)));
}

bool ds_image_load_file(DsImage *image, const char *path, size_t memory_limit,
                        char *error, size_t error_capacity)
{
    clear_error(error, error_capacity);
    if (!image) return fail(error, error_capacity, "No image supplied");
    memset(image, 0, sizeof(*image));
    if (!path) return fail(error, error_capacity, "No input path supplied");
    if (!memory_limit) return fail(error, error_capacity, "Guest-memory budget is zero");
    FILE *file = fopen(path, "rb");
    if (!file) return fail(error, error_capacity, "Could not open input: %s", path);
    bool success = false;
    uint64_t tls_offset = 0;
    bool tls_seen = false;
    image->memory_limit = memory_limit;
    if (fseek(file, 0, SEEK_END) != 0) {
        fail(error, error_capacity, "Could not determine input size");
        goto done;
    }
    long end = ftell(file);
    if (end < 0) {
        fail(error, error_capacity, "Could not determine input size");
        goto done;
    }
    uint64_t file_size = (uint64_t)end;
    if (file_size < DS_ELF_HEADER_SIZE) {
        fail(error, error_capacity, "Input is too small for an ELF64 header");
        goto done;
    }
    unsigned char header[DS_ELF_HEADER_SIZE];
    if (!read_at(file, 0, header, sizeof(header), error, error_capacity)) goto done;
    if (memcmp(header, "SCE\0", 4) == 0) {
        fail(error, error_capacity, "Encrypted SELF input: use the prepared decrypted EBOOT.ELF");
        goto done;
    }
    if (memcmp(header, "\177ELF", 4) != 0 || header[4] != 2 || header[5] != 2 ||
        header[6] != 1 || be16(header + 16) != 2 || be16(header + 18) != 21 ||
        be32(header + 20) != 1 || be16(header + 52) != DS_ELF_HEADER_SIZE) {
        fail(error, error_capacity, "Expected ELF64 big-endian PowerPC64 executable");
        goto done;
    }
    uint64_t entry = be64(header + 24);
    uint64_t program_offset = be64(header + 32);
    uint16_t program_size = be16(header + 54);
    uint16_t program_count = be16(header + 56);
    if (entry > UINT32_MAX || (entry & 3) != 0) {
        fail(error, error_capacity, "Invalid 32-bit aligned entry descriptor");
        goto done;
    }
    image->entry_descriptor = (uint32_t)entry;
    if (program_size != DS_ELF_PROGRAM_HEADER_SIZE || !program_count ||
        program_count == UINT16_MAX || program_offset < DS_ELF_HEADER_SIZE ||
        !file_range(program_offset, (uint64_t)program_count * program_size, file_size)) {
        fail(error, error_capacity, "Invalid or truncated ELF program-header table");
        goto done;
    }
    for (uint16_t i = 0; i < program_count; ++i) {
        unsigned char program[DS_ELF_PROGRAM_HEADER_SIZE];
        if (!read_at(file, program_offset + (uint64_t)i * program_size, program,
                     sizeof(program), error, error_capacity)) goto done;
        uint32_t type = be32(program);
        if (type != 1 && type != 7) continue;
        uint32_t flags = be32(program + 4);
        uint64_t offset = be64(program + 8);
        uint64_t address = be64(program + 16);
        uint64_t file_bytes = be64(program + 32);
        uint64_t memory_bytes = be64(program + 40);
        uint64_t alignment = be64(program + 48);
        if (file_bytes > memory_bytes || !file_range(offset, file_bytes, file_size)) {
            fail(error, error_capacity, "Invalid file range or file/memory size in program header %u",
                 (unsigned)i);
            goto done;
        }
        /* Retail PS3 images include empty LOAD records whose alignment fields
         * are immaterial; no arena or access is created for those records. */
        if (type == 1 && memory_bytes == 0) continue;
        if (!guest_range(address, memory_bytes) || memory_bytes > SIZE_MAX ||
            file_bytes > SIZE_MAX || !valid_alignment(alignment, address, offset)) {
            fail(error, error_capacity, "Invalid guest range or alignment in program header %u",
                 (unsigned)i);
            goto done;
        }
        if (type == 7) {
            if (tls_seen) {
                fail(error, error_capacity, "Multiple TLS templates are unsupported");
                goto done;
            }
            tls_seen = true;
            tls_offset = offset;
            image->tls_address = (uint32_t)address;
            image->tls_file_size = (size_t)file_bytes;
            image->tls_memory_size = (size_t)memory_bytes;
            continue;
        }
        if (!ds_image_add_zero_region(image, (uint32_t)address, (size_t)memory_bytes,
                                      flags, error, error_capacity)) goto done;
        DsSegment *segment = &image->segments[image->segment_count - 1];
        segment->file_offset = offset;
        segment->file_size = (size_t)file_bytes;
        segment->from_elf = true;
        if (!read_at(file, offset, segment->data, segment->file_size,
                     error, error_capacity)) goto done;
    }
    if (!image->segment_count) {
        fail(error, error_capacity, "ELF has no nonempty loadable segments");
        goto done;
    }
    if (tls_seen && image->tls_file_size) {
        const DsSegment *segment = find_segment(image, image->tls_address,
                                                image->tls_file_size, DS_PF_R, true);
        if (!segment || segment->file_offset +
            ((uint64_t)image->tls_address - segment->guest_address) != tls_offset) {
            fail(error, error_capacity, "TLS template is not backed by the matching readable LOAD bytes");
            goto done;
        }
    }
    if (!find_segment(image, image->entry_descriptor, 8, DS_PF_R, true) ||
        !ds_image_read_be32(image, image->entry_descriptor, &image->entry_code) ||
        !ds_image_read_be32(image, image->entry_descriptor + 4, &image->toc)) {
        fail(error, error_capacity, "Entry descriptor is not backed by readable LOAD bytes");
        goto done;
    }
    if ((image->entry_code & 3) != 0 ||
        !find_segment(image, image->entry_code, 4, DS_PF_X, true)) {
        fail(error, error_capacity, "Entry code is not backed by executable LOAD bytes");
        goto done;
    }
    if (!ds_image_address(image, image->toc, 4, DS_PF_R)) {
        fail(error, error_capacity, "Entry TOC is not in readable guest memory");
        goto done;
    }
    success = true;

done:
    if (fclose(file) != 0 && success) {
        fail(error, error_capacity, "Could not close input after loading");
        success = false;
    }
    if (!success) ds_image_destroy(image);
    return success;
}
