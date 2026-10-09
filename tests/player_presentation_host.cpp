#include "player_presentation.h"

#include <cstdio>
#include <cstdlib>

namespace {
unsigned groups = 0;
unsigned checks = 0;
void check(bool condition, const char *message)
{
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
#define CHECK(condition) check((condition), #condition)

float note[] = {440.0f};
float beat[] = {1.0f};
Song first("FIRST", "TEST", note, beat, 1.0f, 1);
Song second("SECOND", "TEST", note, beat, 1.0f, 1);
Song *songs[] = {&first, &second, &first, &second,
                 &first, &second, &first, &second};

PlayerStateSnapshot state(PlayerMode mode)
{
    PlayerStateSnapshot result;
    result.revision = 1;
    result.mode = result.background = mode;
    result.playback = mode == PlayerMode::Playing ? PlayerPlayback::Playing
                    : mode == PlayerMode::Paused ? PlayerPlayback::Paused
                    : PlayerPlayback::Idle;
    result.currentSong = 1;
    result.hasCurrentSong = mode != PlayerMode::Idle;
    result.acceptedVolume = 50;
    result.acceptedVolumeValid = true;
    return result;
}

void record(PlayerStateSnapshot &snapshot, PlayerEventKind kind, uint32_t now,
            bool eligible = false)
{
    PlayerState_RecordEvent(snapshot, kind, now, eligible);
}

void volume(PlayerStateSnapshot &snapshot, uint8_t accepted, uint32_t now,
            bool eligible = true)
{
    snapshot.acceptedVolume = accepted;
    if (++snapshot.volumeRevision == 0) ++snapshot.volumeRevision;
    record(snapshot, PlayerEventKind::VolumeChanged, now, eligible);
    ++snapshot.revision;
}

const PlayerEventStamp &event(const PlayerStateSnapshot &snapshot,
                             PlayerEventKind kind)
{
    return snapshot.events[static_cast<uint8_t>(kind)];
}

unsigned actions(const PlayerPresentationFrame &frame,
                 PlayerPresentationActionKind kind)
{
    unsigned result = 0;
    for (uint8_t i = 0; i < frame.actionCount; ++i)
        if (frame.actions[i].kind == kind) ++result;
    return result;
}

void sameSnapshot(const PlayerStateSnapshot &actual,
                  const PlayerStateSnapshot &original)
{
    CHECK(actual.revision == original.revision);
    CHECK(actual.observedAt == original.observedAt);
    CHECK(actual.mode == original.mode);
    CHECK(actual.background == original.background);
    CHECK(actual.currentSong == original.currentSong);
    CHECK(actual.pendingSong == original.pendingSong);
    CHECK(actual.previewBits == original.previewBits);
    CHECK(actual.capturedBits == original.capturedBits);
    CHECK(actual.hasCurrentSong == original.hasCurrentSong);
    CHECK(actual.acceptedVolumeValid == original.acceptedVolumeValid);
    CHECK(actual.phase == original.phase);
    CHECK(actual.confirmationStart == original.confirmationStart);
    CHECK(actual.confirmationDeadline == original.confirmationDeadline);
    CHECK(actual.timeoutVisible == original.timeoutVisible);
    CHECK(actual.timeoutStart == original.timeoutStart);
    CHECK(actual.acceptedVolume == original.acceptedVolume);
    CHECK(actual.volumeRevision == original.volumeRevision);
    CHECK(actual.playback == original.playback);
    CHECK(actual.lastCompletion == original.lastCompletion);
    CHECK(actual.eventSequence == original.eventSequence);
    for (uint8_t i = 0; i < PlayerPresentationEventCount; ++i) {
        CHECK(actual.events[i].revision == original.events[i].revision);
        CHECK(actual.events[i].timestamp == original.events[i].timestamp);
        CHECK(actual.events[i].overlayEligible == original.events[i].overlayEligible);
    }
}

void mappings()
{
    ++groups;
    PlayerPresentationObserver observer(songs, 8);
    PlayerStateSnapshot playing = state(PlayerMode::Playing);
    PlayerPresentationFrame frame = observer.Observe(playing, 100);
    CHECK(frame.screen == PlayerPresentationScreen::Playing);
    CHECK(frame.view.playback == UiPlaybackState::Playing);
    CHECK(frame.view.currentSong == &second);
    CHECK(frame.view.currentIndex == 1);
    CHECK(frame.view.volume == 50);
    CHECK(frame.led == PLAYER_LED_PLAYING);
    CHECK(frame.actionCount == 0);

    ++groups;
    PlayerStateSnapshot paused = state(PlayerMode::Paused);
    frame = observer.Observe(paused, 120);
    CHECK(frame.screen == PlayerPresentationScreen::Paused);
    CHECK(frame.view.playback == UiPlaybackState::Paused);
    CHECK(frame.led == PLAYER_LED_PAUSED);

    ++groups;
    for (PlayerMode background : {PlayerMode::Playing, PlayerMode::Paused}) {
        PlayerStateSnapshot selecting = state(PlayerMode::Selecting);
        selecting.background = background;
        selecting.playback = background == PlayerMode::Playing
                           ? PlayerPlayback::Playing : PlayerPlayback::Paused;
        selecting.phase = PlayerSelectionPhase::Release;
        selecting.pendingSong = selecting.capturedBits = 6;
        selecting.previewBits = 1;
        frame = observer.Observe(selecting, 140);
        CHECK(frame.screen == PlayerPresentationScreen::Release);
        CHECK(frame.view.selection == UiSelectionState::Release);
        CHECK(frame.view.candidateIndex == 6);
        CHECK(frame.view.candidateSong == songs[6]);
        CHECK(frame.led == PLAYER_LED_SELECTING);
        CHECK(frame.view.playback == (background == PlayerMode::Playing
                                   ? UiPlaybackState::Playing : UiPlaybackState::Paused));
    }

    ++groups;
    PlayerStateSnapshot idle = state(PlayerMode::Idle);
    for (uint8_t bits = 0; bits < 8; ++bits) {
        idle.previewBits = bits;
        frame = observer.Observe(idle, 160);
        CHECK(frame.screen == PlayerPresentationScreen::Idle);
        CHECK(frame.led == PLAYER_LED_IDLE);
        CHECK(frame.view.candidateIndex == bits);
        CHECK(frame.view.candidateSong == songs[bits]);
    }

    ++groups;
    for (PlayerPlayback outcome : {PlayerPlayback::Idle, PlayerPlayback::Finished,
                                   PlayerPlayback::Error}) {
        playing.playback = outcome;
        frame = observer.Observe(playing, 180);
        CHECK(frame.led == PLAYER_LED_IDLE); // Actual transport wins over old mode.
    }
    idle.hasCurrentSong = true; // Historical song does not imply active transport.
    frame = observer.Observe(idle, 200);
    CHECK(frame.screen == PlayerPresentationScreen::Idle);
    CHECK(frame.led == PLAYER_LED_IDLE);
}

void timeouts()
{
    ++groups;
    for (PlayerMode mode : {PlayerMode::Playing, PlayerMode::Paused, PlayerMode::Idle}) {
        PlayerPresentationObserver observer(songs, 8);
        PlayerStateSnapshot snapshot = state(mode);
        snapshot.timeoutVisible = true;
        snapshot.timeoutStart = 1000;
        record(snapshot, PlayerEventKind::Timeout, 1000);
        PlayerPresentationFrame frame = observer.Observe(snapshot, 1000);
        CHECK(frame.screen == PlayerPresentationScreen::Timeout);
        CHECK(frame.view.timeoutVisible);
        frame = observer.Observe(snapshot, 1999);
        CHECK(frame.screen == PlayerPresentationScreen::Timeout);
        frame = observer.Observe(snapshot, 2000);
        CHECK(!frame.view.timeoutVisible);
        CHECK(frame.screen == (mode == PlayerMode::Playing ? PlayerPresentationScreen::Playing
                             : mode == PlayerMode::Paused ? PlayerPresentationScreen::Paused
                             : PlayerPresentationScreen::Idle));
        CHECK(snapshot.timeoutVisible); // Observer cannot clear owner state.
        frame = observer.Observe(snapshot, 2100);
        CHECK(!frame.view.timeoutVisible);
    }

    ++groups;
    PlayerPresentationObserver slow(songs, 8);
    PlayerStateSnapshot old = state(PlayerMode::Playing);
    old.timeoutVisible = true;
    old.timeoutStart = 1000;
    record(old, PlayerEventKind::Timeout, 1000);
    CHECK(slow.Observe(old, 2500).screen == PlayerPresentationScreen::Playing);
    CHECK(slow.LastSeen(PlayerEventKind::Timeout) == event(old, PlayerEventKind::Timeout).revision);

    ++groups;
    PlayerPresentationObserver newerVolume(songs, 8);
    volume(old, 72, 1100);
    PlayerPresentationFrame frame = newerVolume.Observe(old, 1200);
    CHECK(!frame.view.timeoutVisible);
    CHECK(frame.screen == PlayerPresentationScreen::Volume);
    CHECK(old.timeoutVisible);

    ++groups;
    PlayerPresentationObserver newerTimeout(songs, 8);
    PlayerStateSnapshot ordered = state(PlayerMode::Playing);
    volume(ordered, 72, 1000);
    ordered.timeoutVisible = true;
    ordered.timeoutStart = 1100;
    record(ordered, PlayerEventKind::Timeout, 1100);
    frame = newerTimeout.Observe(ordered, 1200);
    CHECK(frame.view.timeoutVisible);
    CHECK(frame.screen == PlayerPresentationScreen::Timeout);
}

void overlays()
{
    ++groups;
    PlayerPresentationObserver observer(songs, 8);
    PlayerStateSnapshot snapshot = state(PlayerMode::Playing);
    (void)observer.Observe(snapshot, 100);
    volume(snapshot, 64, 1000);
    PlayerPresentationFrame frame = observer.Observe(snapshot, 1010);
    CHECK(frame.screen == PlayerPresentationScreen::Volume);
    CHECK(actions(frame, PlayerPresentationActionKind::VolumeChanged) == 1);
    CHECK(frame.actions[0].timestamp == 1000);
    CHECK(frame.volumeRevision == 1);
    CHECK(frame.view.volume == 64);
    for (uint32_t now = 1030; now < 2500; now += 20) {
        ++snapshot.revision; // Repeated control publications are not knob changes.
        frame = observer.Observe(snapshot, now);
        CHECK(frame.screen == PlayerPresentationScreen::Volume);
        CHECK(frame.actionCount == 0);
    }

    ++groups;
    frame = observer.Observe(snapshot, 2500);
    CHECK(frame.screen == PlayerPresentationScreen::Playing);
    CHECK(frame.led == PLAYER_LED_PLAYING);
    CHECK(frame.actionCount == 0);
    CHECK(observer.Observe(snapshot, 2520).screen == PlayerPresentationScreen::Playing);

    ++groups;
    volume(snapshot, 80, 3000);
    frame = observer.Observe(snapshot, 3020);
    CHECK(frame.screen == PlayerPresentationScreen::Volume);
    CHECK(frame.view.volume == 80);
    CHECK(actions(frame, PlayerPresentationActionKind::VolumeChanged) == 1);
    volume(snapshot, 90, 4000);
    frame = observer.Observe(snapshot, 4020);
    CHECK(frame.volumeRevision == 3);
    CHECK(frame.view.volume == 90);
    CHECK(frame.actions[0].timestamp == 4000);
    CHECK(observer.Observe(snapshot, 4500).screen == PlayerPresentationScreen::Volume);
    CHECK(observer.Observe(snapshot, 5499).screen == PlayerPresentationScreen::Volume);
    CHECK(observer.Observe(snapshot, 5500).screen == PlayerPresentationScreen::Playing);

    ++groups;
    PlayerPresentationObserver pausedObserver(songs, 8);
    PlayerStateSnapshot paused = state(PlayerMode::Paused);
    volume(paused, 25, 1000);
    CHECK(pausedObserver.Observe(paused, 1020).screen == PlayerPresentationScreen::Volume);
    frame = pausedObserver.Observe(paused, 2500);
    CHECK(frame.screen == PlayerPresentationScreen::Paused);
    CHECK(frame.view.playback == UiPlaybackState::Paused);
    CHECK(frame.led == PLAYER_LED_PAUSED);

    ++groups;
    PlayerPresentationObserver selectionObserver(songs, 8);
    PlayerStateSnapshot selection = state(PlayerMode::Playing);
    volume(selection, 25, 1000);
    CHECK(selectionObserver.Observe(selection, 1020).screen == PlayerPresentationScreen::Volume);
    selection.mode = PlayerMode::Selecting;
    selection.background = PlayerMode::Playing;
    selection.phase = PlayerSelectionPhase::Release;
    record(selection, PlayerEventKind::Captured, 1100);
    record(selection, PlayerEventKind::PresentationCancelled, 1100);
    frame = selectionObserver.Observe(selection, 1120);
    CHECK(frame.screen == PlayerPresentationScreen::Release);
    CHECK(frame.led == PLAYER_LED_SELECTING);
    CHECK(actions(frame, PlayerPresentationActionKind::CancelTransient) == 1);
    CHECK(selectionObserver.Observe(selection, 2600).screen == PlayerPresentationScreen::Release);

    ++groups;
    PlayerPresentationObserver confirmObserver(songs, 8);
    PlayerStateSnapshot confirm = state(PlayerMode::Playing);
    volume(confirm, 25, 1000);
    CHECK(confirmObserver.Observe(confirm, 1020).screen == PlayerPresentationScreen::Volume);
    confirm.mode = PlayerMode::Selecting;
    confirm.background = PlayerMode::Paused;
    confirm.phase = PlayerSelectionPhase::Confirm;
    confirm.confirmationStart = 1100;
    confirm.confirmationDeadline = 6100;
    record(confirm, PlayerEventKind::ConfirmationReady, 1100);
    CHECK(confirmObserver.Observe(confirm, 1120).screen == PlayerPresentationScreen::Confirm);
    CHECK(confirmObserver.Observe(confirm, 2600).screen == PlayerPresentationScreen::Confirm);

    ++groups;
    for (PlayerMode mode : {PlayerMode::Idle, PlayerMode::Selecting}) {
        PlayerPresentationObserver suppressed(songs, 8);
        PlayerStateSnapshot quiet = state(mode);
        quiet.phase = PlayerSelectionPhase::Release;
        volume(quiet, 20, 1000, false);
        frame = suppressed.Observe(quiet, 1020);
        CHECK(frame.screen != PlayerPresentationScreen::Volume);
        CHECK(actions(frame, PlayerPresentationActionKind::VolumeChanged) == 0);
        CHECK(frame.view.volume == 20);
        CHECK(suppressed.LastSeen(PlayerEventKind::VolumeChanged) != 0);
    }

    ++groups;
    PlayerPresentationObserver slow(songs, 8);
    PlayerStateSnapshot delayed = state(PlayerMode::Playing);
    volume(delayed, 92, 1000);
    frame = slow.Observe(delayed, 2500);
    CHECK(frame.screen == PlayerPresentationScreen::Playing);
    CHECK(frame.actions[0].timestamp == 1000);
    CHECK(actions(frame, PlayerPresentationActionKind::VolumeChanged) == 1);
    CHECK(slow.Observe(delayed, 2520).actionCount == 0);
}

void retainedEvents()
{
    ++groups;
    PlayerPresentationObserver observer(songs, 8);
    PlayerStateSnapshot snapshot = state(PlayerMode::Playing);
    // Several producer events happen without a UI read in between.
    record(snapshot, PlayerEventKind::Captured, 100);
    record(snapshot, PlayerEventKind::PresentationCancelled, 101);
    record(snapshot, PlayerEventKind::ConfirmationReady, 200);
    record(snapshot, PlayerEventKind::Confirmed, 300);
    record(snapshot, PlayerEventKind::Paused, 400);
    record(snapshot, PlayerEventKind::Resumed, 500);
    record(snapshot, PlayerEventKind::Completed, 600);
    record(snapshot, PlayerEventKind::PlaybackError, 700);
    record(snapshot, PlayerEventKind::Timeout, 800);
    volume(snapshot, 61, 900);
    const PlayerStateSnapshot original = snapshot;
    PlayerPresentationFrame frame = observer.Observe(snapshot, 1000);
    CHECK(frame.actionCount == 3);
    CHECK(frame.actions[0].kind == PlayerPresentationActionKind::CancelTransient);
    CHECK(frame.actions[1].kind == PlayerPresentationActionKind::ConfirmationReady);
    CHECK(frame.actions[2].kind == PlayerPresentationActionKind::VolumeChanged);
    CHECK(frame.actions[0].timestamp == 101);
    CHECK(frame.actions[1].timestamp == 200);
    CHECK(frame.actions[2].timestamp == 900);
    for (uint8_t kind = 0; kind < PlayerPresentationEventCount; ++kind)
        CHECK(observer.LastSeen(static_cast<PlayerEventKind>(kind)) == snapshot.events[kind].revision);
    sameSnapshot(snapshot, original);
    CHECK(observer.Observe(snapshot, 1020).actionCount == 0);

    ++groups;
    record(snapshot, PlayerEventKind::PresentationCancelled, 1100);
    frame = observer.Observe(snapshot, 1120);
    CHECK(frame.screen == PlayerPresentationScreen::Playing);
    CHECK(actions(frame, PlayerPresentationActionKind::CancelTransient) == 1);
    CHECK(observer.Observe(snapshot, 1140).actionCount == 0);
    volume(snapshot, 77, 1200);
    frame = observer.Observe(snapshot, 1220);
    CHECK(frame.screen == PlayerPresentationScreen::Volume);
    CHECK(actions(frame, PlayerPresentationActionKind::VolumeChanged) == 1);

    ++groups;
    PlayerPresentationObserver superseded(songs, 8);
    PlayerStateSnapshot burst = state(PlayerMode::Playing);
    volume(burst, 84, 1000);
    record(burst, PlayerEventKind::PresentationCancelled, 1100);
    frame = superseded.Observe(burst, 1120);
    CHECK(frame.screen == PlayerPresentationScreen::Playing);
    CHECK(actions(frame, PlayerPresentationActionKind::VolumeChanged) == 0);
    CHECK(actions(frame, PlayerPresentationActionKind::CancelTransient) == 1);

    ++groups;
    PlayerPresentationObserver coalesced(songs, 8);
    PlayerStateSnapshot many = state(PlayerMode::Playing);
    volume(many, 60, 1000);
    volume(many, 70, 1100);
    volume(many, 80, 1200);
    frame = coalesced.Observe(many, 1220);
    CHECK(frame.view.volume == 80);
    CHECK(frame.volumeRevision == 3);
    CHECK(actions(frame, PlayerPresentationActionKind::VolumeChanged) == 1);
    CHECK(frame.actions[0].timestamp == 1200);

    ++groups;
    // A stale per-kind revision cannot replay after a newer observation.
    const uint32_t latestSeen = coalesced.LastSeen(PlayerEventKind::VolumeChanged);
    --many.events[static_cast<uint8_t>(PlayerEventKind::VolumeChanged)].revision;
    frame = coalesced.Observe(many, 1240);
    CHECK(frame.actionCount == 0);
    CHECK(coalesced.LastSeen(PlayerEventKind::VolumeChanged) == latestSeen);
    CHECK(coalesced.LastSeen(PlayerEventKind::Count) == 0);
}

void confirmation()
{
    ++groups;
    PlayerPresentationObserver observer(songs, 8);
    PlayerStateSnapshot snapshot = state(PlayerMode::Selecting);
    snapshot.background = PlayerMode::Playing;
    snapshot.phase = PlayerSelectionPhase::Confirm;
    snapshot.confirmationStart = 1000;
    snapshot.confirmationDeadline = 6000;
    record(snapshot, PlayerEventKind::ConfirmationReady, 1000);
    PlayerPresentationFrame frame = observer.Observe(snapshot, 2500);
    CHECK(frame.screen == PlayerPresentationScreen::Confirm);
    CHECK(frame.confirmationSeconds == 4);
    CHECK(actions(frame, PlayerPresentationActionKind::ConfirmationReady) == 1);
    CHECK(frame.actions[0].timestamp == 1000);
    CHECK(observer.Observe(snapshot, 3000).confirmationSeconds == 3);
    CHECK(observer.Observe(snapshot, 5999).confirmationSeconds == 1);
    CHECK(observer.Observe(snapshot, 6000).screen == PlayerPresentationScreen::Confirm);
    CHECK(observer.Observe(snapshot, 6000).confirmationSeconds == 1);
    CHECK(snapshot.mode == PlayerMode::Selecting); // UI never owns timeout.
    CHECK(observer.Observe(snapshot, 6020).actionCount == 0);

    ++groups;
    PlayerPresentationObserver fallback(songs, 8);
    snapshot.events[static_cast<uint8_t>(PlayerEventKind::ConfirmationReady)] = PlayerEventStamp{};
    frame = fallback.Observe(snapshot, 2500);
    CHECK(frame.actionCount == 1);
    CHECK(frame.actions[0].kind == PlayerPresentationActionKind::ConfirmationReady);
    CHECK(frame.actions[0].timestamp == 1000);
    CHECK(fallback.Observe(snapshot, 2520).actionCount == 0);
    snapshot.confirmationStart = 7000;
    snapshot.confirmationDeadline = 12000;
    frame = fallback.Observe(snapshot, 7020);
    CHECK(frame.actionCount == 1);
    CHECK(frame.actions[0].timestamp == 7000);

    ++groups;
    snapshot.mode = PlayerMode::Idle;
    snapshot.phase = PlayerSelectionPhase::None;
    (void)fallback.Observe(snapshot, 7040);
    snapshot.mode = PlayerMode::Selecting;
    snapshot.phase = PlayerSelectionPhase::Confirm;
    CHECK(fallback.Observe(snapshot, 7060).actionCount == 1);
}

void wrapping()
{
    ++groups;
    PlayerPresentationObserver observer(songs, 8);
    PlayerStateSnapshot snapshot = state(PlayerMode::Playing);
    snapshot.eventSequence = 0xFFFFFFFCU;
    record(snapshot, PlayerEventKind::PresentationCancelled, 1000); // FFFFFFFD
    record(snapshot, PlayerEventKind::ConfirmationReady, 1100);    // FFFFFFFE
    snapshot.volumeRevision = 0xFFFFFFFEU;
    volume(snapshot, 40, 1200);                                  // FFFFFFFF
    PlayerPresentationFrame frame = observer.Observe(snapshot, 1220);
    CHECK(frame.actionCount == 3);
    CHECK(frame.actions[0].kind == PlayerPresentationActionKind::CancelTransient);
    CHECK(frame.actions[1].kind == PlayerPresentationActionKind::ConfirmationReady);
    CHECK(frame.actions[2].kind == PlayerPresentationActionKind::VolumeChanged);
    CHECK(frame.volumeRevision == 0xFFFFFFFFU);
    volume(snapshot, 45, 1300); // Event and volume skip absent sentinel zero.
    frame = observer.Observe(snapshot, 1320);
    CHECK(frame.volumeRevision == 1);
    CHECK(frame.actionCount == 1);
    CHECK(frame.actions[0].revision == 1);
    CHECK(frame.actions[0].timestamp == 1300);
    record(snapshot, PlayerEventKind::PresentationCancelled, 1400);
    record(snapshot, PlayerEventKind::ConfirmationReady, 1500);
    volume(snapshot, 55, 1600);
    frame = observer.Observe(snapshot, 1620);
    CHECK(frame.actionCount == 3);
    CHECK(frame.actions[0].kind == PlayerPresentationActionKind::CancelTransient);
    CHECK(frame.actions[1].kind == PlayerPresentationActionKind::ConfirmationReady);
    CHECK(frame.actions[2].kind == PlayerPresentationActionKind::VolumeChanged);
    CHECK(observer.Observe(snapshot, 1640).actionCount == 0);

    ++groups;
    PlayerPresentationObserver acrossTick(songs, 8);
    PlayerStateSnapshot tick = state(PlayerMode::Paused);
    const uint32_t start = 0xFFFFFF00U;
    volume(tick, 10, start);
    CHECK(acrossTick.Observe(tick, start + 1499U).screen == PlayerPresentationScreen::Volume);
    CHECK(acrossTick.Observe(tick, start + 1500U).screen == PlayerPresentationScreen::Paused);

    ++groups;
    PlayerPresentationObserver timeout(songs, 8);
    tick.timeoutVisible = true;
    tick.timeoutStart = start;
    tick.volumeRevision = 0;
    tick.events[static_cast<uint8_t>(PlayerEventKind::VolumeChanged)] = PlayerEventStamp{};
    record(tick, PlayerEventKind::Timeout, start);
    CHECK(timeout.Observe(tick, start + 999U).screen == PlayerPresentationScreen::Timeout);
    CHECK(timeout.Observe(tick, start + 1000U).screen == PlayerPresentationScreen::Paused);

    ++groups;
    PlayerPresentationObserver confirm(songs, 8);
    tick.mode = PlayerMode::Selecting;
    tick.phase = PlayerSelectionPhase::Confirm;
    tick.confirmationStart = start;
    tick.confirmationDeadline = start + 5000U;
    record(tick, PlayerEventKind::ConfirmationReady, start);
    frame = confirm.Observe(tick, start + 1000U);
    CHECK(frame.confirmationSeconds == 4);
    CHECK(frame.actions[0].timestamp == start);
    CHECK(confirm.Observe(tick, start + 5000U).confirmationSeconds == 1);
    CHECK(confirm.Observe(tick, start + 5000U).screen == PlayerPresentationScreen::Confirm);
}

void completionAndBounds()
{
    ++groups;
    PlayerPresentationObserver observer(songs, 8);
    PlayerStateSnapshot snapshot = state(PlayerMode::Playing);
    volume(snapshot, 40, 1000);
    CHECK(observer.Observe(snapshot, 1020).screen == PlayerPresentationScreen::Volume);
    snapshot.mode = PlayerMode::Idle;
    snapshot.playback = PlayerPlayback::Idle;
    snapshot.lastCompletion = PlayerPlayback::Finished;
    record(snapshot, PlayerEventKind::Completed, 1100);
    record(snapshot, PlayerEventKind::PresentationCancelled, 1100);
    PlayerPresentationFrame frame = observer.Observe(snapshot, 1120);
    CHECK(frame.screen == PlayerPresentationScreen::Idle);
    CHECK(frame.led == PLAYER_LED_IDLE);
    CHECK(observer.LastSeen(PlayerEventKind::Completed) != 0);

    ++groups;
    snapshot.mode = PlayerMode::Selecting;
    snapshot.background = PlayerMode::Idle;
    snapshot.phase = PlayerSelectionPhase::Confirm;
    snapshot.confirmationStart = 1000;
    snapshot.pendingSong = 5;
    frame = observer.Observe(snapshot, 1200);
    CHECK(frame.screen == PlayerPresentationScreen::Confirm);
    CHECK(frame.led == PLAYER_LED_SELECTING);
    CHECK(frame.view.playback == UiPlaybackState::Idle);
    CHECK(frame.view.candidateIndex == 5);

    ++groups;
    PlayerPresentationObserver noSongs(nullptr, 0);
    snapshot.currentSong = snapshot.pendingSong = 255;
    frame = noSongs.Observe(snapshot, 1300);
    CHECK(frame.view.currentSong == nullptr);
    CHECK(frame.view.candidateSong == nullptr);
}
} // namespace

int main()
{
    mappings();
    timeouts();
    overlays();
    retainedEvents();
    confirmation();
    wrapping();
    completionAndBounds();
    std::printf("PASS: presentation observer %u groups, %u checks\n", groups, checks);
}
