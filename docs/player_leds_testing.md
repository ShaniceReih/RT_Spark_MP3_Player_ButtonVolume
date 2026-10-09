# External player-state LEDs: board verification

This stage adds the laboratory's external red, green and blue player-state LEDs. The user has already physically verified the PF6 potentiometer integration and the existing audio, selection and LCD features. The new LED implementation and its regressions below are **PENDING physical verification**; a successful build or host test does not establish real LED color, wiring, audible continuity or board behavior.

## Confirmed wiring and GPIO behavior

| LED | STM32 GPIO | Expansion connection | ON | OFF |
| --- | --- | --- | --- | --- |
| RED | PA8 | P2 pin 7 | HIGH | LOW |
| GREEN | PA1 | P2 pin 11 | HIGH | LOW |
| BLUE | PB14 | P2 pin 13 | HIGH | LOW |

Each branch is wired **GPIO → 330 ohm series resistor → LED anode**. Each LED cathode connects to the common **GND at P2 pin 6**. Use the confirmed external LEDs; the onboard LED matrix is not part of this implementation. A board power/debug indicator is not a player-state LED and may remain lit when all three external LEDs are off.

`PlayerLeds_Init()` configures only PA8, PA1 and PB14 as push-pull outputs, with no pull and low speed. Their output levels are preloaded LOW before changing those pins to output mode. Initialization ends with all three off. Other GPIO pins, including UART, PF6, LCD, buttons and audio pins, retain their existing configuration.

All writes for these three LEDs are contained in `src/player_leds.cpp`. Main uses `PlayerLeds_Init()` and `PlayerLeds_SetState()` rather than writing LED GPIOs directly. The module caches the requested state, so repeated requests for an unchanged state do not write the outputs again. Runtime state changes have no added delay, timer or DMA operation.

## State mapping and startup

| Player state | RED | GREEN | BLUE |
| --- | --- | --- | --- |
| IDLE / no active song | OFF | OFF | OFF |
| PLAYING | OFF | OFF | ON |
| PAUSED | ON | OFF | OFF |
| SELECTING / changing song | OFF | ON | OFF |

Selection takes priority from first-UP capture through release-all gating and second-UP confirmation. GREEN stays on even if the underlying song finishes during a pending selection. Confirmation starts the chosen song and switches to BLUE. Timeout restores BLUE for a still-playing song, RED for a paused song, or all-off when no song remains active. Stopped/failed playback outside selection uses all-off.

Potentiometer changes and the LCD volume overlay do not choose the LED state. BLUE remains on during playing-volume changes, RED during paused-volume changes, and GREEN during selection-volume changes. The state is handed to the LED module after the loop processes audio, input and potentiometer events and before the existing UI update.

Startup initializes UART and the LEDs, then performs the approved self-test before button/LCD/potentiometer/audio startup. It lights one external LED at a time for approximately 500 ms in this order:

```text
LED TEST: RED
LED TEST: GREEN
LED TEST: BLUE
LED TEST COMPLETE
```

All three LEDs finish off. This adds approximately 1.5 seconds before the normal Startup/Audio Ready screens and retained audio test tone. The startup self-test is blocking while audio DMA is inactive; normal runtime LED updates are non-blocking.

## Setup

Build with `pio run -e black_f407zg`, then use the existing PlatformIO/ST-Link upload method. Open UART1 at **115200 baud, 8 data bits, no parity, 1 stop bit**. Use the already verified speaker/headphones and potentiometer connections. Release all buttons and set the potentiometer near minimum before reset; choose a comfortable audible volume after startup.

Selection still uses DOWN = bit 2, LEFT = bit 1 and RIGHT = bit 0, with active-low GPIO converted to logical pressed bits. Hold the desired bits, press UP once, release **all four buttons**, then press UP again within five seconds. The deadline starts after release-all. UP alone held for at least one second and then released pauses/resumes; a short UP-alone gesture selects `000`. LEFT/RIGHT remain selection bits and do not control volume.

## Exact 15-step physical procedure

