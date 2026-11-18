#pragma once

namespace openosc {
// The OTG1 controller (USB1_OTG_HS with its embedded full-speed PHY) as a peripheral, below USBD/PCD.
class UsbCore {
public:
    // Off: soft disconnect (DCTL.SDIS) and the core's interrupt mask, then its AHB1 reset
    // (GCCFG.PWRDWN = 0: the PHY and its D+ pull-up off), its AHB1 clock off and OTG_HS_IRQn
    // disabled and cleared in the NVIC. Registers only, so also for a core this image never set up
    // (Factory/Recovery's, at boot). HAL_PCD_MspInit clocks and resets it afresh for a start. The
    // USB kernel clock source and the VDD33USB detector are left as they are.
    static void powerOff();
};
} // namespace openosc
