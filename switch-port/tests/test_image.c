#define _POSIX_C_SOURCE 200809L
#include "ds_image.h"
#include "ds_reference.h"

#include <openssl/evp.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned tests, failed;
static const size_t fixture_size = 288;
static char fixture_path[256];

#define CHECK(condition, name) do { \
    ++tests; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s (%s:%d)\n", name, __FILE__, __LINE__); \
        ++failed; \
    } \
} while (0)

static void put16(unsigned char *b, size_t at, uint16_t v)
{
    b[at] = (unsigned char)(v >> 8);
    b[at + 1] = (unsigned char)v;
}

static void put32(unsigned char *b, size_t at, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) b[at + i] = (unsigned char)(v >> ((3 - i) * 8));
}

static void put64(unsigned char *b, size_t at, uint64_t v)
{
    for (unsigned i = 0; i < 8; ++i) b[at + i] = (unsigned char)(v >> ((7 - i) * 8));
}

static void phdr(unsigned char *b, unsigned index, uint32_t type, uint32_t flags,
                 uint64_t offset, uint64_t address, uint64_t filesz, uint64_t memsz,
                 uint64_t alignment)
{
    size_t p = 64 + index * 56;
    put32(b, p, type);
    put32(b, p + 4, flags);
    put64(b, p + 8, offset);
    put64(b, p + 16, address);
    put64(b, p + 24, address);
    put64(b, p + 32, filesz);
    put64(b, p + 40, memsz);
    put64(b, p + 48, alignment);
}

static void fixture(unsigned char b[288])
{
    memset(b, 0, 288);
    memcpy(b, "\177ELF\002\002\001\146", 8);
    put16(b, 16, 2);
    put16(b, 18, 21);
    put32(b, 20, 1);
    put64(b, 24, 0x20000);
    put64(b, 32, 64);
    put16(b, 52, 64);
    put16(b, 54, 56);
    put16(b, 56, 3);
    put16(b, 58, 64);
    phdr(b, 0, 1, DS_PF_R | DS_PF_X, 0, 0x10000, 256, 256, 256);
    phdr(b, 1, 1, DS_PF_R | DS_PF_W, 256, 0x20000, 32, 64, 256);
    phdr(b, 2, 7, DS_PF_R, 264, 0x20008, 4, 16, 8);
    put32(b, 240, 0x4e800020); /* An inert PPC blr word; never executed by tests. */
    put32(b, 256, 0x100f0);
    put32(b, 260, 0x20010);
    put32(b, 264, 0x11223344);
}

static void write_fixture(const unsigned char *bytes, size_t size)
{
    FILE *f = fopen(fixture_path, "wb");
    if (!f || fwrite(bytes, 1, size, f) != size || fclose(f) != 0) {
        perror("fixture write");
        exit(2);
    }
}

static int empty(const DsImage *image)
{
    if (image->segment_count || image->allocated_bytes) return 0;
    for (size_t i = 0; i < DS_MAX_SEGMENTS; ++i)
        if (image->segments[i].data) return 0;
    return 1;
}

static void rejects(const char *name, const unsigned char *bytes, size_t size, size_t budget)
{
    DsImage image = {0};
    char error[256] = {0};
    write_fixture(bytes, size);
    bool ok = ds_image_load_file(&image, fixture_path, budget, error, sizeof(error));
    CHECK(!ok, name);
    CHECK(ok || error[0] != '\0', "rejection has an error message");
    CHECK(ok || empty(&image), "rejection frees partial allocations");
    ds_image_destroy(&image);
}

static void digest(const unsigned char *bytes, size_t size, unsigned char out[32])
{
    unsigned length = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx || EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, bytes, size) != 1 ||
        EVP_DigestFinal_ex(ctx, out, &length) != 1 || length != 32) {
        fprintf(stderr, "OpenSSL SHA-256 failed\n");
        exit(2);
    }
    EVP_MD_CTX_free(ctx);
}

