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
