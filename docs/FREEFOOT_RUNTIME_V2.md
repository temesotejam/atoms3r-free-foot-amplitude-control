# Free-foot Runtime V2 — 0.47.0

This is the integrated implementation in `atoms3r-free-foot-amplitude-control`.
Base: `39a2ff287e3907aabf5fcd408987ae1dda2fec56`. The fixed-foot repositories are
reference sources only. They are not modified.

## Preserved behavior and added observation

- V46al-R2 previous-peak residual correction, rate baseline, MEKF, current model,
  pulse solver, Ki and physical-side conventions are retained.
- Fixed 3 ms crossing compensation, 300 mA / maximum 100 ms, 30 s measurement,
  START/MID/END LED synchronization, actual current and wheel-speed diagnostics.
- BMI270 gyro 400 Hz / accelerometer 200 Hz, 1 ms host polling, selective driver
  patch and queue/fault/backlog guards. These are configured rates, not a claim
  that hardware deadlines were met in this integrated build.
- Added observation-only right/left foot angles. No foot result is read by the
  controller's solver, peak detector, current model or motor-output gate.

## Runtime ownership

| Owner | Core / priority | Responsibility |
|---|---|---|
| BMI270 reader | 1 / 6 | Acquisition, timestamped bounded queue |
| Permanent controller | 1 / 4 | Idle/calibration and run states, sole IMU consumer, commands, LED, log sealing |
| Roller I/O | 0 / 4 | Existing motor I2C, current and wheel observations |
| Camera driver task | 0 / 3 | Camera frame receiver; wrapper checked at link time |
| Foot observer | 0 / 1 | Capture/marker detection, one-time zero, foot records |
| Export worker | 0 / 1 | Completed-run metadata and whole-file CRC |
| Arduino/Web | 1 / 2 | Short HTTP handlers and status snapshots |

The controller is permanent. HTTP never calls the live runner or `imu.update()`.
START/CLEAR are mailbox commands; STOP is a separate priority latch that cancels
queued commands. The control owner checks start conditions when executing the
request. No lock contains network I/O, camera capture, or solver work.

Camera SCCB still borrows I2C0 **only during boot**, before Roller initializes
that port. Runtime capture does not reconfigure sensor registers or touch I2C0.
Receiver/XCLK stay active during idle alignment and START/MEASURE/END, avoiding
20 ms warmup on every frame. After FINISHED/ESTOP the observer stops the receiver
and gates XCLK; a frame already in flight may finish first. Framebuffers remain
allocated. Internal system/Wi-Fi task priorities are unchanged.

## Memory and logs

The former 190,144-byte global event arrays are one explicitly allocated PSRAM
block. Sample memory is 5 MiB (20,321 rows at 258 bytes), leaving room for export.
Even 30 s continuously at the fastest 2 ms cadence plus 10 s at 20 ms requires
15,500 rows, before the remaining margin for event-triggered rows. This budget
is for the current 30-second Autonomous workflow.

Foot storage holds 768 frames in PSRAM; a 40-second run at the 15 fps target
needs about 600. Overflow is explicit in status and metadata. Bad frames produce
invalid records, not stale valid angles. All large metadata appends target a
fixed-capacity 2 MiB PSRAM allocation. Allocation/capacity failure prevents export
publication; there is no silent partial-JSON success. Internal RAM is reserved
for the IMU, RTOS stacks, camera DMA and network services.

RWLOG remains version 51: 110-byte header, existing 258-byte samples and final
CRC32. Full control events and diagnostics are restored in metadata, with
`foot_observation` and `foot_frames` added. Terminal partial timing probes are
committed before sealing. No event/sample writes are accepted after sealing.

Export is performed after completion, on the export worker. Prepared bytes are
immutable. START/CLEAR are rejected while preparation runs; START requires an
explicit clear of the previous result. Clear invalidates the prior export token.
Power loss discards the device's RAM log; browser resume does not change that.

| Request | Result |
|---|---|
| POST `/export/prepare` | Idempotent preparation of the completed run |
| GET `/export/manifest` | Phase, boot/generation token, total length, progress, whole-file CRC and filename |
| GET `/export/chunk?token=…&offset=…&length=…` | At most 4096 data bytes plus a 16-byte envelope |

