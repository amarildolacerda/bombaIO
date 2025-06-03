#pragma

#include "config.h"

/*
#ifdef __AVR__
#include <SoftwareSerial.h>
SoftwareSerial SSerial(Config::RX_PIN, Config::TX_PIN); // RX, TX
#define COMSerial SSerial
#define ShowSerial Serial

RH_RF95<SoftwareSerial> rf95(COMSerial);
#endif
*/

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350) || defined(ARDUINO_XIAO_RA4M1)
#include <SoftwareSerial.h>
SoftwareSerial SSerial(D7, D6); // RX, TX
#define COMSerial SSerial
#define ShowSerial Serial

RH_RF95<SoftwareSerial> rf95(COMSerial);
#endif

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define COMSerial Serial1
#define ShowSerial Serial

RH_RF95<HardwareSerial> rf95(COMSerial);
#endif

#ifdef SEEED_XIAO_M0
#define COMSerial Serial1
#define ShowSerial Serial

RH_RF95<Uart> rf95(COMSerial);
#elif defined(ARDUINO_SAMD_VARIANT_COMPLIANCE)
#define COMSerial Serial1
#define ShowSerial SerialUSB

RH_RF95<Uart> rf95(COMSerial);
#endif

#ifdef ARDUINO_ARCH_STM32F4
#define COMSerial Serial
#define ShowSerial SerialUSB

RH_RF95<HardwareSerial> rf95(COMSerial);
#endif

#if defined(NRF52840_XXAA)
#define COMSerial Serial1
#define ShowSerial Serial

RH_RF95<Uart> rf95(COMSerial);
#endif

#ifdef HELTEC
#include "LoRa32.h"
#else
#include <RH_RF95.h>
#include "LoRaRF95.h"
#endif
#include "AppProcess.h"

class App
{
public:
    void setup()
    {
        Serial.begin(Config::SERIAL_SPEED);
        Serial.println("Starting...");

        // Tentativa de inicialização com retry
        for (int i = 0; i < 3; i++)
        {
            if (lora.begin(Config::TERMINAL_ID, Config::LORA_BAND, false))
            {
                break;
            }
            delay(1000);
        }

        // Verificar se inicializou corretamente
        if (!lora.connected)
        {
            Serial.println("Failed to initialize LoRa!");
            while (1)
                ; // Trava se não inicializar
        }
    }

    void loop()
    {
        static unsigned long lastCheck = millis();

        // Processar LoRa
        lora.loop();
        AppProcess::handle();

        // Verificação periódica do sistema
        if (millis() - lastCheck > 5000)
        {
            lastCheck = millis();
            Serial.println("System OK");
            // Adicione outras verificações aqui
        }
    }
};
static App app;