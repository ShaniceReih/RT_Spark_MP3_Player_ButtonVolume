#include "FreeRTOS.h"
#include "task.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" int __wrap_snprintf(char *destination, size_t capacity,
                                const char *format, ...);

namespace {
unsigned checks = 0;
unsigned groups = 0;
unsigned suspendCalls = 0;
unsigned resumeCalls = 0;
unsigned suspensionDepth = 0;
bool schedulerStarted = false;
BaseType_t resumeResult = pdFALSE;

void check(bool condition, const char *message)
{
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
#define CHECK(condition) check((condition), #condition)

void reset(bool started, unsigned existingDepth = 0)
{
    suspendCalls = 0;
    resumeCalls = 0;
    suspensionDepth = existingDepth;
    schedulerStarted = started;
    resumeResult = pdFALSE;
    StateTestRtos::interruptNumber = 0;
}

void checkBalanced(unsigned initialDepth)
{
    CHECK(suspendCalls == 1U);
    CHECK(resumeCalls == 1U);
    CHECK(suspensionDepth == initialDepth);
}

template<typename... Arguments>
void compare(const char *format, Arguments... arguments)
{
    char expected[256] = {};
    char actual[256] = {};
    const int expectedLength = std::snprintf(expected, sizeof(expected), format,
                                            arguments...);
    const unsigned beforeSuspend = suspendCalls;
    const unsigned beforeResume = resumeCalls;
    const int actualLength = __wrap_snprintf(actual, sizeof(actual), format,
                                             arguments...);
    CHECK(actualLength == expectedLength);
    CHECK(std::strcmp(actual, expected) == 0);
    CHECK(suspendCalls == beforeSuspend + 1U);
    CHECK(resumeCalls == beforeResume + 1U);
    CHECK(suspensionDepth == 0U);
}
} // namespace

namespace StateTestRtos {
uint32_t interruptNumber = 0;
void AssertFailed(const char *condition) { throw std::runtime_error(condition); }
}

BaseType_t xTaskGetSchedulerState()
{
    if (!schedulerStarted) return taskSCHEDULER_NOT_STARTED;
    return suspensionDepth == 0U ? taskSCHEDULER_RUNNING : taskSCHEDULER_SUSPENDED;
}

void vTaskSuspendAll()
{
    CHECK(schedulerStarted);
    CHECK(StateTestRtos::interruptNumber == 0U);
    ++suspendCalls;
    ++suspensionDepth;
}

BaseType_t xTaskResumeAll()
{
    CHECK(schedulerStarted);
    CHECK(StateTestRtos::interruptNumber == 0U);
    CHECK(suspensionDepth > 0U);
    ++resumeCalls;
    --suspensionDepth;
    return resumeResult;
}

int main()
{
    // Bootstrap formatting must not ask a scheduler that has not started to
    // suspend or resume. It still retains normal snprintf semantics.
    reset(false);
    char output[64] = {};
    CHECK(__wrap_snprintf(output, sizeof(output), "SystemCoreClock: %lu Hz",
                          16000000UL) == 28);
    CHECK(std::strcmp(output, "SystemCoreClock: 16000000 Hz") == 0);
    CHECK(suspendCalls == 0U && resumeCalls == 0U && suspensionDepth == 0U);
    ++groups;

    reset(true);
    CHECK(__wrap_snprintf(output, sizeof(output), "VOL %u PCT", 73U) == 10);
    CHECK(std::strcmp(output, "VOL 73 PCT") == 0);
    checkBalanced(0U);
    ++groups;

    // Existing scheduler suspension belongs to the caller. The guard adds and
    // removes just its own nesting level; it must not resume that caller.
    reset(true, 2U);
    CHECK(__wrap_snprintf(output, sizeof(output), "%s - SONG %u", "PAUSED", 7U)
          == 15);
    CHECK(std::strcmp(output, "PAUSED - SONG 7") == 0);
    checkBalanced(2U);
    CHECK(xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED);
    ++groups;

    reset(true);
    resumeResult = pdTRUE; // The kernel may choose to yield while resuming.
    CHECK(__wrap_snprintf(output, sizeof(output), "%u SEC", 5U) == 5);
    CHECK(std::strcmp(output, "5 SEC") == 0);
    checkBalanced(0U);
    ++groups;

    reset(true);
    char truncated[5] = {'X', 'X', 'X', 'X', 'X'};
    CHECK(__wrap_snprintf(truncated, sizeof(truncated), "SONG %u", 8U) == 6);
    CHECK(std::strcmp(truncated, "SONG") == 0);
    checkBalanced(0U);
    ++groups;

    reset(true);
    char one[2] = {'X', 'Y'};
    CHECK(__wrap_snprintf(one, 1U, "%s", "FUR ELISE") == 9);
    CHECK(one[0] == '\0' && one[1] == 'Y');
    checkBalanced(0U);
    ++groups;

    reset(true);
    CHECK(__wrap_snprintf(nullptr, 0U, "%s %u", "VOL", 100U) == 7);
    checkBalanced(0U);
    ++groups;

    // Exercise the integer/string/precision formats used by the unchanged UI
    // and UART, comparing the wrapper with this native C library's formatter.
    reset(true);
    compare("%s %s", "NOCTURNE IN E FLAT", "CHOPIN");
    compare("%.36s", "0123456789012345678901234567890123456789");
    compare("%.*s %s\r\n", 3, "LONG PREFIX", "FUR ELISE");
    compare("Binary: %u%u%u\r\n", 1U, 0U, 1U);
    compare("SONG %u - %u%u%u", 8U, 1U, 1U, 1U);
    compare("VOL %u PCT", 0U);
    compare("VOL %u PCT", 100U);
    compare("%s - SONG %u", "PLAYING", 4U);
    compare("%u SEC", 1U);
    compare("HEAP=%lu MIN_HEAP=%lu", 18480UL, 17904UL);
    compare("UI_STATE_REV=%lu UI_VOL_REV=%lu", 4294967295UL, 4294967294UL);
    CHECK(suspendCalls == 11U && resumeCalls == 11U);
    ++groups;

    reset(true);
    StateTestRtos::interruptNumber = 16U;
    bool assertionSeen = false;
    try { __wrap_snprintf(output, sizeof(output), "%u", 1U); }
    catch (const std::runtime_error &) { assertionSeen = true; }
    CHECK(assertionSeen);
    CHECK(suspendCalls == 0U && resumeCalls == 0U && suspensionDepth == 0U);
    ++groups;

    std::printf("Stage-5 format guard host tests: %u groups, %u checks passed.\n",
                groups, checks);
    return 0;
}
