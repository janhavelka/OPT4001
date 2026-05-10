# OPT4001 High Speed, High Precision, Digital Ambient Light Sensor Datasheet

- Source PDF: `docs/OPT4001_datasheet.pdf`
- Extraction date: 2026-05-09
- Page count: 54
- SHA256: `a12a63ebf414564871fea1427cb767fca0eefb14db55b7365aab61cd21a99d6f`

## Page 1

OPT4001 High Speed, High Precision, Digital Ambient Light Sensor
1 Features 3 Description
- High precision, high speed light-to-digital The OPT4001 is a light-to-digital sensor (single chip
conversion over high speed I2C interface in two lux meter) that measures the intensity of visible
package variants light. The spectral response of the sensor tightly
- Precision optical filtering to closely match human matches the photopic response of the human eye. A
eye with excellent near infrared (IR) rejection specially engineered filter on the device rejects near-
- Semi-logarithmic output with 9 binary logarithmic infrared component from the common light sources to
full-scale light range and highly linear response measure accurate light intensity. Output of OPT4001
within each range is semi-logarithmic with 9 binary logarithmic full-scale
- Built-in automatic full-scale light range selection light ranges along with highly linear response within
logic, which switches measurement range based each range, bringing capability to measure from
on input light condition with excellent gain 312.5 ulux to 83 klux for PicoStar(TM) variant and
matching between ranges 437.5 ulux to 117 klux for the SOT-5X3 variant.
- 28 bits of effective dynamic range from This capability allows light sensor to have 28-bit
- 312.5 ulux to 83 klux for PicoStar(TM) package effective dynamic range. With built-in automatic full-
variant scale range selection, logic users do not have to
- 437.5 ulux to 117 klux for SOT-5x3 package select appropriate gain settings based on light levels.
variant
Device Information
- 12 configurable conversion times from 600 us to
PART NUMBER PACKAGE(1) BODY SIZE (NOM)
800 ms is an excellent choice for a wide variety of
0.84mm x 1.05 mm x
high speed and high precision applications PicoStar(TM) (4)
0.226mm
- External pin interrupt for hardware synchronized OPT4001
SOT-5X3 (8) 1.9mm X 2.1mm X
trigger and interrupts (only on SOT-5X3 package
0.6mm
variant)
- Internal FIFO for output registers with I2C burst (1) For all available packages, see the package option
readout addendum at the end of the data sheet.
- Low operating current: 30 uA
with ultra-low power standby: 2 uA
- Operating temperature range: -40 deg C to +85 deg C
- Wide power-supply range: 1.6 V to 3.6 V
- 5.5 V Tolerant I/O pins
- Selectable I2C address
Wavelength (nm)
- Small-form factor
- PicoStar(TM) package: 0.84 mm x 1.05 mm x
0.226 mm
- SOT-5X3: 2.1mm x 1.9mm x 0.6mm
2 Applications
- Display backlight controls for
- Smartwatches, wearable electronics, and
health fitness bands
- Tablets and notebooks
- Multi-function printers
- Home automation interfaces
- Thermostats and home automation appliances
- Lighting control systems to detect light level (day
or night)
- Point-of-sale terminals
- Outdoor traffic and street lighting
- IP Network Cameras
- Flicker rate detection of light sources
esnopseR
dezilamroN
OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
0. 1 9 O O P P T T 4 4 0 0 0 0 1 1 P S i O co T S -5 ta X r 3 TM 0.8 Human Eye
0.7
0.6
0.5 0.4
0.3
0.2
0.1
0
300 400 500 600 700 800 900 1000
Spectral Response: The OPT4001 and Human Eye
Typical Application Diagram of OPT4001
An IMPORTANT NOTICE at the end of this data sheet addresses availability, warranty, changes, use in safety-critical applications,
intellectual property matters and other important disclaimers. PRODUCTION DATA.

## Page 2

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
Table of Contents
1 Features............................................................................1 8.5 Programming............................................................ 19
2 Applications..................................................................... 1 8.6 Register Maps...........................................................25
3 Description.......................................................................1 9 Application and Implementation.................................. 33
4 Revision History.............................................................. 2 9.1 Application Information............................................. 33
5 Description (continued).................................................. 3 9.2 Typical Application.................................................... 33
6 Pin Configuration and Functions...................................4 9.3 Do's and Don'ts.........................................................37
7 Specifications.................................................................. 5 9.4 Power Supply Recommendations.............................37
7.1 Absolute Maximum Ratings........................................ 5 9.5 Layout....................................................................... 37
7.2 ESD Ratings............................................................... 5 10 Device and Documentation Support..........................43
7.3 Recommended Operating Conditions.........................5 10.1 Documentation Support.......................................... 43
7.4 Thermal Information....................................................5 10.2 Receiving Notification of Documentation Updates..43
7.5 Electrical Characteristics.............................................6 10.3 Support Resources................................................. 43
7.6 Typical Characteristics................................................ 8 10.4 Trademarks.............................................................43
8 Detailed Description......................................................10 10.5 Electrostatic Discharge Caution..............................43
8.1 Overview................................................................... 10 10.6 Glossary..................................................................43
8.2 Functional Block Diagram......................................... 10 11 Mechanical, Packaging, and Orderable
8.3 Feature Description...................................................11 Information.................................................................... 43
8.4 Device Functional Modes..........................................12
4 Revision History
NOTE: Page numbers for previous revisions may differ from page numbers in the current version.
Changes from Revision * (August 2019) to Revision A (January 2022) Page
- Added a new package variant for the device SOT-5X3...................................................................................... 1
- Added Section 8.4.2 for package variants........................................................................................................ 14
- Changed values for Table 8-3 ..........................................................................................................................16
- Added and changed register names for both package variants....................................................................... 25
2 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 3

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
5 Description (continued)
The engineered optical filter on OPT4001 provides strong infrared rejection, which aids in maintaining high
accuracy, despite placing the sensor under dark glass, which is common requirement from industrial design of
end products due to aesthetics.
The OPT4001 is designed for systems that require light level detection to enhance user experience and typically
replaces low accuracy photo diodes, photo-resistors and other ambient light sensors with underwhelming human
eye matching and near infra-red rejection.
The OPT4001 device can be configured to operate with light conversion times all the way from 600 us to
800 ms in 12 steps, providing system flexibility based on application need. Conversion time includes the light
integration time and ADC conversion time. Resolution of the measurement is determined by a combination of
light intensity and the integration time, effectively bringing in capability to measure down to 312.5 ulux of light
intensity changes for the PicoStar(TM) variant and 437.5 ulux for the SOT-5X3 variant.
The digital operation is flexible for system integration. Measurements can be either continuous or triggered
in one shots with register writes or hardware pin (only on SOT-5X3 variant). The device features a threshold
detection logic, which allows the processor to sleep while the sensor watches for appropriate wake-up event to
report via the interrupt pin (only on SOT-5X3 variant).
Digital output, representing the light level is reported over an I2C and SMBus compatible, two-wire serial
interface. An internal FIFO on output registers is available to read out the measurements from the sensor at a
slower pace while still preserving all the data captured by the device. OPT4001 also supports I2C burst mode
helping host read data from FIFO with minimal I2C overhead.
The low power consumption and low power-supply voltage capability of the OPT4001 helps enhance the battery
life of battery-powered systems.
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 3
Product Folder Links: OPT4001

## Page 4

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
6 Pin Configuration and Functions
1 2
A GND SCL
Optical
Sensing
Area
B VDD SDA
Figure 6-1. YMN (PicoStar(TM)) Package, 4-Pin, Top View
Table 6-1. Pin Functions
PIN
DESCRIPTION
NO. NAME TYPE
A1 GND Power Ground
B1 VDD Power Device power. Connect to a 1.6-V to 3.6-V supply.
A2 SCL Digital input I2C clock. Connect with a 10-k ohm resistor to a 1.6-V to 5.5-V supply.
Digital input/
B2 SDA I2C data. Connect with a 10-k ohm resistor to a 1.6-V to 5.5-V supply.
output
VDD 1 8 SDA
ADDR 2 7 INT
NC 3 6 NC
GND 4 5 SCL
Figure 6-2. DTS Package, 6-Pin USON, Top View
Table 6-2. Pin Functions
PIN
TYPE DESCRIPTION
NO. NAME
1 VDD Power Device power. Connect to a 1.6-V to 3.6-V supply.
2 ADDR Digital input Address pin. This pin sets the LSBs of the I2C address.
3 NC No Connection No Connection
4 GND Power Ground
5 SCL Digital input I2C clock. Connect with a 10-k ohm resistor to a 1.6-V to 5.5-V supply.
6 NC No Connection No Connection
7 INT Digital I/O Interrupt input/output open-drain. Connect with a 10-k ohm resistor to a 1.6-V to 5.5-V supply.
8 SDA Digital I/O I2C data. Connect with a 10-k ohm resistor to a 1.6-V to 5.5-V supply.
4 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 5

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
7 Specifications
7.1 Absolute Maximum Ratings
over operating free-air temperature range (unless otherwise noted)(1)
MIN MAX UNIT
Voltage VDD to GND -0.5 6 V
SDA and SCL to GND -0.5 6 V
Current in to any pin 10 mA
T Junction temperature 150  deg C
J
T Storage temperature -65 150(2)  deg C
stg
(1) Operation outside the Absolute Maximum Ratings may cause permanent device damage. Absolute Maximum Ratings do not imply
functional operation of the device at these or any other conditions beyond those listed under Recommended Operating Conditions.
If used outside the Recommended Operating Conditions but within the Absolute Maximum Ratings, the device may not be fully
functional, and this may affect device reliability, functionality, performance, and shorten the device lifetime.
(2) Long exposure to temperatures higher than 105 deg C can cause package discoloration, spectral distortion, and measurement inaccuracy.
7.2 ESD Ratings
VALUE UNIT
Human body model (HBM), per ANSI/ESDA/
+/-2000
JEDEC JS-001, all pins(1)
V Electrostatic discharge V
(ESD)
Charged device model (CDM), per ANSI/ESDA/
+/-500
JEDEC JS-002, all pins(2)
(1) JEDEC document JEP155 states that 500-V HBM allows safe manufacturing with a standard ESD control process.precautions.
(2) JEDEC document JEP157 states that 250-V CDM allows safe manufacturing with a standard ESD control process.
7.3 Recommended Operating Conditions
over operating free-air temperature range (unless otherwise noted)
MIN NOM MAX UNIT
VDD Supply voltage 1.6 3.6 V
T Junction temperature -40 85  deg C
J
7.4 Thermal Information
OPT4001
THERMAL METRIC(1) PicoStarTM (YMN) SOT-5X3 (DTS) UNIT
4 Pins 8 Pins
R Junction-to-ambient thermal resistance 122.8 112.2  deg C/W
JA
R Junction-to-case (top) thermal resistance 1.4 28.4  deg C/W
JC(top)
R Junction-to-board thermal resistance 34.9 22.1  deg C/W
JB
 Junction-to-top characterization parameter 0.8 1.2  deg C/W
JT
 Junction-to-board characterization parameter 35.3 22  deg C/W
JB
(1) For more information about traditional and new thermal metrics, see the Semiconductor and IC Package Thermal Metrics application
report.
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 5
Product Folder Links: OPT4001

