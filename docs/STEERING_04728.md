# 0.47.28: cycle steering and RWLOG v53

This revision implements the requested yaw/asymmetry feedback in the existing
30-second workflow. No extra magnetic test, calibration, network request or
sensor transaction is added to the control path.

## Feedback

- Independent body-to-world gyro quaternion, initialized from the current
  accelerometer at START_SYNC. Raw startup bias remains fixed. Mapping is
  `(-gx, 0.908911*gy, -gz)` after subtracting that raw bias.
- Timestamp-based midpoint-rate integration at each fresh gyro sample, normalized
  every update. No MEKF adaptive bias, MEKF yaw or subsequent acceleration update.
  Heading is unwrapped; positive is the observed left-turn convention.
- The same actual negative peak bounds each full cycle. Heading is captured
  when the amplitude candidate is updated, not at its later acceptance time.
  Exactly one positive peak must lie between the two negative peaks.
- Measured difference `D = A+ - A-` and cycle yaw rate use EWMA alpha 0.35.
  The first 10 seconds establish a baseline without steering.
- Desired difference starts at measured D; it is **not initialized to zero**.
  Update it by `0.12 * deadband(yaw_rate, 0.3 dps) * cycle_seconds`.
  The target offset advances by `0.15 * (desired_D - measured_D)` each cycle,
  bounded to 0.08 degrees per cycle and ±1 degree total.
  Requested D is bounded to ±4 degrees and its lead over measured D to ±0.5.
- Targets are `A0 + delta` and `A0 - delta`, preserving their mean. Each issued
  command's target is latched for its later peak error and side-integrator update.
- When both actuator directions needed for a correction are blocked, or the
  target offset reaches its bound, hold the offset and track desired D back to
  achieved D. A single limited side does not prevent the other side contributing.
  Reversing rotation can unwind the correction.
- Accept cycle durations 0.3–3 seconds. Nonfinite heading or gyro intervals over
  10 ms invalidate gyro heading until the next run and freeze steering. Existing
  acquisition faults and emergency-stop handling remain authoritative.

The objective is zero net rotation per cycle. It does not return the walker to
its initial heading, identify world north, or force measured asymmetry to zero.
The sign is based on the supplied experiments; the gains and physical effect
need confirmation in the next ordinary run. Camera foot angles remain observations.

## Timing and preserved control

Gyro work is inside the existing Filter stage and the same 2.5 ms completion
measurement. It uses no allocation, formatting or I/O. Steering updates only
at a completed cycle. The existing MEKF sway estimator, autonomous solver,
3 ms projection, target choices, startup kick, 300 mA / 100 ms normal-pulse
limits, 30 s duration and LED/network workflow are preserved. MEKF internals
still include a quaternion because tilt estimation and the tilt stop need it.

The added magnetic factory-trim read, raw-byte copy, status, log and display
were removed. Pre-0.47.27 BMI270/M5Unified bus behavior was retained; this is not
a sensor-hub reconfiguration. Offline magnetic conversion remains for old v52
files and retains its Bosch attribution.

## RWLOG v53

112-byte packed samples replace 274-byte v52 samples. Retained:
raw acceleration/gyro, startup gyro bias, MEKF absolute/measurement/detector
pitch and quality, actual current/freshness and pulse charge, output/pulse/LED
state, IMU timing, independent heading and steering state. Removed:
MEKF quaternion/yaw reconstruction, magnetic data, unused comparison filter
and beta series, duplicate zero references and trial/config fields.

Foot frames, pulse timing, solver audit and autonomous events use a lossless
`{"fields":[...],"rows":[...]}` representation. Precision, nulls, complete
event counts and solver float bit patterns are preserved. Dormant protocol
metadata is pruned. Existing control-latency overrun details and acquisition
diagnostics remain. A fixed 768 KiB PSRAM metadata buffer has a 640 KiB hard
budget; failure cannot publish partial JSON.

Python conversion expands tables back to dictionaries and supports historical
versions. Web downloads expand foot tables before CSV export. CRC and interrupted
download resume are unchanged. A 32-bit centidegree heading preserves turns
larger than 327 degrees. Steering reason codes: 0 waiting, 1 settling, 2 active,
3 invalid, 4 saturated. Side targets can be reconstructed from mean and delta;
event rows contain the actual target used for each command/response.

For the two preceding 3,619 / 3,598-sample runs, applying the new layout and
table schemas estimates about 0.607 / 0.606 MB instead of 1.614 / 1.615 MB.
These are size estimates, not new hardware measurements.

## Validation

- Production control at zero correction matches frozen 0.47.24 in 20,000
  decision cases and 32,000 sequential motion samples (22,663,895 serialized
  comparison bytes; NaNs canonicalized).
- Production nonzero side target enters the bounded pulse solver and remains
  latched at peak evaluation even after the outer correction changes.
- Synthetic 3D sway with known heading verifies zero, left, right and >360-degree
  cases; clock wrap, duplicate timestamps, invalid inputs and missing samples.
- Illustrative plant tests cover both correction signs, mean/step/bound limits,
  nonzero asymmetry at zero yaw, saturation and reversal. These do not establish
  physical stability.
- Real maximum-capacity C++ RWLOG serialization, all retained-field conversions,
  compact table round trips, old-format conversion, CRC corruption rejection,
  and browser transfer interruption/resume with 768 foot CSV rows.

The next usual 10-degree run must check measured turn direction/angle, steering
delta and peak response, gyro health, actuator limits and the same 2.5 ms
deadline/IMU-drop metrics. Host tests and successful builds cannot verify these
hardware outcomes.
