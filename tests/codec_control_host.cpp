#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

// Use the real driver and lock wrapper. The fixtures record calls, rather than
// attempting to implement or test the native FreeRTOS mutex scheduler.
#include "../src/audio_codec.cpp"
#include "../src/codec_control.cpp"
#include "../include/song_def.h"

namespace {
unsigned checks = 0;
unsigned groups = 0;
void check(bool condition, const char *message)
{
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
#define CHECK(condition) check((condition), #condition)

BaseType_t scheduler = taskSCHEDULER_NOT_STARTED;
int buttonToken = 0;
int volumeToken = 0;
TaskHandle_t currentTask = &buttonToken;
unsigned creates = 0;
unsigned takes = 0;
unsigned gives = 0;
bool allocationFails = false;
bool gpioMustBeLocked = false;
bool injectDma = false;
unsigned injectedDma = 0;

struct CursorAtLock {
    unsigned next;
    unsigned frames;
};
std::vector<CursorAtLock> cursorTakes;
std::vector<CursorAtLock> cursorGives;

template<typename Call> void expectAssertion(Call call)
{
    const unsigned takesBefore = takes;
    const unsigned givesBefore = gives;
    bool caught = false;
    try { call(); }
    catch (const std::runtime_error &) { caught = true; }
    CHECK(caught);
    CHECK(takes == takesBefore);
    CHECK(gives == givesBefore);
}
} // namespace

struct StateTestMutex {
    bool held = false;
    TaskHandle_t owner = nullptr;
};
static StateTestMutex testMutex;

namespace StateTestRtos {
uint32_t interruptNumber = 0;
void AssertFailed(const char *condition) { throw std::runtime_error(condition); }
}

BaseType_t xTaskGetSchedulerState() { return scheduler; }
TaskHandle_t xTaskGetCurrentTaskHandle() { return currentTask; }

SemaphoreHandle_t xSemaphoreCreateMutex()
{
    CHECK(scheduler == taskSCHEDULER_NOT_STARTED);
    ++creates;
    return allocationFails ? nullptr : &testMutex;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t wait)
{
    CHECK(StateTestRtos::interruptNumber == 0 && !hostInIrq);
    CHECK(scheduler == taskSCHEDULER_RUNNING);
    CHECK(mutex == &testMutex && wait == portMAX_DELAY);
    CHECK(!mutex->held);
    mutex->held = true;
    mutex->owner = currentTask;
    ++takes;
    cursorTakes.push_back({nextNote, noteFramesRemaining});
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
    CHECK(StateTestRtos::interruptNumber == 0 && !hostInIrq);
    CHECK(mutex == &testMutex && mutex->held && mutex->owner == currentTask);
    ++gives;
    cursorGives.push_back({nextNote, noteFramesRemaining});
    mutex->held = false;
    mutex->owner = nullptr;
    return pdTRUE;
}

namespace {
void gpioProbe()
{
    if (gpioMustBeLocked) CHECK(testMutex.held);
    if (injectDma) {
        injectDma = false;
        const unsigned takesBefore = takes;
        const unsigned givesBefore = gives;
        const uint32_t framesBefore = noteFramesRemaining;
        hostInIrq = true;
        StateTestRtos::interruptNumber = 16;
        HAL_I2S_TxHalfCpltCallback(&hi2s3);
        StateTestRtos::interruptNumber = 0;
        hostInIrq = false;
        CHECK(takes == takesBefore && gives == givesBefore);
        CHECK(noteFramesRemaining < framesBefore);
        CHECK(testMutex.held);
        ++injectedDma;
    }
    hostOnGpioWrite = gpioProbe;
}

void watchControlWrites()
{
    gpioMustBeLocked = true;
    hostOnGpioWrite = gpioProbe;
}

void stopWatching()
{
    hostOnGpioWrite = nullptr;
    gpioMustBeLocked = false;
    CHECK(!testMutex.held && takes == gives);
}

void testBootstrapAndCreation()
{
    ++groups;
    CodecControl_Lock();
    CodecControl_Unlock();
    CHECK(Audio_SetMuted(true));
    CHECK(takes == 0 && gives == 0 && creates == 0);
    CHECK(AudioCodec_Init()); // Single-owner pre-scheduler bootstrap.
    CHECK(takes == 0 && gives == 0);

    StateTestRtos::interruptNumber = 16;
    expectAssertion([] { CodecControl_Init(); });
    StateTestRtos::interruptNumber = 0;
    CHECK(creates == 0);
    allocationFails = true;
    expectAssertion([] { CodecControl_Init(); });
    CHECK(creates == 1);
    allocationFails = false;
    CodecControl_Init();
    CHECK(creates == 2);
    expectAssertion([] { CodecControl_Init(); });
    CHECK(creates == 2);
    CodecControl_Lock();
    CodecControl_Unlock();
    CHECK(takes == 0 && gives == 0); // Created mutex still bypassed pre-start.
    scheduler = taskSCHEDULER_RUNNING;
}

void testContextRejection()
{
    ++groups;
    StateTestRtos::interruptNumber = 16;
    hostInIrq = true;
    expectAssertion([] { CodecControl_Lock(); });
    expectAssertion([] { CodecControl_Unlock(); });
    expectAssertion([] { Audio_SetMuted(true); });
    hostInIrq = false;
    StateTestRtos::interruptNumber = 0;
    scheduler = taskSCHEDULER_SUSPENDED;
    expectAssertion([] { CodecControl_Lock(); });
    expectAssertion([] { CodecControl_Unlock(); });
    scheduler = taskSCHEDULER_RUNNING;
}

void testMuteBalanceAndFailure()
{
    ++groups;
    watchControlWrites();
    unsigned before = takes;
    CHECK(Audio_SetMuted(false));
    CHECK(takes == before + 1 && gives == takes);
    hostAck = false;
    before = takes;
    CHECK(!Audio_SetMuted(true));
    CHECK(takes == before + 1 && gives == takes);
    hostAck = true;
    stopWatching();
}

void testPrefillOutsideMutex()
{
    ++groups;
    CHECK(Audio_GetPlaybackState() == AUDIO_STOPPED);
    const unsigned takesBefore = takes;
    const unsigned givesBefore = gives;
    const size_t takeIndex = cursorTakes.size();
    const size_t giveIndex = cursorGives.size();
    watchControlWrites();
    CHECK(Audio_StartSong(&FUR_ELISE));
    CHECK(takes == takesBefore + 2 && gives == givesBefore + 2);
    // Start holds separate mute/unmute transactions. Actual PCM cursor work
    // occurs between their release/acquisition boundaries, not inside a lock.
    CHECK(cursorTakes[takeIndex].next == cursorGives[giveIndex].next);
    CHECK(cursorTakes[takeIndex].frames == cursorGives[giveIndex].frames);
    CHECK(cursorTakes[takeIndex + 1].next > 0);
    CHECK(cursorTakes[takeIndex + 1].frames > 0);
    CHECK(cursorTakes[takeIndex + 1].frames != cursorGives[giveIndex].frames);
    CHECK(hostTransportRunning && Audio_GetPlaybackState() == AUDIO_PLAYING);
    stopWatching();
}

void testVolumePairAndDma()
{
    ++groups;
    currentTask = &volumeToken;
    const unsigned takesBefore = takes;
    const unsigned givesBefore = gives;
    const unsigned gpioBefore = hostGpioCalls;
    watchControlWrites();
    injectDma = true;
    CodecControl_Lock();
    AudioCodec_SetVolume(73);
    CHECK(testMutex.held && testMutex.owner == currentTask);
    CHECK(takes == takesBefore + 1 && gives == givesBefore);
    CodecControl_Unlock();
    CHECK(takes == takesBefore + 1 && gives == givesBefore + 1);
    CHECK(hostGpioCalls > gpioBefore && injectedDma == 1);
    CHECK(Audio_GetPlaybackState() == AUDIO_PLAYING);
    stopWatching();
    currentTask = &buttonToken;
}

void testTransportAndServiceCoverage()
{
    ++groups;
    watchControlWrites();
    unsigned before = takes;
    CHECK(Audio_PauseSong());
    CHECK(takes == before + 1 && gives == takes);
    before = takes;
    CHECK(Audio_ResumeSong());
    CHECK(takes == before + 1 && gives == takes);
    before = takes;
    Audio_StopSong();
    CHECK(takes == before + 1 && gives == takes);
    CHECK(Audio_GetPlaybackState() == AUDIO_STOPPED);

    CHECK(Audio_StartSong(&FUR_ELISE));
    before = takes;
    hostInIrq = true;
    StateTestRtos::interruptNumber = 16;
    HAL_I2S_ErrorCallback(&hi2s3);
    StateTestRtos::interruptNumber = 0;
    hostInIrq = false;
    CHECK(takes == before && Audio_GetPlaybackState() == AUDIO_ERROR);
    Audio_Service();
    CHECK(takes == before + 1 && gives == takes);
    CHECK(!hostTransportRunning);

    Audio_StopSong();
    float notes[] = {0.0f};
    float beats[] = {0.01f};
    Song shortRest("rest", "", notes, beats, 0.25f, 1);
    CHECK(Audio_StartSong(&shortRest));
    before = takes;
    hostInIrq = true;
    StateTestRtos::interruptNumber = 16;
    HAL_I2S_TxHalfCpltCallback(&hi2s3);
    HAL_I2S_TxCpltCallback(&hi2s3);
    StateTestRtos::interruptNumber = 0;
    hostInIrq = false;
    CHECK(takes == before && Audio_GetPlaybackState() == AUDIO_FINISHED);
    Audio_Service();
    CHECK(takes == before + 1 && gives == takes);
    CHECK(!hostTransportRunning);
    Audio_StopSong();
    stopWatching();
}
} // namespace

int main()
{
    testBootstrapAndCreation();
    testContextRejection();
    testMuteBalanceAndFailure();
    testPrefillOutsideMutex();
    testVolumePairAndDma();
    testTransportAndServiceCoverage();
    std::printf("Codec control host tests: %u groups, %u checks passed.\n", groups, checks);
}