## Page 6

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
7.5 Electrical Characteristics
All specifications at TA = 25 deg C, VDD = 3.3 V, 800-ms conversion-time (CONVERSION_TIME=0xB), automatic full-scale
range, white LED and normal-angle incidence of light, unless otherwise specified.
PARAMETER TEST CONDITIONS MIN TYP MAX UNIT
OPTICAL
PicoStarTM Variant
Lowest auto gain range, 800 ms
E 312.5 ulux
vLSB converion-time
Resolution
Lowest auto gain range, 100 ms
E 2.5 mlux
vLSB converion-time
E Full-scale illuminance 83886 lux
vFS
Angular response (FWHM) 96  deg
Drift across temperature Visible Light, Input illuminance = 2000 lux 0.01 %/ deg C
Input illuminance > 328 lux
100 ms conversion-time 2 %
CONVERSION_TIME=0x8
Linearity
Input illuminance < 328 lux
100 ms conversion-time 5 %
CONVERSION_TIME=0x8
SOT-5X3 Variant
Lowest auto gain range, 800 ms
E 437.5 ulux
vLSB converion-time
Resolution
Lowest auto gain range, 100 ms
E 3.5 mlux
vLSB converion-time
E Full-scale illuminance 117441 lux
vFS
Angular response (FWHM) 120  deg
Drift across temperature Visible Light, Input illuminance = 2000 lux 0.015 %/ deg C
Input illuminance > 459 lux
100 ms conversion-time 2 %
CONVERSION_TIME=0x8
Linearity
Input illuminance < 459 lux
100 ms conversion-time 5 %
CONVERSION_TIME=0x8
Common Specifications
Peak irradiance spectral responsivity 550 nm
Effective MANTISSA bits (Register Dependent on Converstion Time
9 20 bits
R_MSB & R_LSB) selected (Register CONVERSION_TIME)
Exponent bits (Register E) Denotes the full-scale range 4 bits
E Measurement output result 2000 lux input(1) 1800 2000 2200 lux
v
Minimum Selectable
(CONVERSION_TIME=0x0), fixed lux 600 us
range, 2000 lux input
Tconv Light Conversion-time(4)
Maximum Selectable
(CONVERSION_TIME=0xB), fixed lux 800 ms
range, 2000 lux input
Light source variation (incandescent,
Bare device, no cover glass 4 %
halogen, fluorescent)
E Infrared response 850nm Near Infrared 0.2 %
vIR
Relative accuracy between gain ranges
0.4 %
(2)
Dark Measurement 0 10 mlux
PSRR Power-supply rejection ratio(3) VDD at 3.6 V and 1.6 V 0.1 %/V
POWER SUPPLY
6 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 7

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
7.5 Electrical Characteristics (continued)
All specifications at TA = 25 deg C, VDD = 3.3 V, 800-ms conversion-time (CONVERSION_TIME=0xB), automatic full-scale
range, white LED and normal-angle incidence of light, unless otherwise specified.
PARAMETER TEST CONDITIONS MIN TYP MAX UNIT
V Power supply 1.6 3.6 V
DD
V Power supply for I2C pull up resistor I2C pullup resistor, V <= 1.6 5.5 V
I2C DD VI2C
Dark 22 uA
I Active Current
QACTIVE
Full-scale lux 30 uA
Dark 1.6 uA
I Quiescent current
Q
Full-scale lux 2 uA
POR Power-on-reset threshold 0.8 V
DIGITAL
C I/O Pin Capacitance 3 pF
IO
T Trigger to Sample Start Low-power shutdown mode 0.5 ms
ss
Low-level input voltage (SDA, SCL, and 0.3 X
V 0 V
IL ADDR) V
DD
High-level input voltage (SDA, SCL, and 0.7 X
V 5.5 V
IH ADDR) V
DD
Low-level input current (SDA, SCL, and
I 0.01 0.25(5) uA
IL ADDR)
V Low-level output voltage (SDA and INT) I =3mA 0.32 V
OL OL
Output logic high, high-Z leakage current
I Measured with V at pin 0.01 0.25(5) uA
ZH (SDA, INT) DD
TEMPERATURE
Specified temperature range -40 85  deg C
(1) Tested with the white LED calibrated to 2000 lux
(2) Characterized by measuring fixed near-full-scale light levels on the higher adjacent full-scale range setting.
(3) PSRR is the percent change of the measured lux output from the current value, divided by the change in power supply voltage, as
characterized by results from 3.6-V and 1.6-V power supplies
(4) The conversion-time, from start of conversion until the data are ready to be read, is the integration-time plus analog-to-digital
conversion-time.
(5) The specified leakage current is dominated by the production test equipment limitations. Typical values are much smaller
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 7
Product Folder Links: OPT4001

## Page 8

7.6 Typical Characteristics
At T = 25 deg C, V = 3.3 V, 800-ms conversion time (CONVERSION_TIME = 0xB), automatic full-scale range (RANGE =
A DD
0xC), white LED, and normal-angle incidence of light, unless otherwise specified.
Wavelength (nm)
esnopseR
dezilamroN
1
OPT4001 PicoStarTM
0.9 OPT4001 SOT-5X3
0.8 Human Eye
0.7
0.6
0.5
0.4
0.3
0.2
0.1
0
300 400 500 600 700 800 900 1000
Illuminance Angle ( deg )
spacer
Figure 7-1. Spectral Response vs Wavelength
esnopseR
dezilamroN
1
OPT4001 PicoStarTM
0.9 OPT4001 SOT-5X3
0.8
0.7
0.6
0.5
0.4
0.3
0.2
0.1
0
-100 -80 -60 -40 -20 0 20 40 60 80 100
Normalized to 0 deg
spacer
Figure 7-2. Device Response vs Illuminance Angle
Input Illuminance (Lux)
tnerruC
ylppuS
dezilamroN
1.5
1.45
1.4
1.35
1.3
1.25
1.2
1.15
1.1
1.05
1
0.95
0.9
0.001 0.01 0.1 1 10 100 1000 10000 100000
Input Illuminance (Lux)
Normalized to dark condition
Figure 7-3. Active Current vs Input Light Level
tnerruC
ylppuS
dezilamroN
1.25
1.2
1.15
1.1
1.05
1
0.95
0.001 0.01 0.1 1 10 100 1000 10000 100000
Normalized to dark condition
Figure 7-4. Standby Current vs Input Light Level
Power Supply (V)
tnerruC
ylppuS
dezilamroN
1.1
1.08
1.06
1.04
1.02
1
0.98
0.96
0.94
0.92
0.9
1.6 1.8 2 2.2 2.4 2.6 2.8 3 3.2 3.4 3.6
Supply Voltage (V)
Normalized to 3.3 V
Figure 7-5. Active Current vs Power Supply
tnerruC
ylppuS
dezilamroN
OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
1.05
1.04
1.03
1.02
1.01
1
0.99
0.98
0.97
0.96
0.95
0.94
1.6 1.8 2 2.2 2.4 2.6 2.8 3 3.2 3.4 3.6
Normalized to 3.3 V
Figure 7-6. Standby Current vs Power Supply
8 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 9

7.6 Typical Characteristics (continued)
At T = 25 deg C, V = 3.3 V, 800-ms conversion time (CONVERSION_TIME = 0xB), automatic full-scale range (RANGE =
A DD
0xC), white LED, and normal-angle incidence of light, unless otherwise specified.
Supply Voltage (V)
esnopseR
dezilamroN
1.005
1.004
1.003
1.002
1.001
1
0.999
0.998
0.997
0.996
0.995
1.6 1.8 2 2.2 2.4 2.6 2.8 3 3.2 3.4 3.6
Input Illuminance (Lux)
Normalized to 3.3 V
Figure 7-7. Device Response vs Power Supply
emiT
noisrevnoC
dezilamroN
2
E=0
E=1
1.8 E=2
E=3
E=4 1.6 E=5
E=6
E=7
1.4 E=8
1.2
1
0.8
20 30 50 100 200 500 1000 10000 100000
Register E (exponent) denotes the full-scale range
Normalized to 600 us
Figure 7-8. Conversion Time at 600 us vs Input Light Level
Input Illuminance (Lux)
emiT
noisrevnoC
dezilamroN
1.035
E=0
E=1
1.03 E=2
E=3
E=4 1.025 E=5
E=6
E=7
1.02 E=8
1.015
1.01
1.005
20 30 50 100 200 500 1000 10000 100000
Temperature ( deg C)
Register E (exponent) denotes the full-scale range
Normalized to 25 ms
Figure 7-9. Conversion Time at 25 ms vs Input Light Level
tnerruC
dezilamroN
1.05
1.04
1.03
1.02
1.01
1
0.99
0.98
0.97
0.96
0.95
-40 -25 -10 5 20 35 50 65 80 95
Normalized to 25 deg C
spacer
Figure 7-10. Active Current vs Temperature
Temperature ( deg C)
tnerruC
dezilamroN
OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
1.4
1.35
1.3
1.25
1.2
1.15
1.1
1.05
1
0.95
-40 -20 0 20 40 60 80 100
Normalized to 25 deg C
Figure 7-11. Standby Current vs Temperature
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 9
Product Folder Links: OPT4001

## Page 10

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
8 Detailed Description
8.1 Overview
OPT4001 measures the ambient light that illuminates the device. This device measures light with a spectral
response very closely matched to the human eye, and with excellent infrared rejection.
Matching the sensor spectral response to that of the human eye response is vital because ambient light sensors
are used to measure and help create excellent human lighting experiences. Strong rejection of infrared light,
which a human does not see, is a crucial component of this matching. This matching makes the OPT4001
especially good for operation underneath windows that are visibly dark, but infrared transmissive.
OPT4001 is fully self-contained to measure the ambient light and report the result in ADC codes directly
proportional to lux digitally over the I2C bus. The result can also be used to alert a system and interrupt
a processor with the INT pin (with SOT-5X3 package variant). The result can also be summarized with a
programmable threshold comparison and communicated with the INT pin(with SOT-5X3 package variant).
OPT4001 is by default configured to operate in automatic full-scale range detection mode that always selects
the best full-scale range setting for the given lighting conditions. There are 9 full-scale range settings, one of
which can be selected manually as well. Setting the device to operate in automatic full-scale range detection
mode frees the user from having to program their software for potential iterative cycles of measurement and
readjustment of the full-scale range until good for any given measurement. With device exhibiting excellent
linearity over the entire 28 bit dynamic range of measurement no additional linearity calibration is required at
system level.
OPT4001 can be configured to operate in continuous or one-shot measurement modes. The device offers 12
conversion times ranging from 600 us to 800 ms. The device starts up in a low-power shutdown state, such that
the OPT4001 only consumes active-operation power after being programmed into an active state.
OPT4001 optical filtering system is not excessively sensitive to small particles and micro-shadows on the optical
surface. This reduced sensitivity is a result of the relatively minor device dependency on uniform density optical
illumination of the sensor area for infrared rejection. Proper optical surface cleanliness is always recommended
for best results on all optical devices.
8.2 Functional Block Diagram
VDD
OPT4001
Ambient
SCL
Light
I2C SDA
Photopic ADC Interface
Light Filter Only SOT-5X3 package
INT
ADDR
GND
Figure 8-1. Functional Block Diagram of OPT4001
10 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 11

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
8.3 Feature Description
8.3.1 Spectral Matching to Human Eye
OPT4001 spectral response closely matches that of the human eye. If the ambient light sensor measurement is
used to help create a good human experience, or create optical conditions that are good for humans, then the
sensor must measure the same spectrum of light that a human sees.
OPT4001 also has excellent infrared light (IR) rejection. This IR rejection is especially important because many
real-world lighting sources have significant infrared content that humans do not see. If the sensor measures
infrared light that the human eye does not see, then a true human experience is not accurately represented.
If the application demands hiding OPT4001 underneath dark window (such that the end-product user cannot
see the sensor) the infrared rejection of the OPT4001 becomes significantly more important because many dark
windows attenuate visible light but transmit infrared light. This attenuation of visible light and lack of attenuation
of IR light amplifies the ratio of the infrared light to visible light that illuminates the sensor. Results can still be
well matched to the human eye under this condition because of the high infrared rejection of the OPT4001.
8.3.2 Automatic Full-Scale Range Setting
The OPT4001 has an automatic full-scale range setting feature that eliminates the need to predict and set
the best range for the device. In this mode, the device automatically selects the best full-scale range for
varying lighting condition each measurement. The device has a high degree of result matching between the
full-scale range settings. This matching eliminates the problem of varying results or the need for range-specific,
user-calibrated gain factors when different full-scale ranges are chosen.
8.3.3 Output Register CRC and Counter
OPT4001 device features additional bits as part of the output register which helps in improving the reliability of
light measurements for the application.
8.3.3.1 Output Sample Counter
The OPT4001 device features a register COUNTER as part of the output registers which increments for every
successful measurement. This register can be read as part of the output registers which helps the application
to keep track of measurements. The 4 bit counter starts at 0 on power-up and counts up to 15 after which the
counter resets back to 0 and continues to count up, which is particularly helpful in situations like the following:
- Host or the controller needs consecutive measurements. Utilizing the COUNTER register allows the controller
to compare samples and makes sure that the samples are in expected order without missing intermediate
counter values.
- As a safety feature where when light level are not changing, the controller can make sure that
the measurements from OPT4001 are not stuck by comparing values of register COUNTER between
measurements. If the COUNTER values continue to change over samples, the device is updating the output
register with the most recent measurement of light levels.
8.3.3.2 Output CRC
CRC register consists of Cyclic Redundancy Checker bits part of the output registers calculated within the
OPT4001 device and updated on every measurement. This feature helps in detecting communication related bit
errors during the output readout from the device. The calculation method for the CRC bits is shown in Figure
8-14, which can be independently verified in the controller or host firmware/software to validate if communication
between the controller and the device was successful without bit errors during transmission.
8.3.4 Output Register FIFO
Output registers always contain the most recent light measurement. Along with output registers there are 3 more
shadow registers which have the data from the previous 3 measurements. For every new measurement, the
data on the 3 shadow registers are updated to contain the most recent measurements discarding the oldest
measurement similar to a FIFO scheme. These shadow registers along with output registers act like a FIFO with
a depth of 4. The INT pin (only on SOT-5X3 variant) can be configured as shown in the figure below to generate
an interrupt every measurement or can be configured to generate an interrupt every 4 measurements using
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 11
Product Folder Links: OPT4001

