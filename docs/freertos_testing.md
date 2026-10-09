# FreeRTOS verification: Stage 5 player and completed Stages 1/2/3/4

**Stages 1 through 5 physical verification PASSED and were approved complete by
the user. `rtos_stage5_player` is the frozen verified baseline. Dedicated
USER_BUTTON work is PENDING HARDWARE. No Stage 6 or further firmware changes are
authorized.**

The physically verified current baseline is `rtos_stage5_player`. `PollButtonsTask` owns controls/player
state and transport/service; `AdjustVolumeTask` remains the sole runtime
PF6/filter/accepted-volume owner; `UpdateLcdLedsTask` is now the sole runtime
PlayerUI/LCD and external LED owner. No audio task is added. The physically
verified Stage-5 image must remain unchanged; Stage 4 remains an older rollback
image.

## Completed Stage-5 physical verification and current hold

The user confirmed that the final control, volume and LCD/LED tasks are stable
on hardware and that the three-task migration is complete. The overall Stage-5
physical verification PASSED. No item-by-item test log or exact numerical
Stage-5 high-water/heap readings were supplied; none are inferred from earlier
stages.

The fifth external pushbutton is not yet available. Documented future USER_BUTTON
hardware/action: PA0 / P2 pin 12, active-low, external 10 kOhm pull-up to 3.3V,
button to GND, owned by `PollButtonsTask`, STOP / REPLAY FROM BEGINNING. Long-UP
remains PAUSE / RESUME. This hardware-compliance item is pending only; no
implementation, merge, upload or testing is authorized until the user physically
has the button. Preserve the Stage-5 source, build configuration and firmware
artifacts. Do not start Stage 6 or add a software confirmation timer; retain the
verified five-second HAL-tick timeout.

## Completed Stage-4 physical evidence

The user physically verified and approved Stage 4 complete. They confirmed
exclusive runtime PF6 ownership, working filtering/attenuation, volume changes
while playing/paused/selecting, retention of paused volume after resume, rapid
potentiometer movement, changing songs while adjusting volume, and pause/resume
while adjusting volume. Public `VOL`/`VOL_REV` updated correctly. Audio continued
and no codec deadlock, HardFault, reboot or freeze was observed. Heap stayed
stable and all task stacks retained positive headroom.

The observed physical potentiometer range was approximately **8% to 100%**.
The user explicitly accepts this as a pass and prohibits calibration during the
RTOS migration. Stage 5 must preserve this observed behavior and the existing
0-100 logical mapping; reaching a physical 0% is not a Stage-5 pass criterion.
No exact Stage-4 raw endpoint, stack or heap readings were supplied, so none are
invented. The historical Stage-4 procedure remains below as recovery evidence.

## Stage-5 build, upload and monitor

Run from the project root in PowerShell:

```powershell
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage5_player
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage5_player -t upload
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' device monitor -e rtos_stage5_player --baud 115200
```

Close any existing monitor first. Reset after opening the monitor to capture
startup; Ctrl+C closes it before another upload. If necessary, list ports using
`platformio.exe device list` and append `--port COM<number>` with the actual port.

Rollback to the physically verified Stage-4 player:

```powershell
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage4_player
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage4_player -t upload
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' device monitor -e rtos_stage4_player --baud 115200
```

All five older environments remain available: `black_f407zg`,
`rtos_stage1_smoke`, `rtos_stage2_player`, `rtos_stage3_player` and
`rtos_stage4_player`. The coding agent performs no firmware upload during
development; the commands above are for the user's board test.

## Stage-5 development verification

All six final firmware environments build without warnings or errors. All eight
existing host suites and both new Stage-5 suites pass without warnings. The
Stage-5 player uses **48,392 bytes RAM and 46,056 bytes Flash**. These are
recorded development results. The user subsequently completed physical Stage-5
verification. No firmware was uploaded by the coding agent, and no exact
Stage-5 numerical high-water or latency readings were supplied with that pass.

| Final environment | RAM | Flash |
| --- | ---: | ---: |
| `black_f407zg` recovery player | 15,008 bytes | 35,260 bytes |
| `rtos_stage1_smoke` diagnostic | 43,692 bytes | 24,284 bytes |
| `rtos_stage2_player` recovery | 48,120 bytes | 40,156 bytes |
| `rtos_stage3_player` recovery | 48,292 bytes | 43,360 bytes |
| `rtos_stage4_player` physically verified rollback | 48,376 bytes | 44,584 bytes |
| `rtos_stage5_player` | 48,392 bytes | 46,056 bytes |

Both `firmware.bin` **and** `firmware.elf` for all five older environments are
byte-for-byte identical to their saved pre-Stage-5 baseline: **10 of 10 artifact
hashes match**. The Stage-4 rollback is therefore the exact physically verified
image, rather than a rebuilt approximation.

| Native verification | Result |
| --- | --- |
| Audio DMA | PASS; original driver/songs and audio completion/error coverage. |
| Song selection | PASS; original controller and all selector/deadline/gesture coverage. |
| Player UI | PASS; 8 groups with the existing renderer. |
| Potentiometer filter | PASS; 8 groups with unchanged mapping/deadband. |
| ADC acquisition | PASS; 15 scenarios / 401 checks. |
| Stage-3 state snapshot | PASS; 9 groups / 196 checks. |
| Stage-4 field merge/revisions | PASS; 13 groups / 426 checks. |
| Stage-4 codec control/driver coverage | PASS; 6 groups / 1,424 checks. |
| Stage-5 pure presentation | PASS; 32 groups / 409 checks under C++11 and C++17. |
| Stage-5 formatting guard | PASS; 9 groups / 176 checks with warnings treated as errors. |

The pure presentation tests cover every requested mapping/transient invariant:
playing/paused/selection and LED precedence, timeout/background restoration,
accepted-volume revisions without duplicate triggers, multiple revisions,
overlay expiry/interruption, all LED states, per-kind retained event consumption,
delayed/superseded events, release-all timestamp reconstruction, and sequence/tick
wraparound. They invoke no STM32 LCD hardware. The formatting fixture verifies
output, boundaries, pre-scheduler behavior, nested scheduler suspension and ISR
rejection; it does not measure real LCD latency or scheduler response time.

Exact Stage-5 file changes:

| File | Change |
| --- | --- |
| `platformio.ini` | Add `rtos_stage5_player`, inheriting Stage 4, and the Stage-5-only `snprintf` link wrapper. |
| `src/main.cpp` | Remove runtime UI/LED paths only in Stage 5; retain control, snapshot/event publication and startup bootstrap; initialize LCD synchronization before tasks. |
| `src/player_tasks_stage4.cpp` | One exclusion guard selects the dedicated Stage-5 task implementation; the Stage-4 recovery body remains intact. |
| `tests/state_stubs/task.h` | Test-only declarations for scheduler suspension/resumption. |
| `tests/README.md` | Add native presentation/format-guard build and run instructions. |
| `docs/freertos_migration.md`, `docs/freertos_testing.md` | Record Stage-4 pass, Stage-5 ownership/verification, commands, rollback and physical checks. |
| `include/player_presentation.h`, `src/player_presentation.cpp` | New pure snapshot-to-view/LED/actions observer and private revision/transient state. |
| `src/player_tasks_stage5.cpp` | New final three-task implementation, real periodic presentation and expanded low-rate health diagnostics. |
| `src/lcd_control.h/.cpp` | New one-mutex checked creation, UI-owner binding and lock/unlock assertions. |
| `src/format_guard.cpp` | New Stage-5-only bounded formatting serialization without newlib configuration or allocation hooks. |
| `tests/player_presentation_host.cpp`, `tests/format_guard_host.cpp` | New native presentation and formatting fixtures. |

The audio driver, codec-control implementation, state store, ADC/filter,
controller, song data, low-level LCD/LED drivers, `player_ui.cpp`, kernel config,
build script and tick/exception support remain unchanged. Stage-4 codec locking
is reused exactly; Stage 5 adds no transport, synthesis, completion or DMA edits.
Exactly seven existing files changed and eight new files were added; all other
51 existing files in the baseline comparison remain byte-identical.

Independent source and compiled-code checks confirm exactly three application
tasks with checked creation and the approved priorities/stacks, one state mutex,
one codec mutex and one LCD mutex. The compiled control loop has no runtime
PlayerUI/LCD/LED, ADC/filter or `AudioCodec_SetVolume()` calls. The volume loop
retains its Stage-4 body and performs no UI/LED/button work. The compiled UI
loop copies one snapshot, runs the pure observer, locks only for UI hooks and
`PlayerUI_Update()`, unlocks, writes LED GPIOs, and blocks using
`vTaskDelayUntil()`. No state/codec/LCD locks are nested or held across a task
delay. The existing codec lock scope is unchanged.

