
#ifdef SINRIC
#include "SinricCom.h"
#include "SinricPro.h"
#include "SinricProTemperaturesensor.h"
#include "Logger.h"

#define APP_KEY "5176f8b7-0e16-429f-8f22-73d4093f7c40"
#define APP_SECRET "66c53efa-f3ce-419d-abe5-ac3fdecf71d9-4a0a01d2-5bd0-4ae1-806e-7c7a02438d73"
#define TEMP_SENSOR_ID "682cb8f7947cbabd201f9689"

#ifdef __cplusplus
extern "C"
{
#endif
    uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

namespace SinricCom
{
    static long sinricTime = 0;

    void sinricUpdate()
    {
#ifdef SINRIC

        uint8_t temp_fahrenheit = temprature_sens_read();
        float temp_celsius = (temp_fahrenheit - 32) / 1.8;

        SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID]; // get temperaturesensor device
        bool success = mySensor.sendTemperatureEvent(temp_celsius);       // send event

        Logger::info(String("Temperatura: " + String(temp_celsius) + "c").c_str());
#endif
        sinricTime = millis();
    }

    void setup()
    {
#ifdef SINRIC
        SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];
        // setup SinricPro
        SinricPro.onConnected([]()
                              { Serial.printf("Connected to SinricPro\r\n"); });
        SinricPro.onDisconnected([]()
                                 { Serial.printf("Disconnected from SinricPro\r\n"); });

        SinricPro.begin(APP_KEY, APP_SECRET);
#endif
    }

    void loop()
    {
        if (millis() - sinricTime > 60000)
        {
            sinricUpdate();
        }
        SinricPro.handle();
    }

}
#endif