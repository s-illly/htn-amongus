# HTN Among Us

A physical, badge-based version of Among Us. Each player wears an ESP32-C3
badge with buttons, an LED strip, a small screen, a tilt sensor, and an NFC
reader. There's no laptop, no router, and no game server: badges talk to each
other directly over ESP-NOW, and whichever badge starts a round becomes the
host for that round, running the game's state machine and broadcasting results
to everyone else.

Crewmates win by tapping their badge on NFC task tags placed around the room
and completing the minigame each one launches. Imposters win by killing crew
(walk up close and hold B) or by sabotaging the shared task bar.

## Hardware

| Component | Interface | Pins |
|---|---|---|
| Buttons (A, B, HOME, D-pad, AUX1) | 74HC165 shift register | DATA=7, LOAD=20, CLK=21 |
| START button | Dedicated GPIO, active-low, internal pull-up | GPIO 9 |
| LED strip (6x WS2812/NeoPixel) | Single-wire | GPIO 3 |
| Display (ST7789 TFT) | SPI | MOSI=10, SCLK=1, CS=2, DC=0, RST=4 |
| IMU (SC7A20 accelerometer) | I2C | SDA=5, SCL=6, addr 0x19 |
| NFC reader (MFRC522) | I2C | addr 0x26 |

Board: `esp32-c3-devkitm-1`, Arduino framework, built with PlatformIO.

## Physical controls

