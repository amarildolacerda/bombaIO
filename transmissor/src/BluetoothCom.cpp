

#ifdef BT
#include "BluetoothSerial.h"
#include "device_info.h"
#include "BluetoothCom.h"

// ... outras inclusões ...

// Inicialização do Bluetooth
BluetoothCom bluetoothCom;
void BluetoothCom::setup()
{
    Serial.begin(115200);
    SerialBT.begin("LoRaGateway"); // Nome para o dispositivo Bluetooth
    Serial.println("O dispositivo está pronto para parear.");
}

// Função para enviar a lista de dispositivos via Bluetooth
void BluetoothCom::sendDeviceList()
{
    for (auto &device : DeviceInfo::deviceList)
    {
        // Envia cada dispositivo usando SerialBT
        SerialBT.println(device.lastSeenISOTime + " - " + device.uniqueName() + ":" + device.status);
    }
}

// Loop principal
void BluetoothCom::loop()
{
    // Check for incoming connection
    if (SerialBT.available())
    {
        String command = SerialBT.readStringUntil('\n');
        command.toLowerCase();
        if (command == "list")
        {
            sendDeviceList();
        }
        delay(1000); // Delay para não sobrecarregar a comunicação
    }
}

#endif