Chunk envelope: little-endian `FCH1`, offset, data length, CRC32 of data. Wrong
identity/range is rejected. Both chunk and final-file CRC are checked by the
browser. Each HTTP socket write has a hard 1.2 s deadline including partial
progress; short writes close the client. Browser fetch timeouts cover body
consumption as well as headers. Status polling recovers automatically.

Validated chunks are cached in IndexedDB. Pause, connection loss or page reload
can resume while the same device log/token remains available. Browsers without
working IndexedDB can resume within the current page using memory cache. The
old single-response `/download/rwlog` endpoint returns an instruction to use the
new page, rather than starting an unbounded transfer.

## Foot calibration and timestamps

Source: `atoms3r-foot-angle-tracker@ac6df8caf59c93956b87cba57521903c25ff9f00`.
The calibrated sparse white-marker detector is retained.

| Lane | Foot | Marker rows | Reference rows | Slope (degrees/pixel) | Calibrated X support |
|---|---|---|---|---|---|
| A / upper | Right | 58, 62, 66, 70, 74 | 38, 42, 46 | 0.167779119 | 42…173 |
| B / lower | Left | 152, 156, 160, 164, 168 | 182, 186, 190 | 0.162645305 | 43.5…177.5 |

`angle = slope * (boot_zero_x - detected_x)`. Positive means X decreases. Image
orientation and marker placement must match the calibration. Angles outside
support are retained with `in_range=false`.

After the 10-second boot guide, zeroing requires gravity-direction error ≤5°,
acceleration norm within 1±0.03 g, angular speed ≤1.5°/s, continuous valid images
for ≥2 s and ≥15 frames. Movement between frames changes the IMU stability epoch
and restarts the average. Missing markers, stale frames and gaps over 300 ms
restart it too. Once locked, zero remains unchanged until reboot.

Logs contain camera driver frame timestamp and delivery time separately. The
frame timestamp is **not claimed to be a verified physical exposure timestamp**.
IMU timestamp, state and LED accompanying a frame are the latest control snapshot
at delivery. Signed run/measurement offsets preserve boundary cases. For precise
video alignment use the existing binary LED anchors and these explicit clocks.
The camera target is 15 fps; actual rate/failures are displayed and recorded.

## Validation and remaining hardware work

`bash tools/test_runtime.sh` exercises actual serializers and runtime helpers:

- Permanent idle/run ownership, busy-command exclusion, STOP precedence, audit
  preservation and 32-bit timestamp wrap.
- Marker lanes/angles, calibration-range flags, continuous zero with intermittent
  movement, missing markers and dropped frames.
- A 7 MiB scatter-read payload requested backwards and repeatedly, range/overflow
  rejection, CRC golden vector, PSRAM allocation and append-capacity failure.
- A maximum-count real RWLOG serialization with 256 peak events, 256 zero-cross
  events, 256 timing records, 128 solver records and 768 foot frames; strict JSON,
  exact final size, CSV conversion and rejection of a corrupted file.
- Header-success/body-stall timeout, browser polling recovery and corrupt/stale
  chunk rejection.
- Existing 12,000-sample acquisition accounting, 269 actual-ImuManager host cases,
  MEKF numerical tests, previous-peak correction tests, RWLOG v44–v51 conversion.

Host doubles are not an ESP32 scheduler, camera, I2C bus, motor or Wi-Fi radio.
Build/CI results do not establish real-time feasibility on hardware. After flash,
use battery power and the device Web UI; USB serial during runs is unnecessary.

1. Cold boot, upright zero and marker alignment. Confirm right/left signs and
   actual fps; save the Web diagnostic JSON if a gate does not complete.
2. Run a 30-second measurement with the browser open, then save RWLOG and video.
   Inspect IMU faults, camera failures, internal/DMA memory and the existing
   2500 µs control/1000 µs acquisition deadline counters without relaxing limits.
3. Interrupt a download, reconnect and resume. Reload the page during download.
   Confirm final CRC success, all foot records and no change of run identity.
4. Clear and repeat; exercise STOP including START_SYNC. Confirm motor-off state,
   retained partial logs, stable zero and absence of accumulating memory loss.

Integrated hardware validation is pending. If target frame rate or deadlines are
not met, the diagnostics identify that condition; this build does not disguise
it as a passing result.
