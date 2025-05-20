
#ifndef SINRICCOM_H
#define SINRICCOM_H

#ifdef SINRIC

#include "fauxmoESP.h"

namespace SinricCom
{
    void setup();
    void loop();
    void sinricUpdate();

}

#endif
#endif