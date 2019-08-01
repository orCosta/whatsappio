CPP= g++
CPPFLAGS= -g -Wall -std=c++11
TAR_FILES= Makefile README whatsappServer.cpp whatsappClient.cpp whatsappio.h whatsappio.cpp

# All Target
all: whatsapp

#Library
whatsapp: server client


# exe Files
client: whatsappClient.cpp
	$(CPP) $(CPPFLAGS) whatsappClient.cpp whatsappio.h whatsappio.cpp -o whatsappClient

server: whatsappServer.cpp
	$(CPP) $(CPPFLAGS) whatsappServer.cpp whatsappio.h whatsappio.cpp -o whatsappServer


# Other Targets

valgrindServer:
	valgrind --leak-check=full --show-possibly-lost=yes --show-reachable=yes --undef-value-errors=yes --track-origins=yes --dsymutil=yes -v ./whatsappServer

valgrindClient:
	valgrind --leak-check=full --show-possibly-lost=yes --show-reachable=yes --undef-value-errors=yes --track-origins=yes --dsymutil=yes -v ./whatsappClient

tar:
	tar -cvf ex4.tar $(TAR_FILES)

clean:
	-rm -f whatsappClient whatsappServer
