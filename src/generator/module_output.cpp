#include "openosc/generator/module_output.hpp"
#include "openosc/generator/capabilities.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/interrupts.hpp"

namespace openosc::generator {
namespace {
// What the USART3 interrupt shares with the foreground: the frame on the wire, its completion count
// (epoch: one per transmission complete) and the bytes received, each with the epoch it came in.
struct Received { uint8_t byte; uint32_t epoch; };
struct Wire {
    const uint8_t *volatile data = nullptr;
    volatile uint16_t index = 0, length = 0;
    volatile uint32_t epoch = 0;
    Received received[16]{};
    volatile uint8_t head = 0, tail = 0; // Written by the interrupt / by the foreground.
};
Wire wire;
constexpr uint32_t BaudRate = 9600;
constexpr unsigned Priority = 7; // Below the capture DMA (5) and USB (4): a byte has ~1 ms.
constexpr uint32_t Errors = USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE;
void interrupt() {
    const uint32_t control = USART3->CR1, status = USART3->ISR;
    if (status & USART_ISR_RXNE_RXFNE) {
        const uint8_t byte = uint8_t(USART3->RDR);
        const uint8_t next = uint8_t((wire.head + 1) % (sizeof wire.received / sizeof *wire.received));
        // A byte with a framing, noise or parity error (or after an overrun) is no acknowledge.
        if (!(status & Errors) && next != wire.tail) { wire.received[wire.head] = {byte, wire.epoch}; wire.head = next; }
    }
    if (status & Errors) USART3->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
    if ((control & USART_CR1_TXEIE_TXFNFIE) && (status & USART_ISR_TXE_TXFNF)) {
        if (wire.index < wire.length) USART3->TDR = wire.data[wire.index++];
        if (wire.index >= wire.length) { // The last byte is in: TC from before it means nothing.
            USART3->ICR = USART_ICR_TCCF;
            USART3->CR1 = (USART3->CR1 & ~USART_CR1_TXEIE_TXFNFIE) | USART_CR1_TCIE;
        }
    } else if ((control & USART_CR1_TCIE) && (USART3->ISR & USART_ISR_TC)) {
        // Armed in an earlier interrupt and read now: a TC still set from before the last TDR write
        // in this same interrupt cannot end the frame early.
        USART3->CR1 &= ~USART_CR1_TCIE; USART3->ICR = USART_ICR_TCCF; wire.epoch = wire.epoch + 1;
    }
}
} // namespace

bool ModuleOutput::start() {
    if (running_) return true;
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();
    // The kernel mux is shared with UART5 (DMM), which selects PCLK1 too.
    RCC_PeriphCLKInitTypeDef clock{};
    clock.PeriphClockSelection = RCC_PERIPHCLK_USART3;
    clock.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
    GPIO_InitTypeDef pins{};
    pins.Pin = GPIO_PIN_10 | GPIO_PIN_11; pins.Mode = GPIO_MODE_AF_PP; pins.Pull = GPIO_PULLUP;
    pins.Speed = GPIO_SPEED_FREQ_LOW; pins.Alternate = GPIO_AF7_USART3;
    if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK) { portFailed_ = true; return false; }
    HAL_GPIO_Init(GPIOC, &pins);
    USART3->CR1 = 0;
    USART3->CR2 = 0; USART3->CR3 = 0; USART3->PRESC = 0;
    USART3->BRR = (HAL_RCC_GetPCLK1Freq() + BaudRate / 2) / BaudRate;
    USART3->ICR = 0xffffffffU;
    wire.data = nullptr; wire.index = wire.length = 0; wire.tail = wire.head;
    if (!Interrupts::attach(USART3_IRQn, interrupt, Priority)) { portFailed_ = true; return false; }
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE_RXFNEIE | USART_CR1_UE;
    running_ = true; portFailed_ = false; onWire_ = false;
    return true;
}
void ModuleOutput::stop() {
    if (!running_) return;
    Interrupts::detach(USART3_IRQn);
    USART3->CR1 = 0;
    wire.data = nullptr; wire.length = 0; onWire_ = false;
    running_ = false;
}
bool ModuleOutput::submit(bool on) {
    ModuleWave::Frame frame;
    if (!ModuleWave::build(settings_, on, frame) || !start()) return false;
    if (on) link_.submit(frame); else link_.submitOff(frame);
    return true;
}
bool ModuleOutput::configure(const Settings &settings) {
    ModuleWave::Frame frame;
    if (!ModuleCapabilities::accepts(settings) || !ModuleWave::build(settings, true, frame)) return false;
    const auto previous = settings_;
    settings_ = settings;
    if (requested_ && !settings_.same(previous) && !submit(true)) { settings_ = previous; return false; }
    plan_ = {}; plan_.count = uint16_t(frame.timing.count); plan_.actualHz = frame.actualHz;
    return true;
}
bool ModuleOutput::enable() {
    if (requested_) return true;
    if (!submit(true)) return false;
    requested_ = true;
    return true;
}
void ModuleOutput::disable() {
    if (!requested_ && (applied_ == Applied::Off || link_.offQueued())) return;
    if (submit(false)) requested_ = false;
}
void ModuleOutput::service() {
    if (!running_) return;
    const uint32_t now = Board::milliseconds();
    // Completion first: an acknowledge already received in the same pass then counts.
    if (onWire_ && wire.epoch != sentEpoch_) { sentEpoch_ = wire.epoch; onWire_ = false; wire.length = 0; link_.sent(now); }
    while (wire.tail != wire.head) {
        const Received r = wire.received[wire.tail];
        wire.tail = uint8_t((wire.tail + 1) % (sizeof wire.received / sizeof *wire.received));
        if (r.epoch == sentEpoch_ && !onWire_ && link_.received(r.byte)) applied_ = link_.currentOff() ? Applied::Off : Applied::On;
    }
    if (link_.service(now) == ModuleLink::Failure::Stuck) { applied_ = Applied::Unknown; stop(); return; }
    if (link_.due() && !onWire_) {
        const auto &frame = link_.begin(now);
        applied_ = Applied::Unknown; // On its way: the module's state is not known until acknowledged.
        sentEpoch_ = wire.epoch; onWire_ = true;
        wire.data = frame.bytes; wire.index = 0; wire.length = SerialFrameCodec::Bytes;
        USART3->CR1 |= USART_CR1_TXEIE_TXFNFIE;
    }
}
bool ModuleOutput::suspendForClock() {
    if (clockBusy()) return false; // DataAccess refuses before this; a frame is never cut.
    link_.interrupt();
    stop();
    return true;
}
bool ModuleOutput::retime(bool) {
    if (!link_.busy() && applied_ != Applied::Unknown && !requested_ && !portFailed_) return true; // Off and idle: no UART needed.
    return start(); // The frame waiting goes on the new clock (service).
}
} // namespace openosc::generator
