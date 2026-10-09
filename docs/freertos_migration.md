# FreeRTOS migration: Stages 1 through 5

**Stages 1 through 5 are physically verified and approved complete by the user.
The three-task FreeRTOS migration is complete. `rtos_stage5_player` is the frozen,
physically verified firmware baseline. No further firmware changes or Stage-6
work are authorized.**

The user confirmed that selector `100`, Nocturne in E-flat, contains an
intentional long internal rest followed by more music. No `AUDIO: SONG COMPLETE`
message preceded that continuation. This was not a restart/completion bug;
song notes, beats, tempo, lengths and completion logic remain unchanged.

The current player environment is `rtos_stage5_player`. Its control, volume and
presentation tasks are stable on the real RT-Spark board, as confirmed by the
user. Runtime LCD/PlayerUI and external LED presentation belong exclusively to
`UpdateLcdLedsTask`. Stage 4 and the earlier environments remain historical
rollback images. Development evidence and the completed Stage-5 procedure are
recorded in [freertos_testing.md](freertos_testing.md). No exact Stage-5 stack,
heap or item-by-item measurements were supplied with the overall physical pass.

## Pending hardware compliance: dedicated USER_BUTTON

**Status: PENDING HARDWARE; implementation and testing are deferred.** The user
does not currently have the fifth external pushbutton. The approved future
design is:

| Item | Approved future design |
| --- | --- |
| GPIO | PA0, P2 header pin 12 |
| Polarity | Active-low |
| Wiring | External 10 kOhm pull-up from PA0 to 3.3V; normally-open button from PA0 to GND |
| Ownership | `PollButtonsTask`; no fourth application task |
| USER_BUTTON action | STOP / REPLAY FROM BEGINNING |
| Existing long-UP action | PAUSE / RESUME; retain it unchanged |

This is a pending dedicated-button hardware-compliance item, not an implemented
feature or a Stage-5 failure. No GPIO initialization, input handling, new build
environment or merge into the verified firmware is authorized now. Wait until
the user physically has the pushbutton before implementing or testing it.

The existing HAL-tick five-second confirmation timeout remains the approved
implementation. A FreeRTOS software confirmation timer must not be added.
Stage 6, idle/sleep changes and all further firmware work remain on hold.

The environments have separate purposes:

| Environment | Purpose |
| --- | --- |
| `black_f407zg` | Default bare-metal player and recovery firmware. |
| `rtos_stage1_smoke` | The completed two-task scheduler diagnostic. |
| `rtos_stage2_player` | The real player loop under FreeRTOS, with the three required application task shells. |
| `rtos_stage3_player` | Authoritative button/player ownership and a mutex-protected public snapshot; volume and UI tasks remain waiting shells. |
| `rtos_stage4_player` | Independent runtime volume task, field-specific public-state updates and codec control mutex; LCD/LED task remains a waiting shell. |
| `rtos_stage5_player` | Control, volume and presentation in their three respective tasks; one dedicated LCD mutex protects runtime UI/LCD calls. |

The completed Stage 3 retained one runtime owner: `PollButtonsTask`. It
established the public snapshot without transferring PF6 processing or LCD/LED
rendering. The other two required tasks announce startup and wait; they do not
duplicate volume, display or LED work in that recovery environment.

The user physically verified the Stage-2 real player under FreeRTOS: all three
tasks remained active, the shell wake counters increased, binary selection,
five-second timeout, pause/resume, potentiometer volume and continuous audio
worked. `110` selected Song 7 and `111` selected Song 8. No HardFault or scheduler
freeze was observed. Typical reported minimum unused stack was approximately
670 words for buttons, 738 words for volume and 994 words for LCD; free and
minimum-ever-free heap were both 20,056 bytes. These are historical Stage-2
observations, not predicted or measured Stage-3 values. The user approved Stage 2
complete and separately authorized Stage 3 only.

## Completed Stage-4 physical evidence

The user verified exclusive runtime PF6 ownership by `AdjustVolumeTask`, working
filtering and codec attenuation, volume while playing/paused/selecting, retained
paused volume after resume, rapid knob movement, song changes while adjusting
volume, and pause/resume while adjusting volume. Shared `VOL`/`VOL_REV` progressed
correctly. Audio continued; no codec deadlock, HardFault, reboot or freeze was
observed. Heap stayed stable and all three task stacks retained positive headroom.
No exact Stage-4 stack or heap values were supplied, so none are inferred here.

The observed physical knob range was approximately **8% to 100%**. The user
explicitly accepted this as a Stage-4 pass and prohibited calibration during the
RTOS migration. PF6/ADC3 channel 4, mapping, filter, deadband and endpoints are
unchanged in Stage 5; a physical zero-percent reading is not a Stage-5 gate.

## Stage 5: sole runtime presentation owner

`rtos_stage5_player` inherits Stage 4 and adds `RTOS_STAGE5_PLAYER=1`. The five
older recovery environments keep their existing flags, behavior and firmware
images. The kernel/port, tick handlers, priorities, heap, FPU settings, clock tree
and verified Stage-4 codec locking are preserved.

