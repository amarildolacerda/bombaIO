#include "device_info.h"
#include <ArduinoJson.h>

// Define o membro estático corretamente
std::vector<DeviceInfoData> DeviceInfo::deviceList;
std::vector<DeviceRegData> DeviceInfo::deviceRegList;
std::vector<DeviceInfoHistory> DeviceInfo::history;

void DeviceInfo::updateDeviceList(uint8_t deviceId, DeviceInfoData data)
{

    if ((deviceId == 0) || (deviceId == 0xFF))
        return;
    bool found = false;
    for (auto &device : deviceList)
    {
        if (device.tid == deviceId)
        {
            found = true;
            // atualiza os dados
            device.event = data.event;
            device.value = data.value;
            device.rssi = data.rssi;
            device.lastSeen = millis(); // data.lastSeenISOTime;
            // if (data.event == "status")
            // {
            // data.value.toUpperCase();
            // device.status = data.value;
            // }
            break;
        }
    }

    // Se não encontrou, adiciona novo dispositivo
    if (!found)
    {
        data.tid = deviceId; // Ensure the ID is set
        // if (data.event == "status")
        //{
        // data.value.toUpperCase();
        // data.status = data.value;
        //}
        deviceList.push_back(data);
    }

    while (history.size() >= 4)
        history.erase(history.end());

    DeviceInfoHistory hist;
    hist.tid = data.tid;
    hist.name = DeviceInfo::findName(data.tid);
    hist.value = data.value;
    history.insert(history.begin(), hist);
}

void DeviceInfo::updateRegList(u_int8_t tid, DeviceRegData data)
{
    bool found = false;
    for (auto &reg : deviceRegList)
    {
        if (reg.tid == tid)
        {
            found = true;
            reg.name = data.name;
            break;
        }
    }

    if (!found)
    {
        data.tid = tid; // Ensure the ID is set
        deviceRegList.push_back(data);
    }
}

int DeviceInfo::getTimeDifferenceSeconds(const unsigned long &lastSeenISOTime)
{
    long dif = millis() - lastSeenISOTime;
    return dif / 1000;
}

// Função que retorna a diferença em segundos entre o tempo atual e o lastSeenISOTime
// Retorna -1 se o formato for inválido ou se lastSeenISOTime estiver vazio
/*int DeviceInfo::getTimeDifferenceSeconds(const String &lastSeenISOTime)
{
    // Se o tempo estiver vazio ou for o valor padrão, retorna -1 (offline)
    if (lastSeenISOTime.length() == 0 || lastSeenISOTime == "1970-01-01T00:00:00Z")
    {
        return -1;
    }

    // Pega o tempo atual em formato ISO
    String currentTimeISO = DeviceInfo::getISOTime();

    // Extrai os componentes de data/hora para lastSeenISOTime
    int lastYear, lastMonth, lastDay, lastHour, lastMin, lastSec;
    if (sscanf(lastSeenISOTime.c_str(), "%d-%d-%dT%d:%d:%dZ",
               &lastYear, &lastMonth, &lastDay, &lastHour, &lastMin, &lastSec) != 6)
    {
        return -1; // Formato inválido
    }

    // Extrai os componentes de data/hora para o tempo atual
    int currYear, currMonth, currDay, currHour, currMin, currSec;
    if (sscanf(currentTimeISO.c_str(), "%d-%d-%dT%d:%d:%dZ",
               &currYear, &currMonth, &currDay, &currHour, &currMin, &currSec) != 6)
    {
        return -1; // Formato inválido
    }

    // Converte para segundos desde epoch (simplificado)
    // Nota: Esta é uma aproximação que funciona para pequenas diferenças de tempo
    long lastTotalSec = lastSec + lastMin * 60 + lastHour * 3600 + lastDay * 86400;
    long currTotalSec = currSec + currMin * 60 + currHour * 3600 + currDay * 86400;

    // Calcula a diferença
    return (currTotalSec - lastTotalSec) +
           (currYear - lastYear) * 31536000 + // ~365 dias
           (currMonth - lastMonth) * 2592000; // ~30 dias
}
           */

String DeviceInfo::getISOTime()
{
#ifdef __AVR__
    return "1970-01-01T00:00:00Z"; // Fallback for AVR
#else

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        return "1970-01-01T00:00:00Z";
    }

    char timeStr[25];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(timeStr);
#endif
}