ELF/vector checks retain one effective SysTick handler and correct SVC/PendSV
bindings. DMA1 Stream5/channel 0 remains priority 5 with syscall threshold 6;
its ISR/refill callbacks contain no FreeRTOS, formatting, LCD, PlayerUI or LED
calls. Kernel configuration, port, heap allocator, FPU flags and clock tree
are unchanged. Software timers, tickless idle/idle sleep and newlib hooks remain
disabled; no calibration, song edit or USER_BUTTON implementation was added.

| Application task | Priority | Allocated stack | Stage-5 runtime work |
| --- | ---: | ---: | --- |
| `PollButtonsTask` | 3 | 1024 words / 4096 bytes | Button/controller, authoritative player/selection state, transport/`Audio_Service()`, completion, public publication and UART; waits 20 ms. No runtime UI, LED, ADC/filter or codec volume write. |
| `AdjustVolumeTask` | 2 | 768 words / 3072 bytes | Runtime PF6 acquisition/filter, accepted volume, codec writes and public volume/revision; waits 20 ms. No UI, LED or button work. |
| `UpdateLcdLedsTask` | 1 | 1024 words / 4096 bytes | Snapshot-driven runtime UI, LCD and LEDs; approximately 20 ms periodic waits. No controller/player mutation, ADC, codec or audio service. |

Stack allocations and priorities remain unchanged; idle is priority 0. Exact
Stage-5 stack high-water values were not supplied with the completed physical
verification; readings from a previously waiting UI shell must not be substituted.

### Runtime LCD mutex and presentation boundary

`LcdControl_Init()` creates one checked LCD mutex after the unchanged hardware
bootstrap and before task execution. `LcdControl_BindOwner()` binds the runtime
UI task; lock/unlock entry assertions reject ISR or non-owner access. Each UI
iteration obtains one complete state snapshot, releases the state mutex, derives
its presentation plan, then takes the LCD mutex for the synchronous PlayerUI
hooks and render call. It releases the LCD mutex before external LED GPIO writes
and its periodic wait. The task does not wait for another mutex while holding
the LCD lock. State, codec and LCD mutexes are never nested.

`PlayerPresentationObserver::Observe(snapshot, now)` returns a pure
`PlayerPresentationFrame`: existing `PlayerUiView`, LED state, ordered transient
actions and diagnostic revisions. Its requested screen decision does not
override the existing PlayerUI startup/audio-ready hold cache.

Pre-scheduler startup LCD screens and LED self-test remain in bootstrap/main.
After scheduling, the UI task is the only runtime PlayerUI/LCD/LED owner. All
PlayerUI mutable caches are private by ownership. The existing fonts, layouts,
bar rendering and partial-update behavior remain unchanged.

Selection/release/confirmation presentation takes priority over background
playing or paused state and maps to GREEN. Otherwise playing maps to BLUE,
paused maps to RED and idle maps to all off. Pins/polarity stay active-high RED
PA8, GREEN PA1 and BLUE PB14. No onboard matrix is used.

Retained per-kind event revisions and original timestamps prevent duplicate
transient triggers and permit delayed UI observation without replaying obsolete
state. New accepted volume revisions alone trigger an eligible overlay; ordinary
ADC samples do not. Overlay age is measured from the accepted-change timestamp,
not delayed render time. Selection retains priority and idle retains its existing
screen. Confirmation uses the published release-all timestamp; countdown is
display only, with the unchanged button controller enforcing the five-second
deadline. Timeout notice age comes from its published timestamp and restores
the current underlying state after approximately one second.

### Formatting safety without infrastructure changes

The active UI task and the button task both perform existing bounded formatting.
Only `rtos_stage5_player` links `-Wl,--wrap=snprintf`:
`__wrap_snprintf()` runs `std::vsnprintf()` with nested-safe scheduler
suspension/resumption around **one formatting call**. Before the scheduler,
there is no suspension. It does not suspend scheduling around a complete render
or UART transmission, introduce another mutex, alter newlib configuration, or
add custom malloc hooks. Interrupts remain enabled, including the priority-5
audio DMA interrupt; existing HAL/kernel tick integration remains intact.
This serializes the current formatting call sites and is not a general contract
for arbitrary future concurrent C-library use.

### Stage-5 UART and health checks

Capture these actual task-start strings once, along with the original startup
instructions and codec/LED messages:

```text
FREERTOS PLAYER MODE
SystemCoreClock: <actual clock> Hz
TASKS CREATED: BUTTONS=3/1024w VOLUME=2/768w LCD/LEDS=1/1024w
<existing operating instructions>
Scheduler starting...
POLL BUTTONS TASK STARTED
SCHEDULER RUNNING
ADJUST VOLUME TASK STARTED
STAGE 4: VOLUME TASK ACTIVE
UPDATE LCD/LEDS TASK STARTED
STAGE 5: LCD/LED TASK ACTIVE
STAGE 3: BUTTON/STATE OWNERSHIP ACTIVE
```

This is an implemented-format expectation, not a captured Stage-5 board log.
Task-start notifications serialize these messages. Do not expect or add
per-iteration LCD diagnostics. Existing ADC/accepted-volume diagnostics continue
to be handed to the button task for printing; the UI task does not print during
normal drawing.

About every 30 seconds, capture `STATE SNAPSHOT` and `RTOS HEALTH`. The latter
retains `BUTTON_STACK`, `VOLUME_STACK`, `LCD_STACK`, `VOLUME_WAKES`, `LCD_WAKES`,
`HEAP` and `MIN_HEAP`, and adds `UI_STATE_REV`/`UI_VOL_REV` after successful UI
processing. `LCD_STACK` now measures actual UI work and `LCD_WAKES`
counts real UI iterations, not one-second shell wakes. Both volume/UI counts
must progress between reports; exact counts depend on synchronous work and
preemption. Stack values are minimum unused **words**; heap values are **bytes**.
Observe at least two reports during playback, selections, overlays and knob
movement. Heap should settle, high-water marks must stay safely above zero, and
no FATAL, HardFault, frozen output or repeated boot banner is acceptable.

```text
RTOS HEALTH: BUTTON_STACK=<free words>w VOLUME_STACK=<free words>w LCD_STACK=<free words>w VOLUME_WAKES=<real volume cycles> LCD_WAKES=<real UI cycles> HEAP=<free bytes> MIN_HEAP=<minimum free bytes> UI_STATE_REV=<processed state revision> UI_VOL_REV=<processed volume revision>
```

The UI revisions may lag the producer by a render/wait interval during activity;
with a stable snapshot they should catch up. This is diagnostic progress, not a
second authority for controls or accepted volume.

## Stage-5 physical player checklist

The user approved Stage-5 physical verification complete overall. This table
retains all 51 requested checks as a reference procedure; individual result logs
were not separately supplied. Its cells describe that missing per-item record,
not a pending Stage-5 approval. DOWN=bit 2, LEFT=bit 1, RIGHT=bit 0; UP captures/confirms and
long-UP alone pauses/resumes. All buttons must be released before the five-second
confirmation window starts. No potentiometer calibration is part of this stage.

