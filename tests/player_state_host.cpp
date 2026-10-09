#include "player_state.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "song_selection.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

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

int ownerToken = 0;
int readerToken = 0;
TaskHandle_t currentTask = &ownerToken;
BaseType_t scheduler = taskSCHEDULER_NOT_STARTED;
unsigned created = 0;
unsigned taken = 0;
unsigned given = 0;
void (*beforeTake)() = nullptr;
void (*afterGive)() = nullptr;
PlayerStateSnapshot observedBefore;
PlayerStateSnapshot observedAfter;

const PlayerEventStamp &event(const PlayerStateSnapshot &snapshot,
                             PlayerEventKind kind)
{
    return snapshot.events[static_cast<uint8_t>(kind)];
}

template<typename Call> void expectAssertion(Call call)
{
    const unsigned takesBefore = taken;
    bool caught = false;
    try { call(); }
    catch (const std::runtime_error &) { caught = true; }
    CHECK(caught);
    CHECK(taken == takesBefore);
}

PlayerStateSnapshot getAsReader()
{
    TaskHandle_t previous = currentTask;
    currentTask = &readerToken;
    PlayerStateSnapshot result;
    CHECK(PlayerState_GetSnapshot(result));
    currentTask = previous;
    return result;
}

void readAfter() { observedAfter = getAsReader(); }
void readBefore()
{
    observedBefore = getAsReader();
    // Arm only after the pre-publication reader releases its own read lock.
    afterGive = readAfter;
}
} // namespace

struct StateTestMutex { bool held = false; };
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
    CHECK(created == 0);
    ++created;
    return &testMutex;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t wait)
{
    CHECK(mutex == &testMutex);
    CHECK(wait == portMAX_DELAY);
    CHECK(!mutex->held);
    if (beforeTake != nullptr) {
        void (*probe)() = beforeTake;
        beforeTake = nullptr;
        probe();
        CHECK(!mutex->held);
    }
    mutex->held = true;
    ++taken;
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
    CHECK(mutex == &testMutex);
    CHECK(mutex->held);
    mutex->held = false;
    ++given;
    if (afterGive != nullptr) {
        void (*probe)() = afterGive;
        afterGive = nullptr;
        probe();
    }
    return pdTRUE;
}

