# PF6 potentiometer volume: board verification

The final volume input is the physically connected potentiometer on **PF6 / ADC3 / ADC channel 4**. The user previously tested this hardware connection successfully. That earlier test does not establish that the newly integrated player has passed the checks below. Integrated physical results and measured raw endpoints are **PENDING** until UART/LCD/audio observations are supplied.

## Current behavior

Software verification passed: PlatformIO `black_f407zg` builds; existing
song-selection, player UI and audio DMA host suites pass; the new filter suite
passes eight groups; the ADC suite passes 15 failure/timing scenarios with 401
checks. The final build uses 15004 bytes RAM and 34788 bytes flash. Protected
selection/song/LCD-driver files match the pre-integration baseline; codec changes
are limited to initial volume handling, and UI changes are two volume hints.

ADC3 uses 12-bit, right-aligned results, so its configured maximum is 4095. PF6 is analog with no pull. Acquisition is software-triggered, uses PCLK/4 and a 480-cycle sample time, checks HAL results and polls with a short bounded timeout. ADC DMA is not used. Runtime sampling follows the existing main loop at approximately 20 ms intervals; synchronous UART/LCD work can lengthen that interval.

Only valid samples enter the four-sample moving average. The first valid filtered reading establishes the accepted value; later accepted changes use a two-percentage-point deadband, with endpoint handling that permits true 0 and 100. Mapping uses rounded integer arithmetic:

```cpp
volume = (filteredRaw * 100u + adcMaximum / 2u) / adcMaximum;
```

Raw and logical values are clamped to their configured ranges. One accepted logical volume drives both `AudioCodec_SetVolume()` and the LCD number/bar/overlay. A failed conversion preserves the previous valid volume instead of treating the failure as zero. ES8388 volume writes have no readback/status API; matching requested UART/LCD values still require an audible check.

Startup samples the potentiometer before audio initialization. The codec's initial logical volume is zero; after successful codec initialization, a valid potentiometer value is applied before the retained one-second test tone. There is no forced 50% assignment. If no startup conversion succeeds, volume remains zero while runtime sampling continues trying.

LEFT/RIGHT no longer adjust volume. They remain bit 1 / bit 0 of the verified selection controller, whose legacy `volumeSteps` output is ignored. Long-UP pause/resume remains enabled. Accepted knob changes can update volume in idle, selection, playing and paused states. The large 1.5-second overlay is for Playing/Paused; selection/confirmation screens retain priority.

## Setup and UART observations

Use the existing PlatformIO/ST-Link upload method and keep the already tested potentiometer wiring. Connect the existing speaker/headphones and open UART1 at **115200 baud, 8 data bits, no parity, 1 stop bit**. Release all four buttons and put the knob near minimum before reset. Use a comfortable speaker/headphone level while checking maximum settings.

Expect startup information identifying the potentiometer, PF6, ADC3 channel 4 and 12-bit ADC configuration. Accepted volume changes print a raw reading, a filtered reading, the accepted `Volume: N PCT`, and observed raw minimum/maximum since reset. The raw reading and filtered reading can differ during rotation because mapping uses the average. Logs are emitted for accepted changes, not every approximately 20 ms sample; stable volume should not flood UART. ADC failures may produce occasional error information.

The new volume telemetry uses the following format; angle-bracket fields are placeholders, not physical measurements:

```text
ADC: <raw> / 4095  FILTERED: <averaged>
Volume: <accepted> PCT
OBSERVED MIN: <minimum>  MAX: <maximum>
```

The existing selection sequence is: hold the desired DOWN/LEFT/RIGHT bits, press UP once to capture, release **all four buttons**, then press UP again within five seconds. The five-second deadline begins after release-all. During playing/paused operation, UP alone must be shorter than one second to select `000`; hold UP alone for at least one second and release for pause/resume.

## Exact 20-step physical procedure

