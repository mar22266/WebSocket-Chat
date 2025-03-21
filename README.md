# Chat Cliente-Servidor en C (WebSockets)

Este proyecto implementa un sistema de **chat en tiempo real** cliente-servidor escrito en C, utilizando **WebSockets** para la comunicación y un protocolo de mensajes en formato JSON. Permite que múltiples usuarios se conecten a un servidor de chat, envíen mensajes públicos (broadcast) a todos los conectados, mensajes privados a usuarios específicos, cambien su estado (activo, ocupado, inactivo) y consulten información de usuarios en línea. El protocolo está diseñado para ser interoperable, de modo que otros clientes/servidores que sigan el mismo formato de mensajes podrán comunicarse con este proyecto.

## Características
- **Mensajes broadcast**: Cualquier usuario puede enviar un mensaje que será retransmitido a todos los usuarios conectados.
- **Mensajes privados**: Posibilidad de enviar mensajes directos a un usuario específico sin que los demás lo vean.
- **Estados de usuario**: Cada usuario tiene un estado (ACTIVO, OCUPADO, INACTIVO) que puede cambiar manualmente o automáticamente tras un periodo de inactividad.
- **Lista de usuarios e información**: Los usuarios pueden listar quiénes están conectados y pedir información detallada (IP y estado) de un usuario en particular.
- **Notificaciones de conexión/desconexión**: El servidor notifica a todos cuando un usuario entra o sale del chat.
- **Protocolo JSON**: Los mensajes se intercambian en formato JSON siguiendo un esquema predefinido, facilitando la integración con otras implementaciones.
- **Multihilo en el cliente**: El cliente utiliza hilos para manejar la entrada de usuario y la recepción de mensajes simultáneamente, asegurando una interfaz no bloqueante.

## Requisitos y Dependencias

- **Sistema operativo**: Linux (u otro SO que soporte `pthread` y *libwebsockets*).
- **Librería libwebsockets** (versión 2.x o 3.x recomendada): necesaria tanto para compilar como para ejecutar. Asegúrese de tener instalada la librería de desarrollo (por ejemplo, en Debian/Ubuntu: `sudo apt-get install libwebsockets-dev`).
- **Compilador GCC** compatible con C99.
- `pthread`: biblioteca para hilos (normalmente incluida en libc, se enlaza con `-lpthread`).

## Compilación

El proyecto incluye un **Makefile** que compila tanto el servidor como el cliente. Para compilar, simplemente ejecute en la raíz del proyecto:

```bash
make
```
Esto generará dos ejecutables:
- **server_chat** – el binario del servidor
- **client_chat** – el binario del cliente

También se puede compilar individualmente:
- `make server_chat` – compila solo el servidor
- `make client_chat` – compila solo el cliente
- `make clean` – elimina los binarios compilados

# Ejecución

## 1. Iniciar el Servidor

Ejecute el programa servidor especificando el puerto de escucha como argumento. Por ejemplo:
./server_chat 8000
Esto iniciará el servidor en el puerto **8000**. Verá un mensaje indicando que el servidor está corriendo y esperando conexiones WebSocket en ese puerto.

## 2. Iniciar Clientes

Ejecute el programa cliente por cada usuario que desee conectar. Debe proporcionar tres argumentos: **nombre_de_usuario**, **IP_del_servidor**, **puerto**. Por ejemplo:
./client_chat andre 127.0.0.1 8000

Este comando conectará al usuario "andre" al servidor ubicado en 127.0.0.1:8000. Si la conexión es exitosa, verá en la consola del cliente un mensaje de "Conexión establecida con el servidor WebSocket." seguido de "Mensaje recibido: ..." confirmando el registro exitoso y listando los usuarios conectados.

Ejecute múltiples instancias del cliente (cada una con un nombre de usuario distinto) para simular una conversación de chat multiusuario.

Nota: El nombre de usuario debe ser único; el servidor rechazará conexiones con un nombre ya en uso.

# Uso y Comandos Disponibles en el Cliente

Una vez conectado, el cliente permite ingresar comandos de chat. Escriba un comando y presione Enter. A continuación, se listan los comandos soportados:

- **broadcast <mensaje>**  
  Envía <mensaje> a todos los usuarios conectados.  
  Ejemplo:
  broadcast Hola a todos!