static void actual_image(const char *path)
{
    DsImage image = {0};
    char error[256] = {0};
    bool loaded = ds_image_load_file(&image, path, (size_t)DS_REF_ALLOCATED_BYTES,
                                     error, sizeof(error));
    CHECK(loaded, "actual ELF loads within exact backed-memory budget");
    if (!loaded) {
        fprintf(stderr, "Actual image loader: %s\n", error);
        return;
    }
    CHECK(image.segment_count == DS_REF_SEGMENT_COUNT, "actual PT_LOAD count");
    CHECK(image.allocated_bytes == DS_REF_ALLOCATED_BYTES, "actual backed-byte count");
    CHECK(image.entry_descriptor == DS_REF_ENTRY_DESCRIPTOR &&
          image.entry_code == DS_REF_ENTRY_CODE && image.toc == DS_REF_TOC,
          "actual entry descriptor, code and TOC");
    CHECK(image.tls_address == DS_REF_TLS_ADDRESS && image.tls_file_size == DS_REF_TLS_FILE_SIZE &&
          image.tls_memory_size == DS_REF_TLS_MEMORY_SIZE, "actual TLS template metadata");
    for (size_t i = 0; i < image.segment_count; ++i) {
        const DsSegment *s = &image.segments[i];
        const DsReferenceSegment *r = &DS_REF_SEGMENTS[i];
        CHECK(s->guest_address == r->guest_address && s->flags == r->flags &&
              s->file_offset == r->file_offset && s->file_size == r->file_size &&
              s->memory_size == r->memory_size && s->from_elf, "actual segment metadata");
        unsigned char hash[32];
        digest(s->data, s->file_size, hash);
        CHECK(memcmp(hash, r->file_sha256, 32) == 0, "loaded file bytes match independent SHA-256");
        digest(s->data, s->memory_size, hash);
        CHECK(memcmp(hash, r->memory_sha256, 32) == 0, "loaded memory plus BSS matches SHA-256");
        bool zero = true;
        for (size_t j = s->file_size; j < s->memory_size; ++j)
            if (s->data[j] != 0) { zero = false; break; }
        CHECK(zero, "actual BSS is zero");
    }
    uint32_t word = 0;
    CHECK(ds_image_read_be32(&image, DS_REF_ENTRY_DESCRIPTOR, &word) && word == DS_REF_ENTRY_CODE,
          "actual translated entry read is big-endian");
    CHECK(ds_image_address(&image, DS_REF_ENTRY_CODE, 4, DS_PF_X) != NULL,
          "actual entry lies in executable segment");
    CHECK(!ds_image_write_be32(&image, DS_REF_ENTRY_CODE, 0), "actual text writes rejected");
    ds_image_destroy(&image);
    CHECK(empty(&image), "actual image cleanup");
    loaded = ds_image_load_file(&image, path, (size_t)DS_REF_ALLOCATED_BYTES - 1,
                                error, sizeof(error));
    CHECK(!loaded && empty(&image), "actual ELF rejects insufficient budget cleanly");
    ds_image_destroy(&image);
}

