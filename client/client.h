#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"
#include <libwebsockets.h>

// Callback del Cliente Maneja los eventos principales del WebSocket
static inline int client_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    switch(reason) {
        // Notifica que se estableció la conexión.
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("Conexión establecida con el servidor WebSocket.\n");
            break;
        // Muestra el mensaje recibido
        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf("Mensaje recibido: %s\n", (char *)in);
            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            // aqui se puede enviar un mensaje de registro u otro mensaje inicial 
            break;
        // Informa que la conexión se cerro
        case LWS_CALLBACK_CLIENT_CLOSED:
            printf("Conexión cerrada.\n");
            break;
        default:
            break;
    }
    return 0;
}

// funcion para enviar mensajes al servidor serializa el mensaje y lo manda mediante lws_write.
static inline int client_send_message(struct lws *wsi, const ProtocolMessage *msg) {
    // Convierte el mensaje a formato JSON.
    char *json = serialize_message(msg);
    if (!json)
        return -1; // Retorna error si falla la serializacion
    int len = strlen(json);
    unsigned char buffer[LWS_PRE + MAX_MESSAGE_LENGTH];
    // Inicializa el buffer con ceros
    memset(buffer, 0, sizeof(buffer));
    // Copia el JSON en el buffer respetando el offset LWS_PRE.
    memcpy(&buffer[LWS_PRE], json, len);
    // Manda el mensaje al servidor
    int n = lws_write(wsi, &buffer[LWS_PRE], len, LWS_WRITE_TEXT);
    // Libera la memoria del JSON
    free(json);
    return n;
}

// Funcion para mostrar en pantalla los comandos disponibles al usuario
static inline void display_help(void) {
    printf("\nComandos disponibles:\n");
    printf("broadcast <mensaje>         - Enviar mensaje a todos los usuarios.\n");
    printf("private <usuario> <mensaje> - Enviar mensaje privado a un usuario.\n");
    printf("list_users                  - Solicitar listado de usuarios conectados.\n");
    printf("user_info <usuario>         - Solicitar información de un usuario.\n");
    printf("change_status <status>      - Cambiar estado (ACTIVO, OCUPADO, INACTIVO).\n");
    printf("disconnect / exit           - Cerrar la conexión y salir.\n");
    printf("help                        - Mostrar esta ayuda.\n\n");
}

// Procesa el comando ingresado y manda el mensaje correspondiente al servidor
static inline void process_user_input(const char *input, const char *username, struct lws *wsi) {
    // Verifica que los params no sean nulos de lo contrario no hace nada
    if (!input || !username || !wsi)
        return;
    
    ProtocolMessage msg;
    memset(&msg, 0, sizeof(msg));
    // Obtiene y guarda la hora actual en el mensaje
    get_current_timestamp(msg.timestamp, MAX_FIELD_LENGTH);
    // Asigna el nombre de usuario al campo sender
    strncpy(msg.sender, username, MAX_FIELD_LENGTH);
    
    // Si el comando empieza con broadcast manda un mensaje general a todos
    if (strncmp(input, "broadcast ", 10) == 0) {
        // Define el tipo de mensaje como broadcast
        strncpy(msg.type, MSG_TYPE_BROADCAST, MAX_FIELD_LENGTH);
        // Obtiene el mensaje a enviar
        const char *message_text = input + 10; 
        // Asigna el contenido del mensaje
        strncpy(msg.content, message_text, MAX_MESSAGE_LENGTH);
        // Manda el mensaje al servidor
        client_send_message(wsi, &msg);
    }
    // Si el comando empieza con private manda un mensaje privado a un usuario especifico
    else if (strncmp(input, "private ", 8) == 0) {
        // Formato private <destinatario> <mensaje>
        // Crea una copia del input para tokenizar
        char *input_copy = strdup(input);
        if (!input_copy)
            return;
        char *token = strtok(input_copy, " "); // private
        token = strtok(NULL, " "); // destinatario
        if (token == NULL) {
            free(input_copy);
            return;
        }
        // Define el tipo de mensaje como privado
        strncpy(msg.type, MSG_TYPE_PRIVATE, MAX_FIELD_LENGTH);
        // Guarda el destinatario en el campo target
        strncpy(msg.target, token, MAX_FIELD_LENGTH);
        char *message_text = strtok(NULL, ""); // Resto del mensaje
        if (message_text)
            strncpy(msg.content, message_text, MAX_MESSAGE_LENGTH); // Asigna el contenido del mensaje.
        client_send_message(wsi, &msg);// Manda el mensaje privado
        free(input_copy); // Libera la memoria asignada para la copia
    }
    // Si el comando es list_users solicita el listado de usuarios conectados
    else if (strcmp(input, "list_users") == 0) {
        // Solicitar listado de usuarios
        strncpy(msg.type, MSG_TYPE_LIST_USERS, MAX_FIELD_LENGTH);
        client_send_message(wsi, &msg);
    }
    // Si el comando empieza con user_info solicita info de un usuario especifico
    else if (strncmp(input, "user_info ", 10) == 0) {
        // Formato user_info <usuario>
        char *input_copy = strdup(input);
        if (!input_copy)
            return;
        char *token = strtok(input_copy, " "); // user_info
        token = strtok(NULL, " "); // usuario objetivo
        if (token == NULL) {
            free(input_copy);
            return;
        }
        // Define el tipo de mensaje como solicitud de info de usuario
        strncpy(msg.type, MSG_TYPE_USER_INFO, MAX_FIELD_LENGTH); 
        // Guarda el nombre del usuario en target
        strncpy(msg.target, token, MAX_FIELD_LENGTH);
        client_send_message(wsi, &msg); // Manda la solicitud al servidor
        free(input_copy);
    }
    // Si el comando empieza con change_status manda un mensaje para cambiar el estado
    else if (strncmp(input, "change_status ", 14) == 0) {
        // Formato change_status <nuevo_status>
        char *input_copy = strdup(input);
        if (!input_copy)
            return;
        char *token = strtok(input_copy, " "); // change_status
        token = strtok(NULL, " "); // nuevo_status
        if (token == NULL) {
            free(input_copy);
            return;
        }
        strncpy(msg.type, MSG_TYPE_CHANGE_STATUS, MAX_FIELD_LENGTH);  // Define el tipo de mensaje como cambio de estado
        strncpy(msg.content, token, MAX_MESSAGE_LENGTH); // Asigna el nuevo estado en el campo content
        client_send_message(wsi, &msg); // Manda el mensaje al servidor
        free(input_copy);
    }
    // Si el comando es disconnect o exit se manda un mensaje para cerrar la conexion
    else if (strcmp(input, "disconnect") == 0 || strcmp(input, "exit") == 0) {
        // Cierre de conexion
        strncpy(msg.type, MSG_TYPE_DISCONNECT, MAX_FIELD_LENGTH);
        strncpy(msg.content, "Cierre de sesión", MAX_MESSAGE_LENGTH);
        client_send_message(wsi, &msg);
    }
    // Si el comando es help muestra en pantalla la ayuda
    else if (strcmp(input, "help") == 0) {
        // Mostrar ayuda se llama funcion display_help
        display_help();
    }
    // Si el comando no es reconocido se imprime un mensaje de error
    else {
        printf("Comando no reconocido. Escriba 'help' para ver los comandos disponibles.\n");
    }
}

#endif 