## Page 12

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
the register INT_CFG. This way the controller reading data from OPT4001 device can minimize the number of
interrupts by a factor of 4 and still get access to all the four measurements between the interrupts. By using the
Burst Read Mode the output and FIFO registers can be read out with minimal I2C clocks.
Continuous Mode
Output registers Measurement(n-1) Measurement(n) Measurement(n+1) Measurement(n+2)
FIFO 0 registers Measurement(n-2) Measurement(n-1) Measurement(n) Measurement(n+1)
FIFO 1 registers Measurement(n-3) Measurement(n-2) Measurement(n-1) Measurement(n)
FIFO 2 registers Measurement(n-4) Measurement(n-3) Measurement(n-2) Measurement(n-1)
INT (output)
On SOT-5X3 only
Interrupt(n-1) Interrupt(n) Interrupt(n+1)
Conversion Time
Figure 8-2. FIFO registers data movement
8.3.5 Threshold Detection
OPT4001 features a threshold detection logic which can be programmed to indicate and update register flags
if measured light levels cross thresholds set by the user. There are independent low and high threshold target
registers with independent flag registers to indicate the status of measured light level. Measured light level
reaching below low threshold and above the high threshold are called faults. Users can program a fault count
register, which counts consecutive number of faults before the flag registers are set. This is particularly useful
in cases where the controller can read the flag register alone to get indication of measured light level not really
needing to do the lux calculations. Details on the register and setting up the threshold is available in Section
8.3.5 and calculations for setting this up is available inThreshold Detection Calculations.
8.4 Device Functional Modes
8.4.1 Modes of Operation
The OPT4001 device has the following modes of operation:
- Power-down mode: This is power-down or standby mode where the device enters a low power state. There
is no active light sensing or conversion in this mode. Device still responds to I2C transactions which can be
utilized to bring the device out of this mode. Register OPERATING_MODE is set to 0.
- Continuous mode: In this mode OPT4001 measures and updates the output registers continuously
determined by the conversion time and generates hardware interrupt on pin INT (Only on SOT-5X3 package
variant) for every successful conversion. TI recommends to configure the INT pin in output mode using the
INT_DIR register. The device active circuits are continuously kept active to minimize the interval between
measurements. Register OPERATING_MODE is set to 3.
- One shot mode of operation: There are several ways in which OPT4001 can be used in one shot mode
of operation with one common theme where OPT4001 stays in standby mode and a conversion is triggered
either by a register write to configuration register or hardware interrupt on the INT pin.
There are two types of one shot modes.
- Force auto-range one shot mode: Every one shot trigger forces a full reset on auto-ranging control
logic and a fresh auto-range detection is initiated ignoring the previous measurements. This is particularly
useful in situations where lighting conditions are expected to change a lot and one shot trigger frequency
is not very often. There is small penalty on conversion time due for the auto-ranging logic to recover from
12 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 13

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
reset state. The full reset cycle on the auto-ranging control logic takes around 500 us which needs to be
accounted for between measurements when this mode is used. Register OPERATING_MODE is set to 1.
- Regular auto-range one shot mode: Auto-range selection logic utilizes the information from the previous
measurements to decide the range for the current trigger. This mode is recommended only when the
device needs time synchronized measurements with frequent triggers from the controller. In other words,
this mode can be used as an alternative to continuous mode the key difference being that the interval
between measurements is determined by the one shot triggers. Register OPERATING_MODE is set to 2.
One Shot can be triggered by the following
- Hardware trigger (Only on SOT-5X3 variant):INT pin can be configured to be an input to trigger a
measurement setting INT_DIR register to 0. Since INT pin is used as input, there is no hardware interrupt
to indicate completion of measurement. The controller needs to keep time from the trigger mechanism and
read out output registers.
- Register trigger: An I2C write to the OPERATING_MODE register triggers a measurement (value of 1 or
2). The register value is reset after next successful measurement. INT pin can be configured to indicate
measurement completion to read out output registers setting the INT_DIR register to 1.
TI highly recommends to set the interval between subsequent triggers to account for all the aspects involved
in the trigger mechanism like the I2C transaction time, device wake-up time, auto-range time (if used) and
device conversion time. If a conversion trigger is received before the completion of current measurement, the
device simply ignores the new request until the previous conversion is completed.
Since the device enters standby after each one shot trigger, measurement interval in the one shot trigger
mechanism needs to account for additional time T as specified in the specification table for the circuits to
ss
recover from standby state. However setting the quick wake up register QWAKE eliminates the need for this
additional T at the cost of not powering down the active circuit with device not entering the standby mode
ss
between triggers.
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 13
Product Folder Links: OPT4001

## Page 14

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
Continuous Mode
Output registers Measurement(n-1) Measurement(n) Measurement(n+1) Measurement(n+2)
INT (output)
On SOT-5X3 only
Interrupt(n-1) Interrupt(n) Interrupt(n+1)
Conversion Time
Device Power Active Power Active Power Active Power Active Power
One-shot Mode (Pin Trigger)
Output registers Measurement(n-1) Measurement(n) Measurement(n+1)
INT (input)
On SOT-5X3 only
Trigger(n) Trigger(n+1)
Conversion Time Conversion Time
Device Power Standby Active Power Standby Active Power Standby
One-shot Mode (Register Trigger)
Output registers Measurement(n-1) Measurement(n) Measurement(n+1)
Trigger bits set by Controller
Config register
Trigger bits reset by Device Trigger bits reset by Device
Trigger(n) Trigger(n+1)
Conversion Time Conversion Time
INT (output)
On SOT-5X3 only
Interrupt(n) Interrupt(n+1)
Device Power Standby Active Power Standby Active Power Standby
Figure 8-3. Timing Diagrams for different Operating modes
8.4.2 Interrupt Modes of Operation
The device has an interrupt reporting system that allows the processor connected to the I2C bus to go to
sleep, or otherwise ignore the device results, until a user-defined event occurs that requires possible action.
Alternatively, this same mechanism can also be used with any system that can take advantage of a single digital
signal that indicates whether the light is above or below levels of interest.
The INT pin has an open-drain output, which requires the use of a pull-up resistor. This open-drain output allows
multiple devices with open-drain INT pins to be connected to the same line, thus creating a logical NOR or AND
function between the devices. The polarity of the INT pin can be controlled by the INT_POL.
There are two major types of interrupt reporting mechanism modes: latched window comparison mode and
transparent hysteresis comparison mode. The configuration register LATCH controls which of these two modes
is used. Table 8-1 and Figure 8-4 summarize the function of these two modes. Additionally, the INT pin can
either be used to indicate a fault in one of these modes (INT_CFG=0) or to indicate a conversion completion
(INT_CFG >0). This is shown in Table 8-2.
14 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 15

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
Figure 8-4. Interrupt Pin Status (for INT_CFG=0 setting) and Register Flag Behavior
Table 8-1. Interrupt Pin Status (for INT_CFG=0 setting) and Register Flag Behavior
INT Pin State (when
LATCH Setting FLAG_H Value FLAG_L Value Latching Behavior
INT_CFG=0)
INT pin indicates if
measurement is above
(INT active) or below (INT
0: If measurement is 0: If measurement is
inactive) the threshold. If
below the low limit above the high limit
measurement is between
1: If measurement is 1: If measurement is
the high and low threshold Not latching: Values
0: Transparent hysteresis above the high limit below the low limit
values then the previous are updated after each
mode If measurement is If measurement is
INT value is maintained. conversion
between high and low between high and low
This prevents the INT
limits previous value is limits previous value is
pin from repeated toggling
maintained maintained
when the measurement
values are close to the
threshold.
INT pin becomes active
if the measurement is
outside the window (above Latching: INT pin,
high threshold or below 1: If measurement is 1: If measurement is FLAG_H and FLAG_L
1: Latched window mode
the low threshold). The above the high limit below the low limit values do not reset until
INT pin does not reset and the register 0x0C is read.
return to the inactive state
until register 0xC is read.
The THRESHOLD_H, THRESHOLD_L, LATCH and FAULT_COUNT registers control the interrupt behavior. The
LATCH field setting allows a choice between the latched window mode and transparent hysteresis mode as
shown in the table. Interrupt reporting can be observed on INT pin (for SOT-5X3 variant only), the FLAG_H, and
the FLAG_L registers.
Results from comparing the current sensor measurements with THRESHOLD_H and THRESHOLD_L registers
are referred to as fault events. The calculations to set these registers can be found in Threshold Detection
Calculations. The FAULT_COUNT register dictates the number of continuous fault events required to trigger an
interrupt event and subsequently change the state of the interrupt reporting mechanisms. For example, with a
FAULT_COUNT value of 2 corresponding to 4 fault counts, the INT pin (for SOT-5X3 variant only), FLAG_H and
FLAG_L states shown in the table are not realized unless 4 consecutive measurements are taken that satisfy the
fault condition.
INT pin function (for SOT-5X3 variant only) listed in Table 8-1is valid only when INT_CFG=0. The INT pin
function can be changed to indicate an end of conversion or FIFO full state as shown in Table 8-2. The FLAG_H
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 15
Product Folder Links: OPT4001

## Page 16

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
and FLAG_L registers continue to behave as listed in Table 8-1 even while INT_CFG>0. The polarity of the INT
pin is controlled by the INT_POL register.
Table 8-2. INT_CFG Setting and Resulting INT Pin Behavior
INT_CFG Setting INT Pin Function
0 As per Table 8-1
1 INT pin asserted with 1us pulse width after every conversion
INT pin asserted with 1us pulse width every 4 conversions to indicate
3
the FIFO is full
8.4.3 Light Range Selection
The OPT4001 has an automatic full-scale-range setting mode that eliminates the need for a user to predict
and set the best range for the device. This mode is entered when register RANGE is set to 0xC. The device
determines the appropriate full-scale range to take the measurement based on a combination of current lighting
conditions and the previous measurement.
If a measurement is towards the low side of full-scale, then the full-scale range is decreased by one or two
settings for the next measurement. If a measurement is towards the upper side of full-scale, the full-scale range
is increased by one setting for the next measurement.
If the measurement exceeds the full-scale range, resulting from a fast increasing optical transient event, then the
current measurement is aborted. This invalid measurement is not reported. If the scale is not at the maximum,
then the device increases the scale by one step and a new measurement is retaken with that scale. Therefore,
during a fast increasing optical transient in this mode, a measurement can possibly take longer to complete and
report than indicated by the configuration register CONVERSION_TIME.
TI highly recommends to use this feature, since the device selects the best range setting based on lighting
condition. However, there is an option to manually set the range. Setting the range manually turns off the
automatic full-scale selection logic and the device operates for a particular range setting.
16 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 17

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
Table 8-3. Range Selection Table
Typical Full-scale Light level for PicoStar(TM) Typical Full-scale Light level for SOT-5X3
RANGE register setting
variant variant
0 328 lux 459 lux
1 655 lux 918 lux
2 1311 lux 1835 lux
3 2621 lux 3670 lux
4 5243 lux 7340 lux
5 10486 lux 14680 lux
6 20972 lux 29360 lux
7 41943 lux 58720 lux
8 83886 lux 117441 lux
12 Determined by automatic full-scale range logic
8.4.4 Selecting Conversion Time
The OPT4001 device offers several conversion times to select from. Conversion Time is defined as how much
time for one measurement to complete and update the results in output register from the time measurement is
initiated. Measurement initiation is determined by the mode of operation as specified in Modes of Operation.
Table 8-4. Conversion Time Selection
CONVERSION_TIME register Typical Conversion Time
0 0.6 ms
1 1 ms
2 1.8 ms
3 3.4 ms
4 6.5 ms
5 12.7 ms
6 25 ms
7 50 ms
8 100 ms
9 200 ms
10 400 ms
11 800 ms
8.4.5 Light Measurement in Lux
The OPT4001 device measures light and updates output registers with proportional ADC codes. Output of the
device is represented by two parts (i) 4 bits of EXPONENT and (ii) 20 bits of MANTISSA. This arrangement of
binary logarithmic full-scale range with linear representation with in a range helps in covering a large dynamic
range of measurements. MANTISSA represents the linear ADC codes proportional to the measured light within a
given full-scale range and the EXPONENT represents the current-full scale range selected. The selected range
can be automatically determined by the auto-range selection logic or manually selected as per Table 8-3.
Lux level can be determined using the following equations:
MANTISSA=(RESULT_MSB<<8) + RESULT_LSB (1)
or
MANTISSA=(RESULT_MSB x 2^8) + RESULT_LSB (2)
where RESULT_MSB, RESULT_LSB and EXPONENT are parts of the output register
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 17
Product Folder Links: OPT4001

