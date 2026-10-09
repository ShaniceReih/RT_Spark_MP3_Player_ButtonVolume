#ifndef SONG_SELECTION_H
#define SONG_SELECTION_H

#include <cstdint>

// Logical inputs: true means pressed, after conversion from active-low GPIO.
struct SelectionButtons
{
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;

    bool anyPressed() const { return up || down || left || right; }
    uint8_t songBits() const
    {
        return (down ? 4 : 0) | (left ? 2 : 0) | (right ? 1 : 0);
    }
};

struct SelectionEvents
{
    bool captured = false;
    bool readyToConfirm = false;
    bool confirmed = false;
    bool timedOut = false;
    bool pauseResume = false;
    int8_t volumeSteps = 0;
};

// Input ownership is independent of audio transport. No HAL or audio calls.
class SongSelectionInput
{
public:
    SelectionEvents update(uint32_t now, const SelectionButtons &raw,
                           bool playbackControlsEnabled)
    {
        SelectionButtons buttons;
        buttons.up = upFilter.update(now, raw.up);
        buttons.down = downFilter.update(now, raw.down);
        buttons.left = leftFilter.update(now, raw.left);
        buttons.right = rightFilter.update(now, raw.right);
        const bool upPressed = buttons.up && !previous.up;
        const bool upReleased = !buttons.up && previous.up;
        const bool allReleased = !buttons.anyPressed() && !raw.anyPressed();
        SelectionEvents events;

        // Expiry wins over an UP edge at or beyond the deadline.
        if (phase == CONFIRM && now - confirmationStart >= 5000)
        {
            phase = NORMAL;
            consumeUntilReleased = true;
            leftVolumeArmed = rightVolumeArmed = false;
            events.timedOut = true;
        }
        else if (consumeUntilReleased)
        {
            if (allReleased)
            {
                consumeUntilReleased = false;
            }
        }
        else if (phase == WAIT_RELEASE)
        {
            if (allReleased)
            {
                phase = CONFIRM;
                confirmationStart = now;
                events.readyToConfirm = true;
            }
        }
        else if (phase == CONFIRM)
        {
            if (upPressed)
            {
                phase = NORMAL;
                consumeUntilReleased = true;
                events.confirmed = true;
            }
        }
        else if (upPressed)
        {
            // Freeze all three independently read bits on the first UP edge.
            pending = buttons.songBits();
            upPressStart = now;
            leftVolumeArmed = rightVolumeArmed = false;
            if (playbackControlsEnabled && pending == 0)
            {
                // UP alone is the retained pause button during playback.
                // Its press-time snapshot is resolved on release: short=000,
                // long=pause/resume. Other chords never use this detector.
                normalUpGesture = true;
            }
            else
            {
                phase = WAIT_RELEASE;
                events.captured = true;
            }
        }
        else if (normalUpGesture)
        {
            if (upReleased)
            {
                normalUpGesture = false;
                if (now - upPressStart >= 1000)
                {
                    events.pauseResume = playbackControlsEnabled;
                    consumeUntilReleased = true;
                }
                else
                {
                    events.captured = true;
                    phase = allReleased ? CONFIRM : WAIT_RELEASE;
                    if (allReleased)
                    {
                        confirmationStart = now;
                        events.readyToConfirm = true;
                    }
                }
            }
        }
        else if (playbackControlsEnabled && !raw.up && !buttons.up)
        {
            // Commit standalone volume taps on release. UP cancels them so
            // preparing a binary selection cannot also change the volume.
            if (buttons.left && !previous.left) leftVolumeArmed = true;
            if (buttons.right && !previous.right) rightVolumeArmed = true;
            if (!buttons.left && previous.left)
            {
                if (leftVolumeArmed) --events.volumeSteps;
                leftVolumeArmed = false;
            }
            if (!buttons.right && previous.right)
            {
                if (rightVolumeArmed) ++events.volumeSteps;
                rightVolumeArmed = false;
            }
        }
        else
        {
            leftVolumeArmed = rightVolumeArmed = false;
        }

        previous = buttons;
        return events;
    }

    uint8_t pendingSong() const { return pending; }
    bool selecting() const { return phase != NORMAL; }
    bool waitingForRelease() const { return phase == WAIT_RELEASE; }

private:
    class ButtonFilter
    {
    public:
        bool update(uint32_t now, bool pressed)
        {
            if (pressed != candidate)
            {
                candidate = pressed;
                changedAt = now;
            }
            if (now - changedAt >= 25)
            {
                stable = candidate;
            }
            return stable;
        }

    private:
        bool candidate = false;
        bool stable = false;
        uint32_t changedAt = 0;
    };

    enum Phase { NORMAL, WAIT_RELEASE, CONFIRM };
    Phase phase = NORMAL;
    ButtonFilter upFilter, downFilter, leftFilter, rightFilter;
    SelectionButtons previous;
    uint8_t pending = 0;
    uint32_t upPressStart = 0;
    uint32_t confirmationStart = 0;
    bool normalUpGesture = false;
    bool consumeUntilReleased = false;
    bool leftVolumeArmed = false;
    bool rightVolumeArmed = false;
};

#endif
