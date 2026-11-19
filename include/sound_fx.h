#pragma once

#include <cstdint>

namespace dmtk {

enum SoundId : uint8_t {
    SFX_TAP       = 0,
    SFX_DICE_ROLL = 1,
    SFX_NAT20     = 2,
    SFX_NAT1      = 3,
    SFX_COUNT     = 4
};

class SoundFx {
public:
    bool init();
    void shutdown();

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }
    bool isHardwareReady() const { return initialized_; }

    void play(SoundId id);

private:
    bool initialized_ = false;
    bool enabled_     = true;
};

} // namespace dmtk