## Page 18

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
RESULT_MSB register carries the most significant 12 bits of the MANTISSA and RESULT_LSB register carries
the least significant 8 bits of the MANTISSA. MANTISSA is then computed using the above equations to get the
20 bit number. EXPONENT is directly read from the register which is 4 bits.
Once the EXPONENT and MANTISSA portions are calculated the linearized ADC_CODES is calculated using
the following equation:
ADC_CODES = (MANTISSA<<E) (3)
or
ADC_CODES = (MANTISSA x 2^E) (4)
With maximum value for register E being 8 ADC_CODES is effectively a 28 bit number. The semi-logarithmic
numbers have been converted to a linear ADC_CODES representation, which is simple to convert to lux given
by the following formula
lux = ADC_CODES x 312.5E-6 for the PicoStar(TM) variant (5)
lux = ADC_CODES x 437.5E-6 for the SOT-5X3 variant (6)
The MANTISSA and ADC_CODES are large numbers with 20 and 28 bits required to represent them. While
developing firmware or software for these calculations, allocating appropriate data types to prevent data overflow
is important. Some explicit typecasting to a larger data type such as 32 bit representation before left shift
operation (<<) operations is recommended.
Threshold Detection Calculations
Threshold result registers THRESHOLD_H_RESULT and THRESHOLD_L_RESULT are 12 bit, while threshold
exponent registers THRESHOLD_H_EXPONENT and THRESHOLD_L_EXPONENT are 4 bits. Since threshold
is compared at linear ADC_CODES, the threshold registers are padded with zeros internally as shown to
compare with the ADC_CODES
ADC_CODES_TH = THRESHOLD_H_RESULT << (8 + THRESHOLD_H_EXPONENT) (7)
or
ADC_CODES_TH = THRESHOLD_H_RESULT x 2^(8 + THRESHOLD_H_EXPONENT) (8)
and
ADC_CODES_TL = THRESHOLD_L_RESULT << (8 + THRESHOLD_L_EXPONENT) (9)
or
ADC_CODES_TL=THRESHOLD_L_RESULT x 2^(8 + THRESHOLD_L_EXPONENT) (10)
Threshold are then compared as shown to detect fault events.
If ADC_CODES < ADC_CODES_TL a fault low is detected (11)
and
If ADC_CODES > ADC_CODES_TH a fault high is detected (12)
Based on the FAULT_COUNT register setting, with consecutive fault high or fault low events, respective
FLAG_H and FLAG_L registers are set, more details of which can be found in Section 8.4.2. Understanding
the relation between THRESHOLD_H_EXPONENT, THRESHOLD_H_RESULT, THRESHOLD_L_EXPONENT,
18 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 19

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
THRESHOLD_L_RESULT and the output registers is important to be able to set appropriate threshold based on
application needs.
8.4.6 Light Resolution
The OPT4001 device's effective resolution is dependent on both the conversion time setting and the full-scale
light range. Although the LSB resolution of the linear ADC_CODES doesn't change, the effective or useful
resolution of the device is dependent on the conversion time setting and the full-scale range as per the table
below. In conversion times where the effective resolution is lower, the LSBs are padded with 0.
Table 8-5. Resolution Table for the Picostar(TM) Variant
EXPONE
0 1 2 3 4 5 6 7 8
CONVE MANTES NT
RSION_ Convers SA
Full-
TIME ion Time effective 328 655 1310 2621 5243 10486 20972 41943 83886
scale lux
register bits
Effective Resolution in lux
0 600us 9 640 m 1.28 2.56 5.12 10.24 20.48 40.96 81.92 163.84
1 1 ms 10 320 m 640 m 1.28 2.56 5.12 10.24 20.48 40.96 81.92
2 1.8 ms 11 160 m 320 m 640 m 1.28 2.56 5.12 10.24 20.48 40.98
3 3.4 ms 12 80 m 160 m 320 m 640 m 1.28 2.56 5.12 10.24 20.48
4 6.5 ms 13 40 m 80 m 160 m 320 m 640 m 1.28 2.56 5.12 10.24
5 12.7 ms 14 20 m 40 m 80 m 160 m 320 m 640 m 1.28 2.56 5.12
6 25 ms 15 10 m 20 m 40 m 80 m 160 m 320 m 640 m 1.28 2.56
7 50 ms 16 5 m 10 m 20 m 40 m 80 m 160 m 320 m 640 m 1.28
8 100 ms 17 2.5 m 5 m 10 m 20 m 40 m 80 m 160 m 320 m 640 m
9 200 ms 18 1.25 m 2.5 m 5 m 10 m 20 m 40 m 80 m 160 m 320 m
10 400 ms 19 0.625 m 1.25 m 2.5 m 5 m 10 m 20 m 40 m 80 m 160 m
11 800 ms 20 0.3125 m 0.625 m 1.25 m 2.5 m 5 m 10 m 20 m 40 m 80 m
Table 8-6. Resolution Table for the SOT-5X3 variant
EXPONE
0 1 2 3 4 5 6 7 8
CONVE MANTES NT
RSION_ Convers SA
Full-
TIME ion Time effective 459 918 1835 3670 7340 14680 29360 58720 117441
scale lux
register bits
Effective Resolution in lux
0 600 us 9 896 m 1.792 3.584 7.168 14.336 28.672 47.344 114.688 229.376
1 1 ms 10 448 m 896 m 1.792 3.584 7.168 14.336 28.672 47.344 114.688
2 1.8 ms 11 224 m 448 m 896 m 1.792 3.584 7.168 14.336 28.672 47.344
3 3.4 ms 12 112 m 224 m 448 m 896 m 1.792 3.584 7.168 14.336 28.672
4 6.5 ms 13 56 m 112 m 224 m 448 m 896 m 1.792 3.584 7.168 14.336
5 12.7 ms 14 28 m 56 m 112 m 224 m 448 m 896 m 1.792 3.584 7.168
6 25 ms 15 14 m 28 m 56 m 112 m 224 m 448 m 896 m 1.792 3.584
7 50 ms 16 7 m 14 m 28 m 56 m 112 m 224 m 448 m 896 m 1.792
8 100 ms 17 3.5 m 7 m 14 m 28 m 56 m 112 m 224 m 448 m 896 m
9 200 ms 18 1.75 m 3.5m 7 m 14 m 28 m 56 m 112 m 224 m 448 m
10 400 ms 19 0.875 m 1.75m 3.5 m 7 m 14 m 28 m 56 m 112 m 224 m
11 800 ms 20 0.4375 m 0.875 m 1.75 m 3.5 m 7 m 14 m 28 m 56 m 112 m
8.5 Programming
The OP4001 supports the transmission protocol for standard mode (up to 100 kHz), fast mode (up to 400 kHz),
and high-speed mode (up to 2.6 MHz). Fast and standard modes are described as the default protocol, referred
to as F/S. High-speed mode is described in the High-Speed I2C Mode section.
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 19
Product Folder Links: OPT4001

## Page 20

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
8.5.1 I2C Bus Overview
The OPT4001 offers compatibility with both I2C and SMBus interfaces. The I2C and SMBus protocols are
essentially compatible with one another. The I2C interface is used throughout this document as the primary
example with the SMBus protocol specified only when a difference between the two protocols is discussed.
The device is connected to the bus with two pins: an SCL clock input pin and an SDA open-drain bidirectional
data pin. The bus must have a controller device that generates the serial clock (SCL), controls the bus access,
and generates start and stop conditions. To address a specific device, the controller initiates a start condition
by pulling the data signal line (SDA) from a high logic level to a low logic level while SCL is high. All targets
on the bus shift in the target address byte on the SCL rising edge, with the last bit indicating whether a read or
write operation is intended. During the ninth clock pulse, the target being addressed responds to the controller by
generating an acknowledge bit by pulling SDA low.
Data transfer is then initiated and eight bits of data are sent, followed by an acknowledge bit. During data
transfer, SDA must remain stable while SCL is high. Any change in SDA while SCL is high is interpreted as
a start or stop condition. When all data are transferred, the controller generates a stop condition, indicated by
pulling SDA from low to high while SCL is high. The device includes a 28-ms timeout on the I2C interface to
prevent locking up the bus. If the SCL line is held low for this duration of time, the bus state machine is reset.
8.5.1.1 Serial Bus Address
To communicate with the OPT4001, the controller must first initiate an I2C start command. Then, the controller
must address target devices via a target address byte. The target address byte consists of a seven bit address
and a direction bit that indicates whether the action is to be a read or write operation.
For the SOT 5X3 variant, four I2C addresses are possible by connecting the ADDR pin to one of four pins:
GND, VDD, SDA, or SCL. Table below summarizes the possible addresses with the corresponding ADDR
pin configuration. The state of the ADDR pin is sampled on every bus communication and must be driven or
connected to the desired level before any activity on the interface occurs.
ADDR PIN CONNECTION DEVICE I2C ADDRESS
GND 1000100
VDD 1000101
SDA 1000110
SCL 1000101
In case of the PicoStar(TM) variant there is no target address selection capability and the device address is hard
coded to 1000101b (0x45).
8.5.1.2 Serial Interface
The OPT4001 operates as a target device on both the I2C bus and SMBus. Connections to the bus are made
via the SCL clock input line and the SDA open-drain I/O line. The device supports the transmission protocol for
standard mode (up to 100 kHz), fast mode (up to 400 kHz), and high-speed mode (up to 2.6 MHz). All data bytes
are transmitted most-significant bits first.
The SDA and SCL pins feature integrated spike-suppression filters and Schmitt triggers to minimize the effects
of input spikes and bus noise. See the Section 9.2.1 for further details of the I2C bus noise immunity.
8.5.2 Writing and Reading
Accessing a specific register on the OPT4001 is accomplished by writing the appropriate register address during
the I2C transaction sequence. Refer to Section 8.6 for a complete list of registers and their corresponding
register addresses. The value for the register address (as shown in Figure 8-5) is the first byte transferred after
the target address byte with the R/W bit low.
20 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 21

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
1 9 1 9
SCL
A1 A0 RA RA RA RA RA RA RA RA
SDA 1 0 0 0 1 or or R/W
7 6 5 4 3 2 1 0
0 1
Start by A1 A0 for SOT-5X3 package only ACK by ACK by Stop by
Controller Device Device Controller
(optional)
Frame 1: Two-Wire Slave Address Byte (1) Frame 2: Register Address Byte
Figure 8-5. Setting the I2C Register Address
Writing to a register begins with the first byte transmitted by the controller. This byte is the target address with
the R/W bit low. The device then acknowledges receipt of a valid address. The next byte transmitted by the
controller is the address of the register that data are to be written to. The next two bytes are written to the
register addressed by the register address. The device acknowledges receipt of each data byte. The controller
can terminate the data transfer by generating a start or stop condition.
When reading from the device, the last value stored in the register address by a write operation determines
which register is read during a read operation. To change the register address for a read operation, a new partial
I2C write transaction must be initiated. This partial write is accomplished by issuing a target address byte with
the R/W bit low, followed by the register address byte and a stop command. The controller then generates a start
condition and sends the target address byte with the R/W bit high to initiate the read command. The next byte
is transmitted by the terget and is the most significant byte of the register indicated by the register address. This
byte is followed by an acknowledge from the controller; then the target transmits the least significant byte. The
controller acknowledges receipt of the data byte. The controller can terminate the data transfer by generating a
not-acknowledge after receiving any data byte, or by generating a start or stop condition. If repeated reads from
the same register are desired, continually sending the register address bytes is not necessary; the device retains
the register address until that number is changed by the next write operation.
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 21
Product Folder Links: OPT4001

## Page 22

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
Figure 8-6 and Figure 8-7 show the write and read operation timing diagrams, respectively. Note that register
bytes are sent most significant byte first, followed by the least significant byte.
1 9 1 9 1 9 1 9
SCL
SDA 1 0 0 0 1 A o 0 1 r A o 1 0 r R/W R 7 A R 6 A R 5 A R 4 A R 3 A R 2 A R 1 A R 0 A D15 D14 D13 D12 D11 D10 D9 D8 D7 D6 D5 D4 D3 D2 D1 D0
Start by A1 A0 for SOT-5X3 package only ACK by ACK by ACK by ACK by Stop by
Controller Device Device Device Device Controller
Frame 1 Two-Wire Slave Address Byte (1) Frame 2 Register Address Byte Frame 3 Data MSByte Frame 4 Data LSByte
Figure 8-6. I2C Write Example
1 9 1 9 1 9
SCL
A1 A0
SDA 1 0 0 0 1 or or R/W D15 D14 D13 D12 D11 D10 D9 D8 D7 D6 D5 D4 D3 D2 D1 D0
0 1
Start by ACK by From ACK by No ACK Stop by
Controller A1 A0 for SOT-5X3 package only Device Device Controller From Device by Controller
Controller(2)
Frame 1 Two-Wire Slave Address Byte (1) Frame 2 Data MSByte Frame 3 Data LSByte
A. An ACK by the controller can also be sent.
Figure 8-7. I2C Read Example
8.5.2.1 High-Speed I2C Mode
When the bus is idle, both the SDA and SCL lines are pulled high by the pullup resistors or active pullup
devices. The controller generates a start condition followed by a valid serial byte containing the high-speed (HS)
controller code 0000 1XXXb. This transmission is made in either standard mode or fast mode (up to 400 kHz).
The device does not acknowledge the HS controller code but does recognize the code and switches its internal
filters to support a 2.6-MHz operation.
The controller then generates a repeated start condition (a repeated start condition has the same timing as
the start condition). After this repeated start condition, the protocol is the same as F/S mode, except that
transmission speeds up to 2.6 MHz are allowed. Instead of using a stop condition, use repeated start conditions
to secure the bus in HS mode. A stop condition ends the HS mode and switches all internal filters of the ddevice
to support the F/S mode.
8.5.2.2 Burst Read Mode
OPT4001 supports I2C burst read mode which helps in minimizing the number of transactions on the bus for
efficient data transfer from the device to the controller.
Before considering the burst mode, a regular I2C read transaction involves an I2C write operation to the device
read pointer, followed by the actual I2C read operation. If the output registers and FIFO registers which are in
continuous locations, are writing the register pointer every 2 bytes, this takes up several clock cycles. With the
burst mode enabled, the read pointer address is auto incremented after every register read (2 bytes), eliminating
the need write operations to set the pointer for subsequent register reads.
Burst mode can be enabled by setting the register I2C_BURST. When a STOP command is issued the pointer
resets to the original register address before the auto-increments.
22 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 23

