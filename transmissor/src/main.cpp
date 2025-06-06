// implementa transmissor ou reception com base em -DRECEPTION ou -DTRANSMISSOR

#ifdef TEST
#include <SoftwareSerial.h>
#include <RH_RF95.h>
#include "config.h"
static SoftwareSerial SSerial(Config::LORA_RX_PIN, Config::LORA_TX_PIN);
// #define LoRaSerial SSerial

static RH_RF95<SoftwareSerial> rf(SSerial);

void setup()
{
    Serial.begin(115200);
    Serial.println(F("Testando transmissor"));
    rf.init();
    rf.setPromiscuous(true);
    rf.setFrequency(868.0);
    rf.setHeaderTo(0x00);
    rf.setHeaderFrom(100);
    rf.setTxPower(14);
}

void loop()
{

    if (rf.available())
    {

        Serial.println("Recebendo mensagem");
        char buf[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);
        if (rf.recv((uint8_t *)buf, &len))
        {
            char info[128];
            snprintf(info, sizeof(info), "Received from: %d to %d id %d", rf.headerFrom(), rf.headerTo(), rf.headerId());
            Serial.println(info);
            Serial.println((char *)buf);
        }
    }
    else
    {
        Serial.println("Enviando mensagem");
        uint8_t msg[] = "Hello World!\0";
        int len = sizeof(msg);
        rf.send(msg, len);
        if (!rf.waitPacketSent())
        {
            Serial.println("Falha ao enviar mensagem");
        }
        else
        {
            Serial.println("Mensagem enviada com sucesso");
        }
        delay(3000);
    }
}

#else
#include "transmissor.h"
void setup()
{
    app.setup();
}
void loop()
{
    app.loop();
}

#endif