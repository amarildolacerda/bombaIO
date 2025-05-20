
#ifndef BLUETOOTHCOM_H
#define BLUETOOTHCOM_H

#ifdef BT
// ... outras inclusões ...

class BluetoothCom
{
    private:
        BluetoothSerial SerialBT;

    public:
    void loop();
    void setup();
};



extern BluetoothCom bluetoothCom;
#endif
#endif