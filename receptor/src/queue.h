#ifndef QUEUE_H
#define QUEUE_H

#include "Arduino.h"

struct MessageRec
{
    uint8_t to;
    uint8_t from;
    char event[16];
    char value[48];
    void print()
    {

        Serial.print("[");
        Serial.print(from);
        Serial.print("-");

        Serial.print(to);
        Serial.print("] ");
        Serial.print(event);
        Serial.print("|");
        Serial.println(value);
    }
};
// Defina o tamanho máximo da lista
#define MAX_ITEMS 4

// Classe para gerenciar a lista FIFO de MessageRec
class FifoQueue
{
private:
    MessageRec items[MAX_ITEMS];
    int head = 0;
    int tail = 0;
    int count = 0;

public:
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

    /**
     * Adiciona uma mensagem na lista
     * @param to Destinatário da mensagem
     * @param event Nome do evento
     * @param value Valor do evento
     */
    void push(uint8_t to, String event, String value, uint8_t from)
    {
        MessageRec msg;
        msg.to = to;
        msg.from = from;
        int i = snprintf(msg.event, sizeof(msg.event) - 1, event.c_str());
        msg.event[i] = '\0';
        i = snprintf(msg.value, sizeof(msg.value) - 1, value.c_str());
        msg.value[i] = '\0';
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

#endif