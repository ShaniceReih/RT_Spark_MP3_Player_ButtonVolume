#include "player_presentation.h"

namespace {
const PlayerEventStamp &event(const PlayerStateSnapshot &snapshot,
                             PlayerEventKind kind)
{
    return snapshot.events[static_cast<uint8_t>(kind)];
}

void append(PlayerPresentationFrame &frame, PlayerPresentationActionKind kind,
            uint32_t timestamp, uint32_t revision)
{
    PlayerPresentationAction &action = frame.actions[frame.actionCount++];
    action.kind = kind;
    action.timestamp = timestamp;
    action.revision = revision;
}

bool playbackMode(PlayerMode mode)
{
    return mode == PlayerMode::Playing || mode == PlayerMode::Paused;
}
} // namespace

PlayerPresentationObserver::PlayerPresentationObserver(Song *const *songs,
                                                       uint8_t songCount)
    : songs_(songs), songCount_(songCount)
{
}

const Song *PlayerPresentationObserver::songAt(uint8_t index) const
{
    return songs_ != nullptr && index < songCount_ ? songs_[index] : nullptr;
}

uint32_t PlayerPresentationObserver::LastSeen(PlayerEventKind kind) const
{
    const uint8_t index = static_cast<uint8_t>(kind);
    return index < PlayerPresentationEventCount ? seen_[index] : 0;
}

