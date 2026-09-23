# Phase 1N: Run-time one-shot coexistence validation

Date: 2026-09-23

## Purpose

Phase 1M proved that the AtomS3R camera can remain idle with the receiver and XCLK off,
capture one QVGA grayscale frame on demand, and return to the idle state while the WebUI remains usable.

Phase 1N answers the next question: can that same one-shot acquisition occur during an
actual Autonomous Energy Control run without disturbing the established IMU/control path?

This phase is deliberately **not** image processing. There is no ArUco, optical flow,
feature tracking, foot-angle calculation, or continuous camera stream.

## Trigger

During an Autonomous Energy Control run, the lower-priority Arduino/HTTP task watches the
thread-safe RunControl snapshot. On the first measurement sample at or after:

- measurement elapsed time: **10,000 ms**

it performs exactly one camera acquisition with the existing 500 ms timeout.

A fresh validation record is armed when ownership is transferred to the RunControl worker.
No second capture is attempted in the same run.

## Priority and ownership boundary

The existing priority hierarchy is retained:

- BMI270 reader: priority 6
- RunControl worker: priority 4
- Arduino / HTTP / Phase 1N camera caller: priority 2
- camera internal `cam_task`: priority 3

The camera call therefore blocks only the priority-2 task. RunControl and BMI270 acquisition
remain able to preempt it. Phase 1N does not call `runner.update()`, `runner.serviceFast()`,
or read live ExperimentRunner state from the HTTP task.

The worker snapshot was extended only with observation fields:

- Autonomous-mode flag
- pulse-active flag
- measurement elapsed time

A compact RunControl health snapshot exposes only timing counters.

## Recorded capture-window checks

Before and immediately after the one-shot, Phase 1N records:

- camera frame and failure counters
- capture duration and frame byte count
- receiver/XCLK state after capture
- IMU acquisition healthy/stale state
- RunControl step count across the capture
- sample-completion deadline violations before/after
- runner-work deadline violations before/after
- whether a motor pulse was active at the trigger

A single serial record beginning with `PHASE1N,` is emitted after the attempt.

The same record is available as JSON at:

`http://192.168.4.1/camera-run-validation`

`window_pass=true` means only that the immediate capture-window checks passed. It is not
the final experiment verdict.

## Final acceptance

After the run, use the normal RWLOG diagnostics as the authoritative full-run audit.
The run should still satisfy the established acquisition/control criteria, especially:

- no IMU acquisition fault
- `queue_drops = 0`
- `delivery_sequence_gaps = 0`
- no unexpected >10 ms acquisition gap
- no new RunControl sample-completion deadline violations attributable to the camera window
- no ESTOP
- one camera frame added
- no camera failure
- receiver and XCLK both off after the capture
- WebUI remains reachable after the capture

If these pass, the next phase can increase camera usage cautiously (low-frequency repeated
one-shots before any ROI or feature-tracking work).

## RWLOG download recovery

A real Phase 1N run exposed a transport weakness that the camera-only Phase 1M
check did not exercise: the previous direct writer treated any partial TCP write
as a fatal download failure.

The RWLOG binary format, header, CRC semantics and converter are unchanged.
The native browser attachment route is retained, with these transport-only changes:

- maximum write attempt: 1460 bytes,
- partial writes advance by the number of bytes actually accepted,
- zero-progress writes retry while the client remains connected,
- 15 s no-progress timeout,
- `Connection: close`,
- browser status polling suppressed for 60 s after download starts,
- serial diagnostics: `RWLOGDL,prepare_begin`, `prepare_end`, `stream_end`.

No Range/resume protocol and no fetch-to-Blob buffering are used.