1. **Startup RED.** Reset with all buttons released. At UART `LED TEST: RED`, confirm only the external RED LED is on for approximately 500 ms. GREEN and BLUE must be off. Check that the light is physically red and is the LED wired to PA8 / P2 pin 7.
2. **Startup GREEN.** At `LED TEST: GREEN`, confirm RED turns off and only the external GREEN LED is on for approximately 500 ms. Check GREEN corresponds to PA1 / P2 pin 11.
3. **Startup BLUE.** At `LED TEST: BLUE`, confirm GREEN turns off and only the external BLUE LED is on for approximately 500 ms. Check BLUE corresponds to PB14 / P2 pin 13.
4. **All off after self-test.** At `LED TEST COMPLETE`, confirm all three external LEDs are off and remain off through the existing Startup/Audio Ready screens, retained test tone and idle song preview. Save startup UART output. An always-on board power/debug indicator is separate from these three LEDs.
5. **Start Fur Elise → BLUE.** After startup, set a comfortable potentiometer volume. Leave DOWN/LEFT/RIGHT released, short-press UP to capture `000`, release all, and press UP again within five seconds. GREEN should indicate the pending selection; confirmation must start song 1, Fur Elise, and leave only BLUE on. Check the existing Now Playing screen and matching UART title/binary.
6. **Pause → RED.** With the other buttons released, hold UP alone for at least one second, then release. At `PLAYER PAUSED`, expect BLUE off, RED on, GREEN off, the existing Paused screen and silence. The song/position must remain retained.
7. **Resume → BLUE.** Hold UP alone for at least one second and release. At `PLAYER RESUMED`, expect RED off, BLUE on, GREEN off and continuation of the same score from its paused position, using the current potentiometer volume.
8. **Begin selection → GREEN.** While the song plays, hold LEFT and RIGHT, leave DOWN released, then press UP to capture `011`. Expect BLUE off and GREEN on immediately when capture is processed. The captured candidate must be song 4, Turkish March; the old song must continue while the selection is pending.
9. **Release/confirmation wait stays GREEN.** Keep at least one captured button held for more than five seconds. GREEN must remain on, the LCD must show `RELEASE ALL`, and timeout must not start yet. Release all four buttons. GREEN must remain on during `UP CONFIRM` and its five-second countdown; proceed to confirmation before the window expires. Bit-button changes after capture must not change the frozen candidate.
10. **Confirm new song → BLUE.** Press UP again within the confirmation window. At `SONG CONFIRMED`, expect GREEN off, BLUE on, RED off and Turkish March starting from its beginning. Hold this confirmation UP for more than one second before release: the consumed gesture must not pause playback or capture again.
11. **Playing timeout restores BLUE.** Start this check early enough in a song that it will remain playing throughout the next five seconds. Capture another candidate and release all buttons. GREEN must indicate the full pending confirmation. Do not confirm; after the five-second timeout, expect BLUE restored, RED/GREEN off and the old song still playing from its ongoing position. The existing Timeout screen may remain visible briefly while the LED already reflects restored playback.
12. **Paused timeout restores RED.** Pause with long-UP/release, then capture a candidate and release all. GREEN must indicate the pending choice even though the old song is paused. Let the five-second window expire. Expect RED restored, BLUE/GREEN off, the old paused song retained and continued silence. Resume before the next playing check.
13. **Potentiometer does not change state color.** While playing, rotate the knob enough to trigger accepted volume changes and the 1.5-second overlay: BLUE must stay on. Pause and repeat: RED must stay on and playback must remain paused. Capture a choice from paused, release all, and rotate the knob during confirmation: GREEN must stay on, the selection screen must keep priority and the frozen bits must remain correct. Let it time out to RED, then resume to BLUE. With the knob still, standalone LEFT/RIGHT taps must not change volume or LED state.
14. **Natural completion → all off.** Confirm a song and let its complete stored score finish. At `AUDIO: SONG COMPLETE`, expect silence, return to idle and all three external LEDs off. No LED may remain on merely because the last screen had shown a volume overlay.
15. **Audio continuity during LED changes.** Start a sufficiently long song. Capture choices and let them time out, vary the potentiometer, allow volume overlays to expire, and pause/resume. Confirm that LED transitions add no audible gaps, clicks or unintended score restarts. Normal melody rests, deliberate pause, volume zero, confirmed song replacement and natural completion are expected. Check that all existing LCD screens, UART messages and button gestures remain responsive and correct.