| # | Physical check | Expected result | Result |
| ---: | --- | --- | --- |
| 1 | Board boots normally | Existing startup sequence/LED test/instructions occur without unexpected reset. | Not separately logged |
| 2 | Startup LCD still works | Existing splash and audio-ready/failure bootstrap presentation remains correct. | Not separately logged |
| 3 | ES8388 is detected | Existing probe and codec initialization succeed. | Not separately logged |
| 4 | Test tone works | Startup tone uses the pre-scheduler potentiometer-seeded level. | Not separately logged |
| 5 | All three tasks start | All required task-start strings and `SCHEDULER RUNNING` appear once. | Not separately logged |
| 6 | Stage-5 ownership announced | `STAGE 5: LCD/LED TASK ACTIVE` appears once. | Not separately logged |
| 7 | LCD does not freeze | Screens continue responding throughout the complete workload. | Not separately logged |
| 8 | LCD does not flicker excessively | Existing partial updates remain effective; no unnecessary full redraw every 20 ms. | Not separately logged |
| 9 | Idle/ready screen correct | Verified idle/selection-ready presentation and all-off LED state remain correct. | Not separately logged |
| 10 | `000` selection screen correct | Fur Elise title, bits and captured candidate remain correct; short-UP alone captures 000. | Not separately logged |
| 11 | `001` selection screen correct | Canon in D title/bits/candidate match selector 001. | Not separately logged |
| 12 | `010` selection screen correct | Minuet in G title/bits/candidate match selector 010. | Not separately logged |
| 13 | `011` selection screen correct | Turkish March title/bits/candidate match selector 011. | Not separately logged |
| 14 | `100` selection screen correct | E-flat Nocturne title/bits/candidate match selector 100. | Not separately logged |
| 15 | `101` selection screen correct | Waltz No. 2 title/bits/candidate match selector 101. | Not separately logged |
| 16 | `110` selection screen correct | C-sharp Nocturne title/bits/candidate match selector 110, Song 7. | Not separately logged |
| 17 | `111` selection screen correct | Symphony No. 40 title/bits/candidate match selector 111, Song 8. | Not separately logged |
| 18 | RELEASE ALL screen works | Captured selection remains frozen; no confirmation countdown until all four buttons release. | Not separately logged |
| 19 | Confirmation countdown works | Existing five-to-one countdown advances from the published release-all time and does not extend eligibility. | Not separately logged |
| 20 | Five-second timeout notice works | Control task cancels at its existing deadline; LCD shows the existing approximately one-second notice. | Not separately logged |
| 21 | Timeout restores correct prior screen | Repeat from idle, playing and paused; correct background returns without starting expired candidate. | Not separately logged |
| 22 | Now Playing screen works | Existing song title/index, accepted volume/bar and hints remain correct. | Not separately logged |
| 23 | Paused screen works | Correct paused song/volume appears while audio stays paused. | Not separately logged |
| 24 | Volume overlay while playing | Real accepted volume changes display/update the existing overlay. | Not separately logged |
| 25 | Volume overlay while paused | Accepted changes display overlay without resuming playback. | Not separately logged |
| 26 | Volume overlay during selection where allowed | Preserve selection/confirmation priority; no forced overlay replaces those screens. Volume still updates. | Not separately logged |
| 27 | Overlay lasts approximately 1.5 seconds | Duration is from the last eligible accepted change; unchanged samples do not restart it. | Not separately logged |
| 28 | Overlay expiry restores underlying screen | Repeat playing, paused and a selection transition; true current screen returns. | Not separately logged |
| 29 | BLUE LED while playing | BLUE only during normal playback without selection. | Not separately logged |
| 30 | RED LED while paused | RED only during normal pause without selection. | Not separately logged |
| 31 | GREEN LED during selection | GREEN only in capture/release/confirmation, overriding background playback. | Not separately logged |
| 32 | BLUE after cancelling selection while playing | Existing song remains playing; BLUE returns after timeout/cancellation. | Not separately logged |
| 33 | RED after cancelling selection while paused | Existing song remains paused; RED returns after timeout/cancellation. | Not separately logged |
| 34 | LEDs do not flicker incorrectly | Only intended state transitions change LEDs; unchanged frames do not toggle them. | Not separately logged |
| 35 | Potentiometer still works | Stage-4 filtering/deadband/paused behavior and observed approximately 8-100% physical range remain; no calibration expected. | Not separately logged |
| 36 | Pause/resume still works | Long-UP release pauses/resumes without position reset; LCD and LED follow. | Not separately logged |
| 37 | Song changes still work | Capture/release/confirm changes to the correct song with current accepted volume. | Not separately logged |
| 38 | Song completion still works | Actual full-score EOF reports once and returns to idle/all off; intentional E-flat internal pause still continues its remaining music. | Not separately logged |
| 39 | Rapid buttons do not freeze LCD | UI remains responsive; capture, release gating, confirmation and timeout retain their existing semantics. | Not separately logged |
| 40 | Rapid potentiometer movement does not freeze LCD | Accepted changes update; no UART flood, UI freeze or lost true player state. | Not separately logged |
| 41 | Change song while overlay is active | Selection takes priority; confirmation/timeout produces correct song/background. | Not separately logged |
| 42 | Pause while overlay is active | Audio pauses, RED appears, and overlay expiry restores paused presentation. | Not separately logged |
| 43 | No LCD mutex deadlock | UI keeps progressing across all screen transitions and repeated selections. | Not separately logged |
| 44 | No codec deadlock | Volume/transport stay responsive under overlapping knob and button use. | Not separately logged |
| 45 | No LCD-task audio glitch | No new audio gap, click or restart while drawing/counting down/updating overlays. | Not separately logged |
| 46 | No HardFault | No observed fault through startup and the complete workload. | Not separately logged |
| 47 | No FATAL | No assert/stack/heap/scheduler-fatal message occurs. | Not separately logged |
| 48 | No reboot/freeze | No repeated boot banners, frozen player/audio or frozen task progress. | Not separately logged |
| 49 | RTOS HEALTH remains stable | All tasks retain positive headroom and volume/UI iteration counts advance across reports. | Not separately logged |
| 50 | LCD stack headroom safely above zero | Measure real UI minimum unused words after all screens/overlays; do not substitute shell headroom. | Not separately logged |
| 51 | Heap remains stable | Free/minimum heap settle with no unexplained continued decline. | Not separately logged |

Repeat selection cancellation from playing and paused, volume during
release/confirmation, overlay cancellation, song completion near selection,
rapid knob/buttons together, and a full E-flat Nocturne score if checking EOF.
Capture the boot transcript, at least two health reports, and any mismatch with
the current public volume/state. Do not diagnose the score's internal rest as a
restart without an actual preceding `AUDIO: SONG COMPLETE` message.

**Stage 5 is complete and its firmware remains frozen.** USER_BUTTON is a
pending hardware-compliance item only. Wait for the actual pushbutton before
implementation/testing; no software confirmation timer, Stage 6, sleep,
calibration, song change or other firmware work is authorized now.

## Completed Stage-3 physical evidence

The user approved Stage 3 complete after physically testing the player. The
apparent selector `100` Nocturne in E-flat issue was confirmed to be the score's
intentional internal rest followed by more music. No `AUDIO: SONG COMPLETE`
preceded the continuation. No song restart bug exists; no musical data or
completion logic was changed.

No item-by-item Stage-3 test log or exact Stage-3 stack/heap readings were
provided with this overall approval. Historical Stage-2 values below are not
Stage-3 or Stage-4 measurements. The Stage-3 procedure and development evidence
are retained later in this guide as the rollback record.

## Historical Stage-4 build, upload and monitor

Run from the project root in PowerShell:

```powershell
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage4_player
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage4_player -t upload
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' device monitor -e rtos_stage4_player --baud 115200
```

Close any existing monitor first. Reset after opening the monitor to capture
startup. Ctrl+C closes it before another upload. If necessary, list ports with
`platformio.exe device list` and append `--port COM<number>` using the actual
port.

Rollback to the physically verified Stage-3 player:

```powershell
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage3_player
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage3_player -t upload
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' device monitor -e rtos_stage3_player --baud 115200
```

`black_f407zg`, `rtos_stage1_smoke` and `rtos_stage2_player` also remain available.
No firmware upload is performed by the coding agent during development.

## Stage-4 development verification

All five final Stage-4 environments built successfully with no compiler/linker
warnings or errors. These are historical development results. The coding agent
did not upload firmware; the user subsequently passed Stage-4 physical
verification, as recorded at the start of this guide.

| Final environment | RAM | Flash |
| --- | ---: | ---: |
| `black_f407zg` recovery player | 15,008 bytes | 35,260 bytes |
| `rtos_stage1_smoke` diagnostic | 43,692 bytes | 24,284 bytes |
| `rtos_stage2_player` recovery | 48,120 bytes | 40,156 bytes |
| `rtos_stage3_player` physically verified rollback | 48,292 bytes | 43,360 bytes |
| `rtos_stage4_player` | 48,376 bytes | 44,584 bytes |

The six pre-existing native suites pass: audio DMA, selection, player UI,
potentiometer filter, ADC acquisition, and the Stage-3 state suite
(9 groups / 196 checks). Two new Stage-4 suites pass:

| New native harness | Result and coverage |
| --- | --- |
| `tests/player_state_stage4_host.cpp` | PASS, 13 groups / 426 checks: accepted-volume retention, stale control merge, independent updates, genuine-change revisions, validity/bounds, event ordering, complete copies and control-only timeout-notice dismissal. |
| `tests/codec_control_host.cpp` | PASS, 6 groups / 1,424 checks: actual lock wrapper and driver mute guard, checked creation, pre-scheduler no-op, ISR rejection, error release, prefill outside lock, atomic volume-register pair, refill while mutex held, pause/resume/stop and completion/error service. |

These native fixtures do not unit-test a real FreeRTOS mutex scheduler or
measure hardware I2C ACK/IRQ/audio latency. Native compile/run commands are in
[`tests/README.md`](../tests/README.md).

The 13 protected files other than `audio_codec.cpp` remain byte-identical, as do
the kernel configuration/build script and RTOS support files. The only audio
driver edits are a Stage-4-only include and short mute-transaction lock hook;
DMA/I2S setup, synthesized PCM callbacks and completion logic are unchanged.
The earlier task/state implementation files change only their Stage-4 exclusion
guards. The older environment definitions remain unchanged.

Independent source/ELF checks retain one effective SysTick handler, the verified
HAL/kernel tick integration and SVC/PendSV handlers, correct DMA vector, one
32 KiB `heap_4`, exactly three application task symbols and checked task creation.
The compiled control loop contains no ADC/filter calls or `AudioCodec_SetVolume()`.
The compiled volume task owns update/read, codec lock/write/unlock, separate
state publication and its delay. The LCD/LED shell only waits for one second.

ELF checks find exactly one codec mutex and one state mutex. Their scopes are
distinct, with no nested acquisition or mutex held across a delay. The compiled
audio ISR contains no codec/state/FreeRTOS calls; DMA priority remains 5.

All four rollback `firmware.bin` images are byte-for-byte identical to the saved
pre-Stage-4 baseline: `black_f407zg`, `rtos_stage1_smoke`, `rtos_stage2_player`
and `rtos_stage3_player`. Stage-3 recovery therefore retains the physically
verified firmware image. Older preprocessed header/body paths preserve their
outputs despite the new Stage-4 guards.

| Application task | Priority | Allocated stack | Stage-4 runtime work |
| --- | ---: | ---: | --- |
| `PollButtonsTask` | 3 | 1024 words / 4096 bytes | Controller/player state, audio transport/service, snapshots, temporary LCD/LED and UART ownership; waits 20 ms. |
| `AdjustVolumeTask` | 2 | 768 words / 3072 bytes | PF6 acquisition/filter, accepted volume, codec attenuation and volume publication; waits 20 ms. |
| `UpdateLcdLedsTask` | 1 | 1024 words / 4096 bytes | One startup message, then one-second waits and wake-counter increments; no LCD/LED work. |

The existing startup PF6 bootstrap/filter history and test-tone volume are
retained before scheduling. After scheduler startup there is exactly one
runtime ADC/filter/volume-register path, owned by the volume task. A failed
conversion retains the accepted value and preserves the existing retry policy.
Filter internals are not public snapshot fields.

One checked codec mutex protects the overlapping ES8388 foreground control-bus
transactions: the volume-register pair and transport mute/unmute. Long synthesis
and DMA-buffer preparation are outside the mutex. Any mute transaction reached
through `Audio_Service()` uses the same short bus guard, without holding a lock
around the whole service function. The audio ISR/callbacks never take a mutex or
call FreeRTOS. DMA1 Stream5/channel 0 remains at priority 5.

Codec and state locks are acquired separately; no nesting is needed. The state
store merges independent control and volume updates so control publication
cannot restore stale volume. Unchanged/deadband-rejected samples do not advance
the accepted-volume event; first-valid zero establishes validity without a false
change event. No mutex is held across ADC polling, LCD drawing, UART or delay.

LCD/UI/LED ownership remains solely in `PollButtonsTask`; there is no LCD mutex,
software timeout timer, event group, newlib hook, idle-sleep change or audio task.
The verified Stage-1 tick/exception support, FPU flags, kernel configuration and
interrupt thresholds are retained. `HAL_GetTick()` still governs the existing
25 ms debounce, one-second long-UP, five-second release-based confirmation,
one-second timeout notice and approximately 1.5-second volume overlay.

## Stage-4 UART and health checks

The task-start notification chain preserves one-time serialized messages. The
following is the implemented-format expectation, not a captured board log:

```text
FREERTOS PLAYER MODE
SystemCoreClock: <actual clock> Hz
TASKS CREATED: BUTTONS=3/1024w VOLUME=2/768w LCD/LEDS=1/1024w
<existing operating instructions>
Scheduler starting...
POLL BUTTONS TASK STARTED
SCHEDULER RUNNING
ADJUST VOLUME TASK STARTED
STAGE 4: VOLUME TASK ACTIVE
UPDATE LCD/LEDS TASK STARTED
STAGE 3: BUTTON/STATE OWNERSHIP ACTIVE
```

The notification chain produces the task-start order shown above. Each task-start
and ownership message appears once. The Stage-3 line identifies the retained
control/state owner, not a second task or a second player loop.

Meaningful accepted changes retain the existing ADC/volume UART format:

```text
ADC: <raw> / 4095  FILTERED: <averaged raw>
Volume: <accepted 0..100> PCT
OBSERVED MIN: <minimum raw>  MAX: <maximum raw>
```

The volume task sends copied diagnostic data to the button task for printing.
After startup the button task is the sole UART/formatting owner, avoiding a new
UART mutex or concurrent use of non-reentrant formatting. It does not sample
ADC or read filter-private globals to generate these messages. Intermediate
diagnostic samples may coalesce if presentation is slower than volume sampling;
accepted volume still updates independently.

About every 30 seconds, observe `STATE SNAPSHOT` and `RTOS HEALTH`. The snapshot
diagnostic includes accepted volume so it can be compared with the accepted
volume messages/LCD. `RTOS HEALTH` retains `BUTTON_STACK`, `VOLUME_STACK`,
`LCD_STACK`, `HEAP` and `MIN_HEAP`.

```text
STATE SNAPSHOT: REV=<publication revision> STATE=<IDLE/SELECTING/PLAYING/PAUSED> CURRENT_INDEX=<0..7> VALID=<0/1> PHASE=<0/1/2> EVENTS=<global event sequence> VOL=<accepted 0..100> VOL_REV=<accepted-change revision>
RTOS HEALTH: BUTTON_STACK=<free words>w VOLUME_STACK=<free words>w LCD_STACK=<free words>w VOLUME_WAKES=<real volume cycles> LCD_WAKES=<shell wake count> HEAP=<free bytes> MIN_HEAP=<minimum free bytes>
```

`VOL_REV` increases only for accepted logical changes; ordinary 20 ms samples and
deadband-rejected noise do not create an event. The button task reads a fresh
public volume copy after control publication and before drawing. Volume events
and presentation cancellation use the canonical shared event order, preserving
the original accepted-change timestamp. An eligible overlay clears a timeout
notice through the separate control-only state helper, so public state and the
rendered notice agree without republishing a stale volume.

The existing idle/selection presentation is intentionally retained; idle knob
changes update the public accepted value and codec level but do not introduce a
new idle overlay or change `player_ui.cpp`. The approximately 1.5-second overlay
continues in playing/paused, while selection screens retain presentation priority.

`VOLUME_WAKES` now counts real volume-task iterations at approximately 20 ms,
not one-second shell wakeups or accepted changes. `LCD_WAKES` remains the
one-second shell counter. Both must progress across reports. Stack headroom is
minimum unused **words**, heap is **bytes**. Keep allocations unchanged and
capture at least two reports under playback, selections and knob movement.
Headroom must stay positive, heap must settle, and no FATAL/HardFault/freeze or
repeated boot banner is acceptable. Do not invent expected Stage-4 heap or stack
measurements from earlier stages.

## Stage-4 physical player checklist

This is the historical Stage-4 procedure. Overall Stage-4 verification PASSED;
the user supplied the explicit observations recorded at the start of this
guide, rather than a completed row-by-row log. Original blank result cells below
are not an outstanding Stage-4 gate. The originally proposed physical zero
endpoint was superseded by the user's accepted approximately 8% minimum.
DOWN=bit 2, LEFT=bit 1, RIGHT=bit 0; UP captures/confirms, and long-UP alone
pauses/resumes. Release all four buttons before the five-second confirmation
window begins. LEFT/RIGHT remain binary inputs and do not adjust volume.