static void valid_and_regions(void)
{
    unsigned char bytes[288]; fixture(bytes); write_fixture(bytes, fixture_size);
    DsImage image = {0};
    char error[256] = {0};
    bool ok = ds_image_load_file(&image, fixture_path, 2048, error, sizeof(error));
    CHECK(ok, "synthetic valid ELF loads");
    if (!ok) { fprintf(stderr, "Synthetic image loader: %s\n", error); return; }
    CHECK(image.segment_count == 2 && image.allocated_bytes == 320, "synthetic backing is sparse");
    CHECK(image.entry_descriptor == 0x20000 && image.entry_code == 0x100f0 && image.toc == 0x20010,
          "synthetic descriptor resolves through guest translation");
    CHECK(image.tls_address == 0x20008 && image.tls_file_size == 4 && image.tls_memory_size == 16,
          "synthetic TLS retained");
    uint32_t word = 0;
    CHECK(ds_image_read_be32(&image, 0x10000, &word) && word == 0x7f454c46,
          "big-endian read of text bytes");
    CHECK(!ds_image_write_be32(&image, 0x100f0, 0), "read-only text rejects writes");
    CHECK(ds_image_address(&image, 0x100ff, 2, DS_PF_R) == NULL, "access crossing mapped boundary rejected");
    CHECK(ds_image_address(&image, UINT32_MAX, 2, 0) == NULL, "access wrapping 32-bit guest address rejected");
    CHECK(ds_image_address(&image, 0x18000, 1, 0) == NULL, "unbacked gap rejects access");
    CHECK(ds_image_address(&image, 0x20000, 4, DS_PF_X) == NULL, "execution permission checked");
    CHECK(ds_image_add_zero_region(&image, 0x30000, 128, DS_PF_R | DS_PF_W,
                                    error, sizeof(error)), "anonymous region added");
    unsigned char *zeroes = ds_image_address(&image, 0x30000, 128, DS_PF_R);
    bool all_zero = zeroes != NULL;
    if (zeroes) for (unsigned i = 0; i < 128; ++i) if (zeroes[i]) all_zero = false;
    CHECK(all_zero, "anonymous backing is zero-filled");
    CHECK(ds_image_write_be32(&image, 0x30003, 0x12345678), "unaligned guest write supported");
    CHECK(ds_image_read_be32(&image, 0x30003, &word) && word == 0x12345678,
          "unaligned guest read preserves endian order");
    uint64_t wide = 0;
    CHECK(ds_image_read_be64(&image, 0x30000, &wide) && wide == UINT64_C(0x0000001234567800),
          "64-bit big-endian guest read");
    size_t count = image.segment_count, used = image.allocated_bytes;
    CHECK(!ds_image_add_zero_region(&image, 0x30040, 64, DS_PF_R | DS_PF_W,
                                     error, sizeof(error)), "anonymous overlap rejected");
    CHECK(image.segment_count == count && image.allocated_bytes == used,
          "rejected mapping retains existing image");
    CHECK(!ds_image_add_zero_region(&image, 0x40000, 2048, DS_PF_R | DS_PF_W,
                                     error, sizeof(error)), "anonymous budget enforced");
    CHECK(!ds_image_add_zero_region(&image, UINT32_MAX - 15, 32, DS_PF_R,
                                     error, sizeof(error)), "anonymous 32-bit wrap rejected");
    while (image.segment_count < DS_MAX_SEGMENTS) {
        uint32_t addr = 0x40000 + (uint32_t)image.segment_count * 0x1000;
        if (!ds_image_add_zero_region(&image, addr, 1, DS_PF_R, error, sizeof(error))) break;
    }
    CHECK(image.segment_count == DS_MAX_SEGMENTS, "region table capacity usable");
    CHECK(!ds_image_add_zero_region(&image, 0x100000, 1, DS_PF_R,
                                     error, sizeof(error)), "region table overflow rejected");
    ds_image_destroy(&image); ds_image_destroy(&image);
    CHECK(empty(&image), "cleanup is idempotent");
}

