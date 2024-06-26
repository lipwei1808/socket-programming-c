# Configuration

CC		= gcc
LD		= gcc

SRCS := server.c client.c

all: build/client build/server

build/client: client.o message.o
							@echo "Linking $@"
							@$(LD) -o $@ $^

build/server: server.o message.o
							@echo "Linking $@"
							@$(LD) -o $@ $^

%.o: %.c | build
					@$(CC) -c -o $@ $<

clean: 
	rm -rf build	

.INTERMEDIATE: client.o server.o message.o

build: ; @mkdir -p $@