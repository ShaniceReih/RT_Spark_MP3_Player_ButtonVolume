#include "song_selection.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

struct Observation {
    uint32_t at;
    SelectionEvents event;
    uint8_t song;
};

struct Harness {
    explicit Harness(bool playback = false, uint32_t start = 0)
        : now(start), playbackControlsEnabled(playback) {
        sample();
    }

    SongSelectionInput controller;
    SelectionButtons buttons{};
    uint32_t now;
    bool playbackControlsEnabled;
    std::vector<Observation> observations;

    void sample() {
        const SelectionEvents event =
            controller.update(now, buttons, playbackControlsEnabled);
        if (event.captured || event.readyToConfirm || event.confirmed ||
            event.timedOut || event.pauseResume || event.volumeSteps != 0) {
            observations.push_back({now, event, controller.pendingSong()});
        }
    }

    void set(const SelectionButtons &next) {
        buttons = next;
        sample();
    }

    void advance(uint32_t milliseconds) {
        for (uint32_t i = 0; i < milliseconds; ++i) {
            ++now;
            sample();
        }
    }

    void settle(const SelectionButtons &next, uint32_t milliseconds = 30) {
        set(next);
        advance(milliseconds);
    }

    unsigned count(bool SelectionEvents::*member) const {
        unsigned result = 0;
        for (const Observation &observation : observations) {
            if (observation.event.*member) ++result;
        }
        return result;
    }

    int volume() const {
        int result = 0;
        for (const Observation &observation : observations) {
            result += observation.event.volumeSteps;
        }
        return result;
    }

    unsigned volumeEvents() const {
        unsigned result = 0;
        for (const Observation &observation : observations) {
            if (observation.event.volumeSteps != 0) ++result;
        }
        return result;
    }

    uint32_t lastTime(bool SelectionEvents::*member) const {
        for (auto it = observations.rbegin(); it != observations.rend(); ++it) {
            if (it->event.*member) return it->at;
        }
        assert(false && "Expected event was not emitted");
        return 0;
    }

    void afterReady(uint32_t milliseconds) {
        const uint32_t elapsed = now - lastTime(&SelectionEvents::readyToConfirm);
        assert(elapsed <= milliseconds);
        advance(milliseconds - elapsed);
    }
};

SelectionButtons chord(uint8_t song, bool up) {
    SelectionButtons buttons{};
    buttons.up = up;
    buttons.down = (song & 4U) != 0;
    buttons.left = (song & 2U) != 0;
    buttons.right = (song & 1U) != 0;
    return buttons;
}

void captureAndRelease(Harness &harness, uint8_t song) {
    harness.settle(chord(song, false));
    harness.settle(chord(song, true));
    if (!harness.playbackControlsEnabled || song != 0) {
        assert(harness.count(&SelectionEvents::captured) == 1);
        assert(harness.controller.pendingSong() == song);
        assert(harness.controller.waitingForRelease());
    } else {
        assert(harness.count(&SelectionEvents::captured) == 0);
    }
    assert(harness.count(&SelectionEvents::readyToConfirm) == 0);
    harness.settle({});
    assert(harness.count(&SelectionEvents::captured) == 1);
    assert(harness.controller.pendingSong() == song);
    assert(harness.count(&SelectionEvents::readyToConfirm) == 1);
    assert(!harness.controller.waitingForRelease());
    assert(harness.controller.selecting());
    assert(harness.count(&SelectionEvents::pauseResume) == 0);
    assert(harness.volumeEvents() == 0);
}

void allEightSongs() {
    // The same playbackControlsEnabled path serves both playing and paused.
    for (unsigned mode = 0; mode < 3; ++mode) {
        for (uint8_t song = 0; song < 8; ++song) {
            Harness harness(mode != 0);
            captureAndRelease(harness, song);
            harness.settle(chord(0, true));
            assert(harness.count(&SelectionEvents::confirmed) == 1);
            assert(harness.controller.pendingSong() == song);
            harness.advance(1100);
            harness.settle({});
            assert(harness.count(&SelectionEvents::confirmed) == 1);
            assert(harness.count(&SelectionEvents::captured) == 1);
            assert(harness.count(&SelectionEvents::pauseResume) == 0);
            assert(harness.volumeEvents() == 0);
        }
    }
}

