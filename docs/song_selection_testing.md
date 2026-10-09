# Song-selection board checks

These are expected results to verify on the physical RT-Spark board after PF6 potentiometer integration. All eight selections and the existing selection/pause behavior were physically verified by the user before this integration; the checks below confirm that the new volume source preserves them. Compilation and host checks do not establish new board results. Synthesized I2S/DMA playback is retained; FreeRTOS and RGB integration remain separate work.

Build with `pio run -e black_f407zg`, then upload through the existing PlatformIO/ST-Link setup. Open UART1 at **115200 baud**, set the potentiometer near minimum, reset the board, and wait for initialization and the startup test tone to finish before pressing buttons. Set a comfortable audible volume with the potentiometer for song checks. LEFT and RIGHT remain selection bits and no longer control volume.

DOWN, LEFT and RIGHT are bits 2, 1 and 0. A pressed button is logical **1**; a released button is logical **0**, despite the active-low GPIO wiring. Hold the desired bits, press UP to capture them, release **all four buttons**, then press UP again within five seconds to confirm. There is no separate UP press to enter selection. The five-second window begins after all buttons have been released. Use deliberate taps and wait for `ALL BUTTONS RELEASED` on UART and `UP CONFIRM` on the LCD before the confirmation press; existing UART/LCD updates take time between button polls.

During normal playing or paused operation, UP alone must be a short press (less than one second) to select `000`. Holding UP alone for at least one second and releasing it pauses/resumes instead. A nonzero selection chord uses UP to capture regardless of its duration. A confirmation press commits on the press; holding that press must never pause the selected song.

## All eight selections

For each row, hold the listed buttons, press UP, release everything, and press UP again within five seconds. Compare the capture and confirmation messages, LCD song number/title, and actual audio. The song order and spellings below come from the active `songs[8]` table and `include/song_def.h`.

Every capture must print `SONG SELECTION CAPTURED`, independent logical DOWN/LEFT/RIGHT values, the binary value, the zero-based song index, the one-based song number, and the song title. Every confirmation must print `SONG CONFIRMED`, the same binary value, `Now playing:` with that title, and `AUDIO: STARTING SONG (CONTINUOUS PCM/DMA)`.

| Binary | Buttons held before UP | Actual song | Expected UART result |
| --- | --- | --- | --- |
| 000 | None | Song 1: Fur Elise - Beethoven | Capture `Binary: 000`, index 0, number 1; confirm `Binary: 000`, matching `Now playing:` title |
| 001 | RIGHT | Song 2: Canon In D - Pachebelbel | Capture `Binary: 001`, index 1, number 2; confirm `Binary: 001`, matching `Now playing:` title |
| 010 | LEFT | Song 3: Minuet in G major - Bach | Capture `Binary: 010`, index 2, number 3; confirm `Binary: 010`, matching `Now playing:` title |
| 011 | LEFT + RIGHT | Song 4: Turkish March - Mozart | Capture `Binary: 011`, index 3, number 4; confirm `Binary: 011`, matching `Now playing:` title |
| 100 | DOWN | Song 5: Nocturne in E flat -Chopin | Capture `Binary: 100`, index 4, number 5; confirm `Binary: 100`, matching `Now playing:` title |
| 101 | DOWN + RIGHT | Song 6: Waltz No. 2 - Shostakovich | Capture `Binary: 101`, index 5, number 6; confirm `Binary: 101`, matching `Now playing:` title |
| 110 | DOWN + LEFT | Song 7: Nocturne in C sharp - Chopin | Capture `Binary: 110`, index 6, number 7; confirm `Binary: 110`, matching `Now playing:` title |
| 111 | DOWN + LEFT + RIGHT | Song 8: Symphony No. 40 - Mozart | Capture `Binary: 111`, index 7, number 8; confirm `Binary: 111`, matching `Now playing:` title |

For `011`, capture must report DOWN = 0, LEFT = 1, RIGHT = 1. The small selection screen should show `SELECTED 011`, `SONG 4`, the supplied title, and `RELEASE ALL`; after release it should show `UP CONFIRM`. Song-title spacing may reflect the two strings stored in the supplied song data.

## Exactly what to press first

Reset and wait for startup before each procedure to check it independently from idle:

1. **000:** Keep DOWN, LEFT and RIGHT released. Short-press UP, release UP, then press UP again within five seconds. Expect Song 1, Fur Elise - Beethoven.
2. **001:** Press and hold RIGHT first. While RIGHT remains held, press UP. Release RIGHT and UP, then press UP again within five seconds. Expect Song 2, Canon In D - Pachebelbel.
3. **010:** Press and hold LEFT first. While LEFT remains held, press UP. Release LEFT and UP, then press UP again within five seconds. Expect Song 3, Minuet in G major - Bach.
4. **011:** Press and hold LEFT and RIGHT together first. While both remain held, press UP. Release LEFT, RIGHT and UP, then press UP again within five seconds. Expect Song 4, Turkish March - Mozart.

## State and conflict checks

- **Release gate:** Capture `011`, then release UP while keeping LEFT and RIGHT held for more than five seconds. Expect `RELEASE ALL` and no confirmation timeout yet. A further UP press while either bit is still held must not confirm. Release everything, then make a distinct UP press within five seconds; Song 4 should start.
- **Frozen candidate:** Capture `011`, release everything, then change the bit buttons during confirmation. UP must still confirm `011`; the pending candidate must not follow later bit changes.
- **Timeout:** While a song plays, capture another selection, release everything, and wait more than five seconds without UP. Expect `SELECTION TIMEOUT` and `Song change cancelled.`; the original song keeps its position and continues. Repeat while paused: it stays paused. Repeat near the old song's end: if it completes during selection, timeout returns to idle.
- **Restart and replacement:** Confirm the current song again; it restarts from its beginning. Confirm a different song while paused; the selected song starts from its beginning in the playing state.
- **UP duration:** During normal playback, short UP alone followed by confirmation selects `000`. Long UP alone, released after at least one second, pauses/resumes without selecting. Hold UP with a nonzero chord for more than one second: it captures the chord and does not pause. Hold a confirmation UP for more than one second, then release: it starts the selected song once and does not pause it.
- **Volume:** Keep the potentiometer still during normal playing and paused operation, then tap LEFT and RIGHT separately and release. Expect no volume change or volume overlay from either tap. Hold a selection chord containing LEFT/RIGHT before pressing UP; capture and confirmation must not change volume. During confirmation, bit-button changes must not adjust volume. Rotate the potentiometer during selection: accepted codec/UART volume may change, but the captured bits must remain frozen, the selection screen must retain priority, and confirmation/timeout must preserve the accepted setting. The verified controller is unchanged; only its legacy `volumeSteps` events are ignored by main.
- **Playback regression:** Let a score pass its twentieth event, pause/resume it, and change volume while it plays. Expect uninterrupted DMA playback and completion only after the full stored score ends. UART should log meaningful changes, without printing bit values every polling cycle.

Record the observed UART lines and any mismatch for each binary row before moving to FreeRTOS integration. Use [potentiometer_volume_testing.md](potentiometer_volume_testing.md) for the exact 20-step volume/audio procedure and measured raw endpoint results. The earlier LEFT/RIGHT volume stage and archived `.cpp.txt` checkpoints are historical references, not the current controls.
