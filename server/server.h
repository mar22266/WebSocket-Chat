#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "protocol.h"
#include <libwebsockets.h>

// Estructura que representa un cliente conectado incluyendo su nombre, IP, estado, conexion WebSocket
typedef struct Client {
    char username[MAX_FIELD_LENGTH]; // Nombre del usuario
    char ip[MAX_FIELD_LENGTH]; // Dirección IP del cliente
    char status[MAX_FIELD_LENGTH]; // Por ejemplo ACTIVO OCUPADO INACTIVO
    struct lws *wsi;             // Puntero a la conexión WebSocket 
    struct Client *next;  // Puntero al siguiente cliente en la lista
} Client;

// Lista global de clientes y mutex para sincronización.
static Client *client_list = NULL; // Lista enlazada de clientes conectados
static pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex para proteger la lista de clientes

// Agrega un nuevo cliente a la lista. Retorna 0 si se agrego exitosamente
// o -1 si ya existe un cliente con el mismo nombre o con la misma IP
static inline int add_client(Client *new_client) {
    int ret = 0;
    pthread_mutex_lock(&client_list_mutex); // Bloquea el mutex para acceso exclusivo a la lista
    Client *curr = client_list; // Inicia el recorrido en la cabeza de la lista
    while (curr != NULL) { // Recorre cada cliente en la lista
        // Verifica si el nombre de usuario ya existe o si la dirección IP ya esta registrada
        if (strcmp(curr->username, new_client->username) == 0 || 
            strcmp(curr->ip, new_client->ip) == 0) {
            ret = -1; // Si se encuentra duplicado nombre o IP se marca error
            break;
        }
        curr = curr->next; // Avanza al siguiente cliente
    }
    if (ret == 0) { // Si no se encontró duplicado, se añade el nuevo cliente
        new_client->next = client_list; // Inserta el nuevo cliente al inicio de la lista
        client_list = new_client; // Actualiza la cabeza de la lista.
    }
    pthread_mutex_unlock(&client_list_mutex); // Libera el mutex.
    return ret; // Retorna 0 en éxito o -1 si se detectó duplicado.
}

