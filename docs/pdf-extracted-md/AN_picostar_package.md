# Advantages of Implementing a Light Sensor in TI's Ultra Thin PicoStar Package

- Source PDF: `docs/AN_picostar_package.pdf`
- Extraction date: 2026-05-09
- Page count: 3
- SHA256: `b94041e33a9e31043324ac16a160bc61d3a39addce3953e851d9b1d77b95762b`

## Page 1

www.ti.com
Application Brief
Advantages of Implementing a Light Sensor in TI's Ultra
Thin PicoStar Package
Rooshi Nagar
Introduction
0.84mm length by width, both providing flexibility
Texas Instruments introduced the first IC in a for small areas. This is extremely important with
wafer chip scale package, coined PicoStar, in 2009. light sensors due to the tight layout constraints
PicoStar has led to numerous different products being of modern day displays. Most modern monitors,
released to fit into space constrained environments phones, tablets, and laptops have extremely thin or
while maintaining the high level of performance nonexistent bezels,limiting where a light sensor can
customers are used to. With this proprietary be placed. For these applications, light sensors are
packaging technology being offered company-wide, quite important and the PicoStar package is able to fit
Texas Instruments released the first ambient light in between the thin gaps between adjacent displays.
sensor with a PicoStar option in 2016 with the
The height of the package is another important
OPT3006, followed with the release of OPT3007 and
dimension that must not be overlooked. The PicoStar
OPT4001. There are many reasons to incorporate a
package comes at a height of 0.22mm, making this
light sensor with these small dimensions into your
package the thinnest light sensor in the industry.
industrial, automotive, and commercial products. This
In addition to XY space on a board, the height
application brief discusses the advantages and use
is important due to volumetric space constraints
cases of using a TI light sensor in the PicoStar
near the display due to illumination boards for the
package.
LCD panel. Light sensors that come in the PicoStar
package require bottom facing implementation to be
Market Trend placed on a flex PCB, allowing for the device to be
In addition to the move towards sleeker displays placed closer to the display and fit into small spaces.
in the personal electronics and industrial markets,
the automotive industry has seen a spike in the Bottom Facing Assembly
number of displays incorporated across the vehicle,
Another advantage to highlight is a part of the unique
including the head unit, center information display,
assembly that this device requires. TI's PicoStar light
passenger display, and rear seat entertainment.
sensors have a bottom facing light sensitive area as
Utilizing an automotive grade light sensor in the
opposed to the traditional top facing. The sensor is
PicoStar package from TI allows for more flexibility
the only bottom facing light sensor in production as
when designing modern dashboards and infotainment
of publication. This feature allows the flex PCB to be
systems. In modern vehicles, there has been a trend
directly attached to the cover glass, maintaing high
of having multiple displays across the dash or one
base level performance. Additionally, this helps avoid
continuous display that stretches from the driver to
the extra cost of adding optical shields and shrouds,
the passenger. With displays such as these, having
reducing the overall system level cost.
a localized sensor is crucial for the user experience
making TI's automotive grade light sensors in the Figure 1 and Figure 2 illustrate how the PicoStar
PicoStar package an excellent choice for a design. package looks when placed in a system, showing
both the side view and the top view.
Small Size Figure 1 shows (not to scale) how TI's PicoStar
The most obvious advantage when utilizing a package light sensor (OPT) is placed in relation to the
PicoStar device is the small size in both the XY cover glass, display stack, back panel, and housing.
and Z dimensions. The YMF package (OPT3006, The sensor is connected to the display stack using a
OPT3007) measures 0.856mm x 0.946mm and flex PCB (FPCB) with a hole cut to expose the sensor
the YMN package (OPT4001) measures 1.05mm x
SBOA598 - JUNE 2024 Advantages of Implementing a Light Sensor in TI's Ultra Thin PicoStar 1
Submit Document Feedback Package
Copyright (C) 2024 Texas Instruments Incorporated

## Page 2

www.ti.com
DNP DTS YMN
area. The height of the flex PCB and the sensor
(PicoStar)
combine to approximately 0.426mm.
Size 2mm x 2mm x 2.1mm x 0.84mm x
0.65mm 1.9mm x 1.05mm x
0.6mm 0.226mm
Pin Count 6 8 4
Sensor Area Top Top Bottom
Figure 1. PicoStar Side View Resolution
400uL 800m 437.5 800m 312.5 800m
ux s uLux s uLux s
Figure 2 shows how the OPT sensor looks from the
top view when attached to the bottom of the flex PCB. 2.5mL 100m 3.5mL 100m 2.5mL 100m
There must be a cut out in the flex PCB so that light ux s ux s ux s
can fall directly on the sensor area through the cover
160m 1.8ms 224m 1.8ms 160m 1.8ms
glass. The hole in the flex PCB can be either circular,
Lux Lux Lux
rectangular (as shown), or cross shaped, depending
on your system needs.
Dynamic 107kLux 117kLux 83kLux
Range Saturation Saturation Saturation
Temperature -40C to 105C -40C to 105C -40C to 125C
Range
Conclusion
In summary, Texas Instruments PicoStar packaged
light sensors offer some advantages for space
constrained system designs across industrial,
automotive, and commercial applications. The
PicoStar package is currently offered for the
OPT3007, OPT4001, and OPT4001-Q1 with more
releases expected soon. TI offers more information
about our Light Sensors and the PicoStar package
in the TI Precision Labs video series. Data sheets,
Figure 2. PicoStar Top View application notes, and more about TI's light sensing
portfolio can be found on the light sensors home
page.
Improved Resolution
TI's OPT4001 in the PicoStar package offers better
Related Information
resolution than the SOT and QFN counterparts,
- OPT3007
with sensitivity as low as 312.5uLux compared to
- OPT4001
400uLux for the other packages. This low sensitivity
- OPT4001-Q1
enables the device to better measure the light in
- TI Precision Labs
the environment while being placed under darker
- Light Sensors
cover glass. In addition to the resolution, overall
performance of the device is better due to having up
to 10x lower power and 4 to 12 more bits of dynamic
range, optimized for stringent requirements. The table
below details the differences between TI's package
offerings for the OPT4001:
2 Advantages of Implementing a Light Sensor in TI's Ultra Thin PicoStar SBOA598 - JUNE 2024
Package Submit Document Feedback
Copyright (C) 2024 Texas Instruments Incorporated

## Page 3

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
Copyright (C) 2024, Texas Instruments Incorporated
