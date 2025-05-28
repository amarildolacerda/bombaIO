#ifndef CONFIG_H
#define CONFIG_H
#include "Arduino.h"

namespace Config
{
#ifdef __AVR__
    constexpr char TERMINAL_NAME[] = RELAY_LORA_NAME;
#else
    constexpr char TERMINAL_NAME[] = "D1";
#endif

    constexpr bool PROMISCUOS = LORA_PROMISCUOS;
    constexpr int TERMINAL_ID = LORA_TERMINAL_ID;
    constexpr int LED_PIN = LED_BUILTIN;

    constexpr int BAND = 868.0; // Grove funciona melhor em 868
#ifndef VRELAY_PIN
    constexpr int RELAY_PIN = 4;
#else
    constexpr int RELAY_PIN = VRELAY_PIN;
#endif
#ifndef MIRX_PIN
    constexpr int RX_PIN = 6;
#else
    constexpr int RX_PIN = MIRX_PIN;
#endif
#ifndef MOTX_PIN
    constexpr int TX_PIN = 7;
#else
    constexpr int TX_PIN = MOTX_PIN;
#endif
    constexpr long LORA_SPEED = 9600;
    constexpr long SERIAL_SPEED = 115200;
    constexpr long STATUS_INTERVAL = 15000;
    constexpr int MESSAGE_MAX_LEN = 64;
};

struct MessageRec
{
    uint8_t to;
    String event;
    String value;
};
// Defina o tamanho máximo da lista
#define MAX_ITEMS 10

// Classe para gerenciar a lista FIFO de MessageRec
class FifoList
{
private:
    MessageRec items[MAX_ITEMS];
    int head = 0;
    int tail = 0;
    int count = 0;

    // Adiciona um item na lista
    bool pushItem(const MessageRec &item)
    {
        if (count >= MAX_ITEMS)
        {
            return false; // Lista cheia
        }

        items[tail] = item; // Armazena a cópia da mensagem
        tail = (tail + 1) % MAX_ITEMS;
        count++;
        return true;
    }

    // Remove e retorna o item no início da lista
    bool popItem(MessageRec &item)
    {
        if (count <= 0)
        {
            return false; // Lista vazia
        }

        item = items[head]; // Copia a mensagem para o parâmetro de saída
        head = (head + 1) % MAX_ITEMS;
        count--;
        return true;
    }

public:
    /**
     * Adiciona uma mensagem na lista
     * @param to Destinatário da mensagem
     * @param event Nome do evento
     * @param value Valor do evento
     */
    void push(uint8_t to, const String &event, const String &value)
    {
        MessageRec msg;
        msg.to = to;
        msg.event = event;
        msg.value = value;
        pushItem(msg);
    }

    /**
     * Remove e retorna uma mensagem da lista
     * @param msg Estrutura de mensagem que será preenchida
     * @return true se uma mensagem foi obtida, false se a lista estava vazia
     */
    bool pop(MessageRec &msg)
    {
        if (!popItem(msg))
        {
            return false;
        }
        return true;
    }

    bool isEmpty() const { return count == 0; }
    bool isFull() const { return count == MAX_ITEMS; }
    int size() const { return count; }
};
class SystemState
{
public:
    long previousMillis = 0;
    bool pinStateChanged : 1;
    bool lastPinState : 1;
    bool mustPresentation : 1;
    FifoList messages;
};

static SystemState systemState;

#endif