// Elimina el cliente identificado por username de la lista retorna 0 si se elimino o -1 si no se encontro
static inline int remove_client(const char *username) {
    int ret = -1; // Inicializa el resultado en -1 no encontrado
    pthread_mutex_lock(&client_list_mutex); // Bloquea el mutex para acceso seguro a la lista
    Client *curr = client_list; // Inicia el recorrido en la cabeza de la lista
    Client *prev = NULL; // Puntero para el cliente anterior, inicialmente nulo
    while (curr != NULL) {
        if (strcmp(curr->username, username) == 0) {
            if (prev == NULL)
                client_list = curr->next; // Si es el primer cliente, actualiza la cabeza de la lista
            else
                prev->next = curr->next; // Si no, enlaza el cliente anterior con el siguiente
            free(curr); // Libera la memoria del cliente eliminado
            ret = 0; // establece el resultado en 0 encontrado y eliminado
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    // libera el mutex
    pthread_mutex_unlock(&client_list_mutex);
    return ret;
}

// Busca y retorna un puntero al cliente cuyo username coincide
static inline Client* find_client(const char *username) {
    Client *found = NULL; // Inicializa el resultado en null
    pthread_mutex_lock(&client_list_mutex); // bloquea el mutex
    Client *curr = client_list; // Inicia el recorrido en la cabeza de la lista
    while (curr != NULL) {
        if (strcmp(curr->username, username) == 0) {
            found = curr; // guarda el puntero del cliente
            break;
        }
        curr = curr->next;
    }
    // libera el mutex
    pthread_mutex_unlock(&client_list_mutex);
    return found;
}

// Manda un mensaje a la conexión WebSocket especificada serializa el mensaje a JSON y lo envia
static inline int send_message(struct lws *wsi, const ProtocolMessage *msg) {
    char *json = serialize_message(msg); // Convierte el mensaje a una cadena JSON
    if (!json) return -1; // -1 si falla la serializacion
    int len = strlen(json); // Calcula la longitud del JSON
    // lws_write requiere que el buffer de datos comience en un offset LWS_PRE,por lo que se utiliza un buffer temporal que incluye ese espacio
    // Reserva un buffer con el espacio requerido por LWS_PRE
    unsigned char buffer[LWS_PRE + MAX_MESSAGE_LENGTH];
    // Inicializa el buffer con ceros
    memset(buffer, 0, sizeof(buffer));
    // Copia el JSON en el buffer, respetando el offset LWS_PRE
    memcpy(&buffer[LWS_PRE], json, len);
    // Manda el mensaje a través del WebSocket
    int n = lws_write(wsi, &buffer[LWS_PRE], len, LWS_WRITE_TEXT);
    free(json); // libera la memoria del JSON
    return n; // retorna el resultado de lws_write
}

// Difunde un mensaje a todos los clientes conectados
static inline void broadcast_message(const ProtocolMessage *msg) {
    pthread_mutex_lock(&client_list_mutex); // Bloquea el mutex
    Client *curr = client_list;
    while (curr != NULL) {
        send_message(curr->wsi, msg); // Manda el mensaje al cliente actual
        curr = curr->next; // Avanza al siguiente cliente
    }
    pthread_mutex_unlock(&client_list_mutex); // libera el Mutex
}

// Manda un mensaje privado a un cliente especifico identificado por dest_username.
static inline int send_private_message(const ProtocolMessage *msg, const char *dest_username) {
    int ret = -1; // Inicializa el resultado en -1 no encontrado
    pthread_mutex_lock(&client_list_mutex); // bloquea el mutex
    Client *curr = client_list;
    while (curr != NULL) {
        // Si encuentra el cliente con el username especificado
        if (strcmp(curr->username, dest_username) == 0) { 
            // manda el mensaje y guarda el resultado
            ret = send_message(curr->wsi, msg);
            break;
        }
        curr = curr->next; // Avanza al siguiente cliente
    }
    // Libera el mutex
    pthread_mutex_unlock(&client_list_mutex);
    // Retorna el resultado de mandar el mensaje o -1 si no se encontro el cliente
    return ret;
}

// Manda un mensaje de registro exitoso al cliente que se acaba de registrar se construye un arreglo JSON con el listado de usuarios conectados
static inline void send_register_success(struct lws *wsi, const char *message) {
    ProtocolMessage msg;
    memset(&msg, 0, sizeof(msg)); // Inicializa el mensaje a cero
    strncpy(msg.type, MSG_TYPE_REGISTER_SUCCESS, MAX_FIELD_LENGTH);  // Define el tipo como register_success
    strncpy(msg.sender, "server", MAX_FIELD_LENGTH); // Establece el remitente como server
    strncpy(msg.content, message, MAX_MESSAGE_LENGTH);  // Asigna el mensaje de exito
    get_current_timestamp(msg.timestamp, MAX_FIELD_LENGTH); // Guarda la hora actual en el mensaje
    
    pthread_mutex_lock(&client_list_mutex); // Bloquea con el mutex
    char users_json[MAX_MESSAGE_LENGTH] = "["; // Inicia el arreglo JSON para la lista de usuarios
    Client *curr = client_list; // Comienza a recorrer la lista de clientes
    while (curr != NULL) {
        strcat(users_json, "\""); // Agrega comillas al inicio
        strcat(users_json, curr->username); // Agrega el nombre del usuario
        strcat(users_json, "\""); // Agrega comillas al final
        if (curr->next != NULL)
            strcat(users_json, ","); // Separa con coma al siguiente usuario
        curr = curr->next; // Pasa al siguiente usuario
    }
    strcat(users_json, "]"); // Cierra el arreglo JSON
    pthread_mutex_unlock(&client_list_mutex); // Libera el mutex
    
    strncpy(msg.userList, users_json, MAX_MESSAGE_LENGTH); // // Copia la lista de usuarios al mensaje
    msg.hasUserList = 1; // Activa la bandera para incluir userList
    send_message(wsi, &msg); // Manda el mensaje al cliente
}


// Manda al cliente solicitante el listado de usuarios conectados
static inline void send_list_users(struct lws *wsi) {
    ProtocolMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.type, MSG_TYPE_LIST_USERS_RESPONSE, MAX_FIELD_LENGTH); // Define el tipo como list_users_response
    strncpy(msg.sender, "server", MAX_FIELD_LENGTH); // Remitente es server
    get_current_timestamp(msg.timestamp, MAX_FIELD_LENGTH); // Establece la hora actual
    
    pthread_mutex_lock(&client_list_mutex); // bloquea el mutex
    // Mismo proceso que en la funcion send_register_success
    char users_json[MAX_MESSAGE_LENGTH] = "[";
    Client *curr = client_list;
    while (curr != NULL) {
        strcat(users_json, "\"");
        strcat(users_json, curr->username);
        strcat(users_json, "\"");
        if (curr->next != NULL)
            strcat(users_json, ",");
        curr = curr->next;
    }
    strcat(users_json, "]"); // Cierra el arreglo JSON
    pthread_mutex_unlock(&client_list_mutex); // libera el mutex
    
    strncpy(msg.content, users_json, MAX_MESSAGE_LENGTH); // Asigna la lista de usuarios al contenido
    send_message(wsi, &msg); // Manda el mensaje al cliente
}


// Manada al solicitante la información IP y status de un usuario especifico
static inline void send_user_info(struct lws *wsi, const char *target_username) {
    ProtocolMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.type, MSG_TYPE_USER_INFO_RESPONSE, MAX_FIELD_LENGTH); // Define el tipo como user_info_response
    strncpy(msg.sender, "server", MAX_FIELD_LENGTH); // Remitente es server
    strncpy(msg.target, target_username, MAX_FIELD_LENGTH); // Establece el usuario objetivo
    get_current_timestamp(msg.timestamp, MAX_FIELD_LENGTH); // Guarda la hora actual
    
    Client *client = find_client(target_username);  // Busca el cliente con el username indicado
    if (client) {
        char info_json[MAX_MESSAGE_LENGTH];
        snprintf(info_json, MAX_MESSAGE_LENGTH, "{\"ip\": \"%s\", \"status\": \"%s\"}", client->ip, client->status); // Formatea la info en JSON
        strncpy(msg.content, info_json, MAX_MESSAGE_LENGTH); // Asigna la info al contenido
    } else {
        strncpy(msg.content, "null", MAX_MESSAGE_LENGTH);
    }
    send_message(wsi, &msg); // Manda el mensaje al solicitante
}

