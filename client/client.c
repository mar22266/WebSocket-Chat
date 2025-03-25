#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <libwebsockets.h>
#include "client.h"
#include "protocol.h"
#include <time.h>
#include <unistd.h>  

// Variables globales, guarda la ultima actividad, el estado actual, la conexión WebSocket, la bandera de salida y el nombre de usuario

static struct lws *client_wsi = NULL;
static volatile int force_exit = 0;
static char *username = NULL;


// Hilo encargado de leer entrada desde stdin y actualiza el timestamp de actividad en cada comando.
void *stdin_thread(void *arg) {
    char buffer[MAX_MESSAGE_LENGTH];
    while (!force_exit) {
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0';
            }
            
            // Procesar el comando ingresado
            process_user_input(buffer, username, client_wsi);
            
             // Si se ingresa disconnect o exit se activa la salida y finaliza el bucle.
            if (strcmp(buffer, "disconnect") == 0 || strcmp(buffer, "exit") == 0) {
                force_exit = 1;
                break;
            }
        }
    }
    return NULL;
}



// Funcion callback para manejar eventos de la conexion cliente
// Gestiona los eventos del cliente en el WebSocket conexion, recepcion de mensajes y cierre
static int callback_client(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    switch(reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("Conexión establecida con el servidor WebSocket.\n");
            client_wsi = wsi; // guarda la conexion WebSocket en la variable global.
            {
                // Enviar mensaje de registro.
                ProtocolMessage reg_msg;
                memset(&reg_msg, 0, sizeof(reg_msg));
                strncpy(reg_msg.type, MSG_TYPE_REGISTER, MAX_FIELD_LENGTH);
                strncpy(reg_msg.sender, username, MAX_FIELD_LENGTH);
                reg_msg.content[0] = '\0'; // Dejar el campo content vacio para el registro.
                get_current_timestamp(reg_msg.timestamp, MAX_FIELD_LENGTH);
                client_send_message(wsi, &reg_msg);
            }
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf("Mensaje recibido: %s\n", (char *)in); // Imprime el mensaje recibido del servidor
            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            printf("Conexión cerrada.\n");
            force_exit = 1;  // Forzar la salida para cerrar la sesion del cliente
            client_wsi = NULL;// Limpia la referencia a la conexion cerrada
            break;
        default:
            break;
    }
    return 0;
}


// Definicion de los protocolos que usara el cliente
static struct lws_protocols protocols[] = {
    {
        "chat-protocol", // Nombre del protocolo para el chat.
        callback_client,
        0,
        MAX_MESSAGE_LENGTH,
    },
    { NULL, NULL, 0, 0 }
};

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <nombredeusuario> <IPdelservidor> <puertodelservidor>\n", argv[0]);
        return EXIT_FAILURE; // Finaliza si no se proporcionan los tres argumentos necesarios
    }
    username = argv[1]; // Establece el nombre de usuario a partir del primer argumento
    const char *server_address = argv[2]; // Asigna la direccion IP del servidor desde el segundo argumento
    int port = atoi(argv[3]); // Convierte el tercer argumento en el número de puerto
    if (port <= 0) {
        fprintf(stderr, "Puerto inválido.\n");
        return EXIT_FAILURE; // Finaliza si el número de puerto no es valido
    }
    
    // Configurar el contexto de libwebsockets para el cliente sin escuchar conexiones entrantes
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options = 0;
    
    // Crear el contexto de libwebsockets
    struct lws_context *context = lws_create_context(&info);
    if (context == NULL) {
        fprintf(stderr, "Error al crear el contexto de libwebsockets.\n");
        return EXIT_FAILURE;
    }
    
    // Configurar la indo de conexion del cliente
    struct lws_client_connect_info connect_info;
    memset(&connect_info, 0, sizeof(connect_info));
    connect_info.context      = context;
    connect_info.address      = server_address;
    connect_info.port         = port;
    connect_info.path         = "/chat";  // el protocolo
    connect_info.host         = lws_canonical_hostname(context);
    connect_info.origin       = "origin";
    connect_info.protocol     = protocols[0].name;
    connect_info.ssl_connection = 0;        // Sin SSL
    
    // Intentar establecer la conexion con el servidor
    client_wsi = lws_client_connect_via_info(&connect_info);
    if (client_wsi == NULL) {
        fprintf(stderr, "Error al conectarse al servidor.\n");
        lws_context_destroy(context);
        return EXIT_FAILURE;
    }
    

    
    // Crear el hilo para leer comandos desde la entrada estandar
    pthread_t tid;
    if (pthread_create(&tid, NULL, stdin_thread, NULL) != 0) {
        fprintf(stderr, "Error al crear el hilo para lectura de stdin.\n");
        lws_context_destroy(context);
        return EXIT_FAILURE;
    }
    
    // Bucle principal del cliente atender eventos de libwebsockets
    while (!force_exit) {
        lws_service(context, 50);
    }
    
    // Esperar a que terminen ambos hilos.
    pthread_join(tid, NULL);
    lws_context_destroy(context);
    
    printf("Cliente finalizado.\n");
    return EXIT_SUCCESS;
}

