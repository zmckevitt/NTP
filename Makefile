all: ntp_client.c ntp_server.c
	gcc -pthread -o ntp_client ntp_client.c
	gcc -pthread -o ntp_server ntp_server.c

.PHONY: all clean

clean:
	rm -f *.o
	rm -f ntp_client
	rm -f ntp_server
