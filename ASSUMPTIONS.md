# OPT4001 Implementation Assumptions

This file records the places where the device notes were incomplete, awkward, or
open to more than one implementation style. The library chooses the safest option
that stays consistent with the other I2C driver repositories in this workspace.

## Assumptions

1. `Config::packageVariant` defaults to `PackageVariant::SOT_5X3`.
   This is the broader package variant and matches the address-selectable part.
   PicoStar users should set the variant explicitly when they need the tighter
   lux LSB and fixed-address validation.

2. `begin()` accepts only stable modes: `POWER_DOWN` or `CONTINUOUS`.
   The OPT4001 datasheet documents one-shot operating modes, but this library does
   not start a one-shot conversion implicitly during initialization. One-shot work
   is started explicitly through `startConversion()` or `readBlocking()`.

3. Fresh-sample readiness requires hardware evidence.
   Elapsed conversion time is only a gate for bounded register polling. Fresh
   reads require the conversion-ready flag or a sample-counter advance from the
   previous accepted fresh sample. A polled SOT-5X3 INT level is only a hint to
   check the counter: it cannot reliably capture a roughly 1 us conversion/FIFO
   pulse or distinguish another source holding a shared line asserted. The
   application owns any interrupt capture and signaling.

4. High-speed I2C entry and SMBus alert response are not wrapped as dedicated
   driver APIs.
   The datasheet documents both, but they are controller-level bus procedures
   rather than normal per-device register transactions. The driver keeps those
   behaviors at the transport/application layer instead of pretending they are
   device-local operations.

5. Window-transmission and similar optical calibration factors are left to the
   application layer.
   The application notes discuss those corrections, but they depend on system
   mechanics such as cover-glass transmission and enclosure geometry, so the
   library does not bake them into the core lux conversion path.

6. Full-scale and resolution helpers treat `Range::AUTO` as the maximum range.
   In auto-range mode the sensor can land on any exponent from 0 to 8, so the
   driver cannot know a single fixed full-scale or resolution value from config
   alone. Helpers such as `getCurrentFullScaleLux()` and
   `getCurrentResolutionLux()` therefore return the conservative worst-case
   values for range 8 when the configured range is `AUTO`.

7. INT-pin hardware trigger is not wrapped as a core driver helper.
   The datasheet documents using the SOT-5X3 INT pin as a one-shot trigger
   input, but the repo documentation does not pin down a portable pulse-driving
   contract, and the driver intentionally does not own GPIO output policy.
   Applications can still configure `INT_DIR = PIN_INPUT` and implement the
   actual pulse at the board layer.
