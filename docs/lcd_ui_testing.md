# LCD/UI physical-board checks

These checks are for the RT-Spark board with its existing 240x240 ST7789V3 LCD, ES8388 output, four buttons and PF6 potentiometer. The user physically verified the LCD redesign and existing playback/selection features before potentiometer integration. The checks below are regressions for the new volume source; their integrated board results remain pending. A successful PlatformIO build or host test does not establish display appearance, button responsiveness or audio continuity on the board.

Flash the built firmware using the project's existing upload method. Connect the existing speaker/headphones and open the UART monitor at **115200 baud, 8 data bits, no parity, 1 stop bit**. Start with all four buttons released and the potentiometer near minimum. Reset obtains the volume from PF6 rather than forcing 50; if no valid startup ADC sample is available, the requested volume remains zero until a valid sample is obtained.

The controls remain:

| Button | Selection bit | Playback action |
| --- | --- | --- |
| DOWN | Bit 2 | Part of a selection chord |
| LEFT | Bit 1 | Selection bit; standalone taps do not change volume |
| RIGHT | Bit 0 | Selection bit; standalone taps do not change volume |
| UP | Capture / confirm | Alone, held at least 1 second and then released: pause / resume |

The potentiometer controls volume through PF6 / ADC3 channel 4. Its accepted 0-100 value drives both the codec setting and LCD number/bar. Keep the knob still when testing whether buttons leave volume unchanged.

Press buttons long enough to pass the existing 25 ms debounce. For a nonzero selection, hold the desired bit buttons before pressing UP. Release **all four buttons** after capture, then press UP again before the five-second confirmation window expires. A short UP-alone gesture during playback selects `000`; a long UP-alone gesture pauses/resumes on release.

## The 14 requested checks

1. **Startup screen.** Reset the board with every button released. Look for cyan `RT-SPARK`, white `MP3 PLAYER` and yellow `INITIALIZING` on black. Check that the orientation and colors remain correct and no content extends past the square display. This initialization screen may be brief; a phone recording can help inspect it. No extra startup hold was added.

2. **Audio Ready screen.** On the same reset, expect green `AUDIO READY` when UART reports `ES8388 DETECTED` and `AUDIO OUTPUT INITIALIZED`. The existing approximately one-second startup test tone is retained, and Audio Ready remains visible through it. Afterwards the idle `SELECT SONG` preview should appear. If detection or initialization reports failure, expect `INIT FAILED` rather than Audio Ready; record the UART result and stop audio-dependent checks until the hardware problem is resolved.

3. **000 selection.** From idle, leave DOWN/LEFT/RIGHT released. Check `000`, `SONG 1`, `FUR ELISE` and `BEETHOVEN`. Press UP once. Keep it held briefly: the captured value must remain `000`, with `RELEASE ALL` and no countdown. Release UP, then press UP again within five seconds. Expect UART `SONG CONFIRMED`, binary `000`, and playback of song 1. During playback, a short UP-alone press/release must also select `000`, without pausing. Release all before confirming again.

4. **011 selection.** Hold LEFT and RIGHT, leaving DOWN released, then press UP. From idle the live boxes should show `011`; after capture, expect `SONG 4`, `TURKISH MARCH` and `MOZART`. Release one button at a time. The captured boxes must stay `011`, and `RELEASE ALL` must remain until every button is released. Confirm with the second UP press. Expect song 4 and unchanged volume: the LEFT/RIGHT chord must not produce volume steps.

5. **111 selection.** Hold DOWN, LEFT and RIGHT, then press UP. Check `111`, `SONG 8`, `SYMPHONY NO 40` and `MOZART`. Release all and confirm within five seconds. Expect binary `111` in UART and song 8 on the playing screen. Keep the confirmation UP held for more than one second before releasing it. This consumed confirmation gesture must not pause playback or capture another selection.

6. **Confirmation countdown.** Capture a choice and keep any captured button held for at least six seconds. Check that no countdown starts during `RELEASE ALL` and the choice is not cancelled. Release the final button. The action should become `UP CONFIRM`, with `5 SEC`, then `4 SEC`, `3 SEC`, `2 SEC`, `1 SEC`. Only the countdown region should change each second; the title and bit boxes should stay stable. Confirm while a positive second remains. The display must not grant an additional second beyond the controller's existing deadline.

7. **Timeout.** While a song is playing, capture a different choice, release all and do not confirm. After the five-second window, expect red `CANCELLED`, `SELECTION TIMEOUT`, `NO SONG CHANGED` and `RETURNING TO PLAYER`. The notice lasts approximately one second, then the original playing screen returns. Audio should continue throughout selection and timeout; the pending song must not start. Repeat while paused: return to the original paused song with audio still paused. Repeat from idle: return to the live selection preview. Also capture near the end of a playing song and let it finish before timeout; the return must be idle rather than restarting the finished song. UART should report the corresponding timeout and song-complete events. A new valid input may supersede the timeout notice.

8. **Now Playing screen.** Confirm a song and inspect the title, composer, green `PLAY`, song number, three-bit identifier, volume number/bar and control hints. Compare song metadata with UART. Test song 4 or another wrapped title: both lines should fit, with composer and footer separated from the title. Source spellings are preserved, including `PACHEBELBEL` for song 2. LCD text uses `PCT`, not an unsupported percent glyph.

