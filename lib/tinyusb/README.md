Subset of TinyUSB 0.20 (`src/` of Adafruit_TinyUSB_Arduino 3.7.7) for the USB DFU transport:
the device stack, CDC, MSC and the Synopsys DWC2 port that drives the nRF54LM20 USBHS block.

Taken from nRF54_Arduino `libraries/Adafruit_TinyUSB_Arduino`, which carries two changes to
`portable/synopsys/dwc2/dwc2_nrf.h` against upstream: the nRF54LM20A production define
(`NRF54LM20A_XXAA`, upstream only checks the engineering `NRF54LM20A_ENGA_XXAA`) and the nrfx 4
location of `nrfx_coredep.h`. Keep both copies in step.
