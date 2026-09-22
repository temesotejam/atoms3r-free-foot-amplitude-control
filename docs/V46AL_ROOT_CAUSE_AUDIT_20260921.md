# V46al root-cause audit — 2026-09-21

## Baseline

The protected reference is the verified V46ak / 0.46.36 behavior.

- stable repository: `temesotejam/atoms3r-amplitude-control-v46ak-stable`
- verified development snapshot: `269281ab6b090778a7c82f6abfadb208ce27757b`
- attitude/control baseline: V46aj fixed 3 ms compensation + V46ak observation only
- RWLOG download transport is frozen separately by `tools/test_v46ak_download_freeze.py`

No firmware-output change is introduced by this audit.

## First suspect: V46al active previous-peak correction

V46al was the first post-V46ak revision that changed the actual control decision.
V46am and later revisions were primarily download/transport work and are not part of
this first control root-cause step.

Historical V46al correction:

```text
correction = c_side + k_side * (A_prev - 8 deg)
A_free_v46al = A_free_v46ak + clamp(correction, -0.70, +0.70)
```

It is active only after 10 s, at target 8 deg, and inside the measured previous-peak
support.

## Offline replay on verified stable capture

Capture: `20260921_124531_257_8a65`.

Using the stable zero-cross and peak metadata, without changing Q:

- 10-30 s evaluated zero-cross events: 46
- V46al support condition met: 43
- stable prediction residual RMSE on the 43 applied events: about 0.550 deg
- after adding the historical V46al correction to the prediction: about 0.353 deg

Therefore the simple hypotheses "correction sign is completely reversed" and
"next-side selection is completely reversed" are not supported by this capture.

However, the correction is large when translated into the existing Q coordinate:

- mean absolute implied Q change: about 1.09 mA*s
- next side +: mean implied Q change about -1.22 mA*s
- largest reduction: about -2.41 mA*s
- next side -: mean implied Q change about +0.23 mA*s, with individual events near +2.06 mA*s

The existing side integral controller is already correcting the same persistent error.
On this stable run, after 10 s the + side integral is roughly -0.45 to -0.90 mA*s,
while V46al often asks for an additional Q reduction in the same direction.

This makes interaction/double-correction with the existing integral loop a stronger
root-cause candidate than a basic sign bug.

## Next rule

Do not re-enable V46al active correction yet.

First use `tools/analyze_v46al_counterfactual.py` on additional known-good V46ak
captures. Confirm:

1. correction direction and magnitude by side,
2. overlap with `integral_side_mA_s`,
3. support-boundary/clamp events,
4. whether the apparent prediction improvement remains across runs.

Only after that should an on-device shadow implementation be considered. Actual Q,
pulse width, MEKF, 3 ms compensation, RWLOG binary layout, and the frozen download
transport stay unchanged during this root-cause phase.
