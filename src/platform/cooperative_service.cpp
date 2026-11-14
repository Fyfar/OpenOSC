#include "openosc/platform/cooperative_service.hpp"
#include "openosc/platform/board.hpp"

namespace openosc {
namespace {
CooperativeService::Callback callback=nullptr;
void *owner=nullptr;
uint32_t previous=0;
bool servicing=false;
}
void CooperativeService::install(Callback function,void *context) {
    callback=function; owner=context; previous=Board::milliseconds()-1;
}
void CooperativeService::remove(void *context) {
    if (owner==context) { callback=nullptr; owner=nullptr; }
}
void CooperativeService::poll(uint32_t now) {
    if (!callback || servicing || __get_IPSR() || __get_PRIMASK() || uint32_t(now-previous)<1) return;
    previous=now; servicing=true; callback(owner,now); servicing=false;
}
} // namespace openosc
