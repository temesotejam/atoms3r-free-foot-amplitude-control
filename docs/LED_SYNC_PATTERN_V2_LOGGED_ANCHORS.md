# LED_SYNC_PATTERN_V2_LOGGED_ANCHORS

This retake uses a synchronization LED whose physical state is recorded in every rwlog sample.

## Start signature

```text
OFF 1000 -> ON 600 -> OFF 300 -> ON 600 -> OFF 300 -> ON 1200 -> OFF 1000 ms
```

The logger begins before this sequence. `t_test_ms=0` occurs immediately after its final OFF segment.

## Measurement anchors

At 2500 ms after `t_test_ms=0`, and every 5000 ms after that:

```text
ON 300 -> OFF 300 -> ON 600 ms
```

Each anchor has `sync_event_id=5`. The exact ON/OFF condition is in `led_state`.

## End signature

```text
OFF 1000 -> ON 1200 -> OFF 300 -> ON 600 -> OFF 300 -> ON 600 -> OFF 1000 ms
```

The logger continues through this sequence. Use the start, periodic, and end transitions to map video time to rwlog time.