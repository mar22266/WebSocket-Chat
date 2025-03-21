#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_FIELD_LENGTH    256 // Longitud max para campos de texto
#define MAX_MESSAGE_LENGTH 1024  // Longitud max para mensajes JSON

// definicion de constantes para identificar el tipo de mensaje en el protocolo
#define MSG_TYPE_REGISTER             "register"
#define MSG_TYPE_REGISTER_SUCCESS     "register_success"
#define MSG_TYPE_BROADCAST            "broadcast"
#define MSG_TYPE_PRIVATE              "private"
#define MSG_TYPE_LIST_USERS           "list_users"
#define MSG_TYPE_LIST_USERS_RESPONSE  "list_users_response"
#define MSG_TYPE_USER_INFO            "user_info"
#define MSG_TYPE_USER_INFO_RESPONSE   "user_info_response"
#define MSG_TYPE_CHANGE_STATUS        "change_status"
#define MSG_TYPE_STATUS_UPDATE        "status_update"
#define MSG_TYPE_DISCONNECT           "disconnect"
#define MSG_TYPE_USER_DISCONNECTED    "user_disconnected"
#define MSG_TYPE_ERROR                "error"

// definicion de constantes para los estados de usuario
#define STATUS_ACTIVE   "ACTIVO"
#define STATUS_BUSY     "OCUPADO"
#define STATUS_INACTIVE "INACTIVO"

/*
   Estructura que define un mensaje en el protocolo
   - type: Tipo del mensaje.
   - sender: el que manda del mensaje usuario o server
   - target: Destinatario, usado en mensajes privados
   - content: Contenido del mensaje 
   - timestamp: Fecha y hora en formato
   - userList: Lista de usuarios 
   - hasUserList: Bandera que indica si se incluye userList 1 si si, 0 si no
*/
typedef struct {
    char type[MAX_FIELD_LENGTH];
    char sender[MAX_FIELD_LENGTH];
    char target[MAX_FIELD_LENGTH];    // Opcional
    char content[MAX_MESSAGE_LENGTH]; // Puede ser una cadena arreglo u objeto en formato JSON
    char timestamp[MAX_FIELD_LENGTH];
    char userList[MAX_MESSAGE_LENGTH]; // Opcional se usa por ejemplo en register_success
    int  hasUserList;                  // Bandera 1 si se debe incluir userList 0 en caso contrario
} ProtocolMessage;

// Obtiene la fecha y hora actual en formato buffer
static inline void get_current_timestamp(char *buffer, size_t bufsize) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, bufsize, "%Y-%m-%dT%H:%M:%S", tm_info);
}

// Extrae el valor de la clave key de una cadena JSON soportando cadenas, arreglos, objetos y null
static inline int extract_json_value(const char *json, const char *key, char *value, size_t value_size) {
    if (!json || !key || !value) return -1; // Verifica que los punteros no sean nulos
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key); // Construye el patrón a buscar para la clave
    
    // Busca el patrón en el JSON
    char *start = strstr(json, pattern);
    if (!start) return -1;
    start += strlen(pattern);
    
    // Saltar espacios y tabulaciones
    while (*start == ' ' || *start == '\t') start++; 
    
    if (*start == '\"') {
        // Valor en cadena
        start++; // Salta la comilla de apertura
        char *end = strchr(start, '\"');
        if (!end) return -1;
        size_t len = end - start;
        if (len >= value_size) len = value_size - 1;
        strncpy(value, start, len);
        value[len] = '\0';
    } else if (*start == '[') {
        // Valor es un arreglo JSON
        char *end = strchr(start, ']');
        if (!end) return -1;
        size_t len = end - start + 1;
        if (len >= value_size) len = value_size - 1;
        strncpy(value, start, len);
        value[len] = '\0';
    } else if (*start == '{') {
        // Valor es un objeto JSON
        char *end = strchr(start, '}');
        if (!end) return -1;
        size_t len = end - start + 1;
        if (len >= value_size) len = value_size - 1;
        strncpy(value, start, len);
        value[len] = '\0';
    } else {
        // Valor literal null
        char *end = start;
        while (*end && *end != ',' && *end != '}') end++;
        size_t len = end - start;
        if (len >= value_size) len = value_size - 1;
        strncpy(value, start, len);
        value[len] = '\0';
    }
    
    return 0;
}

