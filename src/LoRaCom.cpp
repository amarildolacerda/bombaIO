#include "LoRaCom.h"

#if defined(TTGO) || defined(HELTEC)
LoRa32 radio;
#elif RF95
LoRaRF95 radio;
#elif NRF24
RadioNRF24 radio;
#elif DUMMY
LoRaDummy radio;
#endif