| Stage-5 file | Responsibility |
| --- | --- |
| `platformio.ini` | Add the Stage-5 environment and its scoped formatting link wrapper; retain older environments. |
| `src/main.cpp` | Keep startup/bootstrap and control/publication; guard out all Stage-5 runtime UI/LED calls and initialize the LCD mutex before tasks. |
| `src/player_tasks_stage4.cpp` | One exclusion-guard change preserves its earlier task implementation for rollback. |
| `src/player_tasks_stage5.cpp` | New final three-task implementation and real UI iteration/health counters. |
| `include/player_presentation.h`, `src/player_presentation.cpp` | New pure observer converts one snapshot into the existing view, LED state and ordered UI actions. |
| `src/lcd_control.h/.cpp` | New checked LCD mutex, sole task-owner binding and task-context assertions. |
| `src/format_guard.cpp` | New Stage-5-only serialization of existing bounded formatting calls. |
| `tests/player_presentation_host.cpp`, `tests/format_guard_host.cpp` | New native observer/formatting tests. |
| `tests/state_stubs/task.h`, `tests/README.md` | Test-only scheduler declarations and native build/run instructions. |
| `docs/freertos_migration.md`, `docs/freertos_testing.md` | Physical milestones, ownership/lock evidence, development results and all 51 Stage-5 board checks. |

| Application task | Priority | Allocated stack | Stage-5 responsibility and wait |
| --- | ---: | ---: | --- |
| `PollButtonsTask` | 3 | 1024 words / 4096 bytes | Button/controller processing, authoritative player/song/selection state, transport, `Audio_Service()`, completion detection, public-state/event publication and foreground UART; waits 20 ms. |
| `AdjustVolumeTask` | 2 | 768 words / 3072 bytes | Sole runtime PF6 acquisition/filter, accepted volume, codec volume writes and volume/revision publication; waits 20 ms. |
| `UpdateLcdLedsTask` | 1 | 1024 words / 4096 bytes | All runtime PlayerUI/LCD operations, transient presentation, countdowns and external LED updates; approximately 20 ms periodic wait. |

Idle remains priority 0. Initial stack allocations are not reduced. Exact
Stage-5 hardware high-water values were not supplied with the completed physical
verification; earlier shell readings are not measurements of the active UI task.

### Bootstrap and runtime boundary

`main()` retains verified pre-scheduler hardware initialization, LCD startup and
audio-ready/failure screens, PF6 priming, codec/test tone, and the external LED
startup test. These have no concurrent task caller and remain outside the
runtime LCD mutex. `LcdControl_Init()` creates the mutex after the existing state
and codec bootstrap, before task creation, and checks failure. After scheduler
startup, `UpdateLcdLedsTask` is the sole
runtime PlayerUI/LCD and LED owner. The control task publishes state/events and
does not call runtime UI/LED/ADC/filter/volume-write operations. The volume task
does not draw, write LEDs or process buttons.

### Snapshot, presentation and LCD lock scope

`LcdControl_BindOwner()` binds the UI task and its lock functions assert the
correct task context. Each UI iteration copies **one complete public snapshot** under the existing
state mutex, releases it, then derives a pure presentation plan. Private UI-side
revision/cache state consumes retained events and determines the existing view
and transient actions. The UI task takes the one checked LCD mutex only around
the synchronous PlayerUI hooks and `PlayerUI_Update()`, releases it, updates LED
GPIOs outside that mutex, then waits.

There is no nesting between state, codec and LCD mutexes. No LCD lock is held
during another mutex wait, ADC/buttons, codec/audio service, UART output, or a
task delay. No UI/LCD/LED call runs in audio ISR context. Startup bootstrap is
the only pre-scheduler exception to runtime LCD ownership.

`PlayerPresentationObserver::Observe(snapshot, now)` returns a
`PlayerPresentationFrame` containing the existing `PlayerUiView`, LED mapping,
ordered UI actions and diagnostic revisions. The pure presentation helper
never mutates player state or touches STM32 LCD
hardware. Existing `player_ui.cpp`, its screen layouts, fonts, bars and redraw
cache remain functionally unchanged; after scheduling, only the UI task uses
their mutable caches. Native tests exercise presentation decisions separately.
The helper's requested screen does not bypass the existing PlayerUI
startup/audio-ready hold cache.

### Retained events and original timestamps

The UI keeps private last-seen revisions for each retained event kind. A slow
render cannot consume/clear a one-shot Boolean before the event is observed.
Events are processed in canonical order using wrap-safe sequence comparisons;
the most recent snapshot remains the source of truth for the visible state.
Superseded events may be acknowledged without replaying obsolete screens.

Confirmation-ready presentation receives the original published release-all
timestamp. The visual countdown therefore does not gain time from task latency.
`PollButtonsTask` and `HAL_GetTick()` remain authoritative for the five-second
deadline; the UI only displays it. Timeout notice visibility uses the original
published timestamp and approximately one-second duration, returning to the
correct idle, playing or paused background without a UI-owned state machine.

Only an accepted `VOL_REV` change can request the existing volume overlay.
Unchanged samples and deadband-rejected noise do not retrigger it. The original
change timestamp preserves approximately 1.5 seconds, rather than starting a
fresh period when an old event is rendered. Playing/paused overlays show the
accepted public value; expiry restores the true background. Existing
selection/confirmation priority and idle behavior remain unchanged, including
continuing volume acquisition without inventing a new idle overlay.

### LED mapping and timing