int main()
{
    PlayerState_Init(37, true);
    CHECK(created == 1);
    CHECK(taken == 0 && given == 0);
    scheduler = taskSCHEDULER_RUNNING;
    PlayerState_BindPublisher();

    PlayerStateSnapshot initial = getAsReader();
    CHECK(initial.revision == 0);
    CHECK(initial.acceptedVolume == 37 && initial.acceptedVolumeValid);
    CHECK(initial.mode == PlayerMode::Idle);
    CHECK(initial.background == PlayerMode::Idle);
    CHECK(initial.phase == PlayerSelectionPhase::None);
    CHECK(initial.playback == PlayerPlayback::Idle);
    CHECK(initial.lastCompletion == PlayerPlayback::Idle);
    CHECK(initial.eventSequence == 0);
    CHECK(!initial.hasCurrentSong && !initial.timeoutVisible);
    ++groups;

    // A slow reader must see all distinct events even after later publications.
    PlayerStateSnapshot local = initial;
    local.mode = PlayerMode::Selecting;
    local.background = PlayerMode::Playing;
    local.pendingSong = 6;
    local.capturedBits = 6;
    local.previewBits = 1;
    local.phase = PlayerSelectionPhase::Confirm;
    local.confirmationStart = 200;
    local.confirmationDeadline = 5200;
    PlayerState_RecordEvent(local, PlayerEventKind::Captured, 100);
    PlayerState_RecordEvent(local, PlayerEventKind::PresentationCancelled, 100);
    PlayerState_RecordEvent(local, PlayerEventKind::ConfirmationReady, 200);
    PlayerState_Publish(local);
    local.observedAt = 400;
    PlayerState_Publish(local);
    local.observedAt = 600;
    PlayerState_Publish(local);
    PlayerStateSnapshot delayed = getAsReader();
    CHECK(delayed.revision == 3);
    CHECK(delayed.pendingSong == 6 && delayed.capturedBits == 6);
    CHECK(delayed.previewBits == 1);
    CHECK(event(delayed, PlayerEventKind::Captured).timestamp == 100);
    CHECK(event(delayed, PlayerEventKind::PresentationCancelled).timestamp == 100);
    CHECK(event(delayed, PlayerEventKind::ConfirmationReady).timestamp == 200);
    CHECK(delayed.confirmationStart == 200 && delayed.confirmationDeadline == 5200);
    CHECK(PlayerState_EventAfter(
        event(delayed, PlayerEventKind::ConfirmationReady).revision,
        event(delayed, PlayerEventKind::PresentationCancelled).revision));
    ++groups;

    // Latest volume/cancel order is retained without a lossy Boolean handshake.
    PlayerState_RecordEvent(local, PlayerEventKind::VolumeChanged, 650, true);
    const uint32_t volumeBefore = event(local, PlayerEventKind::VolumeChanged).revision;
    PlayerState_RecordEvent(local, PlayerEventKind::PresentationCancelled, 700);
    CHECK(PlayerState_EventAfter(
        event(local, PlayerEventKind::PresentationCancelled).revision, volumeBefore));
    PlayerState_RecordEvent(local, PlayerEventKind::VolumeChanged, 750, false);
    local.acceptedVolume = 58;
    PlayerState_Publish(local);
    delayed = getAsReader();
    CHECK(delayed.acceptedVolume == 58);
    CHECK(event(delayed, PlayerEventKind::VolumeChanged).timestamp == 750);
    CHECK(!event(delayed, PlayerEventKind::VolumeChanged).overlayEligible);
    CHECK(PlayerState_EventAfter(
        event(delayed, PlayerEventKind::VolumeChanged).revision,
        event(delayed, PlayerEventKind::PresentationCancelled).revision));
    CHECK(event(delayed, PlayerEventKind::Captured).timestamp == 100);
    PlayerState_RecordEvent(local, PlayerEventKind::Paused, 800, true);
    CHECK(!event(local, PlayerEventKind::Paused).overlayEligible);
    const uint32_t sequenceBeforeInvalid = local.eventSequence;
    PlayerState_RecordEvent(local, PlayerEventKind::Count, 999);
    CHECK(local.eventSequence == sequenceBeforeInvalid);
    ++groups;

    // Copies are independent both before and after publishing.
    delayed.currentSong = 7;
    delayed.acceptedVolume = 99;
    delayed.events[static_cast<uint8_t>(PlayerEventKind::Captured)].timestamp = 999;
    PlayerStateSnapshot unchanged = getAsReader();
    CHECK(unchanged.currentSong == 0 && unchanged.acceptedVolume == 58);
    CHECK(event(unchanged, PlayerEventKind::Captured).timestamp == 100);
    local.currentSong = 2;
    local.hasCurrentSong = true;
    local.revision = 0xFFFFFFFFU; // Caller cannot replace the store's revision.
    PlayerState_Publish(local);
    local.currentSong = 4;
    unchanged = getAsReader();
    CHECK(unchanged.currentSong == 2 && unchanged.hasCurrentSong);
    CHECK(unchanged.revision == 5);
    ++groups;

    // Reader probes either side of the actual lock observe complete copies.
    const PlayerStateSnapshot prior = unchanged;
    local.mode = PlayerMode::Paused;
    local.background = PlayerMode::Paused;
    local.currentSong = 7;
    local.acceptedVolume = 12;
    local.playback = PlayerPlayback::Paused;
    local.observedAt = 900;
    beforeTake = readBefore;
    PlayerState_Publish(local);
    CHECK(observedBefore.revision == prior.revision);
    CHECK(observedBefore.currentSong == prior.currentSong);
    CHECK(observedBefore.acceptedVolume == prior.acceptedVolume);
    CHECK(observedAfter.revision == prior.revision + 1);
    CHECK(observedAfter.mode == PlayerMode::Paused);
    CHECK(observedAfter.background == PlayerMode::Paused);
    CHECK(observedAfter.currentSong == 7 && observedAfter.acceptedVolume == 12);
    CHECK(observedAfter.playback == PlayerPlayback::Paused);
    CHECK(observedAfter.observedAt == 900);
    CHECK(!testMutex.held && taken == given);
    ++groups;

    // Reject accidental second-writer and ISR access before touching the mutex.
    currentTask = &readerToken;
    expectAssertion([&] { PlayerState_Publish(local); });
    expectAssertion([] { PlayerState_BindPublisher(); });
    currentTask = &ownerToken;
    StateTestRtos::interruptNumber = 21;
    expectAssertion([&] { PlayerState_Publish(local); });
    expectAssertion([&] { PlayerState_GetSnapshot(delayed); });
    StateTestRtos::interruptNumber = 0;
    scheduler = taskSCHEDULER_SUSPENDED;
    expectAssertion([&] { PlayerState_GetSnapshot(delayed); });
    scheduler = taskSCHEDULER_RUNNING;
    ++groups;

    // Real short-UP input can emit capture + ready together at one timestamp.
    SongSelectionInput input;
    SelectionButtons buttons;
    buttons.up = true;
    input.update(1000, buttons, true);
    input.update(1025, buttons, true);
    buttons.up = false;
    input.update(1100, buttons, true);
    const SelectionEvents shortUp = input.update(1125, buttons, true);
    CHECK(shortUp.captured && shortUp.readyToConfirm);
    PlayerState_RecordEvent(local, PlayerEventKind::Captured, 1125);
    PlayerState_RecordEvent(local, PlayerEventKind::PresentationCancelled, 1125);
    PlayerState_RecordEvent(local, PlayerEventKind::ConfirmationReady, 1125);
    local.mode = PlayerMode::Selecting;
    local.phase = PlayerSelectionPhase::Confirm;
    local.pendingSong = input.pendingSong();
    local.capturedBits = local.pendingSong;
    local.confirmationStart = 1125;
    local.confirmationDeadline = 6125;
    PlayerState_Publish(local);
    delayed = getAsReader();
    CHECK(delayed.pendingSong == 0 && delayed.capturedBits == 0);
    CHECK(event(delayed, PlayerEventKind::Captured).timestamp == 1125);
    CHECK(event(delayed, PlayerEventKind::ConfirmationReady).timestamp == 1125);
    CHECK(PlayerState_EventAfter(
        event(delayed, PlayerEventKind::ConfirmationReady).revision,
        event(delayed, PlayerEventKind::Captured).revision));
    CHECK(!input.update(6124, buttons, true).timedOut);
    CHECK(input.update(6125, buttons, true).timedOut);
    ++groups;

    // Completion while selecting cannot erase the candidate or revive playback.
    local.pendingSong = 7;
    local.capturedBits = 7;
    local.mode = PlayerMode::Selecting;
    local.background = PlayerMode::Idle;
    local.playback = PlayerPlayback::Idle;
    local.lastCompletion = PlayerPlayback::Finished;
    PlayerState_RecordEvent(local, PlayerEventKind::Completed, 6200);
    PlayerState_Publish(local);
    delayed = getAsReader();
    CHECK(delayed.mode == PlayerMode::Selecting);
    CHECK(delayed.background == PlayerMode::Idle);
    CHECK(delayed.pendingSong == 7 && delayed.capturedBits == 7);
    CHECK(delayed.lastCompletion == PlayerPlayback::Finished);
    local.mode = local.background;
    local.phase = PlayerSelectionPhase::None;
    local.timeoutVisible = true;
    local.timeoutStart = 6300;
    PlayerState_RecordEvent(local, PlayerEventKind::Timeout, 6300);
    PlayerState_Publish(local);
    delayed = getAsReader();
    CHECK(delayed.mode == PlayerMode::Idle && delayed.playback == PlayerPlayback::Idle);
    CHECK(delayed.timeoutVisible && delayed.timeoutStart == 6300);
    CHECK(delayed.lastCompletion == PlayerPlayback::Finished);
    CHECK(event(delayed, PlayerEventKind::Completed).timestamp == 6200);
    CHECK(event(delayed, PlayerEventKind::Timeout).timestamp == 6300);
    ++groups;

    // Sequence and confirmation timestamps retain unsigned-wrap semantics.
    local.eventSequence = 0xFFFFFFFEU;
    PlayerState_RecordEvent(local, PlayerEventKind::Captured, 0xFFFFFF00U);
    PlayerState_RecordEvent(local, PlayerEventKind::ConfirmationReady, 0xFFFFFFF0U);
    CHECK(event(local, PlayerEventKind::Captured).revision == 0xFFFFFFFFU);
    CHECK(event(local, PlayerEventKind::ConfirmationReady).revision == 1);
    CHECK(PlayerState_EventAfter(1, 0xFFFFFFFFU));
    CHECK(!PlayerState_EventAfter(0xFFFFFFFFU, 1));
    CHECK(PlayerState_EventAfter(1, 0));
    CHECK(!PlayerState_EventAfter(0, 1));
    CHECK(!PlayerState_EventAfter(1, 1));
    local.confirmationStart = 0xFFFFFFF0U;
    local.confirmationDeadline = local.confirmationStart + 5000U;
    PlayerState_Publish(local);
    delayed = getAsReader();
    CHECK(delayed.confirmationDeadline == 4984);
    CHECK(delayed.confirmationDeadline - delayed.confirmationStart == 5000U);
    CHECK(!testMutex.held && taken == given && created == 1);
    ++groups;

    std::printf("Player-state host tests: %u groups, %u checks passed.\n", groups, checks);
    return 0;
}