1. **Reset near minimum.** Keep all buttons released, set the potentiometer near its physical minimum, reset, and save the complete startup UART output. Check the existing Startup and Audio Ready screens and subsequent idle screen.
2. **Check safe startup volume.** Confirm the startup requested volume is low/zero and the retained startup tone is correspondingly quiet/silent. It must follow the knob rather than an arbitrary 50. If ADC startup fails, expect zero and an error report rather than a fabricated valid reading.
3. **Rotate gradually upward.** Move the knob slowly through its range. Give each position roughly 100 ms to settle, allowing the four-sample filter to catch up. Small rotations may be suppressed by the deadband.
4. **Check UART increases.** Observe accepted `Volume: N PCT` values increasing with the knob. Compare raw and filtered readings; do not expect every raw sample or every integer volume to be printed.
5. **Check LCD number.** Select a song if needed to show the playing volume display. Continue moving the knob and check that each accepted LCD number matches the UART volume. The display must never use a separate volume value.
6. **Check LCD bar.** Confirm that the volume bar grows with accepted volume and stays within its existing rectangle. Check the compact `POT VOLUME` / `TURN POT FOR VOLUME` hints and supported `PCT` label.
7. **Check actual loudness.** While a song plays, slowly vary the knob over a comfortable range. Confirm louder/quieter output follows the accepted value without restarting the score. UART/LCD agreement alone is not proof of successful codec writes.
8. **Rotate toward minimum.** Turn downward slowly while playing. Confirm accepted UART/LCD values and the bar decrease together.
9. **Check zero endpoint.** Hold the knob at minimum long enough to settle. Expect logical 0, an empty bar and silent output. Save the accepted endpoint log and its observed raw range. If zero cannot be reached, record the actual smallest value and raw readings rather than declaring this check passed.
10. **Rotate toward maximum.** Turn upward through the range. Confirm the filtered/accepted setting follows and the LCD remains stable while updating.
11. **Check 100 endpoint.** Hold at maximum to settle. Expect logical 100 and a full bar without overflow. Save the accepted endpoint log and raw range. If 100 cannot be reached, record the actual largest value and raw readings. Return to a comfortable listening level before continuing.
12. **Check stationary jitter.** Hold the knob still at a middle setting for at least ten seconds. After the last accepted adjustment, the large overlay should expire in approximately 1.5 seconds and remain closed. Tiny ADC noise must not repeatedly open it or create screen flashing. Deliberate subsequent motion should update the same overlay and extend it after each accepted change.
13. **Play Fur Elise (`000`).** Leave DOWN/LEFT/RIGHT released, short-press UP, release all, and press UP again within five seconds. Confirm song 1, `FUR ELISE`, Beethoven, matching UART binary `000`, and audible playback.
14. **Play Turkish March (`011`).** Hold LEFT and RIGHT, leave DOWN released, press UP, release all four buttons, and confirm with UP. Check song 4, `TURKISH MARCH`, Mozart, and binary `011`. With the knob fixed, those button presses must not change volume.
15. **Play Symphony No 40 (`111`).** Hold DOWN, LEFT and RIGHT, press UP, release all, and confirm. Check song 8, `SYMPHONY NO 40`, Mozart, and binary `111`. Hold the confirmation UP for more than one second before release: that consumed gesture must not pause or capture a new selection.
16. **Complete selection regression.** Test the other five rows in the table below, so all eight selections have been rechecked after this integration. Check independent active-low bit readings, debounce, frozen captured bits, release-all gating, second-UP confirmation and the five-second timeout. Keep a captured button held for more than five seconds: timeout must not start until all are released. Let a later selection time out while playing, then while paused: restore the prior state without changing song. Rotate the knob during confirmation: accepted volume may change, but the selection screen and frozen bits must retain priority. Follow [song_selection_testing.md](song_selection_testing.md) for detailed presses. With the knob still, standalone LEFT/RIGHT taps must not change volume.
17. **Pause playback.** While playing with the other buttons released, hold UP alone for at least one second, then release. Expect `PLAYER PAUSED`, the existing Paused screen, silence and the same song metadata/position.
18. **Adjust while paused.** Rotate the knob enough to make a meaningful accepted change. Expect matching UART and LCD values, the paused overlay/footer and bar update, and continued silence. After roughly 1.5 seconds without another accepted change, return to the Paused screen.
19. **Resume with the new volume.** Hold UP alone for at least one second and release. Expect `PLAYER RESUMED`; the same score continues from its retained position using the new potentiometer-selected setting. It must not revert to its pre-pause volume or 50.
20. **Listen for integration glitches.** Use a sufficiently long song. Rotate the knob, let overlays expire, capture selections, allow countdown/timeout, and pause/resume. Listen for gaps, clicks or unintended restarts associated with ADC sampling or LCD updates. Normal melody rests, deliberate pause and volume zero are expected. Confirm responsive buttons, stable countdown/title regions and normal song completion/return to idle. Save any error logs and describe the exact gesture that caused a mismatch.

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