// Actualiza el status de un cliente y difunde la actualizacion
static inline void update_client_status(const char *username, const char *new_status) {
    pthread_mutex_lock(&client_list_mutex);
    // Buscar el cliente en la lista por nombre y actualizar su status
    Client *curr = client_list;
    while (curr != NULL) {
        if (strcmp(curr->username, username) == 0) {
            strncpy(curr->status, new_status, MAX_FIELD_LENGTH); // actualiza su estado
            break;
        }
        curr = curr->next; // Avanza al siguiente cliente
    }
    pthread_mutex_unlock(&client_list_mutex); // libera el mutex

    // Difundir la actualización del status a todos los clientes
    ProtocolMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.type, MSG_TYPE_STATUS_UPDATE, MAX_FIELD_LENGTH); // Define el tipo como status_update
    strncpy(msg.sender, "server", MAX_FIELD_LENGTH); // El remitente es server
    char status_json[MAX_MESSAGE_LENGTH];
    snprintf(status_json, MAX_MESSAGE_LENGTH, "{\"user\": \"%s\", \"status\": \"%s\"}", username, new_status);  // Formatea la actualización en JSON
    strncpy(msg.content, status_json, MAX_MESSAGE_LENGTH); // Asigna la actualizacion al contenido
    get_current_timestamp(msg.timestamp, MAX_FIELD_LENGTH);
    broadcast_message(&msg); // Difunde la actualizacion a todos los clientes
}

