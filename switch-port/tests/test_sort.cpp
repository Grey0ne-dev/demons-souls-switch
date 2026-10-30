// The runner injects a seed callback at the actual tenth-constructor sort call.
// This host-only fixture never enters the production generator or Switch build.
#include "ds_entry.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

extern "C" uint32_t vm_read32(uint64_t address);
extern "C" void vm_write32(uint64_t address, uint32_t value);
extern "C" {
uint32_t ds_test_sort_depth;
}

static unsigned checks, failures, seed_calls;
static uint32_t sort_count, sort_pattern, guard_before, guard_after;
static std::vector<std::pair<int32_t, uint32_t>> input;

#define CHECK(condition, description) do { \
    ++checks; \
    if (!(condition)) { \
        std::fprintf(stderr, "FAIL count=%u pattern=%u depth=%u: %s\n", \
                     sort_count, sort_pattern, ds_test_sort_depth, description); \
        ++failures; \
    } \
} while (0)

extern "C" uint32_t ds_test_sort_seed(uint32_t begin)
{
    ++seed_calls;
    CHECK(begin == 0x1AD8DA0 && sort_count <= 512, "fixture uses only the actual 4096-byte registry allocation");
    if (begin != 0x1AD8DA0 || sort_count > 512) return begin;
    guard_before = vm_read32(begin - 4);
    guard_after = vm_read32(begin + sort_count * 8);
    input.clear();
    input.reserve(sort_count);
    uint32_t random = 0xC001D00Du ^ sort_count;
    static constexpr uint32_t extremes[] = {0x80000000u,0x7FFFFFFFu,0xFFFFFFFFu,0,1,0x80000000u,0xFFFFFFFFu,0x7FFFFFFFu};
    for (uint32_t i = 0; i < sort_count; ++i) {
        uint32_t key = 0;
        switch (sort_pattern) {
        case 0: key = static_cast<uint32_t>(static_cast<int32_t>(i) - static_cast<int32_t>(sort_count / 2)); break;
        case 1: key = static_cast<uint32_t>(static_cast<int32_t>(sort_count / 2) - static_cast<int32_t>(i)); break;
        case 2: key = 0x80000000u; break;
        case 3: key = extremes[i % 8]; break;
        case 4:
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            key = i == 0 ? 0x80000000u : i == 1 ? 0x7FFFFFFFu : random;
            break;
        case 5: key = i <= sort_count / 2 ? i : sort_count - i; break;
        }
        const uint32_t payload = 0xA5000000u + i;
        input.emplace_back(static_cast<int32_t>(key), payload);
        vm_write32(begin + i * 8, key);
        vm_write32(begin + i * 8 + 4, payload);
    }
    return begin + sort_count * 8;
}

int main(int argc, char **argv)
{
    if (argc != 3 || std::strcmp(argv[2], "--explore-sort-matrix") != 0) return 2;
    static constexpr uint32_t sizes[] = {0,1,2,31,32,33,36,40,41,42,64,128,512};
    unsigned cases = 0;
    for (uint32_t count : sizes) for (uint32_t pattern = 0; pattern < 6; ++pattern)
        for (unsigned fallback = 0; fallback < (count > 32 ? 2u : 1u); ++fallback) {
            sort_count = count; sort_pattern = pattern; ds_test_sort_depth = fallback ? 0 : count;
            seed_calls = 0;
            DsImage image = {}; char error[256] = {}; DsEntryResult result = {};
            const bool loaded = ds_image_load_file(&image,argv[1],48u*1024u*1024u,error,sizeof(error));
            CHECK(loaded, "verified game image loads for the real startup path");
            if (!loaded) return 1;
            const bool ok = ds_entry_run_exploration(&image,&result,error,sizeof(error),10);
            CHECK(!ok && result.stop == DS_ENTRY_EXECUTION_LIMIT && result.constructors_completed == 10 &&
                  result.callee == 0x5FD720 && !result.memory_fault,
                  "original sorter returns through constructor ten to its real boundary");
            CHECK(seed_calls == 1 && input.size() == count, "exactly one tenth-constructor sort is seeded");
            std::vector<std::pair<int32_t,uint32_t>> actual;
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t key = 0, payload = 0;
                if (!ds_image_read_be32(&image,0x1AD8DA0+i*8,&key) ||
                    !ds_image_read_be32(&image,0x1AD8DA4+i*8,&payload)) return 1;
                actual.emplace_back(static_cast<int32_t>(key),payload);
            }
            CHECK(std::is_sorted(actual.begin(),actual.end(),[](const auto &a,const auto &b) { return a.first < b.first; }),
                  "guest output is ordered by signed 32-bit keys");
            auto expected = input;
            std::sort(expected.begin(),expected.end());
            std::sort(actual.begin(),actual.end());
            CHECK(actual == expected, "host oracle confirms every original key/payload pair is retained");
            uint32_t before = 0, after = 0;
            CHECK(ds_image_read_be32(&image,0x1AD8D9C,&before) && before == guard_before &&
                  ds_image_read_be32(&image,0x1AD8DA0+count*8,&after) && after == guard_after,
                  "sort leaves both adjacent guard words unchanged");
            const unsigned partitions = std::count(result.functions,
                result.functions + std::min(result.function_count,static_cast<uint32_t>(DS_ENTRY_FUNCTION_CAPACITY)),0xADF198u);
            CHECK((partitions != 0) == (count > 32 && !fallback),
                  "partition and depth-exhausted fallback paths follow the original threshold");
            CHECK(!result.native_live_after_cleanup && !result.native_rwlocks_after_cleanup &&
                  !result.native_kernel_mutexes_after_cleanup && result.kernel_mutex_calls == 5 &&
                  result.allocation_calls == 14 && result.lock_calls == 16 && result.unlock_calls == 16,
                  "all cases retain the actual service totals and clean every native registry");
            ds_image_destroy(&image);
            CHECK(!image.segment_count && !image.allocated_bytes, "image and arenas are fully released");
            ++cases;
        }
    std::printf("Original sort matrix: %u cases, %u checks, %u failures\n",cases,checks,failures);
    return failures ? 1 : 0;
}
