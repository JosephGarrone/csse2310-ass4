CC=gcc
CFLAGS=-g -lpthread -Wall -pedantic -std=gnu99

all: started station finished

rebuild: clean all

station: station.o utils.o connHandler.o station.h structs.h
	@echo -e "\e[4;49;37mSTATION EXECUTABLE\e[0m"
	$(CC) $(CFLAGS) station.o utils.o connHandler.o -o station
	@echo ""

utils.o: utils.c utils.h
	@echo -e "\e[4;49;37mUTILS.O\e[0m"
	$(CC) $(CFLAGS) -c utils.c
	@echo ""

station.o: station.c station.h
	@echo -e "\e[4;49;37mSTATION.O\e[0m"
	$(CC) $(CFLAGS) -c station.c
	@echo ""

connHandler.o: connHandler.c connHandler.h structs.h
	@echo -e "\e[4;49;37mCONNHANDLER.O\e[0m"
	$(CC) $(CFLAGS) -c connHandler.c
	@echo ""

clean:
	@echo -e "\e[4;49;37mCLEAN\e[0m"
	-rm *.o
	@echo -e "\e[1;49;95mClean as a whistle\e[0m\n"

finished:
	@echo -e "\e[1;49;91mBuild\e[1;49;34m finished\n\n\e[1;49;92mAll your sockets are belong to us\e[0m\n"

started:
	@echo -e "\e[1;49;91mBuild\e[1;49;34m started\e[0m\n"
