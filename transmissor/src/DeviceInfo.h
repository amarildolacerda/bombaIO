#pragma once

#include "Arduino.h"

class DeviceInfo
{
public:
    uint8_t running()
    {
        return 0;
    }
    uint8_t count()
    {
        return 0;
    }
};

static DeviceInfo deviceInfo;
