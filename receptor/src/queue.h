#ifndef QUEUE_H
#define QUEUE_H

#include "Arduino.h"
struct MessageRec
{
    uint8_t to;
    uint8_t from;
    String event;
    String value;
    uint8_t hope;
    uint8_t id;
};
// Defina o tamanho máximo da lista
#define MAX_ITEMS 5

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
    void push(const uint8_t to, const String &event, const String &value, const uint8_t from, const uint8_t hope = 3, const uint8_t id = 0)
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

#endif