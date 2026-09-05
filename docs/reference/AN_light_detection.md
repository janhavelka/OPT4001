# Application Note: How Intelligent Sensors Expand Our Detection of Light

> **Source:** Texas Instruments SSZTD81 (April 2025)
> **Relevance:** Application context only. This is a family-level marketing
> article. It names the OPT4001 exactly once, in the caption of Figure 4
> ("The OPT4001 ALS in displays"); its technical statements are made about the
> OPT4003-Q1 and OPT4041. Nothing here overrides
> [OPT4001_datasheet.md](OPT4001_datasheet.md), and it contains no register,
> timing, or electrical data for the OPT4001.

---

## What The Source Actually Says

- **High IR rejection matters behind dark windows.** Dark materials often
  transmit IR while blocking visible light, skewing sensors with poor IR
  rejection. TI makes this argument for the **OPT4041** in this article; the
  OPT4001 datasheet independently specifies 0.2 % response at 850 nm.
- **Day/night detection** is a common ALS application: outdoor lighting, camera
  systems, and headlights.
- **Conversion-time flexibility** — 12 steps from 600 µs to 800 ms — is cited
  here for the **OPT4003-Q1**. The OPT4001 datasheet (Table 8-4) independently
  documents the same 12 steps.
- **Automotive tunnel entry/exit** is called out as the case where rapid
  reaction time matters for safety.
- **PicoStar™ package** with a bottom-facing sensor enables placement under thin
  display bezels. PicoStar dimensions are *not* given in this article — see
  [AN_picostar_package.md](AN_picostar_package.md).
- **OPT4041** (dual-channel) can detect infrared LED illumination for camera
  night-vision — a different product, same family design principles.
- TI provides **in-line calibration support** with dedicated light sources for
  end-of-line testing.

## Relevance To This Driver

- Both package variants need their own lux LSB constant (312.5e-6 vs 437.5e-6) —
  implemented as `PackageVariant`.
- Threshold detection and fault count should be fully exposed so wake-on-event
  applications can use them — implemented as the threshold and INT API.
- Window-transmission compensation is an application-layer concern; see
  [ASSUMPTIONS.md](../../ASSUMPTIONS.md).