| Published state | Runtime LED output |
| --- | --- |
| Selecting, release-all or confirmation | GREEN on; RED/BLUE off, including over playing/paused background. |
| Playing without selection | BLUE on; RED/GREEN off. |
| Paused without selection | RED on; BLUE/GREEN off. |
| Idle/no song | All off. |

The pins stay active-high RED PA8, GREEN PA1 and BLUE PB14. Cancelling selection
restores BLUE or RED according to actual background playback. The onboard LED
matrix is unused. GPIO writes stay outside the LCD lock.

The UI task targets a 20 ms `vTaskDelayUntil()` period. If synchronous rendering
exceeds that budget, it resets the next wait origin and blocks instead of
spinning through catch-up iterations. Partial-redraw behavior avoids forcing a
full display redraw every period. ADC/button cadence and HAL timing remain
unchanged. DMA1 Stream5/channel 0 stays priority 5 and calls no FreeRTOS API.

### Scoped formatting safety

Moving existing UI formatting into its own task adds a second formatting caller
alongside the control task's UART preparation. Stage 5 alone links
`-Wl,--wrap=snprintf`. The wrapper performs `std::vsnprintf()` while task
scheduling is suspended, then resumes it, preserving nested-suspension semantics.
Before scheduler startup it simply formats. The scope is one existing bounded
formatting call, not an entire draw or UART transmission. Audio DMA interrupts
and the existing HAL/kernel tick integration remain enabled.

No extra mutex, newlib configuration change or custom malloc hook is introduced.
This guards current formatting call sites; adding arbitrary concurrent C-library
use would need its own review. Startup UART messages retain their notification
chain, and only the button task performs normal runtime UART output.

### Completed Stage-5 verification

Development checks, exact changed files, build sizes, native-test results,
rollback hashes, UART expectations and all 51 physical checks are in
[freertos_testing.md](freertos_testing.md). One-time startup includes
`STAGE 5: LCD/LED TASK ACTIVE`; approximately 30-second health output now reports
the real UI task stack and real `LCD_WAKES` progress.
`UI_STATE_REV` and `UI_VOL_REV` identify the revisions processed by that task.

The Stage-5 build uses **48,392 bytes RAM and 46,056 bytes Flash**. All eight
existing native suites pass, as do the new pure presentation tests (32 groups /
409 checks under C++11 and C++17) and bounded-formatting tests (9 groups /
176 checks). All six firmware environments build without warnings/errors. All
five older environments retain byte-for-byte identical BIN and ELF images
(10/10 artifact hash matches); the other 51 existing baseline files remain
unchanged. Independent compiled-code checks confirm the final ownership split,
one state/codec/LCD mutex each without nested scopes, correct exception vectors,
and unchanged priority-5 DMA callbacks without RTOS/UI/LED calls. Complete sizes,
file changes and evidence are in the testing guide. Native fixtures do not
measure real scheduler/IRQ latency, physical display behavior or the now-active
UI task's stack headroom. No firmware upload was performed.

**Stage 5 is physically verified and complete. Preserve its firmware exactly.**
No software confirmation timer, Stage 6 or further firmware change is authorized.
Dedicated USER_BUTTON implementation/testing waits for the physical pushbutton;
only its approved pending design is documented above.

## Historical Stage 4: runtime volume ownership and short codec locking

`rtos_stage4_player` inherits the completed Stage-3 configuration and adds
`RTOS_STAGE4_PLAYER=1`. The four earlier environments retain their existing
configuration, compiler flags and behavior. The Stage-1 kernel/port configuration,
tick and exception support, 32 KiB `heap_4`, clock tree and interrupt priorities
remain unchanged.

| Stage-4 file | Responsibility |
| --- | --- |
| `platformio.ini` | Add `rtos_stage4_player` without changing the four older environments. |
| `src/main.cpp` | Keep startup bootstrap; remove runtime ADC/volume writes from the Stage-4 control path; consume volume snapshots/events and copied diagnostics. |
| `src/player_tasks.h`, `src/player_tasks_stage4.cpp` | Preserve three tasks/stacks/priorities; run real volume acquisition and low-rate diagnostic handoff; keep LCD/LED task as a shell. |
| `src/player_tasks.cpp` | Exclude the earlier task implementation only when the Stage-4 implementation is selected. |
| `include/player_state.h`, `src/player_state_stage4.cpp` | Stage-4 accepted-volume revision, owner binding, field-specific control/volume updates and short consistent copies. |
| `src/player_state.cpp` | Retain the Stage-3 single-owner implementation for recovery builds; exclude it in Stage 4. |
| `src/codec_control.h/.cpp` | One checked, task-context codec mutex and no-op pre-scheduler bootstrap locking. |
| `src/audio_codec.cpp` | Stage-4-only guard around the short transport mute/unmute transaction; synthesis/completion/DMA behavior unchanged. |
| `tests/player_state_stage4_host.cpp`, `tests/codec_control_host.cpp`, `tests/README.md` | New state-merge/revision and codec-lock/driver-coverage harnesses with native build/run instructions. |
| `docs/freertos_migration.md`, `docs/freertos_testing.md` | Ownership/lock evidence, commands, rollback and physical checklist. |