- **private <usuario> <mensaje>**  
Envía un mensaje privado a <usuario> con el contenido <mensaje>. Solo <usuario> recibirá este mensaje.  
Ejemplo:
private bob ¿Cómo estás?
(Este comando enviaría "¿Cómo estás?" únicamente al usuario "bob".)

- **list_users**  
Solicita al servidor la lista de usuarios actualmente conectados. El servidor responderá con un mensaje que contiene la lista en formato JSON.  
Ejemplo de respuesta (mostrada en el cliente):
Mensaje recibido: {"type": "list_users_response", "sender": "server", "content": ["alice","bob","carla"], "timestamp": "2025-03-20T21:03:34"}
(El contenido es un array JSON con los nombres de usuario.)

- **user_info <usuario>**  
Pide información de un usuario específico. El servidor responderá con la información disponible de <usuario> (actualmente su dirección IP y estado).  
Ejemplo de uso:
user_info bob
Ejemplo de respuesta:
Mensaje recibido: {"type": "user_info_response", "sender": "server", "target": "bob", "content": {"ip": "192.168.1.10", "status": "ACTIVO"}, "timestamp": "2025-03-20T21:04:10"}
Si el usuario no existe, content será null.

- **change_status <estado>**  
Cambia tu estado actual y notifica a todos los usuarios. Los estados válidos son ACTIVO, OCUPADO o INACTIVO.  
Ejemplo:
change_status OCUPADO
Esto cambiará tu estado a "OCUPADO" y el servidor enviará a todos un mensaje de actualización de estado. Si un usuario permanece 15 segundos sin escribir nada, el cliente automáticamente enviará change_status INACTIVO y cambiará su estado a inactivo. Al escribir nuevamente, el cliente enviará change_status ACTIVO indicando que está activo de nuevo.

- **disconnect** (o **exit**)  
Cierra la conexión con el servidor y sale del programa cliente. El servidor notificará a los demás usuarios que has salido. Es equivalente a escribir exit.  
Ejemplo:
disconnect
- **help**  
Muestra en la consola un recordatorio de todos los comandos disponibles y su uso.

Mientras el cliente esté conectado, cualquier mensaje recibido del servidor se mostrará automáticamente en pantalla con el formato:

Mensaje recibido: <contenido>

Estos mensajes pueden incluir chat de otros usuarios, notificaciones de nuevos usuarios conectados, notificaciones de desconexión, respuestas a comandos, etc., todos en formato JSON.

# Protocolo de Mensajes (Estructura JSON)

La comunicación entre el cliente y el servidor se realiza mediante mensajes en formato JSON con una estructura consistente. Cada mensaje JSON contiene los siguientes campos clave:

- **type:** Tipo de mensaje (cadena). Indica la acción o naturaleza del mensaje. Ejemplos: "register", "broadcast", "private", "list_users", "user_info", "change_status", "disconnect", y los tipos de respuesta/notificación como "register_success", "list_users_response", "user_info_response", "status_update", "user_disconnected", "error".
- **sender:** Remitente (cadena). Para mensajes enviados por usuarios, es el nombre de usuario; para mensajes del servidor, suele ser "server".
- **target:** Destinatario (cadena, opcional). Usado en mensajes privados para indicar a quién va dirigido el mensaje, o en las respuestas de user_info_response para indicar de quién se proporciona información. En mensajes que no tienen un único destinatario específico (broadcast, listados, etc.), este campo puede no existir o estar vacío.
- **content:** Contenido del mensaje. Puede ser:
- Una cadena de texto (por ejemplo, el mensaje de chat).
- Un objeto JSON o arreglo JSON (por ejemplo, en status_update el content es un objeto {"user": "...", "status": "..."}, en list_users_response el content es un arreglo de nombres de usuario, o en user_info_response el content es un objeto con ip y status).
- Puede estar vacío o no presente en ciertos mensajes (por ejemplo, en register no es necesario).
- **timestamp:** Marca de tiempo (cadena). Es la hora en que se envió el mensaje, formateada como AAAA-MM-DDThh:mm:ss. Este valor lo generan tanto el cliente como el servidor al crear el mensaje.
- **userList:** Lista de usuarios (arreglo JSON, opcional). Solo incluido en mensajes donde es relevante, por ejemplo en register_success para proporcionar al nuevo cliente la lista de todos los usuarios conectados en ese momento.

