## VibeDetector Follow-up Report: Mid and Treble relative levels stuck at 1.00

### Setup
ESP32-S3 (Seeed XIAO), I2S INMP441 mic, 32x48 WS2812B matrix, latest FastLED master (with AttackDecayFilter fix applied).

### Observation
Serial output shows `vM=1.00` and `vT=1.00` constantly, regardless of audio content. Bass now has good dynamic range after the running-max fix. Mid and treble do not move at all.

### Analysis

The normalization divides `mImm[i]` by `mLongMax[i]`, where `mLongMax` is fed by `mAvg[i]`:

```cpp
mLongMax[i] = mLongMaxFilter[i].update(mAvg[i], dt);
mImmRel[i] = mImm[i] / mLongMax[i];
```

The short-term smoothing uses asymmetric attack/decay:

```cpp
rate = (mImm[i] > mAvg[i]) ? 0.2f : 0.5f;   // fast attack, slow decay
mAvg[i] = mAvg[i] * rate + mImm[i] * (1.0f - rate);
```

For **bass** this works because bass energy is impulsive — large spikes followed by near-silence — so `mImm` regularly diverges from `mAvg` and `mLongMax`.

For **mid and treble**, the energy is relatively **continuous** — it doesn't spike and drop the way bass does. So `mAvg` closely tracks `mImm`, and `mLongMax` (running maximum of `mAvg`) closely tracks both. The result is `mImm / mLongMax ≈ 1.0` at all times.

### Root cause

The running maximum normalizer (`mLongMax`) is being fed the **smoothed** value (`mAvg`) rather than the **immediate** value (`mImm`). Since `mAvg` is already heavily smoothed, the running max of a smooth signal stays very close to that signal, collapsing the dynamic range to ~1.0 for bands that don't have sharp transients.

### Suggestions

1. Feed `mImm` (immediate) into `mLongMaxFilter` instead of `mAvg`, so the running max tracks actual peaks rather than smoothed peaks
2. Use separate smoothing time constants per band (mid/treble may need slower decay to create visible variation)
3. Or normalize by a longer-term average rather than running maximum for mid/treble, since these bands have different temporal characteristics than bass
