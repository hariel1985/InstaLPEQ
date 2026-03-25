#pragma once

struct EQBand
{
    enum Type { Peak, LowShelf, HighShelf };

    float frequency = 1000.0f;  // 20 Hz - 20000 Hz
    float gainDb    = 0.0f;     // -24 dB to +24 dB
    float q         = 1.0f;     // 0.1 to 18.0
    Type  type      = Peak;
    bool  enabled   = true;
};