| Application task | Priority | Allocated stack | Stage-4 responsibility and wait |
| --- | ---: | ---: | --- |
| `PollButtonsTask` | 3 | 1024 words / 4096 bytes | Buttons/controller, authoritative player/song state, transport and `Audio_Service()`, state publication, temporary LCD/LED ownership and UART; waits 20 ms. |
| `AdjustVolumeTask` | 2 | 768 words / 3072 bytes | Sole runtime PF6 sampling/filtering, accepted volume, codec volume writes and volume publication; waits 20 ms. |
| `UpdateLcdLedsTask` | 1 | 1024 words / 4096 bytes | One startup message, then one-second waits and its diagnostic wake counter only. |

Idle remains priority 0. There is no audio application task. DMA1 Stream5/channel
0 remains at IRQ priority 5; audio callbacks still synthesize/refill PCM and call
no FreeRTOS API.

The existing `potentiometer_volume` and `potentiometer_filter` implementations
are reused. PF6 is ADC3 channel 4, 12-bit right-aligned, 0-4095, ADC PCLK/4,
480-cycle sample time, software-triggered polling, and no ADC DMA. The four-sample
moving average, approximately two-percentage-point deadband, endpoint behavior,
20 ms acquisition interval, controlled initialization retry and failed-conversion
retention remain unchanged. Their history, accumulator, retry state and accepted
value are private to the volume task after scheduler startup.

The existing one-time pre-scheduler ADC initialization, four priming samples and
startup volume application remain in `main()` to preserve codec initialization
and test-tone level. The same module state then passes to the volume task; no
filter reset or artificial volume jump is introduced. There is no periodic ADC
path in `main()` and no runtime ADC/filter/volume-register call from the button
task in Stage 4. Confirming a song no longer reprograms an old local volume; the
current ES8388 attenuation, owned by the volume task, carries into the song.

### Codec mutex coverage

`CodecControl_Init()` creates one checked FreeRTOS codec mutex before task
execution. `CodecControl_Lock()`/`CodecControl_Unlock()` serialize
the foreground ES8388 software-I2C operations that can overlap: the volume
register pair and transport mute/unmute control. Startup-only codec operations
remain before scheduling.

Only the control-bus transaction portions need the mutex. Song preparation,
buffer prefill and long synthesis work remain outside it. `Audio_Service()`
retains its foreground ownership; any mute transaction it performs uses the same
short bus guard rather than locking the whole service function. The required
Stage-4-only bus guard does not alter notes, completion logic, I2S/DMA setup,
callbacks or synthesis. Earlier builds compile the original control path.

Sampling/filtering occurs before acquiring the codec mutex. The volume task
applies the accepted value, releases the codec mutex, then separately publishes
under the state mutex. Button transport operations and state publication are
also separate. No state/codec mutex nesting, LCD lock, UART lock, wait/delay under
a mutex, or mutex operation in IRQ context is introduced.

### State merge and presentation

The Stage-3 whole-copy store is extended only for Stage 4 to accept independent
control and volume writers. `PlayerState_BindPublisher()` binds the control task;
`PlayerState_BindVolumePublisher()` separately binds the volume task. A control
`PlayerState_Publish()` retains the store's canonical accepted volume, volume
validity and volume event; it cannot restore an older
value from the button task's local projection. `PlayerState_PublishVolume()`
updates only the volume-owned public fields and advances the volume event only
for a real logical change. Unchanged/deadband-rejected samples create no false
overlay event. First-valid zero establishes validity without inventing a change.

The short state mutex covers only snapshot copies/merges. ADC polling, codec
operations, controller processing, UART, LCD drawing and delays remain outside
it. Filter history/raw acquisition internals are not added to the public player
snapshot; the accepted/public 0-100 value and presentation revisions are sufficient.

`PlayerState_GetSnapshot()` provides a complete protected copy. The button task
observes the retained volume event and continues the existing UI overlay
behavior. It remains the only runtime UI/LED owner. The volume task
calls neither `PlayerUI_*` nor LED functions. Overlay timestamps remain the
accepted-change timestamps, preserving approximately 1.5 seconds and existing
selection-screen priority. Control publication and volume events receive one
canonical shared event order; the UI observes a fresh accepted-volume copy after
control publication. `PlayerState_DismissTimeoutNotice()` separately updates
only the control-owned notice when an eligible overlay cancels it, avoiding a
full stale republish or nested locks.

Accepted volume and codec attenuation continue updating in idle, playing, paused
and selecting states. The existing idle/selection screen is retained; no new idle
overlay or UI drawing change is introduced. Playing/paused retain the verified
overlay, and pause never suspends sampling/attenuation updates, so resume uses
the new paused volume.

### UART and verification boundary

The one-time notification chain still serializes task-start messages. The real
volume task announces `STAGE 4: VOLUME TASK ACTIVE` once, then publishes meaningful
ADC/volume diagnostics for the button task to print in the existing format.
It performs no concurrent runtime UART output or formatting. A private diagnostic
mailbox carries copies through `PlayerTasks_GetVolumeDiagnostics()`; the button
task does not read filter globals to print them. Its short copy-only critical
sections leave the priority-5 audio ISR unmasked. This keeps one runtime UART
owner and does not activate a UART mutex or newlib hooks.

