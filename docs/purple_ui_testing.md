# Purple PlayerUI: presentation-only development and board verification

The user authorized this purple/lavender redesign after physically verifying
Stage 5. It is a presentation-only development variant of the same three-task
player. The physically verified `rtos_stage5_player` remains the recovery
baseline; the new appearance requires its own physical verification.

The older FreeRTOS guides record the completed migration and the earlier hold
on firmware changes. This later authorization permits the isolated visual
redesign only. Dedicated USER_BUTTON support remains pending hardware, Stage 6
is not started, and no FreeRTOS software confirmation timer is added.

## Scope and preserved behavior

`rtos_stage5_purple_ui` inherits the verified Stage-5 build settings and adds
`PLAYER_UI_PURPLE_THEME` to select the presentation renderer. The existing UI
API, pure presentation observer, task entry points and mutex architecture are
reused.

| Owner | Unchanged responsibility |
| --- | --- |
| `PollButtonsTask`, priority 3, 1,024 stack words | Buttons, player state, transport/service and state publication |
| `AdjustVolumeTask`, priority 2, 768 stack words | PF6 ADC/filter, accepted volume and codec volume writes |
| `UpdateLcdLedsTask`, priority 1, 1,024 stack words | All runtime PlayerUI/LCD and external LED updates |

Startup graphics still occur before the scheduler. Runtime rendering remains
in the sole UI task under the existing LCD mutex. No drawing is moved into
control, volume or interrupt code. Audio, song data, GPIOs, physical LEDs,
selection semantics, accepted-volume calculation and UART behavior remain the
Stage-5 behavior.

The logical volume mapping remains 0–100. The user's physically observed
potentiometer range was approximately **8–100%**; this redesign does not
calibrate that range. The graphic bar represents the accepted published volume,
not a second value computed from raw ADC samples.

## Visual design

The renderer uses a dark-purple background, deep-violet cards, lavender/pink
accents and bright text. Named RGB565 constants define the palette. Music notes,
play/pause and speaker symbols use lightweight drawing primitives; no external
bitmap assets or font library are needed. The existing scalable 5x7 font is
reused, including bounded formatting, title wrapping and clipping.

The new drawing helpers are `rgb565`, clipped `fillRect`, `roundedPanel`,
`DrawMusicNote`, `DrawPlayIcon`, `DrawPauseIcon`, `DrawSpeakerIcon`,
`DrawSparkle` and `DrawPercent`. The stepped-corner panels use nonoverlapping
bands to avoid painting their interior repeatedly. `DrawPercent` supplies a
primitive percent symbol because the existing font has no percent glyph.
Eight named constants define background, panel, purple, lavender, pink, primary
text, secondary text and muted colors. No dynamic allocation was introduced.

| Screen | Intended presentation and unchanged behavior |
| --- | --- |
| Startup / audio status | Music-player branding and purple decoration; ready/failure remains truthful; the existing 600 ms status behavior is retained |
| Idle / ready to choose | Welcoming selection instruction and three separate binary cards; live bits retain DOWN/LEFT/RIGHT meaning |
| Captured / release | Frozen captured bits and candidate title with a clear release-buttons instruction; no countdown before release-all |
| Confirmation | Candidate title, UP instruction, remaining seconds and a display-only countdown bar; the existing five-second controller deadline remains authoritative |
| Timeout | Clear cancellation notice, then the existing underlying screen returns after the one-second notice |
| Playing | Song title/composer, decorative music card, play indicator and accepted-volume bar |
| Paused | Same song layout with pause indicator; playback cursor and physical RED LED behavior are unchanged |
| Volume overlay | Speaker icon, prominent accepted percentage and lavender bar; expires approximately 1.5 seconds after the original volume event and restores the appropriate screen |

Physical BLUE means playing, RED means paused and GREEN means selecting. Purple
LCD graphics do not change or reinterpret those hardware LED colors.

Screen caches remain active: a complete screen redraw is reserved for a screen
or relevant content change. Countdown and volume changes update their regions.
Repeated identical observations must not redraw the entire 240x240 LCD every
20 ms. Raw ADC samples do not directly trigger overlays or redraws.

## Build, upload, monitor and recovery

Run from the project root in PowerShell:

```powershell
$purplePio = 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe'

& $purplePio run -e rtos_stage5_purple_ui
& $purplePio run -e rtos_stage5_purple_ui -t upload
& $purplePio device monitor -e rtos_stage5_purple_ui --baud 115200
```

Close an existing serial monitor before uploading. Reset after opening the
monitor to capture the verified Stage-5 startup and task messages. There is no
new migration stage or new application task. The coding agent does not upload
firmware during development.

To return to the physically verified Stage-5 appearance, close the monitor and
run:

```powershell
& $purplePio run -e rtos_stage5_player -t upload
& $purplePio device monitor -e rtos_stage5_player --baud 115200
```

## Development evidence

The purple ARM build and all six preceding environments build without warnings
or errors.

