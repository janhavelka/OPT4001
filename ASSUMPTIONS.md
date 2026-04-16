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

3. CRC verification is implemented as an expected-CRC comparison derived from the
   datasheet syndrome equations.
   The documentation describes zero-syndrome verification, while the driver derives
   the equivalent expected 4-bit CRC nibble from the same equations and compares it
   against the received nibble.

4. Register `0x0B` fixed bits are encoded as raw value `0x8000`.
   The datasheet notes describe bits `[15:5]` as needing to equal field value
   `0x400`. In the full 16-bit register image that field occupies bits `[15:5]`,
   which corresponds to raw register value `0x8000`.

5. Sample readiness in the driver core uses time-bounded register polling.
   The config struct carries optional INT pin hooks for board integration, but the
   driver does not rely on catching the device's very short conversion-ready pulse.
   This avoids baking board-specific GPIO interrupt behavior into the core library.

6. High-speed I2C entry and SMBus alert response are not wrapped as dedicated
   driver APIs.
   The datasheet documents both, but they are controller-level bus procedures
   rather than normal per-device register transactions. The driver keeps those
   behaviors at the transport/application layer instead of pretending they are
   device-local operations.

7. Window-transmission and similar optical calibration factors are left to the
   application layer.
   The application notes discuss those corrections, but they depend on system
   mechanics such as cover-glass transmission and enclosure geometry, so the
   library does not bake them into the core lux conversion path.

8. Full-scale and resolution helpers treat `Range::AUTO` as the maximum range.
   In auto-range mode the sensor can land on any exponent from 0 to 8, so the
   driver cannot know a single fixed full-scale or resolution value from config
   alone. Helpers such as `getCurrentFullScaleLux()` and
   `getCurrentResolutionLux()` therefore return the conservative worst-case
   values for range 8 when the configured range is `AUTO`.

9. INT-pin hardware trigger is not wrapped as a core driver helper.
   The datasheet documents using the SOT-5X3 INT pin as a one-shot trigger
   input, but the repo documentation does not pin down a portable pulse-driving
   contract, and the driver intentionally does not own GPIO output policy.
   Applications can still configure `INT_DIR = PIN_INPUT` and implement the
   actual pulse at the board layer.