Low-frequency `STATE SNAPSHOT` and `RTOS HEALTH` continue. Health reports include
the real volume task's stack high-water mark, all three task headroom readings,
free/minimum heap, volume cycles and LCD-shell wake count. `VOLUME_WAKES` now
counts real volume-task iterations at approximately 20 ms rather than one-second
shell wakeups. It is not the number of accepted changes. Hardware values must
come from the Stage-4 board test, not historical Stage-2/3 results.

The host tests exercise field-specific merge behavior, revision/event generation,
value limits, independent player/volume updates and internally consistent copies.
Native fixtures validate state logic and lock scope, not real scheduler/IRQ
latency. The 36-point physical procedure in
[freertos_testing.md](freertos_testing.md) is therefore required after the build.

All five final build environments pass without warnings or errors. Stage 4 uses
48,376 bytes of RAM and 44,584 bytes of Flash. All six pre-existing host suites
pass, including the Stage-3 state suite (9 groups / 196 checks). New Stage-4 state
tests pass 13 groups / 426 checks, and actual codec-lock/driver tests pass
6 groups / 1,424 checks. All 13 non-audio protected files and the verified kernel
config/build/support sources remain byte-identical. Audio-driver edits are only
the Stage-4 bus guard; notes, lengths, synthesis, DMA and completion stay intact.
All four earlier recovery firmware binaries are byte-for-byte identical to their
saved pre-Stage-4 versions, including the physically verified Stage-3 rollback.
Independent compiled-code checks confirm no ADC/filter/volume-register path in
the control loop, one real volume owner, one state and one codec mutex, three
application tasks, correct exception vectors and no RTOS calls from the audio
ISR. DMA IRQ priority remains 5.
Detailed final evidence is in [freertos_testing.md](freertos_testing.md).

This was the Stage-4 development gate. The user subsequently passed physical
Stage-4 testing and authorized Stage 5, which has also passed physical testing.
The current hold is the unavailable fifth pushbutton; Stage 6 is not authorized. The Stage-4 recovery image still
retains its waiting LCD/LED shell and has no LCD mutex, timer or sleep change.

## Completed Stage 1

Stage 1 adds isolated FreeRTOS infrastructure and a UART scheduler smoke test.
The normal `black_f407zg` environment remains the default working player. The
separate `rtos_stage1_smoke` environment selects `RTOS_STAGE1_SMOKE=1` and runs
two temporary diagnostic tasks instead of the player. Uploading the smoke image
temporarily replaces the firmware on the board; uploading `black_f407zg` restores
the normal player.

The player features verified before this migration remain the baseline: eight
binary selections, release-all gating, five-second confirmation, long-UP
pause/resume, continuous synthesized I2S3/DMA audio through ES8388, PF6/ADC3
channel-4 potentiometer volume, the 240x240 LCD UI, and external red/green/blue
LED indication. Earlier guides may still show pending results from their original
development stage; the user's latest physical verification establishes this
pre-migration baseline. The user subsequently verified Stage 1 on the board for
more than 60 seconds: both counters and peer activity advanced; typical HAL and
RTOS differences were both `+1010`, their absolute offset stayed constant, stack
headroom was approximately 574 words, and heap/minimum heap were approximately
25,200 bytes. No fatal message, HardFault, reboot loop or frozen output was
observed. These are reported physical results, not predicted diagnostic values.

## Integration boundaries

The smoke branch is entered after `HAL_Init()` and UART initialization, before
the player initializes the LCD, buttons, potentiometer, LEDs or codec. Its tasks
access UART and RTOS diagnostics only. No button, selection, ADC, LCD, LED, audio
service or confirmation behavior is moved into an RTOS task in this stage.

Kernel sources come from the FreeRTOS V10.3.1 bundled with the installed
STM32CubeF4 framework. The build script compiles that kernel with its GCC
`ARM_CM4F` port and `heap_4.c`; it does not fetch a separate kernel or use Arduino.
The framework remains `stm32cube`. PlatformIO's existing installed versions are
pinned for reproducible builds: `ststm32@20.0.0` and STM32CubeF4 `1.28.3`, using
ARM GCC 7.2.1.

The default player's compiler flags remain `-mthumb -mcpu=cortex-m4`, with no
explicit `-mfpu` or `-mfloat-abi`. Those settings were insufficient for the
FreeRTOS M4F port's floating-point context assembly. The RTOS environments
add `-mfpu=fpv4-sp-d16 -mfloat-abi=softfp`; this uses the M4 FPU while retaining
the base AAPCS calling convention. ELF attributes confirm VFPv4-D16 and base
AAPCS. No FPU/ABI setting changes the default player build.

PlatformIO still supplies nominal board metadata `F_CPU=168000000`. The kernel
does not use that value: `configCPU_CLOCK_HZ` reads `SystemCoreClock` after
`SystemCoreClockUpdate()`. The actual boot value must be recorded on the board;
the existing clock tree is preserved.

| File | Stage-1 responsibility |
| --- | --- |
| `platformio.ini` | Keep the player environment as default; add the isolated smoke environment. |
| `include/FreeRTOSConfig.h` | Kernel features, heap, diagnostics and interrupt priority configuration. |
| `scripts/freertos_build.py` | Locate and compile the installed STM32Cube kernel, GCC M4F port and one heap implementation. |
| `src/rtos_support.h` | Declare the smoke entry and support interfaces. |
| `src/rtos_support.cpp` | Smoke tasks, UART serialization, exception integration and failure diagnostics. |
| `src/main.cpp` | Minimal conditional smoke entry and one effective SysTick handler. |
| `docs/freertos_migration.md` | Record scope and remaining migration stages. |
| `docs/freertos_testing.md` | Build/upload commands, evidence and physical pass criteria. |