## Raw endpoint measurements and result record

Accepted-change logs include raw minimum/maximum observed since that reset. Those statistics continue updating silently between accepted changes, so a printed range is a snapshot. A later settled endpoint may not produce a new log when the accepted volume is already 0 or 100. Do not infer that the last accepted-change raw reading is the exact settled endpoint.

For settled endpoint readings without raw-sample spam, keep the knob fully at minimum, reset with all buttons released, and save the startup raw/filtered/range values from the startup sampling. Repeat with the knob fully at maximum, saving a separate log. These are separate sessions; note which one each reading belongs to. Afterwards reset near minimum and restore a comfortable listening level before further playback. Compare these samples with the low/high values seen during the full-range sweep. Noise may make one sample differ from the average; record both raw range and filtered value.

| Observation | Actual result |
| --- | --- |
| Integrated firmware physically tested | PENDING |
| Minimum reset: startup raw minimum / maximum | PENDING / PENDING |
| Minimum reset: startup filtered raw / accepted volume | PENDING / PENDING |
| Maximum reset: startup raw minimum / maximum | PENDING / PENDING |
| Maximum reset: startup filtered raw / accepted volume | PENDING / PENDING |
| Full-range sweep: observed raw minimum / maximum | PENDING / PENDING |
| Physical minimum reaches logical 0 | PENDING |
| Physical maximum reaches logical 100 | PENDING |
| Stationary knob: no recurring overlay | PENDING |
| All eight selections, release gate and timeout | PENDING |
| Paused adjustment and resume at new setting | PENDING |
| Audible continuity during ADC/LCD work | PENDING |

No physical raw minimum/maximum has been measured by Codex. Share the logs or fill these values after board testing so the endpoint range can be assessed. Calibration is not implemented in this change. If the settled physical range prevents logical 0 or 100, document the observed range first; do not disguise it as 0-4095 or silently alter the configured mapping.

## Laboratory scope and later FreeRTOS task

The external potentiometer is the final laboratory volume input. LEFT/RIGHT volume was an earlier modified/alternative development stage; archived `.cpp.txt` checkpoints remain historical. Successful integration and physical verification address the potentiometer-volume requirement, not every laboratory requirement.

ADC hardware acquisition is in `potentiometer_volume`, with filtering/change detection in `potentiometer_filter`. Later, the required `adjust_volume()` task can sample periodically, retain one accepted setting, apply codec changes and publish that setting safely to the LCD/RGB task before waiting. Shared codec access, including pause/resume mute writes, needs synchronization as part of that later integration. UI rendering stays separate from acquisition.

This change does not add external RGB LEDs, the onboard LED matrix, FreeRTOS, an LCD mutex, USER_BUTTON changes, sleep mode or calibration. I2S, audio DMA, synthesis, DMA callbacks and song/pause architecture remain the verified implementation; the codec initialization change is limited to initial volume handling. LED/RTOS work waits until this potentiometer integration is physically verified.