static void malformed(void)
{
    unsigned char b[288];
#define BAD(label, code) do { fixture(b); code; rejects(label, b, sizeof(b), 1024 * 1024); } while (0)
    fixture(b); rejects("truncated ELF header", b, 32, 1024);
    BAD("SELF container rejected", memcpy(b, "SCE\0", 4));
    BAD("ELF32 rejected", b[4] = 1);
    BAD("little-endian ELF rejected", b[5] = 1);
    BAD("non-EXEC ELF rejected", put16(b, 16, 3));
    BAD("wrong CPU machine rejected", put16(b, 18, 183));
    BAD("undersized program header rejected", put16(b, 54, 8));
    BAD("program table past EOF rejected", put16(b, 56, 65535));
    BAD("program table overflow rejected", put64(b, 32, UINT64_MAX - 24));
    BAD("segment file offset overflow rejected", put64(b, 64 + 8, UINT64_MAX - 16));
    BAD("file size exceeds memory size rejected", put64(b, 64 + 32, 257));
    BAD("segment data truncated rejected", put64(b, 120 + 8, 280));
    BAD("overlapping guest segments rejected", put64(b, 120 + 16, 0x100f0));
    BAD("guest segment outside 32-bit space rejected", put64(b, 64 + 16, UINT64_C(0x100000000)));
    BAD("guest segment wraps address limit rejected", put64(b, 120 + 16, UINT64_C(0xfffffff0)));
    BAD("unmapped entry descriptor rejected", put64(b, 24, 0x50000));
    BAD("partial entry descriptor rejected", put64(b, 24, 0x2003c));
    BAD("non-executable entry code rejected", put32(b, 256, 0x20010));
    BAD("unmapped TOC rejected", put32(b, 260, 0x50000));
    BAD("TLS file larger than memory rejected", put64(b, 176 + 32, 17));
    BAD("TLS file outside input rejected", put64(b, 176 + 8, UINT64_MAX - 2));
    BAD("no load segments rejected", put32(b, 64, 0); put32(b, 120, 0));
    fixture(b); rejects("insufficient backed-memory budget rejected", b, sizeof(b), 319);
#undef BAD
    DsImage image = {0};
    unsigned char canary[4] = {0xa1, 0xb2, 0xc3, 0xd4};
    CHECK(!ds_image_load_file(&image, "/nonexistent/ds-loader-test", 1024,
                              (char *)&canary[1], 1), "missing file rejected");
    CHECK(canary[0] == 0xa1 && canary[1] == 0 && canary[2] == 0xc3 && canary[3] == 0xd4,
          "short error buffer stays bounded");
    CHECK(empty(&image), "missing-file path has no allocations");
    ds_image_destroy(&image);
}

static uint32_t random_state = UINT32_C(0xa971e583);
static uint32_t next_random(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static void mutate_headers(void)
{
    unsigned char bytes[288];
    for (unsigned iteration = 0; iteration < 1000; ++iteration) {
        fixture(bytes);
        unsigned changes = next_random() % 4 + 1;
        for (unsigned i = 0; i < changes; ++i)
            bytes[next_random() % sizeof(bytes)] ^= (unsigned char)(1u << (next_random() % 8));
        write_fixture(bytes, sizeof(bytes));
        DsImage image = {0};
        char error[64];
        bool loaded = ds_image_load_file(&image, fixture_path, 1024 * 1024, error, sizeof(error));
        if (loaded) {
            CHECK(image.segment_count <= DS_MAX_SEGMENTS && image.allocated_bytes <= 1024 * 1024,
                  "mutated accepted header stays bounded");
            CHECK(ds_image_address(&image, image.entry_descriptor, 8, DS_PF_R) != NULL &&
                  ds_image_address(&image, image.entry_code, 4, DS_PF_X) != NULL,
                  "mutated accepted image has valid entry mappings");
        } else CHECK(empty(&image), "mutated rejected header frees allocations");
        ds_image_destroy(&image);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "Usage: %s verified-EBOOT.ELF\n", argv[0]); return 2; }
    char directory[] = "/tmp/ds-image-tests-XXXXXX";
    if (!mkdtemp(directory)) { perror("mkdtemp"); return 2; }
    snprintf(fixture_path, sizeof(fixture_path), "%s/input.elf", directory);
    actual_image(argv[1]);
    valid_and_regions();
    malformed();
    mutate_headers();
    unlink(fixture_path);
    rmdir(directory);
    printf("Image loader tests: %u checks, %u failures (including 1000 deterministic header mutations)\n", tests, failed);
    return failed ? 1 : 0;
}