Register Write Operation
W
Register Read Operation
pointer set
Start
Stop
Pointer reset to
Register Address
pointer auto increments by 1 every 2 bytes
KCA KCA KCA KCA Register
Address
W KCA KCA Register
Address
KCA R KCA KCA
W
KCA KCA Register
Address
KCA
R
KCA
KCA
KCA
KCA
KCA
KCA
KCA
KCA
KCA
KCA
OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
Target Address data[15:8] data[7:0]
Target Address Target Address data[15:8] data[7:0]
Register Address
Target Address Target Address data[15:8] data[7:0]
Register Address+1 Register Address+2
data[15:8] data[7:0] data[15:8] data[7:0]
Register Address+3 Register Address+4
data[15:8] data[7:0] data[15:8] data[7:0]
Figure 8-8. I2C Operations
8.5.2.3 General-Call Reset Command
The I2C general-call reset allows the host controller in one command to reset all devices on the bus that respond
to the general-call reset command. The general call is initiated by writing to the I2C address 0 (0000 0000b). The
reset command is initiated when the subsequent second address byte is 06h (0000 0110b). With this transaction,
the device issues an acknowledge bit and sets all registers to the power-on-reset default condition.
8.5.2.4 SMBus Alert Response
The SMBus alert response provides a quick identification for which device issued the interrupt. Without this alert
response capability, the processor does not know which device pulled the interrupt line when there are multiple
target devices connected.
OPT4001 is designed to respond to the SMBus alert response address, when in the latched window-style
comparison mode. The OPT4001 does not respond to the SMBus alert response when in transparent mode.
The response behavior of the device to the SMBus alert response is shown in Figure 8-9. When the interrupt
line to the processor is pulled to active, the controller can broadcast the alert response target address. Following
this alert response, any target devices that generated an alert identify themselves by acknowledging the alert
response and sending respective I2C address on the bus. The alert response can activate several different
target devices simultaneously. If more than one target attempts to respond, bus arbitration rules apply. The
device with the lowest address wins the arbitration. If the OPT4001 loses the arbitration, the device does
not acknowledge the I2C transaction and the INT pin remains in an active state, prompting the I2C controller
processor to issue a subsequent SMBus alert response. When the OPT4001 wins the arbitration, the device
acknowledges the transaction and sets the INT pin to inactive. The controller can issue that same command
again, as many times as necessary to clear the INT pin. See Section 8.4.2 for additional details of how the
flags and INT pin are controlled. The controller can obtain information about the source of the OPT4001 interrupt
from the address broadcast in the above process. The FLAG_H value is sent as the final LSB of the address to
provide the controller additional information about the cause of the OPT4001 interrupt. If the controller requires
additional information, the result register or the configuration register can be queried. The FLAG_H and FLAG_L
fields are not cleared upon an SMBus alert response.
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 23
Product Folder Links: OPT4001

## Page 24

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
INT
1 9 1 9
SCL
SDA 0 0 0 1 1 0 0 R/ W 1 0 0 0 1 A1 A0 FH(1)
Start By ACK By From NACK By Stop By
Controller Device Device Controller Controller
Frame 1 SMBus ALERT Response Address Byte Frame 2 Target Address Byte(2)
A. FH is the FLAG_H register
B. A1 and A0 are determined by the ADDR pin (only on SOT-5X3 version)
Figure 8-9. Timing Diagram for SMBus Alert Response
24 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 25

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
8.6 Register Maps
Figure 8-10. ALL Register Map
ADD D15 D14 D13 D12 D11 D10 D9 D8 D7 D6 D5 D4 D3 D2 D1 D0
00h EXPONENT RESULT_MSB
01h RESULT_LSB COUNTER CRC
02h EXPONENT_FIFO0 RESULT_MSB_FIFO0
03h RESULT_LSB_FIFO0 COUNTER_FIFO0 CRC_FIFO0
04h EXPONENT_FIFO1 RESULT_MSB_FIFO1
05h RESULT_LSB_FIFO1 COUNTER_FIFO1 CRC_FIFO1
06h EXPONENT_FIFO2 RESULT_MSB_FIFO2
07h RESULT_LSB_FIFO2 COUNTER_FIFO2 CRC_FIFO2
08h THRESHOLD_L_EXPONENT THRESHOLD_L_RESULT
09h THRESHOLD_H_EXPONENT THRESHOLD_H_RESULT
0Ah QWAKE 0 RANGE CONVERSION_TIME OPERATING_MODE LATCH INT_POL FAULT_COUNT
0Bh 1024 INT_DIR INT_CFG 0 I2C_BURST
0 OVERLOAD CONVERSI FLAG_H FLAG_L
_FLAG ON_READY
0Ch _FLAG
11h 0 DIDL DIDH
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 25
Product Folder Links: OPT4001

