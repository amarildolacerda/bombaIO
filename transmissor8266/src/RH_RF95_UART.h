#ifndef RH_RF95_UART_H
#define RH_RF95_UART_H

#include <RHGenericDriver.h>
#include <SoftwareSerial.h>

template <typename SerialT>
class RH_RF95_UART : public RHGenericDriver
{
public:
    RH_RF95_UART(SerialT &serial)
        : _serial(serial), _timeout(200), _lastRssi(0), _lastSnr(0) {}

    bool init()
    {
        _serial.begin(9600);
        delay(100);
        return true;
    }

    bool available() override
    {
        return _serial.available();
    }

    // Implementação dos métodos virtuais puros de RHGenericDriver
    bool recv(uint8_t *buf, uint8_t *len) override
    {
        // Implementação simplificada para exemplo
        if (!available())
            return false;

        *len = read(buf, *len);
        return true;
    }

    bool send(const uint8_t *data, uint8_t len) override
    {
        return write(data, len) == len;
    }

    uint8_t maxMessageLength() override
    {
        return 255; // Valor arbitrário para o comprimento máximo da mensagem
    }

    // Removendo o specifier override de métodos que não sobrescrevem
    bool waitAvailable()
    {
        unsigned long start = millis();
        while (!_serial.available())
        {
            if (millis() - start > _timeout)
                return false;
            yield();
        }
        return true;
    }

    size_t write(const uint8_t *data, size_t len)
    {
        return _serial.write(data, len);
    }

    size_t read(uint8_t *buf, size_t len)
    {
        size_t i = 0;
        while (i < len && _serial.available())
        {
            buf[i++] = _serial.read();
        }
        return i;
    }

    // Métodos específicos do RF95
    bool setFrequency(float centre)
    {
        // Implementação simplificada
        _frequency = centre;
        return true;
    }

    bool setTxPower(int8_t power, bool useRFO = false)
    {
        _txPower = power;
        return true;
    }

    int16_t lastRssi() { return _lastRssi; }
    float lastSnr() { return _lastSnr; }

    // Controle de cabeçalho
    void setHeaderTo(uint8_t to) { _headerTo = to; }
    void setHeaderFrom(uint8_t from) { _headerFrom = from; }
    void setHeaderId(uint8_t id) { _headerId = id; }
    void setHeaderFlags(uint8_t flags) { _headerFlags = flags; }
    uint8_t headerTo() { return _headerTo; }
    uint8_t headerFrom() { return _headerFrom; }
    uint8_t headerId() { return _headerId; }
    uint8_t headerFlags() { return _headerFlags; }

protected:
    SerialT &_serial;
    unsigned long _timeout;
    int16_t _lastRssi;
    float _lastSnr;
    float _frequency;
    int8_t _txPower;
    uint8_t _headerTo, _headerFrom, _headerId, _headerFlags;
};

#endif