Protected modules are `audio_codec.cpp/.h`, song headers, `song_selection.h`,
`potentiometer_volume.cpp/.h`, `potentiometer_filter.h`, `lcd_display.cpp/.h`,
`player_ui.cpp/.h`, and `player_leds.cpp/.h`. Their working behavior is preserved.

## Configuration and timing

| Setting | Stage-1 value or behavior |
| --- | --- |
| Scheduling | Preemption and time slicing enabled. |
| Tick | 1000 Hz, 32-bit tick type. |
| CPU clock | Updated `SystemCoreClock`, printed at boot; no 168 MHz assumption or clock-tree change. |
| Memory | Dynamic allocation with `heap_4`, 32 KiB kernel heap. |
| Synchronization | Mutexes and task notifications enabled. |
| Diagnostic task priority | A and B both priority 1. |
| Diagnostic task stack | 768 `StackType_t` words each, 3072 bytes on this Cortex-M4. |
| Diagnostic task waiting | Both announce startup; each heartbeat waits 1000 ms. B waits another 500 ms before its first heartbeat loop. |
| UART | A mutex serializes smoke output; no iteration flood. |
| Failure detection | Stack overflow mode 2, malloc-failure hook and assertions. |
| Deferred features | Software timers, tickless idle, idle-hook sleep and newlib reentrancy disabled. |

The Stage-1 diagnostic tasks are separate from the laboratory application
roles. The Stage-2 player has exactly three application tasks:
`PollButtonsTask`, `AdjustVolumeTask`, and `UpdateLcdLedsTask`. FreeRTOS's idle
task is kernel support. There is no fourth audio application task.

There is one effective `SysTick_Handler`. Before the scheduler starts, it advances
the HAL millisecond tick. After scheduler startup, it advances HAL time once and
dispatches the FreeRTOS port tick once. The wrapper checks
`xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED`; a suspended scheduler
still receives ticks so the kernel can account for pended ticks. SVC and PendSV
bind to the Cortex-M4F port handlers through `vPortSVCHandler` and
`xPortPendSVHandler` aliases. Existing `HAL_GetTick()` users remain on a
millisecond timebase.

STM32F407 implements four interrupt priority bits. Stage 1 validates
`NVIC_PRIORITYGROUP_4`, kernel priority 15 (`0xF0`) and library maximum syscall
priority 6 (`0x60`). The existing audio DMA1 Stream5 priority 5 is unchanged and
calls no FreeRTOS API. It remains above the syscall threshold, so it must not call
`FromISR` APIs. The smoke firmware does not initialize or start audio DMA; its
priority configuration is checked in the preserved source and build evidence,
not by playing audio in a diagnostic task.

Failure hooks stop execution with interrupts disabled. A UART failure message is
best effort, uses bounded direct USART polling, and does not acquire the smoke
UART mutex or depend on an advancing HAL tick. Stack overflow, allocation
failure, assertions, HardFault and a returned scheduler enter the same failure
path. It reports `FATAL: ...` and `HALTED - RESET REQUIRED`, with a debugger-visible
failure record retained if UART cannot print. It does not continue normal work.

## Laboratory mapping and later work

The official `Laboratory Activity 2 (1).pdf` requires FreeRTOS or Zephyr on
physical page 3. Physical page 7 (printed Page|6) requires the three application
threads, infinite loops with waiting between updates, an LCD mutex, startup
instructions and sleep behavior. Stage 1 established the kernel only. Stage 2
establishes the three task shells and runs the player under the scheduler.
Stage 5 establishes the final division of control/volume/presentation and the
LCD mutex; sleep has not been implemented and is not authorized now. This does not certify the entire laboratory
checklist complete before its remaining hardware/timing requirements are met.

The PDF's Timeout/Ticker explanation is on physical page 6, with the timing
requirement on physical page 7. The verified HAL-tick confirmation controller
remains the approved implementation. The user explicitly declined the optional
FreeRTOS one-shot timer; it is not a pending implementation step. Recurring audio remains the working DMA refill callbacks, documented
as the STM32/FreeRTOS adaptation of the reference Ticker concept.

| Stage | Responsibility |
| --- | --- |
| 2 | Physically verified complete: three application tasks with the complete player loop in one task. |
| 3 | Physically verified complete: authoritative button/player-state ownership and a short-lock public snapshot; volume and UI remain shells in its recovery image. |
| 4 | Physically verified complete: exclusive PF6 acquisition/filter/codec-volume role in the volume task. |
| 5 | Physically verified complete: final three-task split, sole runtime LCD/RGB owner, LCD mutex and retained-event presentation. |
| 6 | Not authorized; do not implement a software confirmation timer. Keep the verified HAL-tick timeout. |
| 7 | Not implemented or authorized; no further firmware changes are requested. |

Dedicated USER_BUTTON remains pending hardware availability. Its approved
future PA0/P2-12 STOP/REPLAY design is documented above, but no pin configuration
or input support is implemented. Long-UP pause/resume remains unchanged.

## Stage 2: player task and shells

