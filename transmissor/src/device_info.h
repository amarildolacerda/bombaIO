#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include <Arduino.h>

#ifndef __AVR__
#include <map>
#include <utility> // For std::pair
#endif

#ifndef __AVR__
// Estrutura para armazenar informações do dispositivo
struct DeviceInfoData
{
    uint8_t tid;
    String event;
    String value;
    String name;
    String lastSeenISOTime;
    String status;
    int rssi;
    // Adicione outros campos conforme necessário
};
#endif
class DeviceInfo
{
public:
    static void updateDeviceList(uint8_t deviceId, DeviceInfoData data);
    static String getISOTime();

#ifndef __AVR__
    // Declaração correta de deviceList como membro estático
    static std::map<uint8_t, std::pair<String, DeviceInfoData>> deviceList;
#endif
};

#endif
