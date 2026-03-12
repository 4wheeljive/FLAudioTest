## VibeDetector: Mid/Treble still pinned at 1.00 after latest fix (build b953de01)

### Summary

The mid and treble relative levels (`getVibeMid()`, `getVibeTreb()`) remain stuck at exactly `1.00` on real hardware with live audio. This is unchanged from the original report. The bass fix (AttackDecayFilter in `fe389a4`) did work, but the latest build (`b953de01`) reverted to a slow symmetric EMA, and the mid/treble problem persists.

### What we're seeing

Serial output with music playing:

```
vB=1.43  vM=1.00  vT=1.00
vB=0.67  vM=1.00  vT=1.00
vB=2.11  vM=1.00  vT=1.00
vB=0.38  vM=1.00  vT=1.00
```

Bass varies. Mid and treble are locked at 1.00 regardless of audio content.

### Why the current algorithm can't work for mid/treble

The normalization chain is:

```
mAvg[i] = mAvg[i] * rate + mImm[i] * (1 - rate)        // short-term EMA
mLongAvg[i] = mLongAvg[i] * 0.992 + mAvg[i] * 0.008    // long-term EMA
mImmRel[i] = mImm[i] / mLongAvg[i]                      // relative level
```

This is mathematically correct for the MilkDrop algorithm, but the issue is that **mid and treble energy in real music is relatively continuous** — unlike bass, which has sharp transients between near-silence and loud kicks.

For a continuous signal, `mAvg` closely tracks `mImm` (because the short-term EMA converges quickly on a steady input). Then `mLongAvg` closely tracks `mAvg` (because it's a slow EMA of an already-smooth signal). The result: `mImm / mLongAvg ≈ 1.0` at all times.

For bass this works because bass energy is **impulsive** — large spikes followed by near-silence create a meaningful gap between `mImm` and `mLongAvg`.

### Why the in-silico tests don't catch this

The AI-assisted verification (comment [#4016326863](https://github.com/FastLED/FastLED/issues/2193#issuecomment-4016326863)) used assertions like:

```
✅ Bass level range > 0.05
✅ Total dynamic range > 0.1
```

These thresholds are far too loose. A range of 0.05 is invisible on an LED display. The test would pass even if mid/treble varied between 0.99 and 1.01 — which is exactly what happens. The test proves the values aren't *bit-identical*, but that's not the same as having usable dynamic range for visualization.

### The core issue: same normalization strategy can't serve all bands

The MilkDrop algorithm was designed for desktop visualizers driven by line-level stereo audio with high SNR. On an ESP32 with an I2S MEMS mic:

- **Bass** (20-3650 Hz): Dominated by impulsive content (kicks, bass notes). Large spikes, quiet gaps. Self-normalization works well.
- **Mid** (3650-7350 Hz): Relatively continuous energy (vocals, harmonics, sustained instruments). The signal doesn't have the spike/silence pattern that creates divergence between immediate and long-term averages.
- **Treble** (7350-11025 Hz): Even more continuous (cymbals, sibilance, noise floor). Essentially steady-state energy with minimal variation.

### Suggestions (same as previous report, not yet implemented)

1. **Feed `mImm` into `mLongAvg` instead of `mAvg`** — the immediate value has more variation than the already-smoothed average, so the long-term tracker would see actual peaks rather than a flattened signal
2. **Use per-band smoothing time constants** — mid/treble need much slower short-term decay (e.g. `rate = 0.8` instead of `0.5`) to let `mAvg` lag behind `mImm` enough to create visible relative variation
3. **Different normalization strategy for mid/treble** — e.g. running min/max with slow decay instead of EMA, which would track the actual observed range rather than converging on the mean
4. **Add raw/absolute outputs alongside relative** — even if the self-normalizing algorithm is faithful to MilkDrop, exposing `mImm` directly (or a simple 0-1 normalized version) would let visualizers use whichever representation works for their use case

### Test setup

- Hardware: ESP32-S3 (Seeed XIAO), INMP441 I2S mic, 32x48 WS2812B matrix
- Build: `b953de01ed160c6983cd9bf89d5536443cf7a427`
- Test code: Three horizontal bar zones driven by `getVibeBass()`/`getVibeMid()`/`getVibeTreb()`, filling proportionally from center. Serial logging every 500ms confirms mid/treble values are 1.00.
- Audio source: Various music genres at moderate volume