| # | Physical check | Expected result | Result |
| ---: | --- | --- | --- |
| 1 | Board boots normally | Verified startup screens/LED test/instructions remain correct. | Not separately logged; overall stage PASS |
| 2 | ES8388 detected | Codec detection and initialization succeed. | Not separately logged; overall stage PASS |
| 3 | Test tone works | Startup tone uses the potentiometer-seeded level. | Not separately logged; overall stage PASS |
| 4 | All three tasks start | Each required task-start message and scheduler-running message appears once. | Not separately logged; overall stage PASS |
| 5 | Stage-4 ownership announced | `STAGE 4: VOLUME TASK ACTIVE` appears once. | Not separately logged; overall stage PASS |
| 6 | `000` plays normally | Short-UP capture/release/confirm selects Fur Elise and continuous audio begins. | Not separately logged; overall stage PASS |
| 7 | Potentiometer minimum | Observed approximately 8%, accepted by the user; no calibration during RTOS migration. | PASS; user accepted observed range |
| 8 | Potentiometer middle | Accepted/LCD volume is approximately 50%. | Not separately logged; overall stage PASS |
| 9 | Potentiometer maximum | Accepted/LCD volume reaches approximately 100%. | Not separately logged; overall stage PASS |
| 10 | Smooth volume changes | Existing moving average/deadband remain effective; stationary knob creates no repeated false events. | Not separately logged; overall stage PASS |
| 11 | Volume overlay | Accepted changes show the existing overlay in playing/paused; idle keeps its existing screen while accepted volume updates publicly. | Not separately logged; overall stage PASS |
| 12 | Overlay duration | Last accepted eligible change starts approximately 1.5 seconds; unchanged samples do not restart it. | Not separately logged; overall stage PASS |
| 13 | Volume while playing | Sampling, display and ES8388 attenuation continue during playback. | Not separately logged; overall stage PASS |
| 14 | Volume while paused | Sampling, accepted value, overlay and codec attenuation still update without resuming audio. | Not separately logged; overall stage PASS |
| 15 | Resume retains paused volume | New paused level is used after long-UP resume. | Not separately logged; overall stage PASS |
| 16 | Volume during selection | Sampling continues during capture/release/confirmation; selection presentation retains priority. | Not separately logged; overall stage PASS |
| 17 | Rapid knob movement | System continues responding, with no frozen tasks or UART flood. | Not separately logged; overall stage PASS |
| 18 | Change song while moving knob | Confirmed song starts with current attenuation; no stale volume overwrite. | Not separately logged; overall stage PASS |
| 19 | Pause/resume while moving knob | State and volume remain correct under overlapping codec control. | Not separately logged; overall stage PASS |
| 20 | No codec deadlock | Controls and volume continue responding throughout the test. | Not separately logged; overall stage PASS |
| 21 | No I2C/codec error | No observed codec-related errors; audible response follows accepted volume. | Not separately logged; overall stage PASS |
| 22 | No mutex-related audio stutter | Continuous audio remains clean during volume and transport activity. | Not separately logged; overall stage PASS |
| 23 | BLUE playing LED | BLUE on, RED/GREEN off during normal playback. | Not separately logged; overall stage PASS |
| 24 | RED paused LED | RED on, BLUE/GREEN off during normal pause. | Not separately logged; overall stage PASS |
| 25 | GREEN selecting LED | GREEN on during selection, including selection over playing/paused background. | Not separately logged; overall stage PASS |
| 26 | LCD screens | Startup/ready, selection/release, confirmation, playing, paused and timeout remain correct. | Not separately logged; overall stage PASS |
| 27 | Selection timeout | Starts after release-all; cancels at existing five-second deadline, with no stale song start. | Not separately logged; overall stage PASS |
| 28 | All `000`-`111` selections | All eight confirmed titles play; `110` is Song 7 and `111` is Song 8. | Not separately logged; overall stage PASS |
| 29 | Song completion | Full score reports `AUDIO: SONG COMPLETE` once and returns to idle; `100` internal rest continues intentionally before actual EOF. | Not separately logged; overall stage PASS |
| 30 | Snapshot volume | `STATE SNAPSHOT` accepted volume matches the current accepted level; compare stable knob readings. | Not separately logged; overall stage PASS |
| 31 | RTOS health | Real volume iterations and LCD shell wake counts progress in successive reports. | Not separately logged; overall stage PASS |
| 32 | Stack headroom | Buttons, real volume and LCD shell high-water values stay positive. | Not separately logged; overall stage PASS |
| 33 | Stable heap | Free/minimum heap settle; no unexplained continued decline. | Not separately logged; overall stage PASS |
| 34 | No HardFault | No observed fault during startup and runtime workload. | Not separately logged; overall stage PASS |
| 35 | No FATAL | No fatal/assert/stack/heap message occurs. | Not separately logged; overall stage PASS |
| 36 | No reboot/freeze | No reboot loop, repeated startup banners or frozen audio/player/task output. | Not separately logged; overall stage PASS |

If ADC failure is observed or deliberately reproduced using a controlled test,
confirm that the last accepted volume remains valid, no forced zero occurs and
the existing retry interval is respected. Avoid assuming normal knob movement
alone exercises conversion failure; native ADC harness results cover that
logic separately.

Repeat transitions from idle, playing and paused, including a selection near
song completion. Capture UART boot and at least two health reports, accepted
volume/snapshot comparisons and any mismatch.

The Stage-4 physical gate subsequently passed and the user authorized Stage 5.
Its recovery image still has no LCD mutex or UI ownership transfer. Stage 5
has since passed physical verification. Current firmware is frozen pending
pushbutton availability; software confirmation timers and Stage 6 are not
authorized.

## Historical Stage-3 procedure and recovery evidence

The Stage-3 test was `rtos_stage3_player`, with exactly three application tasks.
`PollButtonsTask` owns button/controller/player state and publishes a consistent
public snapshot under a short-held state mutex. It still temporarily performs
all PF6, codec-volume, UI and LED work. `AdjustVolumeTask` and
`UpdateLcdLedsTask` remain waiting shells only. There is no audio task.

## Stage-3 build, upload and monitor

Run from the project root in PowerShell:

```powershell
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage3_player
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage3_player -t upload
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' device monitor -e rtos_stage3_player --baud 115200
```

Close any existing monitor first. Reset after opening the monitor to capture the
full startup. Ctrl+C closes it before another upload. If needed, list ports with
`platformio.exe device list` and add `--port COM<number>` using the real port.

Recovery to the physically verified Stage-2 RTOS player:

```powershell
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage2_player
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage2_player -t upload
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' device monitor -e rtos_stage2_player --baud 115200
```

The `black_f407zg` bare-metal recovery and completed `rtos_stage1_smoke`
environments also remain available; their commands and historical evidence are
below. No firmware upload is performed by the coding agent during development.

## Stage-3 development verification

All four final environments build without compiler/linker warnings or errors:

| Current environment | RAM | Flash |
| --- | ---: | ---: |
| `black_f407zg` recovery player | 15,008 bytes | 35,260 bytes |
| `rtos_stage1_smoke` diagnostic | 43,692 bytes | 24,284 bytes |
| `rtos_stage2_player` verified RTOS recovery | 48,120 bytes | 40,156 bytes |
| `rtos_stage3_player` | 48,292 bytes | 43,360 bytes |

All five existing host suites pass without warnings: audio DMA, selection,
player UI, potentiometer filter and ADC acquisition. The new state harness also
passes **9 groups / 196 checks** with both C++11 and C++17 using
`-Wall -Wextra -Werror -pedantic`. The 14 protected hardware/player files,
`player_tasks.h/.cpp`, `rtos_support.h/.cpp`, `FreeRTOSConfig.h` and the kernel
build script remain byte-identical to the pre-Stage-3 baseline.

All three recovery `firmware.bin` images (bare-metal, Stage-1 smoke and Stage-2
player) also match their saved pre-Stage-3 binaries byte-for-byte. The verified
Stage-2 RTOS player can therefore be restored with the recovery command above.

Source/ELF/map checks confirm one effective SysTick handler, one HAL increment
per tick, correct Cortex-M4F SVC/PendSV bindings, one 32 KiB `heap_4`, the existing
three checked task creations and one new application state mutex. The Stage-3
strong handlers are SysTick at `0x08006784`, SVC at `0x080006b0` and PendSV at
`0x08000740`. DMA1 Stream5/channel 0 stays at priority 5, above the unchanged
syscall threshold 6. Its callbacks and audio ISR contain no new RTOS calls.

