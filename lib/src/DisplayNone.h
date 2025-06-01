#ifndef TTGO_DISPLAY_H
#define TTGO_DISPLAY_H

#ifdef __AVR__

class NoneDisplay : public DisplayInterface
{
private:
    void showFooter()
    {
    }

public:
    void initialize() override
    {
    }

    void clear() override
    {
    }

    void update() override
    {
    }

    void handle() override
    {
    }

    void setWifiStatus(bool connected) override
    {
    }

    void setLoraStatus(bool initialized) override
    {
    }

    void setRelayStatus(const String &status) override
    {
    }

    void showMessage(const String &message) override
    {
    }

    void showInfo(const String &info) override
    {
    }

    void setLoraMessage(uint8_t id, const String &event, const String &value) override
    {
    }

    void setVersion(const String &v) override
    {
    }

    void setDisplayCount(int count) override
    {
    }
};

extern TNoneDisplay displayManager;

#endif // TTGO
#endif // TTGO_DISPLAY_H