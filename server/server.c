#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <libwebsockets.h>
#include "server.h"
#include <unistd.h> 
#include <time.h>

// Bandera para terminar el bucle principal de forma controlada
static volatile int force_exit = 0;

// Manejador de señal para finalizar el servidor Ctrl+C
static void sighandler(int sig) {
    force_exit = 1;
}


 
// Funcion callback para manejar los eventos de WebSocket procesa el establecimiento de conexion, recepcion de mensajes y cierre de la conexion
static int callback_chat(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    switch(reason) {
        case LWS_CALLBACK_ESTABLISHED:
            // Conexion establecida
            lwsl_user("Conexión establecida con un cliente.\n");
            break;
            
        case LWS_CALLBACK_RECEIVE:
            {
                // Se recibe un mensaje: asignar memoria, procesarlo y liberar
                char *received = (char *)malloc(len + 1);
                if (received) {
                    memcpy(received, in, len);
                    received[len] = '\0';
                    lwsl_user("Mensaje recibido: %s\n", received);
                    // Procesa el mensaje recibido
                    handle_incoming_message(wsi, received);
                    free(received);
                }

                // Actualizar la última actividad del cliente y, si estaba inactivo, cambiar a ACTIVO.
                int should_activate = 0;
                char uname[MAX_FIELD_LENGTH] = "";

                pthread_mutex_lock(&client_list_mutex);
                Client *cli = client_list;
                while (cli != NULL) {
                    if (cli->wsi == wsi) {
                        // Actualiza la marca de tiempo con la hora actual
                        cli->last_activity = time(NULL);
                        // Si el cliente estaba marcado como INACTIVO, marcar para activarlo
                        if (strcmp(cli->status, STATUS_INACTIVE) == 0) {
                            strncpy(uname, cli->username, MAX_FIELD_LENGTH);
                            should_activate = 1;
                        }
                        break;
                    }
                    cli = cli->next;
                }
                pthread_mutex_unlock(&client_list_mutex);

                if (should_activate) {
                    update_client_status(uname, STATUS_ACTIVE);
                }
            }
            break;

            
    // Se cierra la conexion se maneja la desconexion del cliente
    case LWS_CALLBACK_CLOSED:
        // notificar la desconexion
        lwsl_user("Conexión cerrada con un cliente.\n");
        // Eliminar al cliente de la lista si aun esta presente
        pthread_mutex_lock(&client_list_mutex);
        Client *curr = client_list;
        char username_to_remove[MAX_FIELD_LENGTH] = "";
        while (curr != NULL) {
            if (curr->wsi == wsi) {
                strncpy(username_to_remove, curr->username, MAX_FIELD_LENGTH); // Guarda el nombre del usuario
                break;
            }
            curr = curr->next;
        }
        pthread_mutex_unlock(&client_list_mutex);
        if (username_to_remove[0] != '\0') {
            // Se elimina el cliente de la lista y se difunde la notificacion de desconexion
            remove_client(username_to_remove);
            ProtocolMessage disc_msg;
            memset(&disc_msg, 0, sizeof(disc_msg));
            strncpy(disc_msg.type, MSG_TYPE_USER_DISCONNECTED, MAX_FIELD_LENGTH);
            strncpy(disc_msg.sender, "server", MAX_FIELD_LENGTH);
            snprintf(disc_msg.content, MAX_MESSAGE_LENGTH, "%s ha salido", username_to_remove);
            get_current_timestamp(disc_msg.timestamp, MAX_FIELD_LENGTH);
            broadcast_message(&disc_msg);
        }
        break;

            
        default:
            break;
    }
    return 0;
}
// Hilo de inactividad del servidor: revisa clientes cada 1 s y actualiza estado
static void* inactivity_monitor(void *arg) {
    while (!force_exit) {
        sleep(1);
        time_t now = time(NULL);
        pthread_mutex_lock(&client_list_mutex);
        Client *curr = client_list;
        while (curr != NULL) {
            if (difftime(now, curr->last_activity) >= 15 && 
                strcmp(curr->status, STATUS_INACTIVE) != 0) {
                char uname[MAX_FIELD_LENGTH];
                strncpy(uname, curr->username, MAX_FIELD_LENGTH);
                pthread_mutex_unlock(&client_list_mutex);
                update_client_status(uname, STATUS_INACTIVE);
                pthread_mutex_lock(&client_list_mutex);
                // Reinicia el recorrido para evitar inconsistencias.
                curr = client_list;
                continue;
            }
            curr = curr->next;
        }
        pthread_mutex_unlock(&client_list_mutex);
    }
    return NULL;
}


// Definicion de los protocolos que usara libwebsockets
static struct lws_protocols protocols[] = {
    {
        "chat-protocol", // Nombre del protocolo
        callback_chat, // Función callback que gestiona los eventos del WebSocket
        0,               // Tamaño de datos por sesion
        MAX_MESSAGE_LENGTH, // Tamaño máximo del buffer de recepción
    },
    { NULL, NULL, 0, 0 } // Elemento terminador
};

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <puertodelservidor>\n", argv[0]); // Informa el uso correcto si no se pasa el puerto
        return EXIT_FAILURE;  // Termina el programa con error
    }
    
    int port = atoi(argv[1]);  // Convierte el argumento del puerto a entero
    if (port <= 0) {
        fprintf(stderr, "Puerto inválido.\n"); // Notifica si el puerto es invalido
        return EXIT_FAILURE;
    }
    
    // Configurar el manejador de señal para finalizar el servidor con Ctrl+C
    signal(SIGINT, sighandler);
    
    // Configuración del contexto de libwebsockets.
    struct lws_context_creation_info info; 
    memset(&info, 0, sizeof(info));  // Inicializa la estructura a cero
    info.port = port;  // Establece el protocolo definido
    info.protocols = protocols;  // Establece el protocolo definido
    info.gid = -1;
    info.uid = -1;
    info.options = 0; // Opciones adicionales según se requiera
    
    // Crear el contexto de libwebsockets
    struct lws_context *context = lws_create_context(&info);
    if (context == NULL) {
        fprintf(stderr, "Error al iniciar libwebsockets.\n");
        return EXIT_FAILURE;
    }
    
    lwsl_user("Servidor iniciado en el puerto %d.\n", port);
    // Lanzar el hilo de monitoreo de inactividad del servidor
    pthread_t inactivity_tid;
    if (pthread_create(&inactivity_tid, NULL, inactivity_monitor, NULL) != 0) {
        fprintf(stderr, "Error al crear el hilo de inactividad.\n");
        lws_context_destroy(context);
        return EXIT_FAILURE;
    }

    // Bucle principal del servidor
    while (!force_exit) {
        lws_service(context, 50);
    }
    
    pthread_join(inactivity_tid, NULL);
    // Limpieza y finalizacion
    lws_context_destroy(context);
    lwsl_user("Servidor finalizado.\n");
    
    return EXIT_SUCCESS;
}