There are **8 momentary buttons behind the shift register** (A, B, HOME,
UP/DOWN/LEFT/RIGHT, and AUX1), **one dedicated momentary button** (START, its
own GPIO since it's also the boot-mode strapping pin), and **one maintained
toggle switch** (read as AUX1's bit, but physically a switch that stays where
you leave it rather than springing back).

That distinction matters for how the firmware reads each one:
- The 8 shift-register buttons and START are all edge-detected in software
  (`isButtonPressed()` fires once on press, `isButtonHeld()` is true for as
  long as it's down) -- debounced over a ~10ms poll in `buttons.cpp`.
- AUX1 is read as a live level, not an edge -- the firmware just mirrors
  whatever position the switch is currently in.

### AUX1 -- NFC reader on/off

The AUX1 switch turns the NFC reader on and off. With it **off**, the MFRC522
sits in low-power standby (antenna off, PowerDown bit set, registers
retained). Flip it **on** and the reader wakes quickly and starts polling for
task tags; the scanned UID is handed to the game. Leave it off when you're not
actively doing a task so a stray tag doesn't pop a minigame up.

### Buttons, by game phase

| Phase | Button | Action |
|---|---|---|
| **Lobby** | UP / DOWN | Move the settings cursor (imposter count, discuss/vote timers, meeting limit) |
| | LEFT / RIGHT | Change the selected setting's value (synced live to every badge) |
| | START | Start the game. **Whoever presses this becomes the host for the round, and is guaranteed to be an imposter.** |
| **Playing** | HOME (tap) | Call an emergency meeting (only while alive) |
| | START (hold) | Reveal your role card: your role, your fellow imposters (if any), and -- for imposters -- a live kill-cooldown countdown |
| | B (hold) | Context-sensitive: if you're an imposter, off cooldown, and a killable crewmate is in proximity range, this kills them. Otherwise, if a dead player's badge is in proximity range, this reports the body and calls a meeting. If neither applies, holding B does nothing. |
| **In a task** | START | Back out of the minigame (no progress is saved) |
| | A (hold 1s) | Imposters only: **sabotage** -- knocks one step off the shared task bar and closes the minigame. Once per hold. |
| **Gather** (meeting called) | A | Move to discussion |
| | B | Cancel, return to playing |
| **Discuss** | B | End discussion early, move to voting |
| **Voting** | LEFT / RIGHT | Cycle your vote target (living players, or SKIP). Dead players can't vote and can't be voted for. |
| | A | Cast your vote |
| **Game over** | START | Return to the lobby (host only) |

Kills and body reports aren't manually aimed -- there's no target-cycling
button. The nearest qualifying badge in ESP-NOW proximity range (an RSSI
threshold, roughly "a couple meters," tuned via `KILL_RSSI`/`REPORT_RSSI` in
`game.cpp`) is picked automatically when you hold B.

## Tasks

There are four NFC tags, each mapped 1:1 to a minigame by its UID (set in
that minigame's file under `src/tasks/`). With AUX1 on, tap a tag during play to
launch its minigame:

| Tag | Minigame | How to win |
|---|---|---|
| Wires | Button sequence | Enter the shown sequence of arrows/A/B (5 inputs), 3 rounds |
| Garbage | Tilt maze | Tilt the badge to roll the trash around randomized walls into the chute, 3 times |
| Window wipe | Shake to clean | Hold the badge upright and fan-wipe it; harder wipes clean faster. Done at 85% |
| Rhythm | DDR | Hit the matching D-pad direction as arrows cross the line; 8 hits |

Rules:
- The HUD shows a **shared task bar** for the whole crew. The host sets the
  target to `TASKS_TO_WIN` (2) per crewmate, and each crewmate is only
  credited for up to that many, so replaying minigames can't pad the bar.
  **A full bar wins the game for the crew.**
- Each tag can only be completed once per badge per game; scanning it again
  shows "ALREADY COMPLETED".
- Only living crewmates get credit. Imposters can play tasks to blend in, but
  completing one does nothing; holding A for a second mid-task sabotages
  instead.
- Any minigame times out after 20s, and any meeting cancels an in-progress task.

To use your own tags, flip AUX1 on, scan each one, copy the UID printed on the
serial monitor into the matching task file in `src/tasks/`, and reflash.

### Adding a task

Each minigame is a single file in `src/tasks/` that defines a `TaskDef`
(see `include/task_api.h`):

```cpp
static void start() { /* reset state; next run() redraws from scratch */ }
static void run()   { /* one frame: read input, draw; call taskFinish() when won */ }

extern const TaskDef TASK_MY_GAME = { "MY GAME", "04AABBCCDD2A81", start, run };
```

Then declare it and add it to `TASK_LIST` in `src/tasks/task_list.cpp`.
That's it -- the test menu, NFC lookup, and "ALREADY COMPLETED" notice all read
from the list. Shared helpers (`taskFinish()`, the palette, `drawGlyph()`)
come from `task_api.h`; drawing primitives (`gfx*`) from `display.h`.

### Task test mode

Hold **A** while the badge boots to enter a solo test harness: a menu that
launches any of the four minigames directly (UP/DOWN to pick, A to play, START
to exit a game). No tags, other badges, or networking needed. (START can't be
the trigger since GPIO 9 held at reset puts the chip in download mode.)

## Networking

No WiFi router, no MQTT broker, no laptop -- badges connect directly over
ESP-NOW on a fixed channel (`ESPNOW_CHANNEL` in `espnow_radio.h`), which
every badge must share since there's no access point for it to be inherited
from. The radio runs at max TX power with WiFi sleep disabled so it never
misses packets. Two independent message types ride the same radio, tagged so
a shared receive callback can tell them apart:

- **Proximity pings** -- each badge broadcasts its short ID every 200ms;
  RSSI on each received ping (read straight off the ESP-NOW receive
  callback) drives the "who's nearby" checks used for kills and body
  reports.
- **Game-state mesh** -- a small TTL-flood protocol: every game message
  (colors, role assignment, phase changes, alive list, votes, results, task
  progress) is relayed by whichever badges hear it, up to a bounded number of
  hops, so the whole group stays in sync without needing every badge to be
  in direct radio range of every other one. A dedup cache stops this from
  turning into a broadcast storm.

Whichever badge presses START in the lobby becomes the **host** for that
round: it owns the phase timer, deals roles, validates every
kill/report/vote/task/sabotage request, and broadcasts the results. Every
other badge just follows the host's messages.

A few details that keep rounds consistent:
- **Colors** -- every badge gets a unique color, picked deterministically
  from its ID so all badges agree even before the game starts. At START the
  host broadcasts the final map (`COL:`) and locks it for the round.
- **Late joiners** -- if a badge's presence hadn't reached the host when
  roles were dealt, it keeps asking (`ASK:`) and the host folds it in as
  crew, adding its share to the task bar.

## Building & flashing

```
pio run                      # build
pio run -t upload            # flash (add --upload-port /dev/cu.XXXX if it can't autodetect)
pio device monitor           # serial log (115200 baud)
```

Every badge should be flashed from the same firmware build, since the
ESP-NOW channel and task tag UIDs are hardcoded and shared -- there's no
per-device configuration needed.

## Project layout

| File | Responsibility |
|---|---|
| `main.cpp` | Boot sequence, test-mode entry, AUX1/NFC handling, and the main loop wiring everything together |
| `espnow_radio.*` | Shared ESP-NOW radio bring-up (fixed channel, no AP), demuxes the one physical receive callback by packet type |
| `broadcast.*` | TTL-flood mesh transport for game-state messages |
| `espnow_prox.*` | Proximity/RSSI ranging used for kills and body reports |
| `players.*` | Player identity, color assignment, roster, and per-player state (alive, role, meetings) |
| `game.*` | The host-authoritative game state machine: phases, roles, votes, kills, task progress, sabotage |
| `tasks.*` | Task framework: which minigame is running/done, NFC UID lookup, timeout |
| `task_api.h` | The `TaskDef` interface and shared helpers every minigame uses |
| `tasks/task_list.cpp` | The list of every minigame in the game |
| `tasks/*.cpp` | One file per minigame (wires, garbage, window wipe, rhythm) |
| `tasktest.*` | Solo task test harness (hold A at boot) |
| `buttons.*` | Shift-register + START button reading and debouncing |
| `leds.*` | The 6-LED status strip |
| `display.*` | All TFT screens (lobby, HUD + task bar, role card, voting, results, etc.) and drawing helpers for the minigames |
| `imu.*` | Accelerometer reads (tilt and shake) used by the Garbage and Window wipe minigames |
| `nfc.*` | MFRC522 driver: init, low-power standby, wake, UID scanning |