## Page 26

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
8.6.1 ALL Register Map
8.6.1.1 Register 0h (offset = 0h) [reset = 0h]
Figure 8-11. Register 0h
15 14 13 12 11 10 9 8
EXPONENT RESULT_MSB
R-0h R-0h
7 6 5 4 3 2 1 0
RESULT_MSB
R-0h
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-12. Register 00 Field Descriptions
Bit Field Type Reset Description
EXPONENT output. Determines the full-scale range of
15-12 EXPONENT R 0h the light measurement. Used as a scaling factor for lux
calculation
Result register MSB (Most significant bits). Used to
11-0 RESULT_MSB R 0h calculate the MANTISSA representing light level within a
given EXPONENT or full-scale range
8.6.1.2 Register 1h (offset = 1h) [reset = 0h]
Figure 8-13. Register 1h
15 14 13 12 11 10 9 8
RESULT_LSB
R-0h
7 6 5 4 3 2 1 0
COUNTER CRC
R-0h R-0h
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-14. Register 01 Field Descriptions
Bit Field Type Reset Description
Result register LSB(Least significant bits). Used to
15-8 RESULT_LSB R 0h calculate MANTISSA representing light level within a given
EXPONENT or full-scale range
Sample counter. Rolling counter which increments for every
7-4 COUNTER R 0h
conversion
CRC bits.
R[19:0]=MANTISSA=((RESULT_MSB<<8)+ RESULT_LSB
X[0]=XOR(E[3:0],R[19:0],C[3:0]) XOR of all bits
3-0 CRC R 0h X[1]=XOR(C[1],C[3],R[1],R[3],R[5],R[7],R[9],R[11],R[13],R[1
5],R[17],R[19],E[1],E[3])
X[2]=XOR(C[3],R[3],R[7],R[11],R[15],R[19],E[3])
X[3]=XOR(R[3],R[11],R[19])
8.6.1.3 Register 2h (offset = 2h) [reset = 0h]
Figure 8-15. Register 2h
15 14 13 12 11 10 9 8
26 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 27

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
Figure 8-15. Register 2h (continued)
EXPONENT_FIFO0 RESULT_MSB_FIFO0
R-0h R-0h
7 6 5 4 3 2 1 0
RESULT_MSB_FIFO0
R-0h
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-16. Register 02 Field Descriptions
Bit Field Type Reset Description
EXPONENT_FIF
15-12 R 0h EXPONENT register from FIFO 0
O0
RESULT_MSB_FI
11-0 R 0h RESULT_MSB Register from FIFO 0
FO0
8.6.1.4 Register 3h (offset = 3h) [reset = 0h]
Figure 8-17. Register 3h
15 14 13 12 11 10 9 8
RESULT_LSB_FIFO0
R-0h
7 6 5 4 3 2 1 0
COUNTER_FIFO0 CRC_FIFO0
R-0h R-0h
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-18. Register 03 Field Descriptions
Bit Field Type Reset Description
RESULT_LSB_FI
15-8 R 0h RESULT_LSB Register from FIFO 0
FO0
COUNTER_FIFO
7-4 R 0h COUNTER Register from FIFO 0
0
3-0 CRC_FIFO0 R 0h CRC Register from FIFO 0
8.6.1.5 Register 4h (offset = 4h) [reset = 0h]
Figure 8-19. Register 4h
15 14 13 12 11 10 9 8
EXPONENT_FIFO1 RESULT_MSB_FIFO1
R-0h R-0h
7 6 5 4 3 2 1 0
RESULT_MSB_FIFO1
R-0h
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-20. Register 04 Field Descriptions
Bit Field Type Reset Description
EXPONENT_FIF
15-12 R 0h EXPONENT register from FIFO 1
O1
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 27
Product Folder Links: OPT4001

## Page 28

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
Figure 8-20. Register 04 Field Descriptions (continued)
Bit Field Type Reset Description
RESULT_MSB_FI
11-0 R 0h RESULT_MSB Register from FIFO 1
FO1
8.6.1.6 Register 5h (offset = 5h) [reset = 0h]
Figure 8-21. Register 5h
15 14 13 12 11 10 9 8
RESULT_LSB_FIFO1
R-0h
7 6 5 4 3 2 1 0
COUNTER_FIFO1 CRC_FIFO1
R-0h R-0h
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-22. Register 05 Field Descriptions
Bit Field Type Reset Description
RESULT_LSB_FI
15-8 R 0h RESULT_LSB Register from FIFO 1
FO1
COUNTER_FIFO
7-4 R 0h COUNTER Register from FIFO 1
1
3-0 CRC_FIFO1 R 0h CRC Register from FIFO 1
8.6.1.7 Register 6h (offset = 6h) [reset = 0h]
Figure 8-23. Register 6h
15 14 13 12 11 10 9 8
EXPONENT_FIFO2 RESULT_MSB_FIFO2
R-0h R-0h
7 6 5 4 3 2 1 0
RESULT_MSB_FIFO2
R-0h
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-24. Register 06 Field Descriptions
Bit Field Type Reset Description
EXPONENT_FIF
15-12 R 0h EXPONENT register from FIFO 2
O2
RESULT_MSB_FI
11-0 R 0h RESULT_MSB Register from FIFO 2
FO2
8.6.1.8 Register 7h (offset = 7h) [reset = 0h]
Figure 8-25. Register 7h
15 14 13 12 11 10 9 8
RESULT_LSB_FIFO2
R-0h
7 6 5 4 3 2 1 0
28 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 29

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
Figure 8-25. Register 7h (continued)
COUNTER_FIFO2 CRC_FIFO2
R-0h R-0h
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-26. Register 07 Field Descriptions
Bit Field Type Reset Description
RESULT_LSB_FI
15-8 R 0h RESULT_LSB Register from FIFO 2
FO2
COUNTER_FIFO
7-4 R 0h COUNTER Register from FIFO 2
2
3-0 CRC_FIFO2 R 0h CRC Register from FIFO 2
8.6.1.9 Register 8h (offset = 8h) [reset = 0h]
Figure 8-27. Register 8h
15 14 13 12 11 10 9 8
THRESHOLD_L_EXPONENT THRESHOLD_L_RESULT
R/W-0h R/W-0h
7 6 5 4 3 2 1 0
THRESHOLD_L_RESULT
R/W-0h
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-28. Register 08 Field Descriptions
Bit Field Type Reset Description
THRESHOLD_L_
15-12 R/W 0h Threshold low register exponent
EXPONENT
THRESHOLD_L_
11-0 R/W 0h Threshold low register result
RESULT
8.6.1.10 Register 9h (offset = 9h) [reset = BFFFh]
Figure 8-29. Register 9h
15 14 13 12 11 10 9 8
THRESHOLD_H_EXPONENT THRESHOLD_H_RESULT
R/W-Bh R/W-Fh
7 6 5 4 3 2 1 0
THRESHOLD_H_RESULT
R/W-FFh
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-30. Register 09 Field Descriptions
Bit Field Type Reset Description
THRESHOLD_H_
15-12 R/W Bh Threshold high register exponent
EXPONENT
THRESHOLD_H_
11-0 R/W FFFh Threshold high register result
RESULT
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 29
Product Folder Links: OPT4001

## Page 30

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
8.6.1.11 Register Ah (offset = Ah) [reset = 3208h]
Figure 8-31. Register Ah
15 14 13 12 11 10 9 8
QWAKE 0 RANGE CONVERSION_TIME
R/W-0h R/W-0h R/W-Ch R/W-2h
7 6 5 4 3 2 1 0
CONVERSION_TIME OPERATING_MODE LATCH INT_POL FAULT_COUNT
R/W-0h R/W-0h R/W-1h R/W-0h R/W-0h
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-32. Register 0A Field Descriptions
Bit Field Type Reset Description
Quick Wake-up from Standby in one shot mode by not
powering down all circuits. Applicable only in One-shot
15-15 QWAKE R/W 0h
mode and helps get out of standby mode faster with penalty
in power consumption compared to full standby mode.
14-14 0 R/W 0h Must read or write 0
Controls the full-scale light level range of the device. The
format of this register is same as the EXPONENT register
for all values from 0 to 8. PicoStar(TM) variant:
0 : 328lux
1 : 655lux
2 : 1.3klux
3 : 2.6klux
4 : 5.2klux
5 : 10.5klux
6 : 21klux
7 : 42klux
8 : 83klux
13-10 RANGE R/W Ch
12 : Auto-Range
SOT-5X3 variant:
0 : 459lux
1 : 918lux
2 : 1.8klux
3 : 3.7klux
4 : 7.3klux
5 : 14.7klux
6 : 29.4klux
7 : 58.7klux
8 : 117.4klux
12 : Auto-range
Controls the device conversion time
0 : 600us
1 : 1ms
2 : 1.8ms
3 : 3.4ms
4 : 6.5ms
CONVERSION_TI
9-6 R/W 8h 5 : 12.7ms
ME
6 : 25ms
7 : 50ms
8 : 100ms
9 : 200ms
10 : 400ms
11 : 800ms
Controls device mode of operation
0 : Power-down
OPERATING_MO
5-4 R/W 0h 1 : Forced auto-range One-shot
DE
2 : One-shot
3 : Continuous
Controls the functionality of the interrupt reporting
3-3 LATCH R/W 1h
mechanisms for INT pin for the threshold detection logic.
30 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 31

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
Figure 8-32. Register 0A Field Descriptions (continued)
Bit Field Type Reset Description
Controls the polarity or active state of the INT pin.
2-2 INT_POL R/W 0h 0 : Active Low
1 : Active High
Fault count register instructs the device as to how
many consecutive fault events are required to trigger the
threshold mechanisms: the flag high (FLAG_H) and the flag
low (FLAG_L) registers.
1-0 FAULT_COUNT R/W 0h
0 : One fault Count
1 : Two Fault Counts
2 : Four Fault Counts
3 : Eight Fault Counts
8.6.1.12 Register Bh (offset = Bh) [reset = 8011h]
Figure 8-33. Register Bh
15 14 13 12 11 10 9 8
1 0 0 0 0 0 0 0
R/W-1h R/W-0h R/W-0h R/W-0h R/W-0h R/W-0h R/W-0h R/W-0h
7 6 5 4 3 2 1 0
0 0 0 INT_DIR INT_CFG 0 I2C_BURST
R/W-0h R/W-0h R/W-0h R/W-1h R/W-0h R/W-0h R/W-1h
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-34. Register 0B Field Descriptions
Bit Field Type Reset Description
15-5 1024 R/W 400h Must read or write 1024
Determines the direction of the INT pin.
4-4 INT_DIR R/W 1h 0 : Input
1 : Output
Controls the output interrupt mechanism after end of
conversion
0 : SMBUS Alert
3-2 INT_CFG R/W 0h
1 : INT Pin asserted after every conversion
2: Invalid
3: INT pin asserted after every 4 conversions (FIFO full)
1-1 0 R/W 0h Must read or write 0
When set, enables I2C burst mode minimizing I2C read
0-0 I2C_BURST R/W 1h cycles by auto incrementing read register pointer by 1 after
every register read
8.6.1.13 Register Ch (offset = Ch) [reset = 0h]
Figure 8-35. Register Ch
15 14 13 12 11 10 9 8
0 0 0 0 0 0 0 0
R/W-0h R/W-0h R/W-0h R/W-0h R/W-0h R/W-0h R/W-0h R/W-0h
7 6 5 4 3 2 1 0
0 0 0 0 OVERLOAD_F CONVERSION FLAG_H FLAG_L
LAG _READY_FLAG
R/W-0h R/W-0h R/W-0h R/W-0h R-0h R-0h R-0h R-0h
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 31
Product Folder Links: OPT4001

## Page 32

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-36. Register 0C Field Descriptions
Bit Field Type Reset Description
15-4 0 R/W 0h Must read or write 0
Indicates when an overflow condition occurs in the data
OVERLOAD_FLA
3-3 R 0h conversion process, typically because the light illuminating
G
the device exceeds the full-scale range.
Conversion ready flag indicates when a conversion
completes. The flag is set to 1 at the end of a conversion
CONVERSION_R and is cleared (set to 0) when register address 0xC is either
2-2 R 0h
EADY_FLAG read or written with any non-zero value
0 : Conversion in progress
1 : Conversion is complete
Flag high register identifies that the result of a
conversion is measurement than a specified level of
interest. FLAG_H is set to 1 when the result is larger
1-1 FLAG_H R 0h than the level in the THRESHOLD_H_EXPONENT and
THRESHOLD_H_RESULT registers for a consecutive
number of measurements defined by the FAULT_COUNT
register.
Flag low register identifies that the result of a
measurement is smaller than a specified level of
interest. FL is set to 1 when the result is smaller
0-0 FLAG_L R 0h than the level in the THRESHOLD_LOW_EXPONENT
and THRESHOLD_L_RESULT registers for a consecutive
number of measurements defined by the FAULT_COUNT
register.
8.6.1.14 Register 11h (offset = 11h) [reset = 121h]
Figure 8-37. Register 11h
15 14 13 12 11 10 9 8
0 0 DIDL DIDH
R/W-0h R/W-0h R-0h R-1h
7 6 5 4 3 2 1 0
DIDH
R-21h
LEGEND: R/W = Read/Write; W = Write only; -n = value after reset
Figure 8-38. Register 11 Field Descriptions
Bit Field Type Reset Description
15-14 0 R/W 0h Must read or write 0
13-12 DIDL R 0h Device ID L
11-0 DIDH R 121h Device ID H
32 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 33

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
9 Application and Implementation
Note
Information in the following applications sections is not part of the TI component specification,
and TI does not warrant its accuracy or completeness. TI's customers are responsible for
determining suitability of components for their purposes, as well as validating and testing their design
implementation to confirm system functionality.
9.1 Application Information
Ambient light sensors are used in a wide variety of applications that require precise measurement of light as
perceived by human eye, since they have a specialized filter that mimic human eye. The following sections
shows crucial information about integrating OPT4001 in applications.
9.2 Typical Application
9.2.1 Electrical Interface
The electrical interface is quite simple, as illustrated in Figure 9-1 below. Connect the OPT4001 I2C SDA and
SCL pins to the same pins of an applications processor, micro controller, or other digital processor. If that digital
processor requires an interrupt resulting from an event of interest from theOPT4001, then connect the INT pin
to either an interrupt or general-purpose I/O pin of the processor (Only for the SOT-5X3). There are multiple
uses for this INT pin, including triggering a measurement on one-shot mode, signaling the system to wake up
from low-power mode, processing other tasks while waiting for an ambient light event of interest, or alerting the
processor that a sample is ready to be read.. Connect pullup resistors between a power supply appropriate for
digital communication and the SDA and SCL pins (because the pins have open-drain output structures). If the
INT pin is used, connect a pullup resistor to the INT pin. A typical value for these pullup resistors is 10 k ohm. The
resistor choice can be optimized in conjunction to the bus capacitance to balance the system speed, power,
noise immunity, and other requirements.
Short or Open
VIO: 1.6V to 5V
VDD:1.6V to 3.6V 0.1uF 10k 10k 10k
Ambient
VDD
Light VIO
OPT4001
SDA SDA
SCL SCL
INT GPIO/TimerPin
Only SOT-5X3
ADDR
Controller
GND Micro Controller or Processor
GND
Figure 9-1. Typical Application Schematic
The power supply and grounding considerations are discussed in the Section 9.4.
Although spike suppression is integrated in the SDA and SCL pin circuits, use proper layout practices to
minimize the amount of coupling into the communication lines. One possible introduction of noise occurs from
capacitively coupling signal edges between the two communication lines themselves. Another possible noise
introduction comes from other switching noise sources present in the system, especially for long communication
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 33
Product Folder Links: OPT4001

## Page 34

lines. In noisy environments, shield communication lines to reduce the possibility of unintended noise coupling
into the digital I/O lines that can be incorrectly interpreted.
9.2.1.1 Design Requirements
9.2.1.1.1 Optical Interface
The optical interface is physically located on the same side of the device pins as the electrical interface for the
PicoStar(TM) variant and facing away from the pins for the SOT-5X3 variant, as shown in Figure 9-2 and Figure 9-3
1 2
B VDD SDA
A GND SCL
3.0
0.38
Figure 9-2. Sensor Position on PicoStar(TM) Variant
A A 3.0 93.0
OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
1 8
0.38 Section A
Figure 9-3. Sensor Position on the SOT-5X3 Variant
In case of the PicoStar(TM) variant systems, light that illuminates the sensor must come through the FPCB.
Typically, the best method is to create a cutout area in the FPCB. Other methods are possible, but with
associated design tradeoffs. This cutout must be carefully designed because the dimensions and tolerances
impact the net-system, optical field-of-view performance. The design of this cutout is discussed more in the
Section 9.5.2.
Physical components, such as a plastic housing and a window that allows light from outside of the design to
illuminate the sensor (see Figure 9-4), can help protect the device and neighboring circuitry. Sometimes, a dark
or opaque window is used to further enhance the visual appeal of the design by hiding the sensor from view.
This window material is typically transparent plastic or glass.
Generally for both package variants, any physical component that affects the light that illuminates the sensing
area of a light sensor also affects the performance of that light sensor. Therefore, for the best performance,
34 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 35

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
make sure to understand and control the effect of these components. Design a window width and height to
permit light from a sufficient field of view to illuminate the sensor. For best performance, use a field of view of
at least +/-35 deg , or preferably +/-45 deg  or more. Understanding and designing the field of view is discussed further in
application report OPT3001: Ambient Light Sensor Application Guide (SBEA002).
The visible-spectrum transmission for dark windows typically ranges between 5% to 30%, but can be less than
1%. Specify a visible-spectrum transmission as low as, but no more than, necessary to achieve sufficient visual
appeal because decreased transmission decreases the available light for the sensor to measure. The windows
are made dark by either applying an ink to a transparent window material, or including a dye or other optical
substance within the window material itself. This attenuating transmission in the visible spectrum of the window
creates a ratio between the light on the outside of the design and the light that is measured by the device. To
accurately measure the light outside of the design, compensate the device measurement for this ratio.
Although the inks and dyes of dark windows serve their primary purpose of being minimally transmissive to
visible light, some inks and dyes can also be very transmissive to infrared light. The use of these inks and
dyes further decreases the ratio of visible to infrared light, and thus decreases sensor measurement accuracy.
However, because of the excellent red and infrared rejection of the device, this effect is minimized, and good
results are achieved under a dark window with similar spectral responses.
For best accuracy, avoid grill-like window structures, unless the designer understands the optical effects
sufficiently. These grill-like window structures create a nonuniform illumination pattern at the sensor that make
light measurement results vary with placement tolerances and angle of incidence of the light. If a grill-like
structure is desired, then the device is an excellent sensor choice because the device is minimally sensitive to
illumination uniformity issues disrupting the measurement process.
Light pipes can appear attractive for aiding in the optomechanical design that brings light to the sensor; however,
do not use light pipes with any light sensor unless the system designer fully understands the ramifications of the
optical physics of light pipes within the full context of his design and objectives.
9.2.1.2 Detailed Design Procedure
9.2.1.2.1 Optomechanical Design (PicoStar(TM) Variant)
After completing the electrical design and understanding optical interface, the next task is the optomechanical
design of the FPCB cutout. Design this cutout in conjunction with the tolerance capabilities of the FPCB
manufacturer. Or, conversely, choose the FPCB manufacturer for the capabilities of creating this cutout. A
semi-rectangular shape of the cutout, created with a standard FPCB laser, is presented here. There are many
alternate approaches with different cost, tolerance, and performance tradeoffs.
An image of the created FPCB with the plus shaped cutout and a rectangular shaped cutout is shown below.
The plus shape is a good choice for light collection in both directions with a wider field of view. In case of the
rectangular cutout shape, the long (vertical) direction of the cutout has minimal effect on the angular response
because any shadows created from the FPCB do not come near the sensor. The long cutout direction defines
the axis of rotation with the less restricted field of view. The narrow (horizontal) direction of the cutout, which
is limited by the electrical connections to OPT4001, can create shadows that can have a minor impact on the
angular response. The narrow cutout direction defines the axis of rotation of the more restricted view. The
possibility of shadows are illustrated in Figure 9-6, a cross-sectional diagram showing the OPT4001 device, with
the sensing area, so ldered to the FPCB with the cutout. A circular cutout is more restrictive in the field of view
casting shadow from all directions of light. TI recommends to take in to account the effect of shadows and impact
of this on the field of view of the sensor. The product folder has application notes and tools to help understand
these artifacts.
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 35
Product Folder Links: OPT4001

## Page 36

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
Figure 9-4. Image of FPCB With OPT4001 Mounted, Receiving Light Through the Cutout with a Plus
Shape
Figure 9-5. Image of FPCB With OPT4001 Mounted, Receiving Light Through the Cutout with a
Rectangular Shape
Device
Illuminated Shadowed
Sensor Sensor
Copper Pillar Electrical Connection Sensing Area
Solder
FPCB Shadow FPCB
Shadow Limiting
Point
Light entering from
30 degree angle
Figure 9-6. Cross-Sectional Diagram of OPT4001 Soldered to an FPCB With a Cutout, Including Light
Entering From an Angle
There can be an additional need to put a product casing over the assembly of the device and the FPCB. The
window sizing and placement for such an assembly is discussed in more rigorous detail in application report
OPT3001: Ambient Light Sensor Application Guide (SBEA002).
9.2.1.2.2 Optomechanical Design (SOT-5X3 Variant)
After completing the electrical design, the next task is the optomechanical design. Window sizing and placement
is discussed in more rigorous detail in OPT3001: Ambient Light Sensor Application Guide.
36 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 37

9.2.1.3 Application Curves (PicoStar(TM) Variant)
Figure 9-7 and Figure 9-8 show example response curves of the device for a rectangular cut out hole as shown
in Figure 9-13. The shape of the cutout affects the overall light collection and the field of view can clearly be
seen.
Incidence Angle (Degrees)
esnopseR
dezilamroN
1
0.9
0.8
0.7
0.6
0.5
0.4
0.3
0.2
0.1
0
-90 -75 -60 -45 -30 -15 0 15 30 45 60 75 90
Incidence Angle (Degrees)
D010
Figure 9-7. Angular Response of this FPCB Design
Along the Less-Restricted Rotational Axis
esnopseR
dezilamroN
OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
1
0.9
0.8
0.7
0.6
0.5
0.4
0.3
0.2
0.1
0
-90 -75 -60 -45 -30 -15 0 15 30 45 60 75 90
D022
Figure 9-8. Angular Response of this FPCB Design
Along the More-Restricted Rotational Axis
9.3 Do's and Don'ts
As with any optical product, take special care when handling the OPT4001. In case of the PicoStar(TM) variant,
the device is a piece of active silicon, without the mechanical protection of an epoxy-like package or other
reinforcement. This design allows the device to be as thin as possible. Take extra care to handle the device
gently to not crack or break the device. Use a properly-sized vacuum manipulation tool to handle the device.
Generally for both package variants, the optical surface of the device must be kept clean for the best
performance, both when prototyping with the device, and during mass production manufacturing procedures.
Keep the optical surface clean of fingerprints, dust, and other optical-inhibiting contaminants.
If the optical surface of the device requires cleaning, then use a few gentle brushes with a soft swab of deionized
water or isopropyl alcohol. Avoid potentially abrasive cleaning and manipulating tools and excessive force that
can scratch the optical surface.
If the OPT4001 performs less than excellent, then inspect the optical surface for dirt, scratches, or other optical
artifacts.
9.4 Power Supply Recommendations
Although the OPT4001 has low sensitivity to power-supply issues, good practices are always recommended.
For best performance, the device VDD pin must have a stable, low-noise power supply with a 100-nF bypass
capacitor close to the device and solid grounding. There are many options for powering the device because of
the device low current consumption levels.
9.5 Layout
9.5.1 Layout Guidelines
Before understanding the layout requirement for OPT4001, understanding the placement on the PCB is critical.
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 37
Product Folder Links: OPT4001

## Page 38

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
OPT4001
SOT-5X3
FPCB Hole 0.2mm thickness
OPT4001 PicoStarTM
PCB
1.2mm thickness
Figure 9-9. Placement Side View of Packages
In case of the SOT-5X3 package variant the device, since the lighting sensitive area and the device pins are
on opposite sides of each other, a conventional placement on the PCB makes sure of good light collection. In
case of the PicoStar(TM) variant of the device, since the light sensitive area and the device pins are on the same
side, special arrangement as shown in the figure is required to achieve good light collection. Typically a thin
flexible PCB with a hole or a cutout centered around the optical area is required for wide angle light collection
for the PicoStar(TM) variant. A regular PCB can be used but the amount of light collected and the field of view of
light collection are not very good and generally not recommended. Cut out for the light collection can be of any
shape with large enough opening to let ample light fall on the light sensitive area. Figure 9-12 and Figure 9-13
show examples of two such shapes which help maximize light collection. A circular cut out as much larger as
the manufacturing allows is also acceptable but can restrict the field of view and reduce the light collection. Tools
and documentation are available on TI product folder to estimate the field of view based on the hole size.
Placing the decoupling capacitor close to the device is highly recommended at the same time, note that optically
reflective surfaces of components also affect the performance of the design. The three-dimensional geometry
of all components and structures around the sensor must be taken into consideration to prevent unexpected
results from secondary optical reflections. Placing capacitors and components at a distance of at least twice the
height of the component is usually sufficient. The best optical layout is to place all close components on the
opposite side of the PCB from the OPT4001. However, this approach is not be practical for the constraints of
every design.
The device layout is also critical for good SMT assembly. Two types of land pattern pads can be used for
this package: solder mask defined pads (SMD) and non-solder mask defined pads (NSMD). SMD pads have
a solder mask opening that is smaller than the metal pads, whereas NSMD has a solder mask opening that is
larger than the metal pad. Figure 9-10 illustrates these types of landing-pattern pads. SMD is preferred because
SMD provides a more accurate soldering-pad dimension with the trace connections. For further discussion of
SMT and PCB recommendations, see the Soldering and Handling Recommendations.
Figure 9-10. Soldermask Defined Pad (SMD) and Non-Soldermask Defined Pad (NSMD)
38 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 39

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
9.5.2 Layout Example
VDD VDD SDA
ADDR INT
GND NC NC
VSS SCL
Figure 9-11. Layout Example for SOT-5X3 package
GND
SCL
A1 A2
+ shape Cut-out
for
Light Collection
B1 B2
VDD SDA
Figure 9-12. Layout Example with a plus shaped cut out
GND SCL
A1 A2
Rectangular Cut-out
for
Light Collection
B1 B2
VDD SDA
Figure 9-13. Layout Example with a rectangular shaped cut out
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 39
Product Folder Links: OPT4001

## Page 40

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
9.5.2.1 Soldering and Handling Recommendations (SOT-5X3 Variant)
The OPT4001 has been qualified for three soldering reflow operations per JEDEC JSTD-020.
Note that excessive heat can discolor the device and affect optical performance.
See application report SLUA271, QFN/SON PCB Attachment, for details on soldering thermal profile and other
information. If the OPT4001 must be removed from a PCB, discard the device and do not reattach.
As with most optical devices, handle the device with special care to make sure that optical surfaces stay clean
and free from damage. See Section 9.3 for more detailed recommendations. For best optical performance,
solder flux and any other possible debris must be cleaned after soldering processes.
Note
The bottom side of the device features an angled feature to denote the PIN 1
Figure 9-14. Identification Feature for PIN 1
Figure 9-15. Identification Features for PIN 1 on Package
9.5.2.2 Soldering and Handling Recommendations (PicoStar(TM) Variant)
The OPT4001 is a small device with special soldering and handling considerations. See Section 9.2.1.2.1 for
implications of alignment between the device and the cutout area. See Section 9.5.1 for considerations of the
soldering pads.
If the OPT4001 must be removed from a PCB, discard the device and do not reattach.
Note that excessive heat can discolor the device and affect optical performance.
As with most optical devices, handle the OPT4001 with special care to make sure that optical surfaces stay
clean and free from damage. See Section 9.3 for more detailed recommendations. For best optical performance,
solder flux and any other possible debris must be cleaned after soldering processes.
9.5.2.2.1 Solder Paste
For solder-paste deposition, use a stencil-printing process that involves the transfer of solder paste through
predefined apertures with the application of pressure. Stencil parameters, such as aperture area ratio and
fabrication process, have a significant impact on paste deposition. Cut the stencil apertures using a laser with
an electropolish-fabrication method. Taper the stencil aperture walls by 5 deg  to facilitate paste release. Shifting the
40 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 41

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
solder-paste towards the outside of the device minimizes the possibility of solder getting into the device sensing
area. See the mechanical packages attached to the end of this data sheet.
Use solder paste selection type 4 or higher, no-clean, lead-free solder paste. If solder splatters in the reflow
process, choose a solder paste with normal- or low-flux contents, or alter the reflow profile per the Section
9.5.2.2.3.
9.5.2.2.2 Package Placement
Use a pick-and-place nozzle with a size number larger than 0.6 mm. If the placement method is done by
programming the component thickness, then add 0.04 mm to the actual component thickness so that the
package sits halfway into the solder paste. If placement is by force, then choose minimum force no larger than
3N to avoid forcing out solder paste, or free falling the package, and to avoid soldering problems such as
bridging and solder balling.
9.5.2.2.3 Reflow Profile
Use the profile in Figure 9-16, and adjust if necessary. Use a slow solder reflow ramp rate of 1 deg C to 1.2 deg C/s to
minimize chances of solder splattering onto the sensing area.
Figure 9-16. Recommended Solder Reflow Temperature Profile
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 41
Product Folder Links: OPT4001

## Page 42

OPT4001
SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022 www.ti.com
9.5.2.2.4 Special Flexible Printed-Circuit Board (FPCB) Recommendations
Special flexible printed-circuit board (FPCB) design recommendations include:
- Fabricate per IPC-6013.
- Use material of flexible copper clad per IPC 4204/11 (Define polyimide and copper thickness per product
application).
- Finish: All exposed copper are electroless Ni immersion gold (ENIG) per IPC 4556.
- Solder mask per IPC SM840.
- Use a laser to create the cutout for light sensing for better accuracy, and to avoid affecting the soldering
pad dimension. Other options, such as punched cutouts, are possible. See the Section 9.2.1.2.1 for further
discussion ranging from the implications of the device to cutout region size and alignment. The full design
must be considered, including the tolerances.
To assist the handling of the very thin flexible circuit, design and fabricate a fixture to hold the flexible circuit
through the paste-printing, pick-and-place, and reflow processes. Contact the factory for examples of such
fixtures.
9.5.2.2.5 Rework Process
If the device must be removed from a PCB, discard the device and do not reattach. To remove the package from
the PCB/Flexi cable, heat the solder joints above liquidus temperature. Bake the board at 125 deg C for 4 hours prior
to rework to remove moisture that may crack the PCB or causing delamination. Use a thermal heating profile
to remove a package that is close to the profile that mounts the package. Clean the site to remove any excess
solder and residue to prepare for installing a new package. Use a mini stencil (localized stencil) to apply solder
paste to the land pattern. In case a mini stencil cannot be used because of spacing or other reasons, apply
solder paste on the package pads directly, then mount, and reflow.
42 Submit Document Feedback Copyright (C) 2022 Texas Instruments Incorporated
Product Folder Links: OPT4001

## Page 43

OPT4001
www.ti.com SBOS993A - DECEMBER 2021 - REVISED DECEMBER 2022
10 Device and Documentation Support
10.1 Documentation Support
10.1.1 Related Documentation
For related documentation see the following:
- OPT3001: Ambient Light Sensor Application Guide (SBEA002)
- OPT4001EVM User's Guide (SBOU278)
- QFN/SON PCB Attachment Application Report (SLUA271)
10.2 Receiving Notification of Documentation Updates
To receive notification of documentation updates, navigate to the device product folder on ti.com. Click on
Subscribe to updates to register and receive a weekly digest of any product information that has changed. For
change details, review the revision history included in any revised document.
10.3 Support Resources
TI E2E(TM) support forums are an engineer's go-to source for fast, verified answers and design help - straight
from the experts. Search existing answers or ask your own question to get the quick design help you need.
Linked content is provided "AS IS" by the respective contributors. They do not constitute TI specifications and do
not necessarily reflect TI's views; see TI's Terms of Use.
10.4 Trademarks
PicoStar(TM), Picostar(TM), and TI E2E(TM) are trademarks of Texas Instruments.
All trademarks are the property of their respective owners.
10.5 Electrostatic Discharge Caution
This integrated circuit can be damaged by ESD. Texas Instruments recommends that all integrated circuits be handled
with appropriate precautions. Failure to observe proper handling and installation procedures can cause damage.
ESD damage can range from subtle performance degradation to complete device failure. Precision integrated circuits may
be more susceptible to damage because very small parametric changes could cause the device not to meet its published
specifications.
10.6 Glossary
TI Glossary This glossary lists and explains terms, acronyms, and definitions.
11 Mechanical, Packaging, and Orderable Information
The following pages include mechanical, packaging, and orderable information. This information is the most
current data available for the designated devices. This data is subject to change without notice and revision of
this document. For browser-based versions of this data sheet, refer to the left-hand navigation.
Copyright (C) 2022 Texas Instruments Incorporated Submit Document Feedback 43
Product Folder Links: OPT4001

## Page 44

PACKAGE OPTION ADDENDUM
www.ti.com 29-Aug-2023
PACKAGING INFORMATION
Orderable Device Status Package Type Package Pins Package Eco Plan Lead finish/ MSL Peak Temp Op Temp ( deg C) Device Marking Samples
(1) Drawing Qty (2) Ball material (3) (4/5)
(6)
OPT4001DTSR ACTIVE SOT-5X3 DTS 8 3000 RoHS & Green NIPDAU Level-2-260C-1 YEAR -40 to 85 4001
Samples
OPT4001DTST ACTIVE SOT-5X3 DTS 8 250 RoHS & Green NIPDAU Level-2-260C-1 YEAR -40 to 85 4001
Samples
OPT4001YMNR ACTIVE PICOSTAR YMN 4 3000 RoHS & Green SAC396 Level-1-260C-UNLIM -40 to 85 01
Samples
OPT4001YMNT ACTIVE PICOSTAR YMN 4 250 RoHS & Green SAC396 Level-1-260C-UNLIM -40 to 85 01
Samples
(1) The marketing status values are defined as follows:
ACTIVE: Product device recommended for new designs.
LIFEBUY: TI has announced that the device will be discontinued, and a lifetime-buy period is in effect.
NRND: Not recommended for new designs. Device is in production to support existing customers, but TI does not recommend using this part in a new design.
PREVIEW: Device has been announced but is not in production. Samples may or may not be available.
OBSOLETE: TI has discontinued the production of the device.
(2) RoHS: TI defines "RoHS" to mean semiconductor products that are compliant with the current EU RoHS requirements for all 10 RoHS substances, including the requirement that RoHS substance
do not exceed 0.1% by weight in homogeneous materials. Where designed to be soldered at high temperatures, "RoHS" products are suitable for use in specified lead-free processes. TI may
reference these types of products as "Pb-Free".
RoHS Exempt: TI defines "RoHS Exempt" to mean products that contain lead but are compliant with EU RoHS pursuant to a specific EU RoHS exemption.
Green: TI defines "Green" to mean the content of Chlorine (Cl) and Bromine (Br) based flame retardants meet JS709B low halogen requirements of <=1000ppm threshold. Antimony trioxide based
flame retardants must also meet the <=1000ppm threshold requirement.
(3) MSL, Peak Temp. - The Moisture Sensitivity Level rating according to the JEDEC industry standard classifications, and peak solder temperature.
(4) There may be additional marking, which relates to the logo, the lot trace code information, or the environmental category on the device.
(5) Multiple Device Markings will be inside parentheses. Only one Device Marking contained in parentheses and separated by a "~" will appear on a device. If a line is indented then it is a continuation
of the previous line and the two combined represent the entire Device Marking for that device.
(6) Lead finish/Ball material - Orderable Devices may have multiple material finish options. Finish options are separated by a vertical ruled line. Lead finish/Ball material values may wrap to two
lines if the finish value exceeds the maximum column width.
Important Information and Disclaimer:The information provided on this page represents TI's knowledge and belief as of the date that it is provided. TI bases its knowledge and belief on information
provided by third parties, and makes no representation or warranty as to the accuracy of such information. Efforts are underway to better integrate information from third parties. TI has taken and
Addendum-Page 1

## Page 45

PACKAGE OPTION ADDENDUM
www.ti.com 29-Aug-2023
continues to take reasonable steps to provide representative and accurate information but may not have conducted destructive testing or chemical analysis on incoming materials and chemicals.
TI and TI suppliers consider certain information to be proprietary, and thus CAS numbers and other limited information may not be available for release.
In no event shall TI's liability arising out of such information exceed the total purchase price of the TI part(s) at issue in this document sold by TI to Customer on an annual basis.
OTHER QUALIFIED VERSIONS OF OPT4001 :
- Automotive : OPT4001-Q1
NOTE: Qualified Version Definitions:
- Automotive - Q100 devices qualified for high-reliability automotive applications targeting zero defects
Addendum-Page 2

## Page 46

PACKAGE MATERIALS INFORMATION
www.ti.com 10-Jan-2022
TAPE AND REEL INFORMATION
*All dimensions are nominal
Device Package Package Pins SPQ Reel Reel A0 B0 K0 P1 W Pin1
Type Drawing Diameter Width (mm) (mm) (mm) (mm) (mm) Quadrant
(mm) W1 (mm)
OPT4001YMNR PICOST YMN 4 3000 180.0 8.4 0.94 1.15 0.37 2.0 8.0 Q1
AR
OPT4001YMNT PICOST YMN 4 250 180.0 8.4 0.94 1.15 0.37 2.0 8.0 Q1
AR
Pack Materials-Page 1

## Page 47

PACKAGE MATERIALS INFORMATION
www.ti.com 10-Jan-2022
*All dimensions are nominal
Device Package Type Package Drawing Pins SPQ Length (mm) Width (mm) Height (mm)
OPT4001YMNR PICOSTAR YMN 4 3000 182.0 182.0 20.0
OPT4001YMNT PICOSTAR YMN 4 250 182.0 182.0 20.0
Pack Materials-Page 2

## Page 48

PACKAGE OUTLINE
YMN0004A PicoStar T M - 0.226 mm max height
SCALE 15.000
PicoStar
0.87
B 0.81 A
PIN A1
CORNER
1.08
1.02
C
0.226 MAX
SEATING PLANE
0.0087 TYP 0.007
OPTICAL FILTER
(0.397) (0.04) TYP
SENSING AREA
PULL BACK
NO CONNECT
PAD
B
SENSING AREA
(0.286)
PKG
0.83 D(:0 M.31a4x) = 1.08 mm, Min = 1.02 mm
TYP (0.743)
E: Max = 0.87 mm, Min = 0.81 mm
0.415
TYP NO CONNECT
PAD
A
0.18
4X 1 2
0.12
0.015 C A B (0.478)
OPTICAL FILTER
0.62
4226006/A 06/2020
NOTES: PicoStar is a trademark of Texas Instruments.
1. All linear dimensions are in millimeters. Any dimensions in parenthesis are for reference only. Dimensioning and tolerancing
per ASME Y14.5M.
2. This drawing is subject to change without notice.
www.ti.com

## Page 49

EXAMPLE BOARD LAYOUT
YMN0004A PicoStar T M - 0.226 mm max height
PicoStar
(0.93)
( 0.15) TYP (0.31) TYP
SOLDER MASK
OPENING 1 2
A
PCB CUTOUT
(REFER TO THE LAYOUT GUIDELINES
SECTION OF THE DATASHEET)
SYMM
(0.83) TYP
ALTERNATE PCB
CUT OUT SHAPE
B
( 0.25)
METAL UNDER
SOLDER MASK SYMM
LAND PATTERN EXAMPLE
SCALE: 55X
4226006/A 06/2020
NOTES: (continued)
3. Final dimensions may vary due to manufacturing tolerance considerations and also routing constraints.
For more information, see Texas Instruments literature number SLUA271 (www.ti.com/lit/slua271).
www.ti.com

## Page 50

EXAMPLE STENCIL DESIGN
YMN0004A PicoStar T M - 0.226 mm max height
PicoStar
(0.315) PCB CUT OUT
TYP
1 2 (0.21)
TYP
A
ALTERNATE PCB
CUT OUT
SYMM
(0.83)
TYP
4X SOLDER MASK
OPENING
B
(R0.05)
SYMM 4X METAL UNDER
TYP
SOLDER MASK
SOLDER PASTE EXAMPLE
BASED on 0.075 mm THICK STENCIL
SCALE: 55X
4226006/A 06/2020
NOTES: (continued)
4. Laser cutting apertures with trapezoidal walls and rounded corners may offer better paste release.
www.ti.com

## Page 51

PACKAGE OUTLINE
DTS0008A FCSOT - 0.6 mm max height
FLIPCHIP SOT
1.3
B
1.1
A
PIN 1
INDEX AREA
1
8
6X (0.5)
7
2
2.2
2.0
3 6
5
4
0.27
8X
0.17
0.1 C A B 2.0
1.8
0.05 C
SEATING PLANE
0.18
0.08
C
0.6 MAX
0.45
8X 0.25 0.05
0.00
4 5
3 6
2 7
1 8
0.05 C
4226132/B 07/2021
NOTES:
1. All linear dimensions are in millimeters. Any dimensions in parenthesis are for reference only. Dimensioning and tolerancing
per ASME Y14.5M.
2. This drawing is subject to change without notice.
3. Body dimensions do not incude mold flash, protrusions or gate burrs.
Mold flash, interlead flash, protrusions or gate burrs shall not exceed 0.15 per end or side
www.ti.com

## Page 52

EXAMPLE BOARD LAYOUT
DTS0008A FCSOT - 0.6 mm max height
FLIPCHIP SOT
(1.72)
8X (0.72)
1
8X (0.3) 8
2 7
SYMM
3
6
6X (0.5)
5
4
(R0.05) TYP SYMM
LAND PATTERN EXAMPLE
EXPOSED METAL SHOWN
SCALE: 30X
SOLDER MASK METAL UNDER SOLDER MASK
OPENING METAL SOLDER MASK OPENING
NON SOLDER MASK SOLDER MASK
DEFINED DEFINED
(PREFERRED)
SOLDER MASK DETAILS
4226132/B 07/2021
NOTES: (continued)
4. Publication IPC-7351 may have alternate designs.
5. Solder mask tolerances between and around signal pads can vary based on board fabrication site.
www.ti.com

## Page 53

EXAMPLE STENCIL DESIGN
DTS0008A FCSOT - 0.6 mm max height
FLIPCHIP SOT
(1.72)
8X (0.72)
1
8X (0.3) 8
2 7
SYMM
3
6
6X (0.5)
5
4
SYMM
(R0.05) TYP
SOLDER PASTE EXAMPLE
BASED ON 0.125 mm THICK STENCIL
SCALE: 30X
4226132/B 07/2021
NOTES: (continued)
6. Laser cutting apertures with trapezoidal walls and rounded corners may offer better paste release. IPC-7525 may have alternate
design recommendations.
7. Board assembly site may have different recommendations for stencil design.
www.ti.com

## Page 54

IMPORTANT NOTICE AND DISCLAIMER
TI PROVIDES TECHNICAL AND RELIABILITY DATA (INCLUDING DATA SHEETS), DESIGN RESOURCES (INCLUDING REFERENCE
DESIGNS), APPLICATION OR OTHER DESIGN ADVICE, WEB TOOLS, SAFETY INFORMATION, AND OTHER RESOURCES "AS IS"
AND WITH ALL FAULTS, AND DISCLAIMS ALL WARRANTIES, EXPRESS AND IMPLIED, INCLUDING WITHOUT LIMITATION ANY
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT OF THIRD
PARTY INTELLECTUAL PROPERTY RIGHTS.
These resources are intended for skilled developers designing with TI products. You are solely responsible for (1) selecting the appropriate
TI products for your application, (2) designing, validating and testing your application, and (3) ensuring your application meets applicable
standards, and any other safety, security, regulatory or other requirements.
These resources are subject to change without notice. TI grants you permission to use these resources only for development of an
application that uses the TI products described in the resource. Other reproduction and display of these resources is prohibited. No license
is granted to any other TI intellectual property right or to any third party intellectual property right. TI disclaims responsibility for, and you
will fully indemnify TI and its representatives against, any claims, damages, costs, losses, and liabilities arising out of your use of these
resources.
TI's products are provided subject to TI's Terms of Sale or other applicable terms available either on ti.com or provided in conjunction with
such TI products. TI's provision of these resources does not expand or otherwise alter TI's applicable warranties or warranty disclaimers for
TI products.
TI objects to and rejects any additional or different terms you may have proposed. IMPORTANT NOTICE
Mailing Address: Texas Instruments, Post Office Box 655303, Dallas, Texas 75265
Copyright (C) 2023, Texas Instruments Incorporated