| Firmware | RAM bytes | Flash bytes |
| --- | ---: | ---: |
| Verified `rtos_stage5_player` | 48,392 | 46,056 |
| `rtos_stage5_purple_ui` | 48,392 | 47,344 |
| Presentation delta | **0** | **+1,288 (+2.80%)** |

All ten existing host suites pass unchanged with strict warnings treated as
errors. The new `tests/player_ui_purple_host.cpp` fixture passes **8 groups /
17,394 checks**, compiling both renderer sources with the theme selector
enabled. Checks include per-call geometry/glyph guards, all screens and titles,
clipping, cached redraw behavior, volume endpoints and retained timing; the
check count is not a count of simulated LCD pixels.

All **12** saved BIN/ELF SHA256 hashes for the six preceding environments match
after rebuilding. The existing Stage-5 recovery is therefore byte-for-byte
unchanged. Of 59 original source/include/script/test/config files compared,
**56 remain unchanged**. Only the three intended original files below differ.
Protected task entry points, kernel configuration, state store, presenter,
mutex implementations, ADC/filter, audio/DMA, selector, scores, physical LED
driver and low-level LCD driver are unchanged.

`UpdateLcdLedsTask` remains the only runtime LCD owner. Its existing
lock/render/unlock sequence is unchanged, as are priorities, stacks and
waiting. Software-framebuffer review of ten screens using the actual 37 font
glyphs found coherent layouts without overlap; this is a software review,
not physical verification. No firmware was uploaded.

Exactly six files were changed or added for this presentation work:

| File | Change |
| --- | --- |
| `platformio.ini` | Add the isolated presentation environment inheriting Stage 5 |
| `src/player_ui.cpp` | Add only a theme-exclusion guard and line-number directive; retain the original renderer body |
| `tests/README.md` | Add purple-renderer fixture build/run instructions |
| `src/player_ui_purple.cpp` | New guarded purple renderer with the same PlayerUI API |
| `tests/player_ui_purple_host.cpp` | New graphic/presentation host fixture |
| `docs/purple_ui_testing.md` | This scope, evidence, commands and board checklist |

Host tests validate program behavior and drawing calls. They do not establish
physical readability, LCD orientation/flicker, audio continuity or real task
headroom. Those observations remain pending the user's board test. Actual
stack high-water values must be recorded from that test, not inferred from the
unchanged allocation sizes.

## Physical verification: 21 checks

1. Startup branding and audio ready/failure presentation appear correctly;
   codec detection and the original startup/test tone behave as before.
2. Purple/lavender text, panels and bars are readable at the actual viewing
   angle; no content merges into the background.
3. Idle presentation is welcoming and concise, with live binary selection
   cards and the correct UP instruction.
4. All binary selections `000` through `111` retain their original candidate
   mapping and update the visual cards correctly.
5. The first UP press shows the captured song and frozen captured bits.
6. Release-all presentation remains until all selection buttons are released;
   holding any button does not start the confirmation countdown.
7. Confirmation shows the existing remaining-second progression and display
   bar; second-UP confirmation and five-second expiry work as before.
8. Timeout/cancel presentation lasts approximately one second and restores the
   correct idle/playing/paused background without restarting a song.
9. Playing presentation shows the correct song/composer, play indicator and
   accepted volume without changing playback or completion behavior.
10. Paused presentation matches the playing layout, shows the pause indicator,
    and long-UP resume continues at the retained position.
11. A genuine accepted-volume change shows the overlay, extends it only for a
    newer accepted change, and restores the correct screen after about 1.5 s.
12. Long titles wrap cleanly within 240x240; text never overwrites surrounding
    cards, icons or volume controls.
13. Check all eight names: Fur Elise, Canon in D, Minuet in G Major, Turkish
    March, Nocturne in E Flat, Waltz No 2, Nocturne in C Sharp and Symphony
    No 40. Existing musical data and composer strings remain unchanged.
14. Physical BLUE/RED/GREEN still indicate playing/paused/selecting, with the
    unchanged startup LED test and all-off idle behavior.
15. PF6 volume behaves as before while playing and paused; the observed
    approximately 8–100% range is preserved without recalibration.
16. DOWN/LEFT/RIGHT still mean bit2/bit1/bit0, UP still captures/confirms, and
    long-UP still pauses/resumes. No USER_BUTTON hardware support is present.
17. Stable screens do not visibly flicker or repeatedly clear. Fast selection,
    countdown and volume changes remain visually coherent.
18. Audio has no new stutter, interruption or glitch while screens change,
    confirmation counts down or the potentiometer moves rapidly.
19. Repeated song changes, timeouts and overlays do not freeze the player or
    scheduler.
20. No HardFault, FATAL, spontaneous reset or reboot loop occurs.
21. Capture at least two `RTOS HEALTH` reports during mixed use. Task wake and
    revision counters progress, stack headroom stays positive and heap remains
    stable. Record actual readings instead of copying earlier-stage values.

Stop after the presentation build/test work. USER_BUTTON hardware remains
pending, the verified confirmation timer is unchanged and no Stage-6 work is
authorized by this visual task.