PlayerPresentationFrame PlayerPresentationObserver::Observe(
    const PlayerStateSnapshot &snapshot, uint32_t now)
{
    PlayerPresentationFrame frame;
    stateRevision_ = snapshot.revision;
    frame.stateRevision = stateRevision_;
#if defined(RTOS_STAGE4_PLAYER) && RTOS_STAGE4_PLAYER
    const bool changedVolume = snapshot.volumeRevision != volumeRevision_;
    volumeRevision_ = snapshot.volumeRevision;
#else
    const bool changedVolume = PlayerState_EventAfter(
        event(snapshot, PlayerEventKind::VolumeChanged).revision,
        seen_[static_cast<uint8_t>(PlayerEventKind::VolumeChanged)]);
#endif
    frame.volumeRevision = volumeRevision_;

    const PlayerMode background = snapshot.mode == PlayerMode::Selecting
                                ? snapshot.background : snapshot.mode;
    frame.view.playback = background == PlayerMode::Playing
                        ? UiPlaybackState::Playing
                        : background == PlayerMode::Paused
                        ? UiPlaybackState::Paused : UiPlaybackState::Idle;
    frame.view.currentIndex = snapshot.currentSong;
    frame.view.currentSong = songAt(snapshot.currentSong);
    frame.view.volume = snapshot.acceptedVolume;
    frame.view.candidateIndex = snapshot.mode == PlayerMode::Selecting
                             ? snapshot.pendingSong : snapshot.previewBits;
    frame.view.candidateSong = songAt(frame.view.candidateIndex);
    if (snapshot.mode == PlayerMode::Selecting) {
        frame.view.selection = snapshot.phase == PlayerSelectionPhase::Release
                             ? UiSelectionState::Release : UiSelectionState::Confirm;
        frame.led = PLAYER_LED_SELECTING;
    } else if (snapshot.playback == PlayerPlayback::Playing) {
        frame.led = PLAYER_LED_PLAYING;
    } else if (snapshot.playback == PlayerPlayback::Paused) {
        frame.led = PLAYER_LED_PAUSED;
    }

    // Collect latest retained occurrences in global publication order. Each
    // kind has its own watermark, so an unrelated newer event cannot hide it.
    uint8_t pending[PlayerPresentationEventCount] = {};
    uint8_t count = 0;
    for (uint8_t kind = 0; kind < PlayerPresentationEventCount; ++kind) {
        const uint32_t revision = snapshot.events[kind].revision;
        if (!PlayerState_EventAfter(revision, seen_[kind])) continue;
        uint8_t position = count;
        while (position > 0U && PlayerState_EventAfter(
                   snapshot.events[pending[position - 1U]].revision, revision)) {
            pending[position] = pending[position - 1U];
            --position;
        }
        pending[position] = kind;
        ++count;
    }

    const PlayerEventStamp &cancel = event(snapshot, PlayerEventKind::PresentationCancelled);
    const PlayerEventStamp &volume = event(snapshot, PlayerEventKind::VolumeChanged);
    const bool eligibleVolume = volume.overlayEligible && playbackMode(snapshot.mode) &&
                               !PlayerState_EventAfter(cancel.revision, volume.revision);
    bool readyAction = false;
    for (uint8_t item = 0; item < count; ++item) {
        const uint8_t index = pending[item];
        const PlayerEventKind kind = static_cast<PlayerEventKind>(index);
        const PlayerEventStamp &stamp = snapshot.events[index];
        seen_[index] = stamp.revision;
        if (kind == PlayerEventKind::PresentationCancelled) {
            overlayActive_ = false;
            append(frame, PlayerPresentationActionKind::CancelTransient,
                   stamp.timestamp, stamp.revision);
        } else if (kind == PlayerEventKind::ConfirmationReady) {
            const uint32_t start = snapshot.phase == PlayerSelectionPhase::Confirm
                                 ? snapshot.confirmationStart : stamp.timestamp;
            append(frame, PlayerPresentationActionKind::ConfirmationReady,
                   start, stamp.revision);
            confirmationSeen_ = true;
            confirmationStart_ = start;
            readyAction = true;
        } else if (kind == PlayerEventKind::VolumeChanged && changedVolume &&
                   eligibleVolume) {
            overlayActive_ = true;
            overlayStart_ = stamp.timestamp;
            append(frame, PlayerPresentationActionKind::VolumeChanged,
                   stamp.timestamp, stamp.revision);
        }
    }

    // A stable phase/start payload reconstructs a missed initial ready event;
    // it never extends the controller's deadline to the UI arrival time.
    const bool confirming = snapshot.mode == PlayerMode::Selecting &&
                            snapshot.phase == PlayerSelectionPhase::Confirm;
    if (confirming) {
        if (!readyAction && (!confirmationSeen_ ||
                             confirmationStart_ != snapshot.confirmationStart)) {
            append(frame, PlayerPresentationActionKind::ConfirmationReady,
                   snapshot.confirmationStart,
                   event(snapshot, PlayerEventKind::ConfirmationReady).revision);
            confirmationSeen_ = true;
            confirmationStart_ = snapshot.confirmationStart;
        }
        const uint32_t elapsed = now - snapshot.confirmationStart;
        frame.confirmationSeconds = elapsed >= 5000U
            ? 1U : static_cast<uint8_t>((5000U - elapsed + 999U) / 1000U);
    } else {
        confirmationSeen_ = false;
    }

    if (snapshot.mode == PlayerMode::Selecting ||
        frame.view.playback == UiPlaybackState::Idle ||
        (overlayActive_ && now - overlayStart_ >= 1500U)) {
        overlayActive_ = false;
    }

    // Notice expiry/supersession is a display decision only. The source is
    // never changed, and a slow observer cannot replay an expired notice.
    const PlayerEventStamp &timeout = event(snapshot, PlayerEventKind::Timeout);
    const bool volumeAfterTimeout = eligibleVolume &&
        PlayerState_EventAfter(volume.revision, timeout.revision);
    frame.view.timeoutVisible = snapshot.timeoutVisible &&
        now - snapshot.timeoutStart < 1000U && !volumeAfterTimeout;

    if (frame.view.selection == UiSelectionState::Release)
        screen_ = PlayerPresentationScreen::Release;
    else if (frame.view.selection == UiSelectionState::Confirm)
        screen_ = PlayerPresentationScreen::Confirm;
    else if (frame.view.timeoutVisible)
        screen_ = PlayerPresentationScreen::Timeout;
    else if (overlayActive_)
        screen_ = PlayerPresentationScreen::Volume;
    else if (frame.view.playback == UiPlaybackState::Playing)
        screen_ = PlayerPresentationScreen::Playing;
    else if (frame.view.playback == UiPlaybackState::Paused)
        screen_ = PlayerPresentationScreen::Paused;
    else
        screen_ = PlayerPresentationScreen::Idle;
    frame.screen = screen_;
    return frame;
}