9. **Potentiometer volume down.** Begin playing at a comfortable knob setting. Rotate toward minimum. Expect accepted UART and LCD values to decrease together and the bar to shrink. The large overlay should update for meaningful accepted changes. At the physical minimum, check that volume reaches 0, with an empty bar and no drawing corruption. The playing song must not change. Tap LEFT alone with the knob still: it must not lower volume or open a volume overlay.

10. **Potentiometer volume up.** Rotate gradually toward maximum. Expect accepted UART and LCD values to increase together, reaching 100 with a full bar and no overflow at the physical maximum. The four-sample average and two-percentage-point deadband may suppress or delay small changes; the display need not show every intermediate integer. Tap RIGHT alone with the knob still: it must not raise volume or open a volume overlay. Return to a comfortable setting for the remaining checks. These numbers are the same control setting passed to `AudioCodec_SetVolume()`; they are not measured loudness or codec readback. Record endpoint raw readings using [the potentiometer guide](potentiometer_volume_testing.md).

11. **Large volume overlay.** Make meaningful knob adjustments less than 1.5 seconds apart. Check the large number, `PCT`, bar, `TURN POT FOR VOLUME` hint and correct `PLAY - SONG N` footer. Each accepted adjustment should update the number/bar without clearing the whole screen and restart the approximately 1.5-second overlay timer. Hold the knob still: the overlay should expire rather than repeatedly restart from small ADC noise. After the final adjustment, expect return to the current playing screen. Repeat while paused: the footer and return screen must remain paused. While an overlay is visible, begin a valid selection chord and capture it: the selection screen must take priority immediately, and overlay expiry must not overwrite confirmation. Move the knob during confirmation; it must not replace the selection screen. Pause/resume during an overlay should similarly show the resulting playback state. If the song ends during the overlay, expect idle rather than a stale playing screen.

12. **Pause screen.** While playing with all other buttons released, hold UP alone for at least one second, then release it. Expect UART `PLAYER PAUSED`, red `PAUSED`, the same song title/composer and volume, and `HOLD UP RESUME`. The sound should stop. Check that changed heading/state/hint regions contain no remnants of longer prior text. Rotate the potentiometer while paused: UART/LCD volume must update, the overlay may appear, and playback must remain paused. LEFT/RIGHT taps must neither change volume nor resume audio.

13. **Resume screen.** From paused, hold UP alone for at least one second and release. Expect UART `PLAYER RESUMED`, cyan `NOW PLAYING`, green `PLAY` and `HOLD UP PAUSE`. The same song should continue from its paused position, retaining the chosen volume. Repeat pause/resume once more to check that partial label updates leave no stray pixels.

14. **Continuous audio during LCD updates.** Use a sufficiently long song and listen while repeatedly adjusting volume, allowing overlays to expire, capturing another selection and letting its countdown time out. There should be no new gaps, restarts or clicks associated with LCD redraws; normal rests in the melody and deliberate pause/volume-zero operations are expected. Keep UART visible for playback errors. Watch for full-screen flashing during countdown or repeated volume updates, and check that capture/confirm remains responsive. Major screen transitions may redraw the screen. Let one song run to completion and verify UART `SONG COMPLETE` and return to idle.

## Remaining selection regressions

All eight binary selections were physically verified before this LCD change. Recheck the five combinations beyond the required `000`, `011` and `111` above:

| Bits | Hold before the first UP press | Song number | Expected title |
| --- | --- | --- | --- |
| 001 | RIGHT | 2 | CANON IN D |
| 010 | LEFT | 3 | MINUET IN G MAJOR |
| 100 | DOWN | 5 | NOCTURNE IN E FLAT |
| 101 | DOWN + RIGHT | 6 | WALTZ NO 2 |
| 110 | DOWN + LEFT | 7 | NOCTURNE IN C SHARP |

For each, verify the idle preview, captured/frozen bits, release gate, second-UP confirmation, title and UART binary value. Include at least one selection started while playing and one while paused. Nonzero UP chords must select even when held longer than one second, without triggering pause/resume. LEFT/RIGHT used as chord bits must not change volume. A pending choice must never replace the current song until confirmation succeeds. The existing controller's internal `volumeSteps` events are ignored by the player; selection and long-UP handling remain enabled.

## Scope and reliability

The LCD hardware initialization, screen layouts and existing playback/selection behavior are retained. Volume hints are now `POT VOLUME` and `TURN POT FOR VOLUME`; the previous button-volume hints are retired. UI timers add no new `HAL_Delay()` calls. The pre-existing startup test tone and main-loop delay remain. LCD writes are synchronous over the existing bus, so board observation is still required to establish responsiveness and audible continuity with ADC sampling.

`AUDIO READY` reflects the existing codec detection/initialization return values. Some codec writes do not expose individual acknowledgement results. Likewise, the volume API has no status/readback: the display shows the exact requested setting, not proof that every hardware write succeeded.

LEFT/RIGHT volume was an earlier **modified / alternative implementation**. The final volume source is the external potentiometer required by the laboratory, read through PF6; its integration still needs the physical checks in [potentiometer_volume_testing.md](potentiometer_volume_testing.md). This does not complete the remaining laboratory work: FreeRTOS tasks, LCD mutex, RGB state indication and dedicated USER_BUTTON handling remain separate. No LED, RTOS, MP3/WAV decoder or audio transport changes belong to this integration. Record pass/fail observations rather than marking new checks complete from a successful build alone.
