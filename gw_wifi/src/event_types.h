#ifndef EVENT_TYPES_H
#define EVENT_TYPES_H

// Tipos de mensagens
#define MSG_TYPE_CMD 0x01 // Comando
#define MSG_TYPE_EVT 0x02 // Evento
#define MSG_TYPE_ACT 0x03 // Atuador
#define MSG_TYPE_MGT 0x04 // Gerenciamento
#define MSG_TYPE_ERR 0x05 // Erro

// Comandos (CMD_)
#define CMD_IDENTIFY "CMD_IDENT"
#define CMD_IDENTIFY_ACK "CMD_IDENT_ACK"
#define CMD_PING "CMD_PING"
#define CMD_PONG "CMD_PONG"
#define CMD_REBOOT "CMD_REBOOT"
#define CMD_CONFIG_GET "CMD_CFG_GET"
#define CMD_CONFIG_SET "CMD_CFG_SET"

// Eventos (EVT_)
#define EVT_TEMPERATURE "EVT_TEMP"
#define EVT_HUMIDITY "EVT_HUMID"
#define EVT_PRESSURE "EVT_PRESS"
#define EVT_STATUS "EVT_STATUS"

// Atuadores (ACT_)
#define ACT_RELAY "ACT_RELAY"
#define ACT_LED "ACT_LED"
#define ACT_BUZZER "ACT_BUZZ"

// Gerenciamento (MGT_)
#define MGT_DISCOVER "MGT_DISC"
#define MGT_DISCOVER_RESP "MGT_DISC_RESP"
#define MGT_TOPOLOGY "MGT_TOPOLOGY"

// Erros (ERR_)
#define ERR_GENERIC "ERR_GEN"
#define ERR_SENSOR "ERR_SENS"

// Macros para identificação
#define IS_CMD(evt) (strncmp(evt, "CMD_", 4) == 0)
#define IS_EVT(evt) (strncmp(evt, "EVT_", 4) == 0)
#define IS_ACT(evt) (strncmp(evt, "ACT_", 4) == 0)
#define IS_MGT(evt) (strncmp(evt, "MGT_", 4) == 0)
#define IS_ERR(evt) (strncmp(evt, "ERR_", 4) == 0)

// Estrutura para metadados de eventos
struct EventMeta
{
    const char *name;
    uint8_t type;
    uint8_t priority;
};

// Tabela de eventos
static const EventMeta EVENT_TABLE[] = {
    {CMD_IDENTIFY, MSG_TYPE_CMD, 1},
    {CMD_PING, MSG_TYPE_CMD, 1},
    {EVT_TEMPERATURE, MSG_TYPE_EVT, 2},
    // ... adicione todos os eventos
};

#endif