The verified hardware startup stays in `main()` before scheduler startup: HAL,
clock update, UART, LED initialization/startup test, buttons, LCD/startup screens,
PF6 ADC priming, ES8388 probe/initialization and startup test tone. Operating
instructions remain on UART. After hardware startup, `PlayerTasks_Create()`
checks all three task allocations; main prints the original instructions and
then `PlayerTasks_StartScheduler()` starts scheduling. A returned scheduler is
fatal. The UART handle, codec-initialized flag and loop function pointer used by
tasks are stored statically, rather than referring to main's former stack.

| Stage-2 file | Responsibility |
| --- | --- |
| `platformio.ini` | Add `rtos_stage2_player` with the existing kernel build script and M4F/softfp flags. |
| `src/player_tasks.h/.cpp` | Create the three tasks, coordinate their one-time startup and report low-rate task health. |
| `src/main.cpp` | Compile the intact loop as `RunPlayerLoop()` only in Stage 2; preserve the original `main()` loop in recovery builds. |
| `src/rtos_support.h/.cpp` | Make the verified exception/failure support available to Stage 2 while retaining the separate smoke path. |
| `include/FreeRTOSConfig.h`, `scripts/freertos_build.py` | Reuse the verified Stage-1 infrastructure without changes. |

| Application task | Priority | Initial stack | Stage-2 work |
| --- | ---: | ---: | --- |
| `PollButtonsTask` | 3 | 1024 words / 4096 bytes | Own the entire existing foreground loop; block for 20 ms after each iteration. |
| `AdjustVolumeTask` | 2 | 768 words / 3072 bytes | Print one startup message, then repeatedly block for 1000 ms and increment its diagnostic wake counter. |
| `UpdateLcdLedsTask` | 1 | 1024 words / 4096 bytes | Print one startup message, then repeatedly block for 1000 ms and increment its diagnostic wake counter. |

Idle remains priority 0. Each application task has an infinite loop and waits;
there is no audio application task. `Audio_Service()` stays in the temporary
complete player loop, while synthesized PCM refill and I2S DMA remain driven by
their existing interrupts. DMA1 Stream5 stays priority 5 and calls no RTOS API.

For this stage, `PollButtonsTask` still handles ADC sampling/filtering and codec
volume, button input, selection/confirmation/timeout, pause/resume, LCD view
generation/rendering, LEDs and foreground UART diagnostics. The other shells
do not access player state, ADC, codec, LCD or LEDs. Once the scheduler starts,
`main()` does not execute a second copy of the player loop.

The final foreground delay changes from `HAL_Delay(20)` to
`vTaskDelay(pdMS_TO_TICKS(20))`. Required startup delays remain before scheduling.
`HAL_GetTick()` still controls 25 ms debounce, the one-second long-UP gesture,
the five-second confirmation window, one-second timeout notice and 1.5-second
volume overlay. The software confirmation timer is not enabled.

Final state/codec/UART/LCD mutexes, queues and event groups are withheld because
only one task currently performs player operations. Software timers, tickless
idle, idle-hook WFI and newlib reentrancy remain disabled. A one-time notification
chain orders startup UART ownership: buttons prints, notifies volume and waits;
volume prints and notifies UI; UI prints and releases buttons to enter the player
loop. The shells print no further messages. This startup gate needs no Stage-2
UART mutex and does not synchronize shared player state.

The two shell wake counters are diagnostic data, not player state. Every 30
seconds, the single player-loop owner prints `RTOS HEALTH` with the three stack
high-water marks, shell wake counts and free/minimum heap. It does not print each
iteration. Shell counters advance after each one-second delay and allow physical
confirmation that those tasks continue to wake. Stack readings are in words and
heap readings are in bytes; they must be observed during the player regression.

The dedicated Stage-2 environment retains the Stage-1 FreeRTOS kernel, M4F port,
heap, exception/tick integration and interrupt thresholds. It leaves the default
bare-metal recovery environment and the completed smoke environment available.

All three development builds pass without warnings. Stage 2 uses 48,120 bytes
of RAM and 40,156 bytes of Flash. All five existing host suites pass, all 14
protected player files match their baseline hashes, and the kernel config/build
script remain byte-identical. The recovery firmware binary exactly matches the
previous stable image. The user subsequently physically verified and approved
Stage 2 complete; see [freertos_testing.md](freertos_testing.md).

## Stage 3: authoritative control and public snapshot

| Stage-3 file | Responsibility |
| --- | --- |
| `platformio.ini` | Add isolated `rtos_stage3_player`; preserve the Stage-2 and bare-metal recovery environments. |
| `include/player_state.h` | Public snapshot, retained event stamps and revision comparison helpers. |
| `src/player_state.cpp` | Checked state-mutex creation, sole-publisher binding and short snapshot copies. |
| `src/main.cpp` | Project the sole owner's existing runtime state/events into the snapshot without changing the controller. |
| `src/player_tasks.h/.cpp` | Reuse the same three tasks, startup chain and health reporting unchanged. |
| `src/rtos_support.h/.cpp` | Reuse the verified support through the inherited Stage-2 build define. |
| `tests/player_state_host.cpp`, `tests/state_stubs/` | Exercise short-copy synchronization, ownership and persistent event/revision behavior with native RTOS fixtures. |
| `include/FreeRTOSConfig.h`, `scripts/freertos_build.py` | Reuse the physically verified Stage-1 infrastructure unchanged. |