### Tipos de Mensaje Principales

- **register:** Enviado por el cliente al conectarse para registrar su nombre de usuario.
- **register_success:** Respuesta del servidor que confirma el registro y proporciona la lista de usuarios conectados.
- **broadcast:** Mensaje de chat público enviado por un cliente y difundido a todos.
- **private:** Mensaje de chat privado, enviado a un usuario específico (se utiliza el campo target).
- **list_users:** Petición de lista de usuarios conectados.
- **list_users_response:** Respuesta con un arreglo JSON de los usuarios conectados.
- **user_info:** Petición de información sobre un usuario específico.
- **user_info_response:** Respuesta con un objeto JSON que contiene la información (IP y estado) de un usuario.
- **change_status:** Mensaje para cambiar el estado del usuario.
- **status_update:** Notificación enviada por el servidor a todos cuando un usuario cambia de estado. En content se incluye un objeto JSON {"user": "<nombre>", "status": "<nuevo_status>"}.
- **disconnect:** Mensaje para desconectarse voluntariamente.
- **user_disconnected:** Notificación del servidor a todos indicando que un usuario se ha desconectado.
- **error:** Mensaje de error en caso de problemas (por ejemplo, nombre duplicado, JSON inválido, mensaje desconocido).

### Ejemplo de Intercambio

1. **Registro:**
   - **Cliente envía:**
     ```json
     {"type":"register","sender":"alice","timestamp":"2025-03-20T21:00:00"}
     ```
     — Solicita registrarse como "alice".
   - **Servidor responde:**
     ```json
     {"type":"register_success","sender":"server","content":"Registro exitoso","userList":["alice","bob"],"timestamp":"2025-03-20T21:00:01"}
     ```
     — Confirma el registro y proporciona la lista actual de usuarios (en este ejemplo, ya estaba "bob" conectado, además de "alice").

2. **Mensaje Broadcast:**
   - **Bob envía:**
     ```json
     {"type":"broadcast","sender":"bob","content":"Hola a todos!","timestamp":"2025-03-20T21:03:00"}
     ```
   - **Todos los usuarios reciben:**
     ```json
     {"type":"broadcast","sender":"bob","content":"Hola a todos!","timestamp":"2025-03-20T21:03:00"}
     ```

3. **Mensaje Privado:**
   - **Carla envía:**
     ```json
     {"type":"private","sender":"carla","target":"bob","content":"¿Cómo estás?","timestamp":"2025-03-20T21:04:00"}
     ```
   - **Solo Bob recibe:**
     ```json
     {"type":"private","sender":"carla","target":"bob","content":"¿Cómo estás?","timestamp":"2025-03-20T21:04:00"}
     ```

4. **Cambio de Estado:**
   - **Un cliente envía:**
     ```json
     {"type":"change_status","sender":"alice","content":"OCUPADO","timestamp":"2025-03-20T21:05:00"}
     ```
   - **Servidor actualiza el estado y envía a todos:**
     ```json
     {"type":"status_update","sender":"server","content":{"user":"alice","status":"OCUPADO"},"timestamp":"2025-03-20T21:05:01"}
     ```

5. **List Users:**
   - **Cliente envía:**
     ```json
     {"type":"list_users","sender":"client","timestamp":"2025-03-20T21:06:00"}
     ```
   - **Servidor responde:**
     ```json
     {"type":"list_users_response","sender":"server","content":["alice","bob"],"timestamp":"2025-03-20T21:06:01"}
     ```

6. **User Info:**
   - **Cliente envía:**
     ```json
     {"type":"user_info","sender":"client","target":"alice","timestamp":"2025-03-20T21:07:00"}
     ```
   - **Servidor responde:**
     ```json
     {"type":"user_info_response","sender":"server","target":"alice","content":{"ip":"127.0.0.1","status":"ACTIVO"},"timestamp":"2025-03-20T21:07:01"}
     ```

7. **Desconexión:**
   - **Cliente envía:**
     ```json
     {"type":"disconnect","sender":"bob","content":"Cierre de sesión","timestamp":"2025-03-20T21:08:00"}
     ```
   - **Servidor responde:**
     ```json
     {"type":"user_disconnected","sender":"server","content":"bob ha salido","timestamp":"2025-03-20T21:08:01"}
     ```