// Si el campo target no es vacio se incluye
// Si la bandera hasUserList esta activa se incluye el campo userList
// Para content Si comienza con { o [ se asume que ya es JSON de lo contrario se lo envuelve entre comillas
// Convierte una estructura ProtocolMessage a una cadena JSON y la retorna 
static inline char *serialize_message(const ProtocolMessage *msg) {
    if (!msg) return NULL;
    
    char content_field[MAX_MESSAGE_LENGTH];
    if (msg->content[0] == '{' || msg->content[0] == '[') {
        // Se asume que es objeto o arreglo JSON ya formateado
        snprintf(content_field, sizeof(content_field), "%s", msg->content);
    } else {
        // Se lo envuelve en comillas para que sea una cadena JSON
        snprintf(content_field, sizeof(content_field), "\"%s\"", msg->content);
    }
    
    char *json_str = (char *)malloc(MAX_MESSAGE_LENGTH);
    if (!json_str) return NULL; // Si falla la asignacion de memoria se retorna null
    
    if (strlen(msg->target) > 0 && msg->hasUserList) {
        // Se incluye target' y userList
        snprintf(json_str, MAX_MESSAGE_LENGTH,
                 "{\"type\": \"%s\", \"sender\": \"%s\", \"target\": \"%s\", \"content\": %s, \"userList\": %s, \"timestamp\": \"%s\"}",
                 msg->type, msg->sender, msg->target, content_field, msg->userList, msg->timestamp);
    } else if (strlen(msg->target) > 0) {
        // Se incluye target pero no userList
        snprintf(json_str, MAX_MESSAGE_LENGTH,
                 "{\"type\": \"%s\", \"sender\": \"%s\", \"target\": \"%s\", \"content\": %s, \"timestamp\": \"%s\"}",
                 msg->type, msg->sender, msg->target, content_field, msg->timestamp);
    } else if (msg->hasUserList) {
        // Se incluye userList pero no target
        snprintf(json_str, MAX_MESSAGE_LENGTH,
                 "{\"type\": \"%s\", \"sender\": \"%s\", \"content\": %s, \"userList\": %s, \"timestamp\": \"%s\"}",
                 msg->type, msg->sender, content_field, msg->userList, msg->timestamp);
    } else {
        // Sin target ni userList
        snprintf(json_str, MAX_MESSAGE_LENGTH,
                 "{\"type\": \"%s\", \"sender\": \"%s\", \"content\": %s, \"timestamp\": \"%s\"}",
                 msg->type, msg->sender, content_field, msg->timestamp);
    }
    
    return json_str;
}


// Se extraen los campos basicos type, sender, content y timestamp
// Parsea una cadena JSON y rellena ProtocolMessage retorna 0 si es exitoso o -1 en error
static inline int deserialize_message(const char *json_str, ProtocolMessage *msg) {
    if (!json_str || !msg) return -1; // Verifica que el JSON y el mensaje no sean nulos
    
     // Extrae el campo type si falla retorna error
    if (extract_json_value(json_str, "type", msg->type, MAX_FIELD_LENGTH) < 0)
        return -1;
    // Extrae el campo sender si falla retorna error
    if (extract_json_value(json_str, "sender", msg->sender, MAX_FIELD_LENGTH) < 0)
        return -1;
    // Extrae el campo content si falla se deja vacio
    if (extract_json_value(json_str, "content", msg->content, MAX_MESSAGE_LENGTH) < 0)
        msg->content[0] = '\0';
    // Extrae el campo timestamp si falla se deja vacio
    if (extract_json_value(json_str, "timestamp", msg->timestamp, MAX_FIELD_LENGTH) < 0)
        msg->timestamp[0] = '\0';
    
    // Extrae el campo target opcional si no se encuentra se deja vacio
    if (extract_json_value(json_str, "target", msg->target, MAX_FIELD_LENGTH) < 0)
        msg->target[0] = '\0';
    
    // Extrae el campo userList opcional establece la bandera hasUserList segun si se encontro
    if (extract_json_value(json_str, "userList", msg->userList, MAX_MESSAGE_LENGTH) < 0) {
        msg->userList[0] = '\0';
        msg->hasUserList = 0;
    } else {
        msg->hasUserList = 1;
    }
    
    return 0;
}

#endif 