`PollButtonsTask` is the authoritative owner of button reads, the unchanged
`SongSelectionInput` controller, pending/current song, selection phase,
playing/paused/idle state, background return state and song-completion
transitions. Its private variables are not shared directly with future tasks.
It also temporarily continues PF6 acquisition/filtering, accepted-volume codec
writes, all runtime UI hooks/rendering, LED updates, foreground audio transport,
`Audio_Service()` and UART diagnostics. Each iteration ends with
`vTaskDelay(pdMS_TO_TICKS(20))`.

`AdjustVolumeTask` and `UpdateLcdLedsTask` remain waiting shells, with their same
one-time startup messages and one-second diagnostic wake counters. Neither
samples ADC, changes codec volume, renders UI, writes LEDs or observes player
state unnecessarily. Audio synthesis/refill remains interrupt-driven through
the existing I2S3/DMA path; there is no audio task.

A dedicated shared-state mutex protects only public snapshot copy/publication.
The owner first constructs a complete snapshot outside the lock, then takes the
mutex, copies it and releases the mutex. Readers similarly obtain one complete
copy. No button read, delay, ADC polling, UART output, LCD drawing,
`Audio_Service()` or codec call occurs while this mutex is held.

`include/player_state.h` defines `PlayerStateSnapshot`, and
`src/player_state.cpp` provides `PlayerState_Init()`,
`PlayerState_BindPublisher()`, `PlayerState_Publish()` and
`PlayerState_GetSnapshot()`. Initialization occurs before scheduling; publisher
binding occurs in `PollButtonsTask`. An assertion rejects publication from a
different task. Snapshot access is task-context-only, never an ISR operation.
The shell tasks do not read it unnecessarily. Future task code can obtain a
consistent copy through `PlayerState_GetSnapshot()` rather than accessing the
owner's private variables.

Whole-copy publication is deliberately single-owner in the Stage-3 recovery
environment. The separately authorized Stage 4 adds protected field merges so a
stale player projection cannot overwrite a newer accepted volume. The Stage-3
store behavior remains unchanged when built without the Stage-4 define.

Public fields describe mode/background, zero-based current/pending song indices,
preview/captured bits, song and volume validity, release/confirmation phase,
confirmation start/deadline, timeout visibility/start, accepted volume, actual
playback status and retained last completion/error. Confirmation start/deadline
are meaningful only in the Confirm phase. `observedAt` timestamps the projection;
the shared store increments the publication revision.
`hasCurrentSong` means the current index has been confirmed and remains true
after completion or a failed start; mode and actual playback determine whether
audio is active. `lastCompletion` retains the last observed completion/error
instead of erasing that outcome when a later song starts.

Snapshot events retain sequence/revision identifiers and original HAL
timestamps, so a reader arriving after an event can distinguish it from the
last event it saw. Controller internals remain private. These event records
support eventual UI ownership; they are not a queue of every past transition.
The last event of each kind persists with a global event sequence and HAL
timestamp: capture, confirmation ready, timeout, confirmed song, pause, resume,
completion, playback error, presentation cancellation and accepted-volume
change. Repeated instances of the same kind coalesce, so this is a current-state
presentation mechanism rather than an event history. Volume events retain their
overlay eligibility. The wrap-aware sequence helper orders nearby revisions.
The current UI still uses its existing calls from the single owner, without
changing visible behavior.

The eight-song selector remains `000` through `111`, including Song 7 for `110`
and Song 8 for `111`; unused Song 9/10 definitions are not added to it.
`HAL_GetTick()` remains authoritative for debounce, long-UP, confirmation,
timeout notices and volume overlays. The five-second window starts only after
all captured buttons are released, and expiry wins at the existing deadline.

No codec, LCD or player UART mutex, queue, event group, software timeout timer,
newlib reentrancy, tickless idle or idle sleep is activated. The verified kernel
config, build script, clock tree, M4F flags, tick/exception algorithms and audio
IRQ priority 5 remain unchanged. The Stage-2 player and bare-metal recovery
environments remain available.

All four environments build without warnings or errors. Stage 3 uses 48,292
bytes of RAM and 43,360 bytes of Flash. All five existing native suites and the
new state suite (9 groups / 196 checks under C++11 and C++17) pass. The 14
protected files, task/support sources, kernel config and build script remain
byte-identical. Source/ELF checks retain one SysTick, one HAL tick increment,
correct SVC/PendSV bindings, DMA priority 5 and the same 32 KiB kernel heap.
Only the application state mutex is newly created.

Detailed Stage-3 build evidence and commands are retained in
[freertos_testing.md](freertos_testing.md). The coding agent did not upload the
firmware; the user subsequently tested it and approved Stage 3 complete. The
apparent Nocturne continuation was confirmed to be an intentional internal rest,
with no completion message before continuation. No song/completion fix was made.
No exact Stage-3 stack/heap readings were supplied with that approval; Stage-2
readings cannot be presented as measured Stage-3 headroom.

Stages 3, 4 and 5 have passed their physical gates. The Stage-3 recovery image
retains the architecture described above. Current Stage-5 firmware is frozen;
USER_BUTTON work awaits the physical pushbutton, and Stage 6 is not authorized.
