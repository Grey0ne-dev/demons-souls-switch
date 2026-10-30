#include "ds_atomic.h"

#include <inttypes.h>
#include <stdio.h>

static unsigned checks, failures;
#define CHECK(condition, description) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s (%s:%d)\n", description, __FILE__, __LINE__); \
        ++failures; \
    } \
} while (0)

static bool make_region(DsImage *image, uint32_t base, size_t size, uint32_t flags)
{
    image->memory_limit = 0x10000;
    char error[128] = {0};
    bool ok = ds_image_add_zero_region(image, base, size, flags, error, sizeof(error));
    CHECK(ok, "backed anonymous test region created");
    if (!ok) fprintf(stderr, "Region error: %s\n", error);
    return ok;
}

static void destroy(DsImage *image)
{
    ds_image_destroy(image);
    CHECK(image->segment_count == 0 && image->allocated_bytes == 0,
          "atomic test releases every owned allocation");
}

static void values_and_outcomes(void)
{
    DsImage image = {0};
    if (!make_region(&image, 0x20000, 0x1000, DS_PF_R | DS_PF_W)) return;
    bool exchanged = false;
    uint32_t word = 0;
    CHECK(ds_image_write_be32(&image, 0x20000, 0x12345678), "seed non-symmetric guest byte pattern");
    CHECK(ds_image_compare_exchange_be32(&image, 0x20000, 0x12345678, 0x89ABCDEF, &exchanged) && exchanged,
          "matching expected value performs a real CAS");
    CHECK(ds_image_read_be32(&image, 0x20000, &word) && word == 0x89ABCDEF,
          "successful CAS preserves big-endian guest word meaning");
    const unsigned char *bytes = ds_image_address(&image, 0x20000, 4, DS_PF_R);
    CHECK(bytes && bytes[0] == 0x89 && bytes[1] == 0xAB && bytes[2] == 0xCD && bytes[3] == 0xEF,
          "actual backing contains exact desired big-endian bytes");
    exchanged = true;
    CHECK(ds_image_compare_exchange_be32(&image, 0x20000, 0x12345678, 0, &exchanged) && !exchanged,
          "a valid CAS reports failure after the observed value has changed");
    CHECK(ds_image_read_be32(&image, 0x20000, &word) && word == 0x89ABCDEF,
          "changed-value CAS failure writes nothing");
    CHECK(ds_image_compare_exchange_be32(&image, 0x20000, 0x89ABCDEF, UINT32_MAX, &exchanged) && exchanged &&
          ds_image_read_be32(&image, 0x20000, &word) && word == UINT32_MAX,
          "high-bit/all-ones desired values are not sign-extended or byte-swapped incorrectly");
    CHECK(ds_image_compare_exchange_be32(&image, 0x20000, UINT32_MAX, 0, &exchanged) && exchanged &&
          ds_image_read_be32(&image, 0x20000, &word) && word == 0,
          "zero desired value is a normal successful store");
    CHECK(ds_image_compare_exchange_be32(&image, 0x20000, 0, 0, &exchanged) && exchanged,
          "same-value CAS succeeds when comparison matches");
    CHECK(ds_image_write_be32(&image, 0x20FFC, 7) &&
          ds_image_compare_exchange_be32(&image, 0x20FFC, 7, 8, &exchanged) && exchanged &&
          ds_image_read_be32(&image, 0x20FFC, &word) && word == 8,
          "last complete aligned word of an arena is accessible");
    destroy(&image);
}

static void invalid_ranges_and_permissions(void)
{
    DsImage image = {0};
    if (!make_region(&image, 0x30000, 0x1002, DS_PF_R | DS_PF_W)) return;
    CHECK(ds_image_write_be32(&image, 0x30000, 0x10203040), "seed permission/range fixture");
    bool exchanged = true;
    CHECK(!ds_image_compare_exchange_be32(NULL, 0x30000, 0x10203040, 1, &exchanged) && exchanged,
          "null image rejects without changing outcome output");
    CHECK(!ds_image_compare_exchange_be32(&image, 0x30000, 0x10203040, 1, NULL),
          "missing outcome output rejects before any write");
    CHECK(!ds_image_compare_exchange_be32(&image, 0x30001, 0, 1, &exchanged) && exchanged,
          "guest misalignment rejects without changing outcome output");
    CHECK(!ds_image_compare_exchange_be32(&image, UINT64_C(0x100030000), 0x10203040, 1, &exchanged) && exchanged,
          "64-bit out-of-guest-range addresses never truncate into a valid arena");
    CHECK(!ds_image_compare_exchange_be32(&image, 0x40000, 0, 1, &exchanged) && exchanged,
          "unmapped guest address rejects without fabrication");
    unsigned char *tail = ds_image_address(&image, 0x31000, 2, DS_PF_W);
    if (tail) { tail[0] = 0xA5; tail[1] = 0x5A; }
    CHECK(!ds_image_compare_exchange_be32(&image, 0x31000, 0xA55A0000, 1, &exchanged) && exchanged &&
          tail && tail[0] == 0xA5 && tail[1] == 0x5A,
          "aligned CAS crossing arena boundary preserves the backed prefix");
    image.segments[0].flags = DS_PF_R;
    CHECK(!ds_image_compare_exchange_be32(&image, 0x30000, 0x10203040, 1, &exchanged) && exchanged,
          "read-only memory cannot be changed by atomic helper");
    image.segments[0].flags = DS_PF_W;
    CHECK(!ds_image_compare_exchange_be32(&image, 0x30000, 0x10203040, 1, &exchanged) && exchanged,
          "write-only memory cannot supply atomic comparison input");
    image.segments[0].flags = DS_PF_R | DS_PF_W;
    uint32_t word = 0;
    CHECK(ds_image_read_be32(&image, 0x30000, &word) && word == 0x10203040,
          "all invalid operations preserve original guest data");
    destroy(&image);
}

static void host_alignment(void)
{
    DsImage image = {0};
    if (!make_region(&image, 0x50001, 8, DS_PF_R | DS_PF_W)) return;
    CHECK(ds_image_write_be32(&image, 0x50004, 0x11223344), "seed naturally guest-aligned but host-offset fixture");
    void *host = ds_image_address(&image, 0x50004, 4, DS_PF_R | DS_PF_W);
    CHECK(host && (uintptr_t)host % _Alignof(uint32_t) != 0,
          "unaligned arena base creates deliberately misaligned host word");
    bool exchanged = false;
    CHECK(!ds_image_compare_exchange_be32(&image, 0x50004, 0x11223344, 0, &exchanged) && !exchanged,
          "host misalignment rejects without invoking an undefined native atomic");
    uint32_t word = 0;
    CHECK(ds_image_read_be32(&image, 0x50004, &word) && word == 0x11223344,
          "host alignment rejection preserves actual guest bytes");
    destroy(&image);
}

int main(void)
{
    values_and_outcomes();
    invalid_ranges_and_permissions();
    host_alignment();
    printf("Atomic backend tests: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
