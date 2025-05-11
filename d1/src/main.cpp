

#include <SoftwareSerial.h>
#include <Arduino.h>
#include <RH_RF95.h>
// #include <EspSoftwareSerial.h>


RH_RF95 rf95(Serial);

void setup()
{
    Serial.swap();
    if (rf95.init())
    {
        Serial.println("LoRa radio init OK!");
        rf95.setFrequency(868.0);
        rf95.setTxPower(13, false);
        // rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
        rf95.setPreambleLength(8);
        // rf95.setSyncWord(0x12);
        rf95.setHeaderTo(0xFF);
    }
    else
    {
        Serial.println("LoRa radio init failed");
        while (1)
            ;
    }
    Serial.println("LoRa radio init OK!");
}

void loop()
{
    static unsigned long lastMillis = 0;
    if (millis() - lastMillis < 1000)
    {
        return;
    }
    lastMillis = millis();

    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN] = "{\"event\":\"status\",\"value\":\"on\"}";
    uint8_t len = sizeof(buf);
    // rf95.printBuffer("Hello, LoRa!", buf, len);
    rf95.setHeaderTo(0x00);
    rf95.setHeaderFrom(101);
    rf95.send(buf, sizeof(buf));
    rf95.waitPacketSent();
    Serial.println("Message sent!");
    delay(1000);
    if (rf95
            .available())
    {
        uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);
        if (rf95.recv(buf, &len))
        {
            // Serial.print("Received: ");
            // Serial.println((char *)buf);
            digitalWrite(LED_BUILTIN, HIGH); // Turn on LED
        }
    }
    else
    {
        digitalWrite(LED_BUILTIN, LOW); // Turn off LED
    }
    delay(1000);
}