There is only one runtime controller-update location, one PF6 update location,
one runtime LCD update location and one runtime LED update location, all
reachable only through `PollButtonsTask`. Snapshot getters are not called by
the shells. No software timer, extra audio task, LCD/codec/player-UART mutex,
queue, event group, newlib hook or sleep change is introduced. Stage 3 inherits
the same M4F/softfp flags and preserves the clock tree/tick algorithms.

These were development checks before the user's successful board test. Stage-3
physical verification and player behavior are approved complete. Exact Stage-3
task stack high-water/runtime heap values were not supplied with that approval.
Allocated stacks are unchanged; historical Stage-2 headroom is recorded
separately below and is not a Stage-3 measurement.

## Stage-3 ownership and runtime checks

| Application task | Priority | Allocated stack | Runtime work |
| --- | ---: | ---: | --- |
| `PollButtonsTask` | 3 | 1024 words / 4096 bytes | Authoritative button/controller/player owner; sole runtime ADC/codec/UI/LED owner for now; publishes snapshot; waits 20 ms per loop. |
| `AdjustVolumeTask` | 2 | 768 words / 3072 bytes | One startup message, then one-second waits and diagnostic wake counter only. |
| `UpdateLcdLedsTask` | 1 | 1024 words / 4096 bytes | One startup message, then one-second waits and diagnostic wake counter only. |

Startup hardware sequences, test tone, original UART instructions and all three
task-start messages remain present. `SCHEDULER RUNNING` is printed in task
context. Capture the actual boot log; task-start messages must appear only once.
The following excerpt shows implemented strings, not a captured board log:

```text
FREERTOS PLAYER MODE
SystemCoreClock: <actual clock> Hz
TASKS CREATED: BUTTONS=3/1024w VOLUME=2/768w LCD/LEDS=1/1024w
<existing operating instructions>
Scheduler starting...
POLL BUTTONS TASK STARTED
SCHEDULER RUNNING
ADJUST VOLUME TASK STARTED
UPDATE LCD/LEDS TASK STARTED
STAGE 3: BUTTON/STATE OWNERSHIP ACTIVE
```

About every 30 seconds the player owner also prints:

```text
STATE SNAPSHOT: REV=<publication revision> STATE=<IDLE/SELECTING/PLAYING/PAUSED> CURRENT_INDEX=<0..7> VALID=<0/1> PHASE=<0/1/2> EVENTS=<event sequence>
RTOS HEALTH: BUTTON_STACK=<free words>w VOLUME_STACK=<free words>w LCD_STACK=<free words>w VOLUME_WAKES=<count> LCD_WAKES=<count> HEAP=<free bytes> MIN_HEAP=<minimum free bytes>
```

`CURRENT_INDEX` is zero-based. `VALID` means a current index has been confirmed;
it stays set after completion or a failed start. It is not ADC validity or an
active-playback flag; mode and actual playback status determine activity. Phase
values are None=0, Release=1 and Confirm=2.
Publication revision advances each loop; event sequence advances only when a
recorded event occurs. Hardware work and UART duration can delay these reports.
Only this diagnostic currently calls the snapshot getter; the waiting shells
remain passive.

The shared-state mutex surrounds snapshot copies/publication only. It never
surrounds ADC polling, buttons, delays, codec/audio calls, UART or LCD drawing.
There is no codec/LCD/UART mutex, queue, event group, software timer or idle-sleep
change in this stage. `HAL_GetTick()` retains authority for all existing timings.
DMA1 Stream5/channel 0 remains priority 5, with no RTOS calls from audio ISRs.

Observe at least two 30-second `RTOS HEALTH` reports. Shell wake counters must
increase; all stack high-water marks must remain positive and heap must settle.
Stack figures are minimum unused **words**, heap figures are **bytes**. Save
the real Stage-3 readings. The Stage-2 values below are historical observations
and cannot be reported as measured Stage-3 results. Do not reduce the stacks.

The new state harness is
[`tests/player_state_host.cpp`](../tests/player_state_host.cpp). It compiles the
actual snapshot store against native RTOS fixtures to check ownership rejection,
ISR rejection, short complete-copy locking, persistent event ordering,
completion during selection, release-started timing and unsigned wrap. Native
fixtures do not measure real scheduler contention, IRQ latency or audio timing;
the board checklist remains necessary. Build/run instructions are in
[`tests/README.md`](../tests/README.md).

## Stage-3 physical player checklist

The user approved Stage-3 physical verification complete overall. The checklist
below is retained as the original procedure; individual rows were not separately
logged. Its result cells record the missing item-by-item log; the overall stage
passed. The completed Stage-5 reference checklist is above.

For each selection, DOWN=bit 2, LEFT=bit 1, RIGHT=bit 0. Hold the requested bits,
press UP to capture, release all four buttons, then press UP again within five
seconds. `000` uses UP alone; during playback make a short UP press/release.
LEFT/RIGHT remain binary inputs and do not adjust volume.

| # | Check | Expected result | Physical result |
| ---: | --- | --- | --- |
| 1 | Scheduler and task startup | Player-mode/scheduler lines and all three task-start messages appear once; startup screens, codec detection and test tone remain normal. | Not separately logged |
| 2 | `000` | Song 1, Fur Elise, captures, confirms and plays. | Not separately logged |
| 3 | `001` | Song 2, Canon In D, captures, confirms and plays. | Not separately logged |
| 4 | `010` | Song 3, Minuet in G major, captures, confirms and plays. | Not separately logged |
| 5 | `011` | Song 4, Turkish March, captures, confirms and plays. | Not separately logged |
| 6 | `100` | Song 5, Nocturne in E flat, captures, confirms and plays. | Not separately logged |
| 7 | `101` | Song 6, Waltz No. 2, captures, confirms and plays. | Not separately logged |
| 8 | `110` | Song 7, Nocturne in C sharp, plays; unused Song 9 is not selected. | Not separately logged |
| 9 | `111` | Song 8, Symphony No. 40, plays; unused Song 10 is not selected. | Not separately logged |
| 10 | Frozen capture | Change the bit buttons after capture; candidate remains the originally captured song. | Not separately logged |
| 11 | Release-all gate | Hold captured buttons for more than five seconds; confirmation/countdown does not start until every button is released. | Not separately logged |
| 12 | Staggered release | Release each button separately; the final release starts the confirmation window once. | Not separately logged |
| 13 | Second-UP confirmation | A fresh UP press within the window confirms exactly once; holding UP does not restart playback. | Not separately logged |
| 14 | Five-second deadline | Without confirmation, selection cancels at the existing deadline; confirmation after expiry does not play the expired candidate. | Not separately logged |
| 15 | Timeout from idle | Timeout notice appears, then idle returns with all state LEDs off. | Not separately logged |
| 16 | Selection while playing | GREEN indicates selection while the existing song continues; confirmation changes song, timeout restores playing. | Not separately logged |
| 17 | Selection while paused | Background remains paused; timeout restores paused with RED, while confirmation starts the selected song. | Not separately logged |
| 18 | Long-UP pause | Hold UP alone for at least one second, then release; current song pauses without resetting its position. | Not separately logged |
| 19 | Long-UP resume | Repeat long-UP/release; the song continues from its paused position. | Not separately logged |
| 20 | Song completion | Full score ends, reports completion, returns to idle and turns all LEDs off. | Not separately logged |
| 21 | Completion during selection | Capture near a song's end; EOF preserves pending selection, and later timeout returns to idle without resurrecting the finished song. | Not separately logged |
| 22 | PF6 potentiometer | Existing 0–100 mapping, filtering/deadband and stationary-knob stability remain correct. | Not separately logged |
| 23 | Volume while paused | Accepted/displayed volume changes while sound remains paused; resumed audio uses that volume. | Not separately logged |
| 24 | Volume during selection | Knob still changes accepted volume; frozen candidate and selection screen retain priority. | Not separately logged |
| 25 | LCD selection/confirmation | Correct bits/song/release instruction and five-to-one countdown; no deadline extension from rendering. | Not separately logged |
| 26 | LCD playing/paused/timeout | Existing titles, status, volume/bar, hints and timeout screen remain correct. | Not separately logged |
| 27 | Volume overlay | Knob changes show overlay; it expires after 1.5 seconds and yields to selection. | Not separately logged |
| 28 | LED state mapping | BLUE playing, RED paused, GREEN selecting; only the appropriate LED on; idle all off. | Not separately logged |
| 29 | Continuous audio | No new gaps, clicks or unintended restarts during input, volume, snapshot publication or drawing. | Not separately logged |
| 30 | RTOS health | Both shell wake counts progress; stack headroom remains positive; heap is stable; no scheduler freeze, HardFault, fatal message or reboot loop. | Not separately logged |

