#include "ds_rwlock.h"

#include <cstdio>

static unsigned checks, failures;
#define CHECK(condition, description) do { \
    ++checks; \
    if (!(condition)) { \
        std::fprintf(stderr, "FAIL %s (%s:%d)\n", description, __FILE__, __LINE__); \
        ++failures; \
    } \
} while (0)

int main()
{
    CHECK(ds_rwlock_reset() && ds_rwlock_live() == 0 && ds_rwlock_held() == 0,
          "registry starts empty");
    CHECK(!ds_rwlock_write_acquire(0, 1) && !ds_rwlock_write_acquire(9, 1) &&
          !ds_rwlock_write_release(0, 1), "invalid handles cannot acquire or release");
    uint32_t first = 0, second = 0;
    CHECK(ds_rwlock_create(&first) && ds_rwlock_create(&second) && first != second &&
          ds_rwlock_live() == 2, "creation provides distinct live native objects");
    CHECK(!ds_rwlock_write_acquire(first, 0) && !ds_rwlock_write_release(first, 0) &&
          ds_rwlock_held() == 0, "zero guest owner cannot claim or release an object");
    CHECK(ds_rwlock_write_acquire(first, 1) && ds_rwlock_held() == 1,
          "uncontended write acquisition takes real native lock");
    CHECK(!ds_rwlock_write_acquire(first, 1) && !ds_rwlock_write_acquire(first, 2) &&
          ds_rwlock_held() == 1, "duplicate and contended attempts preserve original writer");
    CHECK(!ds_rwlock_write_release(first, 2) && ds_rwlock_held() == 1,
          "wrong guest owner cannot release writer");
    CHECK(ds_rwlock_write_acquire(second, 1) && ds_rwlock_held() == 2,
          "independent object may be acquired by the same active guest thread");
    CHECK(ds_rwlock_write_release(first, 1) && ds_rwlock_held() == 1,
          "correct owner releases one native writer without affecting another");
    CHECK(!ds_rwlock_write_release(first, 1) && ds_rwlock_held() == 1,
          "unheld native writer cannot be released twice");
    CHECK(ds_rwlock_write_acquire(first, 2) && ds_rwlock_held() == 2,
          "released object can be acquired with a new guest owner");
    CHECK(ds_rwlock_reset() && ds_rwlock_live() == 0 && ds_rwlock_held() == 0,
          "teardown releases held writers before destroying native objects");
    CHECK(!ds_rwlock_contains(first) && !ds_rwlock_contains(second) &&
          !ds_rwlock_write_acquire(first, 1) && !ds_rwlock_write_release(second, 1),
          "stale handles are invalid after teardown");
    CHECK(ds_rwlock_reset(), "empty teardown is repeatable");
    uint32_t recreated = 0;
    CHECK(ds_rwlock_create(&recreated) && ds_rwlock_write_acquire(recreated, 1) &&
          ds_rwlock_write_release(recreated, 1) && ds_rwlock_reset(),
          "registry can initialize and clean a subsequent process slice");
    std::printf("Native rwlock write tests: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