// wsi: Puntero a la conexión WebSocket del cliente.
// json_str: Cadena JSON recibida.
// Procesa un mensaje JSON recibido desde un cliente y ejecuta la accion correspondiente
static inline void handle_incoming_message(struct lws *wsi, const char *json_str) {
    ProtocolMessage msg;
    if (deserialize_message(json_str, &msg) != 0) {
        // Si falla el parseo, enviar mensaje de error.
        ProtocolMessage error_msg;
        memset(&error_msg, 0, sizeof(error_msg));
        strncpy(error_msg.type, MSG_TYPE_ERROR, MAX_FIELD_LENGTH);
        strncpy(error_msg.sender, "server", MAX_FIELD_LENGTH); // El remitente es el servidor
        strncpy(error_msg.content, "Error al parsear el mensaje.", MAX_MESSAGE_LENGTH); // Mensaje de error
        get_current_timestamp(error_msg.timestamp, MAX_FIELD_LENGTH);
        send_message(wsi, &error_msg); // Manda el mensaje de error al cliente
        return;
    }
    
    if (strcmp(msg.type, MSG_TYPE_REGISTER) == 0) {
        // Registro de usuario sender contiene el nombre del usuario
        Client *new_client = (Client *)malloc(sizeof(Client));
        if (!new_client) return;
        memset(new_client, 0, sizeof(Client));
        strncpy(new_client->username, msg.sender, MAX_FIELD_LENGTH); // Copia el nombre de usuario
        
        // Obtener la IP real del cliente esto también es util para entornos remotos
        char client_ip[128] = {0};
        lws_get_peer_simple(wsi, client_ip, sizeof(client_ip)); // Extrae la IP del cliente
        strncpy(new_client->ip, client_ip, MAX_FIELD_LENGTH); // Copia la IP en la estructura
        
        strncpy(new_client->status, STATUS_ACTIVE, MAX_FIELD_LENGTH); // Establece el estado a ACTIVO
        new_client->wsi = wsi;
        if (add_client(new_client) == 0) {
            // Si el registro es exitoso manda un mensaje de registro exitoso
            send_register_success(wsi, "Registro exitoso");
        } else {
            // Si ya existe el usuario, envía un mensaje de error y cierra la conexion
            ProtocolMessage error_msg;
            memset(&error_msg, 0, sizeof(error_msg));
            strncpy(error_msg.type, MSG_TYPE_ERROR, MAX_FIELD_LENGTH);
            strncpy(error_msg.sender, "server", MAX_FIELD_LENGTH);
            strncpy(error_msg.content, "Nombre de usuario ya existe.", MAX_MESSAGE_LENGTH);
            get_current_timestamp(error_msg.timestamp, MAX_FIELD_LENGTH);
            send_message(wsi, &error_msg);
            free(new_client);
            // Cerrar la conexion para rechazar la solicitud de registro duplicado
            lws_close_reason(wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION,
                             (unsigned char *)"Nombre de usuario duplicado", strlen("Nombre de usuario duplicado"));
            lws_set_timeout(wsi, PENDING_TIMEOUT_CLOSE_SEND, 1);
        }
    }
    
    else if (strcmp(msg.type, MSG_TYPE_BROADCAST) == 0) {
        // Mensaje broadcast difunde el mensaje a todos los clientes
        broadcast_message(&msg);
    } else if (strcmp(msg.type, MSG_TYPE_PRIVATE) == 0) {
        // manda el mensaje unicamente al usuario destino
        send_private_message(&msg, msg.target);
    } else if (strcmp(msg.type, MSG_TYPE_LIST_USERS) == 0) {
        // Solicitud de listado de usuarios manda la lista al cliente solicitante
        send_list_users(wsi);
    } else if (strcmp(msg.type, MSG_TYPE_USER_INFO) == 0) {
        // Solicitud de info de un usuario manda la info correspondiente
        send_user_info(wsi, msg.target);
    } else if (strcmp(msg.type, MSG_TYPE_CHANGE_STATUS) == 0) {
        //Cambio de estado solicitado actualiza el estado del usuario
        update_client_status(msg.sender, msg.content);
        // elimina al usuario y difunde la notificacion de salida
    } else if (strcmp(msg.type, MSG_TYPE_DISCONNECT) == 0) {
        remove_client(msg.sender);
        ProtocolMessage disc_msg;
        memset(&disc_msg, 0, sizeof(disc_msg));
        strncpy(disc_msg.type, MSG_TYPE_USER_DISCONNECTED, MAX_FIELD_LENGTH);
        strncpy(disc_msg.sender, "server", MAX_FIELD_LENGTH);
        snprintf(disc_msg.content, MAX_MESSAGE_LENGTH, "%s ha salido", msg.sender);
        get_current_timestamp(disc_msg.timestamp, MAX_FIELD_LENGTH);
        broadcast_message(&disc_msg); // Difunde el mensaje a todos los clientes
    } else {
        // Tipo de mensaje desconocido manda un mensaje de error
        ProtocolMessage error_msg;
        memset(&error_msg, 0, sizeof(error_msg));
        strncpy(error_msg.type, MSG_TYPE_ERROR, MAX_FIELD_LENGTH);
        strncpy(error_msg.sender, "server", MAX_FIELD_LENGTH);
        strncpy(error_msg.content, "Tipo de mensaje desconocido.", MAX_MESSAGE_LENGTH);
        get_current_timestamp(error_msg.timestamp, MAX_FIELD_LENGTH);
        send_message(wsi, &error_msg);
    }
}

#endif 