Repeat selections/timeouts from idle, playing and paused. Observe health through
several songs, pause/resume gestures, overlays and confirmation windows. The
snapshot is internal state preparation and must not introduce new visible
behavior or per-iteration UART output.

Stages 3 and 4 have passed their physical gates. In the Stage-3 rollback image,
PF6 stays in `PollButtonsTask` and volume remains a shell. Stage 4 transfers
volume ownership; Stage 5 separately transfers UI/LED ownership and adds its
LCD mutex. Stage 5 is now physically verified; software confirmation timers and
further firmware changes are not authorized.

## Completed Stage-2 physical evidence

The user tested the real player under FreeRTOS and approved Stage 2 complete.
They reported all three tasks running, increasing shell wake counts, working
pause/resume, potentiometer, five-second timeout and binary selection. They
specifically confirmed `110` selects Song 7 and `111` selects Song 8. Audio
continued normally with no HardFault or scheduler freeze; health stayed stable.
The following are **reported Stage-2 board values**, not Stage-3 measurements:

| Diagnostic | Typical reported Stage-2 value |
| --- | ---: |
| Buttons minimum unused stack | Approximately 670 words |
| Volume minimum unused stack | Approximately 738 words |
| LCD/LED minimum unused stack | Approximately 994 words |
| Free heap | 20,056 bytes |
| Minimum-ever-free heap | 20,056 bytes |

No exact runtime clock or item-by-item log of the original 32-point procedure
was supplied. Overall Stage-2 physical verification and approval are recorded
without inventing additional readings.

## Stage-2 build, upload and monitor

Run from the project root in PowerShell:

```powershell
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage2_player
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage2_player -t upload
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' device monitor -e rtos_stage2_player --baud 115200
```

If needed, list serial ports with `platformio.exe device list` and append
`--port COM<number>` using the actual port. Close any existing serial monitor
first. Reset after the monitor opens to capture all startup messages; Ctrl+C
closes it before another upload.

The equivalent commands when `pio` is on `PATH` are `pio run -e
rtos_stage2_player`, `pio run -e rtos_stage2_player -t upload`, and `pio device
monitor -e rtos_stage2_player --baud 115200`.

Immediate recovery to the default player:

```powershell
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e black_f407zg
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e black_f407zg -t upload
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' device monitor -e black_f407zg --baud 115200
```

## Stage-2 development verification

The three Stage-2 development environments built successfully without
compiler/linker warnings or errors. The coding agent performed no upload; the
user subsequently ran the board and approved Stage 2 complete.

| Recorded Stage-2 environment | RAM | Flash |
| --- | ---: | ---: |
| `black_f407zg` recovery player | 15,008 bytes | 35,260 bytes |
| `rtos_stage1_smoke` diagnostic | 43,692 bytes | 24,284 bytes |
| `rtos_stage2_player` | 48,120 bytes | 40,156 bytes |

The recovery `firmware.bin` is byte-for-byte identical to its pre-Stage-2 stable
image (SHA256 `F0FC1C4C057EDD1A954239B338F9C4AC0E025810BC27B62757B84DE64921452D`).
Conditional compilation keeps the original loop in `main()` for recovery; only
Stage 2 places that same source block in `RunPlayerLoop()` on the player task.

The five existing host suites pass: audio DMA (72 Fur Elise events and 725,526
PCM frames), song selection, player UI (8 groups), potentiometer filter
(8 groups), and ADC acquisition (15 scenarios / 401 checks). Their compilation
and execution return zero without warnings. All 14 protected files match their
baseline hashes. `FreeRTOSConfig.h` and the kernel build script are unchanged.
The complete player-loop body and its local initialization match the original
bytes after reversing only the guarded health-report/RTOS-delay tail.

ELF/map and source verification confirms one effective SysTick, correctly
resolved SVC/PendSV vectors, one 32 KiB `heap_4`, and three checked application
task creation calls with the approved priorities and stacks. SysTick advances
HAL time once and the kernel tick after scheduler startup. DMA1 Stream5/channel 0
and priority 5 remain unchanged. There are no Stage-2 mutex creations, software
timers, smoke tasks or extra audio task. Stock newlib hooks are unchanged.

Stage 2 uses the verified `-mthumb -mcpu=cortex-m4 -mfpu=fpv4-sp-d16
-mfloat-abi=softfp` settings; the default player retains its original flags.
The shared support changes only its environment guards and adds the Stage-2
startup/fatal interfaces; the existing tick/failure algorithms are retained.
The small smoke-image size change from its historical Stage-1 figure reflects
that source layout. The recorded Stage-1 results later in this guide remain
historical evidence.

## Stage-2 UART and task-health evidence

The original LED test, PF6 ADC initialization, ES8388 detection/initialization,
startup tone and operating instructions are retained. The following excerpt is
the implemented RTOS message format, **not a captured Stage-2 hardware result**.
Replace the clock and diagnostic placeholders with the observed values:

```text
FREERTOS PLAYER MODE
SystemCoreClock: <actual clock> Hz
TASKS CREATED: BUTTONS=3/1024w VOLUME=2/768w LCD/LEDS=1/1024w
<existing operating instructions>
Scheduler starting...
POLL BUTTONS TASK STARTED
SCHEDULER RUNNING
ADJUST VOLUME TASK STARTED
UPDATE LCD/LEDS TASK STARTED
RTOS HEALTH: BUTTON_STACK=<free words>w VOLUME_STACK=<free words>w LCD_STACK=<free words>w VOLUME_WAKES=<count> LCD_WAKES=<count> HEAP=<free bytes> MIN_HEAP=<minimum free bytes>
```

The task-start lines appear once in the order shown. A one-time task-notification
chain completes each UART write before the next task prints; only then does the
complete player loop begin. This uses no Stage-2 UART mutex. The two shells do
not print or touch ADC, codec, LCD, LEDs or player state after startup.

The first health report is due approximately 30 seconds after the player task
begins its loop, followed by another near 60 seconds, then every 30 seconds.
Ordinary player work and UART/LCD duration can delay a report slightly. Observe
at least two reports during the full regression:

- `VOLUME_WAKES` and `LCD_WAKES` must increase across reports. They usually advance
  roughly once per second, so an increment near 30 is expected; do not require an
  exact count or precise wall-clock spacing.
- All three stack high-water values must remain comfortably above zero through
  playback, selections, volume changes and overlays. They are minimum unused
  words, not bytes, and can decrease when a previously unused code path runs.
- `HEAP` and `MIN_HEAP` are bytes. Do not substitute the Stage-1 smoke heap result
  for Stage-2 readings; task count and stack reservations differ. Investigate
  unexplained continued heap decline rather than inferring safety from a build.
- No `FATAL:`, HardFault, repeated boot banner or frozen player/output is
  acceptable. A fatal path attempts `HALTED - RESET REQUIRED` and stops.

## Completed Stage-1 evidence

The Stage-1 smoke image tests UART, task scheduling and the shared HAL/FreeRTOS
millisecond tick. It does not run the MP3-player state machine or exercise its
audio, buttons, ADC, LCD or LEDs. The normal player remains available as the
default `black_f407zg` build.

The user physically observed both tasks for more than 60 seconds. Counters and
`OTHER` peer activity increased, typical HAL/RTOS differences were both `+1010`,
and the small absolute tick offset remained constant. Stack headroom stayed near
574 words; free heap and minimum free heap stayed near 25,200 bytes. No FATAL,
HardFault, reboot loop or frozen output was observed. The user approved Stage 1
complete and separately authorized Stage 2. No actual `SystemCoreClock` reading
was supplied with that report, so this guide does not invent one.

## Build, upload and monitor

Run these PowerShell commands from the project root. The absolute executable is
present on this machine and avoids depending on PlatformIO being on `PATH`.

```powershell
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage1_smoke
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e rtos_stage1_smoke -t upload
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' device monitor -e rtos_stage1_smoke --baud 115200
```

If the serial port is not selected automatically, list ports and add the actual
one to the monitor command with `--port COM<number>`:

```powershell
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' device list
```

Close another serial monitor before opening this one. If necessary, reset the
board after the monitor opens to capture the boot messages. Use Ctrl+C to close
the monitor before another upload.