void allButtonsMustReleaseAndCandidateStaysFrozen() {
    Harness harness(true);
    harness.settle(chord(3, false));
    harness.settle(chord(3, true));
    assert(harness.controller.pendingSong() == 3);
    harness.advance(1500);
    assert(harness.count(&SelectionEvents::captured) == 1);
    assert(harness.count(&SelectionEvents::pauseResume) == 0);
    assert(harness.count(&SelectionEvents::readyToConfirm) == 0);

    // Release UP and RIGHT, but keep LEFT held. Another UP must not confirm.
    harness.settle(chord(2, false));
    harness.settle(chord(6, true));
    harness.settle(chord(4, false));
    assert(harness.controller.pendingSong() == 3);
    assert(harness.count(&SelectionEvents::readyToConfirm) == 0);
    assert(harness.count(&SelectionEvents::confirmed) == 0);
    assert(harness.controller.waitingForRelease());
    harness.settle({});
    assert(harness.count(&SelectionEvents::readyToConfirm) == 1);

    // Changing all selection bits after capture must not change the candidate.
    harness.settle(chord(7, false));
    harness.settle(chord(7, true));
    assert(harness.count(&SelectionEvents::confirmed) == 1);
    assert(harness.controller.pendingSong() == 3);
    harness.settle(chord(1, false));
    harness.advance(1100);
    assert(harness.count(&SelectionEvents::captured) == 1);
    assert(harness.count(&SelectionEvents::pauseResume) == 0);
    assert(harness.volumeEvents() == 0);
    harness.settle({});
}

void confirmationWindowBeginsAfterLastRelease() {
    Harness harness(false);
    harness.settle(chord(5, false));
    harness.settle(chord(5, true));
    harness.advance(6000);
    assert(harness.count(&SelectionEvents::timedOut) == 0);
    harness.settle(chord(1, false));
    harness.advance(6000);
    assert(harness.count(&SelectionEvents::timedOut) == 0);
    assert(harness.count(&SelectionEvents::readyToConfirm) == 0);
    harness.settle({});
    assert(harness.count(&SelectionEvents::readyToConfirm) == 1);
    harness.afterReady(4999);
    assert(harness.count(&SelectionEvents::timedOut) == 0);
    harness.advance(1);
    assert(harness.count(&SelectionEvents::timedOut) == 1);
}

void shortZeroAndLongPause() {
    Harness shortPress(true);
    shortPress.settle(chord(0, true));
    shortPress.advance(200);
    assert(shortPress.count(&SelectionEvents::captured) == 0);
    shortPress.settle({});
    assert(shortPress.count(&SelectionEvents::captured) == 1);
    assert(shortPress.controller.pendingSong() == 0);
    assert(shortPress.count(&SelectionEvents::readyToConfirm) == 1);
    assert(shortPress.count(&SelectionEvents::pauseResume) == 0);

    Harness longPress(true);
    longPress.settle(chord(0, true));
    longPress.advance(1100);
    assert(longPress.count(&SelectionEvents::pauseResume) == 0);
    longPress.settle({});
    assert(longPress.count(&SelectionEvents::pauseResume) == 1);
    assert(longPress.count(&SelectionEvents::captured) == 0);
    assert(longPress.count(&SelectionEvents::readyToConfirm) == 0);
    longPress.advance(100);
    assert(longPress.count(&SelectionEvents::pauseResume) == 1);
    assert(!longPress.controller.selecting());

    Harness idleLong(false);
    idleLong.settle(chord(0, true));
    idleLong.advance(1100);
    idleLong.settle({});
    assert(idleLong.count(&SelectionEvents::captured) == 1);
    assert(idleLong.count(&SelectionEvents::readyToConfirm) == 1);
    assert(idleLong.count(&SelectionEvents::pauseResume) == 0);

    // Completion during a held pause gesture must not pause or select at idle.
    Harness finishedDuringHold(true);
    finishedDuringHold.settle(chord(0, true));
    finishedDuringHold.advance(1100);
    finishedDuringHold.playbackControlsEnabled = false;
    finishedDuringHold.settle({});
    assert(finishedDuringHold.count(&SelectionEvents::pauseResume) == 0);
    assert(finishedDuringHold.count(&SelectionEvents::captured) == 0);
    assert(finishedDuringHold.count(&SelectionEvents::readyToConfirm) == 0);
}

void zeroSnapshotSurvivesLaterBitChanges() {
    Harness harness(true);
    harness.settle(chord(0, true));
    harness.settle(chord(7, true));
    harness.settle(chord(7, false));
    assert(harness.count(&SelectionEvents::captured) == 1);
    assert(harness.controller.pendingSong() == 0);
    assert(harness.count(&SelectionEvents::readyToConfirm) == 0);
    assert(harness.volumeEvents() == 0);
    harness.settle({});
    assert(harness.count(&SelectionEvents::readyToConfirm) == 1);
}