## Additional selection and completion regressions

Recheck all eight binary rows after LED integration. For each row, confirm capture bits/title, release-all gate, second-UP confirmation, GREEN while pending and BLUE after a successful start. Use [song_selection_testing.md](song_selection_testing.md) for detailed gesture checks; preserve at least one selection begun while playing and one begun while paused.

| Binary | Hold before first UP | Song number | Expected title |
| --- | --- | --- | --- |
| 000 | None | 1 | Fur Elise |
| 001 | RIGHT | 2 | Canon In D |
| 010 | LEFT | 3 | Minuet in G major |
| 011 | LEFT + RIGHT | 4 | Turkish March |
| 100 | DOWN | 5 | Nocturne in E flat |
| 101 | DOWN + RIGHT | 6 | Waltz No. 2 |
| 110 | DOWN + LEFT | 7 | Nocturne in C sharp |
| 111 | DOWN + LEFT + RIGHT | 8 | Symphony No. 40 |

- **Timeout from idle:** With no song active, capture a candidate, release all and let the five-second window expire. Expect GREEN while pending, then all LEDs off and the idle preview. No song should start.
- **Old song ends while pending:** Near a song's natural end, capture a candidate and keep a captured button held. When the old audio finishes, GREEN must remain on because release/confirmation is still pending. Release all, then let confirmation time out. Expect all-off and idle, with no restart of the finished song. Repeat if necessary to ensure the old song actually completes before timeout.
- **Long nonzero chord:** Hold a nonzero chord with UP longer than one second. It must capture that chord and show GREEN rather than pause. Confirmation is still a separate UP press after release-all.
- **Confirmed replacement from paused:** Capture and confirm a different song while paused. GREEN must give way to BLUE as the new song starts from its beginning; RED must not remain on.
- **No volume-color coupling:** Repeat knob adjustments in all three active states and verify only the selected state color stays lit. The LCD overlay and its expiry must not choose a different LED color.

## Result record and scope

Software verification passed for this implementation: PlatformIO
`black_f407zg` builds successfully (15008 bytes RAM, 35260 bytes flash). Existing
song-selection, player UI, potentiometer-filter, potentiometer-ADC and audio DMA
host suites all pass. The filter suite covers eight groups, and the ADC suite
covers 15 failure/timing scenarios with 401 checks. Baseline comparisons confirm
the 13 protected existing files, including audio/DMA and PF6 ADC configuration,
are byte-identical; `main.cpp` changes consist only of LED integration additions.
An independent code review also checked GPIO masks, polarity, safe initialization,
cached output updates and selection priority. These are software results; the
physical results below remain pending.

| Observation | Physical result |
| --- | --- |
| RED / PA8 startup test | PENDING |
| GREEN / PA1 startup test | PENDING |
| BLUE / PB14 startup test | PENDING |
| All-off after self-test and in idle | PENDING |
| Playing BLUE / Paused RED / Selecting GREEN | PENDING |
| Release gate and confirmation keep GREEN | PENDING |
| Timeout restores Playing / Paused / Idle | PENDING |
| Old-song completion during selection, then timeout to idle | PENDING |
| All eight binary selections preserved | PENDING |
| PF6 volume and overlays preserve LED state | PENDING |
| Natural completion all-off | PENDING |
| Audible continuity, LCD and UART regressions | PENDING |

The external LED arrangement implements the laboratory's specified red/green/blue hardware and state indication. It is not an onboard-LED substitution. That addresses the LED hardware/state portion only; it does not establish completion of the whole laboratory.

The LED module is structured so `PlayerLeds_SetState()` can later be called from the required `update_lcd_leds_thread()` using the same player-state snapshot as the display. Selection priority should remain part of that mapping. No FreeRTOS task, LCD mutex, onboard LED matrix, USER_BUTTON change or sleep mode is added here. The verified PF6 ADC3 channel 4 configuration, ES8388 volume path, I2S/DMA, synthesis, song data, selection controller and low-level LCD code are preserved.

Record build/host verification separately from the physical results above. Run the PlatformIO build and the existing selection, UI and potentiometer host checks for this stage; they do not replace the board procedure. Save relevant UART output and describe any mismatch with its exact gesture before beginning the later RTOS work.