The smoke firmware temporarily replaces the player on the board. Restore the
normal player using these commands:

```powershell
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e black_f407zg
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' run -e black_f407zg -t upload
& 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe' device monitor -e black_f407zg --baud 115200
```

When `pio` is available on `PATH`, the equivalent build/upload commands are
`pio run -e rtos_stage1_smoke` and `pio run -e rtos_stage1_smoke -t upload`;
monitor with `pio device monitor -e rtos_stage1_smoke --baud 115200`.

## Recorded Stage-1 build evidence

Both final verbose builds succeeded with no reported compiler/linker warnings or
errors. They use `framework = stm32cube`, `ststm32@20.0.0`, STM32CubeF4 `1.28.3`
and ARM GCC 7.2.1.

| Environment | RAM | Flash | Kernel linkage |
| --- | ---: | ---: | --- |
| `black_f407zg` | 15,008 bytes | 35,260 bytes | No FreeRTOS kernel linked; normal player. |
| `rtos_stage1_smoke` | 43,692 bytes | 24,280 bytes | Installed STM32Cube FreeRTOS V10.3.1. |

Verified source, map, ELF and disassembly evidence:

- Smoke links exactly the selected `tasks.c`, `queue.c`, `list.c`, GCC
  `ARM_CM4F/port.c` and `heap_4.c` kernel objects. There is no second heap
  implementation or timer daemon. The single `ucHeap` is `0x8000` bytes in
  ordinary SRAM; the build RAM total includes this reserved 32 KiB array.
- Smoke has one strong `SysTick_Handler` at `0x08002638`, `SVC_Handler` at
  `0x080006b0`, and `PendSV_Handler` at `0x08000740`. Vector entries resolve to
  these handlers. SysTick disassembly advances HAL time exactly once and guards
  the FreeRTOS tick until the scheduler starts.
- Default flags are `-mthumb -mcpu=cortex-m4`, without explicit FPU/ABI flags.
  Smoke adds `-mfpu=fpv4-sp-d16 -mfloat-abi=softfp` because the existing flags
  could not assemble the M4F port's FPU context instructions. Smoke ELF attributes
  confirm VFPv4-D16 and base AAPCS. The default player's ABI is unchanged.
- `F_CPU=168000000` remains board metadata, but kernel timing uses updated
  `SystemCoreClock`. The printed runtime clock is still a physical-test result.
- `configUSE_NEWLIB_REENTRANT=0`; no custom `__malloc_lock()`/`__malloc_unlock()`
  hooks are added. Timers, tickless idle and the idle hook remain disabled.
- Hashes of all 14 protected player files match the pre-FreeRTOS baseline.
  `main.cpp` has only guarded smoke include/entry/SysTick edits. The audio
  DMA1 Stream5/channel-0 setup and priority 5 are unchanged; its ISR has no new
  FreeRTOS calls. Smoke does not initialize audio DMA, so the UART IRQ check is
  not an audio IRQ runtime test.

All five existing host harnesses in [tests/README.md](../tests/README.md) passed
before integration and again after integration, with compiler and runtime exit
codes zero and no reported warnings:

| Harness | Post-integration result |
| --- | --- |
| Audio DMA | PASS; all 72 Fur Elise events, 725,526 generated frames. |
| Song selection | PASS. |
| Player UI | PASS; 8 test groups. |
| Potentiometer filter | PASS; 8 test groups. |
| ADC acquisition | PASS; 15 scenarios, 401 assertions. |

The 14 protected-file hashes were checked again after integration. Removing the
three guarded additions from `main.cpp` reconstructs its exact pre-stage bytes.
No firmware upload was performed by the coding agent as part of these development
checks. The subsequent user-run physical smoke result is recorded above.

## Expected UART format

This is an illustrative transcript of the implemented strings, **not a captured
hardware result**. Replace angle-bracket fields with the observed values. Task
startup order can differ; both STARTED lines and the scheduler/kernel IRQ lines
must appear. Each task prints at most one heartbeat per second.

```text
FREERTOS STAGE 1
SystemCoreClock: <actual clock> Hz
KERNEL: STM32CUBE FREERTOS V10.3.1
IRQ CHECK: BITS=4 GROUP=4 KERNEL=15 MAX_SYSCALL=6 (0x60) PASS
SMOKE ONLY: PLAYER PERIPHERALS NOT INITIALIZED
Scheduler starting...
RTOS SMOKE TASK A STARTED
SCHEDULER RUNNING
KERNEL IRQ CHECK: SYSTICK=15 PENDSV=15 PASS
RTOS SMOKE TASK B STARTED
TASK A: count=<A count> HAL=<HAL tick> (+<HAL delta>) RTOS=<RTOS tick> (+<RTOS delta>) OTHER=<B count> STACK=<free words>w HEAP=<free bytes> MIN_HEAP=<minimum free bytes>
TASK B: count=<B count> HAL=<HAL tick> (+<HAL delta>) RTOS=<RTOS tick> (+<RTOS delta>) OTHER=<A count> STACK=<free words>w HEAP=<free bytes> MIN_HEAP=<minimum free bytes>
```

`OTHER` is the peer task's last heartbeat count. `STACK` is the minimum observed
unused stack in words, while `HEAP` and `MIN_HEAP` are bytes. These numbers must
come from the board; no expected clock, stack or heap reading is assumed.

## Physical smoke-test pass criteria

1. Capture the boot output. It must identify FreeRTOS Stage 1, print the actual
   `SystemCoreClock`, report valid interrupt configuration, and announce scheduler
   startup. Do not expect a hardcoded 168 MHz clock.
2. Confirm both task-start messages and the `SCHEDULER RUNNING` message emitted
   from task context. A message immediately before `vTaskStartScheduler()` alone
   is not evidence that the scheduler actually ran.
3. Observe periodic A and B diagnostic lines for at least 60 seconds. Each task's
   count and its peer's `OTHER` value must increase. B's 500 ms initial heartbeat
   stagger should distribute output;
   exact line order or exact wall-clock spacing is not a pass requirement.
4. Each line reports HAL tick, RTOS tick, their elapsed differences, task count,
   stack high-water mark and available heap. HAL and RTOS ticks must both advance.
   Compare differences after the first sample: they should agree closely and be
   approximately 1000 ms/ticks per task iteration. UART transmission and ordinary
   execution add a small amount to a delay-based interval. Absolute HAL and RTOS
   tick values may differ because HAL timing begins before scheduler timing.
5. Both tasks must continue progressing while using `vTaskDelay()`. This gives
   board evidence that delayed tasks wake and execution switches between task
   contexts; it does not exercise the future three-task player workload.
6. Stack high-water marks, measured in words, must remain comfortably above zero.
   Capture the observed values rather than assuming the initial 768-word stacks
   are sufficient. Available heap should settle; unexplained continuous decline
   is a failure requiring investigation.
7. No `FATAL: FREERTOS STACK OVERFLOW`, `FATAL: FREERTOS HEAP ALLOCATION FAILED`,
   `FATAL: FREERTOS ASSERT`, `FATAL: HARDFAULT` or other `FATAL:` line may appear.
   A fatal path prints `HALTED - RESET REQUIRED` if UART can report it and stops.
   Repeated boot banners, frozen diagnostics or loss of one task's output are
   failures. Continuous output without these symptoms is evidence of no observed
   fault during this test, not a proof that every fault is impossible.
8. Reset once and repeat the startup checks. Record whether both tasks resume
   output reliably.

The test does not initialize I2S/DMA audio, ADC volume or LCD/LED runtime. The
absence of music or player screens in the smoke environment is expected.

| Check | Physical result |
| --- | --- |
| Overall smoke verification | PASS; user approved complete. |
| Task A and B startup and advancing counts | PASS; both active for more than 60 seconds. |
| HAL and RTOS tick differences agree | PASS; typical `+1010` each and constant absolute offset. |
| Delay wakeups and peer-task activity | PASS; both counters and `OTHER` advanced. |
| Stack headroom and stable heap | PASS; approximately 574 words and 25,200 bytes. |
| No observed fatal/fault/freeze/reboot loop | PASS during the user's observation period. |
| Exact boot clock value | Not supplied in the physical report. |

Stage 1 is complete; its smoke environment remains available for diagnosis.
Stage 5 is physically complete. Current work is documentation only for the
pending USER_BUTTON hardware. Stage-5 firmware remains frozen; Stage 6 is not
authorized.