void timeoutBoundariesAndWrap() {
    for (uint32_t start : {0U, UINT32_MAX - 200U}) {
        Harness beforeBoundary(false, start);
        captureAndRelease(beforeBoundary, 4);
        beforeBoundary.afterReady(4974);
        beforeBoundary.set(chord(0, true));
        beforeBoundary.advance(25);
        assert(beforeBoundary.count(&SelectionEvents::confirmed) == 1);
        assert(beforeBoundary.count(&SelectionEvents::timedOut) == 0);

        Harness atBoundary(true, start);
        captureAndRelease(atBoundary, 6);
        atBoundary.afterReady(4975);
        atBoundary.set(chord(0, true));
        atBoundary.advance(25);
        assert(atBoundary.count(&SelectionEvents::confirmed) == 0);
        assert(atBoundary.count(&SelectionEvents::timedOut) == 1);
        // The UP gesture that crossed the deadline is consumed until release.
        atBoundary.advance(1100);
        assert(atBoundary.count(&SelectionEvents::captured) == 1);
        assert(atBoundary.count(&SelectionEvents::pauseResume) == 0);
        atBoundary.settle({});
        atBoundary.settle(chord(1, false));
        atBoundary.settle(chord(1, true));
        assert(atBoundary.count(&SelectionEvents::captured) == 2);
        assert(atBoundary.controller.pendingSong() == 1);

        Harness crossed(false, start);
        captureAndRelease(crossed, 2);
        crossed.afterReady(5001);
        assert(crossed.count(&SelectionEvents::timedOut) == 1);
        crossed.settle(chord(0, true));
        // With no gesture held at expiration, the next press is a fresh capture.
        assert(crossed.count(&SelectionEvents::captured) == 2);
        assert(crossed.controller.pendingSong() == 0);
        assert(crossed.count(&SelectionEvents::confirmed) == 0);
    }
}

void standaloneVolumeAndChordCancellation() {
    Harness harness(true);
    harness.settle(chord(2, false));
    assert(harness.volumeEvents() == 0);
    harness.advance(100);
    harness.settle({});
    assert(harness.volume() == -1);
    assert(harness.volumeEvents() == 1);
    harness.settle(chord(1, false));
    harness.settle({});
    assert(harness.volume() == 0);
    assert(harness.volumeEvents() == 2);

    Harness canceled(true);
    canceled.settle(chord(1, false));
    canceled.set(chord(1, true));
    canceled.advance(10); // Even a raw UP bounce cancels a volume tap.
    canceled.set(chord(1, false));
    canceled.advance(30);
    canceled.settle({});
    assert(canceled.volumeEvents() == 0);
    assert(canceled.count(&SelectionEvents::captured) == 0);

    Harness selection(true);
    captureAndRelease(selection, 3);
    selection.settle(chord(2, false));
    selection.settle({});
    selection.settle(chord(1, false));
    selection.settle({});
    assert(selection.volumeEvents() == 0);
    assert(selection.controller.pendingSong() == 3);

    Harness idle(false);
    idle.settle(chord(1, false));
    idle.settle({});
    assert(idle.volumeEvents() == 0);
}

void debounceAndDuplicatePrevention() {
    Harness harness(false);
    for (unsigned bounce = 0; bounce < 3; ++bounce) {
        harness.set(chord(0, true));
        harness.advance(10);
        harness.set({});
        harness.advance(10);
    }
    assert(harness.count(&SelectionEvents::captured) == 0);
    harness.settle(chord(0, true));
    harness.advance(2000);
    assert(harness.count(&SelectionEvents::captured) == 1);
    harness.settle({});
    assert(harness.count(&SelectionEvents::readyToConfirm) == 1);
    harness.set(chord(0, true));
    harness.advance(10);
    harness.settle({});
    assert(harness.count(&SelectionEvents::confirmed) == 0);
    harness.settle(chord(0, true));
    harness.advance(1100);
    harness.settle({});
    harness.advance(100);
    assert(harness.count(&SelectionEvents::confirmed) == 1);
    assert(harness.count(&SelectionEvents::captured) == 1);
    assert(harness.count(&SelectionEvents::pauseResume) == 0);
}

} // namespace

int main() {
    allEightSongs();
    allButtonsMustReleaseAndCandidateStaysFrozen();
    confirmationWindowBeginsAfterLastRelease();
    shortZeroAndLongPause();
    zeroSnapshotSurvivesLaterBitChanges();
    timeoutBoundariesAndWrap();
    standaloneVolumeAndChordCancellation();
    debounceAndDuplicatePrevention();
    std::cout << "Song selection host tests passed\n";
}
