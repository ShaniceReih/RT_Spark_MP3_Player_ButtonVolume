#include "player_state.h"
#include "potentiometer_filter.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

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

int controlToken = 0;
int volumeToken = 0;
int readerToken = 0;
TaskHandle_t currentTask = &controlToken;
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
    const TaskHandle_t previous = currentTask;
    currentTask = &readerToken;
    PlayerStateSnapshot result;
    CHECK(PlayerState_GetSnapshot(result));
    currentTask = previous;
    return result;
}

void publishVolume(uint8_t volume, uint32_t now, bool valid = true)
{
    const TaskHandle_t previous = currentTask;
    currentTask = &volumeToken;
    PlayerState_PublishVolume(volume, valid, now);
    currentTask = previous;
}

void readAfter() { observedAfter = getAsReader(); }
void readBefore()
{
    observedBefore = getAsReader();
    afterGive = readAfter;
}

void publishVolumeBeforeControlTake()
{
    observedBefore = getAsReader();
    publishVolume(73, 3000);
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
    PlayerState_Init(0, false);
    CHECK(created == 1 && taken == 0 && given == 0);
    scheduler = taskSCHEDULER_RUNNING;
    currentTask = &volumeToken;
    PlayerState_BindVolumePublisher();
    PlayerStateSnapshot initial = getAsReader();
    CHECK(initial.mode == PlayerMode::Idle);
    CHECK(initial.acceptedVolume == 0 && !initial.acceptedVolumeValid);
    CHECK(initial.volumeRevision == 0 && initial.eventSequence == 0);
    // The volume owner can establish validity before the control owner binds.
    PlayerState_PublishVolume(0, true, 10);
    initial = getAsReader();
    CHECK(initial.acceptedVolume == 0 && initial.acceptedVolumeValid);
    CHECK(initial.revision == 1 && initial.volumeRevision == 0);
    CHECK(event(initial, PlayerEventKind::VolumeChanged).revision == 0);
    currentTask = &controlToken;
    PlayerState_BindPublisher();
    ++groups;

    // A stale whole control snapshot cannot restore old volume or validity.
    PlayerStateSnapshot local;
    local.mode = PlayerMode::Playing;
    local.background = PlayerMode::Playing;
    local.playback = PlayerPlayback::Playing;
    local.currentSong = 4;
    local.hasCurrentSong = true;
    local.observedAt = 100;
    publishVolume(70, 90);
    PlayerStateSnapshot accepted = getAsReader();
    CHECK(accepted.acceptedVolume == 70 && accepted.volumeRevision == 1);
    CHECK(!event(accepted, PlayerEventKind::VolumeChanged).overlayEligible);
    const uint32_t volumeEvent = event(accepted, PlayerEventKind::VolumeChanged).revision;
    local.acceptedVolume = 0;
    local.acceptedVolumeValid = false;
    local.volumeRevision = 0xFFFFFFFFU;
    PlayerState_RecordEvent(local, PlayerEventKind::VolumeChanged, 999, true);
    PlayerState_Publish(local);
    accepted = getAsReader();
    CHECK(accepted.acceptedVolume == 70 && accepted.acceptedVolumeValid);
    CHECK(accepted.volumeRevision == 1);
    CHECK(event(accepted, PlayerEventKind::VolumeChanged).revision == volumeEvent);
    CHECK(event(accepted, PlayerEventKind::VolumeChanged).timestamp == 90);
    CHECK(accepted.mode == PlayerMode::Playing && accepted.currentSong == 4);
    CHECK(accepted.hasCurrentSong && accepted.playback == PlayerPlayback::Playing);
    ++groups;

    // No accepted change and failed/invalid input leave every revision intact.
    const uint32_t revisionBefore = accepted.revision;
    const uint32_t sequenceBefore = accepted.eventSequence;
    publishVolume(70, 110);
    publishVolume(70, 120);
    const unsigned beforeInvalid = taken;
    publishVolume(0, 130, false);
    CHECK(taken == beforeInvalid);
    accepted = getAsReader();
    CHECK(accepted.revision == revisionBefore);
    CHECK(accepted.eventSequence == sequenceBefore && accepted.volumeRevision == 1);
    CHECK(accepted.acceptedVolume == 70 && accepted.acceptedVolumeValid);
    CHECK(event(accepted, PlayerEventKind::VolumeChanged).timestamp == 90);
    ++groups;

    // Filter noise/deadband rejection does not invent publication events.
    PotentiometerFilter filter;
    CHECK(filter.addSample(2866)); // Rounded logical 70, same as public state.
    publishVolume(filter.volume(), 200);
    for (uint32_t raw : {2865U, 2867U, 2880U, 2890U, 2880U, 2866U}) {
        CHECK(!filter.addSample(raw));
        // Defensive same-value publications are harmless, even if called.
        publishVolume(filter.volume(), 220);
    }
    accepted = getAsReader();
    CHECK(accepted.revision == revisionBefore && accepted.volumeRevision == 1);
    CHECK(accepted.acceptedVolume == 70 && accepted.eventSequence == sequenceBefore);
    ++groups;

    // Logical range is bounded; clamping the same value creates no new event.
    publishVolume(150, 300);
    accepted = getAsReader();
    CHECK(accepted.acceptedVolume == 100 && accepted.volumeRevision == 2);
    CHECK(event(accepted, PlayerEventKind::VolumeChanged).overlayEligible);
    const uint32_t atMaximum = accepted.revision;
    publishVolume(255, 310);
    accepted = getAsReader();
    CHECK(accepted.acceptedVolume == 100 && accepted.revision == atMaximum);
    publishVolume(0, 320);
    accepted = getAsReader();
    CHECK(accepted.acceptedVolume == 0 && accepted.volumeRevision == 3);
    CHECK(event(accepted, PlayerEventKind::VolumeChanged).timestamp == 320);
    ++groups;

    // Independent volume publications preserve all player/selection metadata.
    local.mode = PlayerMode::Paused;
    local.background = PlayerMode::Paused;
    local.playback = PlayerPlayback::Paused;
    local.currentSong = 6;
    local.pendingSong = 7;
    local.previewBits = 2;
    local.capturedBits = 7;
    local.phase = PlayerSelectionPhase::Confirm;
    local.confirmationStart = 400;
    local.confirmationDeadline = 5400;
    local.timeoutVisible = true;
    local.timeoutStart = 350;
    local.lastCompletion = PlayerPlayback::Finished;
    local.observedAt = 410;
    PlayerState_Publish(local);
    publishVolume(22, 420);
    accepted = getAsReader();
    CHECK(accepted.acceptedVolume == 22 && accepted.volumeRevision == 4);
    CHECK(accepted.mode == PlayerMode::Paused && accepted.background == PlayerMode::Paused);
    CHECK(accepted.playback == PlayerPlayback::Paused && accepted.currentSong == 6);
    CHECK(accepted.pendingSong == 7 && accepted.previewBits == 2 && accepted.capturedBits == 7);
    CHECK(accepted.phase == PlayerSelectionPhase::Confirm);
    CHECK(accepted.confirmationStart == 400 && accepted.confirmationDeadline == 5400);
    CHECK(accepted.timeoutVisible && accepted.timeoutStart == 350);
    CHECK(accepted.lastCompletion == PlayerPlayback::Finished && accepted.observedAt == 410);
    CHECK(event(accepted, PlayerEventKind::VolumeChanged).overlayEligible);
    ++groups;

    // A control-owned notice acknowledgement merges without republishing stale
    // volume or inventing a new presentation/volume event.
    const PlayerStateSnapshot beforeDismiss = accepted;
    PlayerState_DismissTimeoutNotice();
    accepted = getAsReader();
    CHECK(!accepted.timeoutVisible && accepted.timeoutStart == beforeDismiss.timeoutStart);
    CHECK(accepted.revision == beforeDismiss.revision + 1U);
    CHECK(accepted.acceptedVolume == beforeDismiss.acceptedVolume);
    CHECK(accepted.acceptedVolumeValid == beforeDismiss.acceptedVolumeValid);
    CHECK(accepted.volumeRevision == beforeDismiss.volumeRevision);
    CHECK(accepted.eventSequence == beforeDismiss.eventSequence);
    CHECK(event(accepted, PlayerEventKind::VolumeChanged).revision ==
          event(beforeDismiss, PlayerEventKind::VolumeChanged).revision);
    CHECK(event(accepted, PlayerEventKind::VolumeChanged).timestamp ==
          event(beforeDismiss, PlayerEventKind::VolumeChanged).timestamp);
    CHECK(accepted.mode == beforeDismiss.mode && accepted.currentSong == beforeDismiss.currentSong);
    const uint32_t dismissedRevision = accepted.revision;
    PlayerState_DismissTimeoutNotice();
    accepted = getAsReader();
    CHECK(accepted.revision == dismissedRevision);
    currentTask = &volumeToken;
    expectAssertion([] { PlayerState_DismissTimeoutNotice(); });
    currentTask = &controlToken;
    local.timeoutVisible = false;
    ++groups;

    // Producers' private counters map to one retained, globally ordered clock.
    PlayerState_RecordEvent(local, PlayerEventKind::PresentationCancelled, 500);
    PlayerState_Publish(local);
    accepted = getAsReader();
    const uint32_t firstCancel = event(accepted, PlayerEventKind::PresentationCancelled).revision;
    CHECK(PlayerState_EventAfter(firstCancel,
        event(accepted, PlayerEventKind::VolumeChanged).revision));
    publishVolume(40, 510);
    accepted = getAsReader();
    const uint32_t laterVolume = event(accepted, PlayerEventKind::VolumeChanged).revision;
    CHECK(PlayerState_EventAfter(laterVolume, firstCancel));
    PlayerState_RecordEvent(local, PlayerEventKind::ConfirmationReady, 520);
    CHECK(event(local, PlayerEventKind::ConfirmationReady).revision < laterVolume);
    PlayerState_Publish(local);
    accepted = getAsReader();
    CHECK(PlayerState_EventAfter(event(accepted, PlayerEventKind::ConfirmationReady).revision,
                                 laterVolume));
    CHECK(event(accepted, PlayerEventKind::VolumeChanged).timestamp == 510);
    const uint32_t withoutNewEvents = accepted.eventSequence;
    PlayerState_Publish(local);
    accepted = getAsReader();
    CHECK(accepted.eventSequence == withoutNewEvents);
    CHECK(accepted.acceptedVolume == 40 && accepted.volumeRevision == 5);
    ++groups;

    // A same-update capture/cancel/ready retains source order independent of
    // event enum order; selection has priority over concurrent volume overlays.
    local.mode = PlayerMode::Selecting;
    PlayerState_RecordEvent(local, PlayerEventKind::Captured, 600);
    PlayerState_RecordEvent(local, PlayerEventKind::PresentationCancelled, 600);
    PlayerState_RecordEvent(local, PlayerEventKind::ConfirmationReady, 600);
    PlayerState_Publish(local);
    accepted = getAsReader();
    CHECK(PlayerState_EventAfter(event(accepted, PlayerEventKind::PresentationCancelled).revision,
                                 event(accepted, PlayerEventKind::Captured).revision));
    CHECK(PlayerState_EventAfter(event(accepted, PlayerEventKind::ConfirmationReady).revision,
                                 event(accepted, PlayerEventKind::PresentationCancelled).revision));
    publishVolume(50, 610);
    accepted = getAsReader();
    CHECK(!event(accepted, PlayerEventKind::VolumeChanged).overlayEligible);
    CHECK(accepted.mode == PlayerMode::Selecting && accepted.pendingSong == 7);
    CHECK(PlayerState_EventAfter(event(accepted, PlayerEventKind::VolumeChanged).revision,
                                 event(accepted, PlayerEventKind::ConfirmationReady).revision));
    ++groups;

    // Interleave a real second-owner publication just before control acquires
    // its mutex. The resulting snapshot must merge both complete transactions.
    const PlayerStateSnapshot prior = accepted;
    local.mode = PlayerMode::Playing;
    local.background = PlayerMode::Playing;
    local.playback = PlayerPlayback::Playing;
    local.currentSong = 3;
    local.acceptedVolume = 1; // Deliberately stale and untrusted.
    local.observedAt = 3010;
    beforeTake = publishVolumeBeforeControlTake;
    PlayerState_Publish(local);
    CHECK(observedBefore.revision == prior.revision);
    CHECK(observedBefore.currentSong == prior.currentSong);
    CHECK(observedAfter.mode == PlayerMode::Playing && observedAfter.currentSong == 3);
    CHECK(observedAfter.playback == PlayerPlayback::Playing && observedAfter.observedAt == 3010);
    CHECK(observedAfter.acceptedVolume == 73 && observedAfter.volumeRevision == 7);
    CHECK(observedAfter.acceptedVolumeValid);
    CHECK(event(observedAfter, PlayerEventKind::VolumeChanged).timestamp == 3000);
    CHECK(!testMutex.held && taken == given);
    ++groups;

    // A reader on both boundaries sees complete old or new volume transactions.
    const PlayerStateSnapshot beforeVolume = getAsReader();
    beforeTake = readBefore;
    publishVolume(81, 3100);
    CHECK(observedBefore.revision == beforeVolume.revision);
    CHECK(observedBefore.acceptedVolume == 73 && observedBefore.volumeRevision == 7);
    CHECK(observedAfter.acceptedVolume == 81 && observedAfter.volumeRevision == 8);
    CHECK(observedAfter.mode == beforeVolume.mode && observedAfter.currentSong == beforeVolume.currentSong);
    CHECK(event(observedAfter, PlayerEventKind::VolumeChanged).timestamp == 3100);
    CHECK(observedAfter.revision == beforeVolume.revision + 1U);
    PlayerStateSnapshot detached = observedAfter;
    detached.acceptedVolume = 99;
    detached.currentSong = 0;
    detached.events[static_cast<uint8_t>(PlayerEventKind::VolumeChanged)].timestamp = 0;
    CHECK(detached.acceptedVolume == 99 && detached.currentSong == 0);
    accepted = getAsReader();
    CHECK(accepted.acceptedVolume == 81 && accepted.currentSong == 3);
    CHECK(event(accepted, PlayerEventKind::VolumeChanged).timestamp == 3100);
    ++groups;

    // Owner, scheduler and IRQ checks reject access before taking a mutex.
    currentTask = &readerToken;
    expectAssertion([&] { PlayerState_Publish(local); });
    expectAssertion([] { PlayerState_PublishVolume(99, true, 3200); });
    expectAssertion([] { PlayerState_BindPublisher(); });
    expectAssertion([] { PlayerState_BindVolumePublisher(); });
    currentTask = &controlToken;
    expectAssertion([] { PlayerState_PublishVolume(99, true, 3200); });
    currentTask = &volumeToken;
    expectAssertion([&] { PlayerState_Publish(local); });
    StateTestRtos::interruptNumber = 21;
    expectAssertion([] { PlayerState_PublishVolume(99, true, 3200); });
    expectAssertion([&] { PlayerState_GetSnapshot(accepted); });
    StateTestRtos::interruptNumber = 0;
    scheduler = taskSCHEDULER_SUSPENDED;
    expectAssertion([&] { PlayerState_GetSnapshot(accepted); });
    scheduler = taskSCHEDULER_RUNNING;
    currentTask = &controlToken;
    accepted = getAsReader();
    CHECK(accepted.acceptedVolume == 81 && accepted.volumeRevision == 8);
    ++groups;

    // Wrap-safe private control source order and uint32 millisecond timestamps.
    // First make a fresh source kind eligible close to its sequence wrap, then
    // cross wrap on that kind; canonical shared revisions remain independent.
    local.eventSequence = 0xFFFFFFFEU;
    PlayerState_RecordEvent(local, PlayerEventKind::Completed, 0xFFFFFFF0U);
    PlayerState_Publish(local);
    accepted = getAsReader();
    const uint32_t completedBeforeWrap = event(accepted, PlayerEventKind::Completed).revision;
    PlayerState_RecordEvent(local, PlayerEventKind::Completed, 20);
    CHECK(event(local, PlayerEventKind::Completed).revision == 1);
    local.confirmationStart = 0xFFFFFFF0U;
    local.confirmationDeadline = local.confirmationStart + 5000U;
    PlayerState_Publish(local);
    accepted = getAsReader();
    CHECK(PlayerState_EventAfter(event(accepted, PlayerEventKind::Completed).revision,
                                 completedBeforeWrap));
    CHECK(event(accepted, PlayerEventKind::Completed).timestamp == 20);
    CHECK(accepted.confirmationDeadline == 4984);
    CHECK(accepted.confirmationDeadline - accepted.confirmationStart == 5000U);
    CHECK(accepted.acceptedVolume == 81 && accepted.volumeRevision == 8);
    CHECK(!testMutex.held && taken == given && created == 1);
    ++groups;

    std::printf("Stage-4 player-state host tests: %u groups, %u checks passed.\n", groups, checks);
    return 0;
}
