# Variables de compilacion
CC      = gcc
CFLAGS  = -Wall -O2 -I./include -I./client -I./server
LIBS    = -lwebsockets -lpthread

# ubicacion de los fuentes
SERVER_SRC = server/server.c
CLIENT_SRC = client/client.c

# Nombres que van a tener  los ejecutables
SERVER_BIN = server_chat
CLIENT_BIN = client_chat

# Compilar ambos binarios
all: $(SERVER_BIN) $(CLIENT_BIN)

# Compilacion del servidor
$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(LIBS)

# Compilacion del cliente
$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC) $(LIBS)

# Elimina los binarios compilados
clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
