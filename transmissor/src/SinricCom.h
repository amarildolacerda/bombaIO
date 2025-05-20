
#ifndef SINRICCOM_H
#define SINRICCOM_H

#ifdef SINRIC

#include "fauxmoESP.h"

class SinricCom
{
private:
    long sinricTime = 0;

public:
    void setup();
    void loop();
    void update();
};

extern SinricCom sinricCom;

#endif
#endif