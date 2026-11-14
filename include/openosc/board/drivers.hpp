#pragma once
#include "openosc/board/current.hpp"
#include "openosc/generator/module_output.hpp"
#include "openosc/generator/output.hpp"
#include "openosc/measurement/dmm/no_serial_output.hpp"
#include "openosc/measurement/dmm/serial_output.hpp"
#include <type_traits>

namespace openosc::board {
// Concrete drivers are chosen at build time: there are exactly two known boards,
// and these types are used from time-critical service paths.
using GeneratorOutput = std::conditional_t<Current::Features.generator == GeneratorDriver::Dac, generator::Output, generator::ModuleOutput>;
using DmmSerialOutput =
    std::conditional_t<Current::Features.dmmSerialOutput, dmm::SerialOutput, dmm::NoSerialOutput>;
} // namespace openosc::board
