CC = gcc
CFLAGS = -Wall

all: serwer klient

serwer: inf141329_s.c 
	$(CC) $(CFLAGS) inf141329_s.c -o serwer

klient: inf141329_k.c 
	$(CC) $(CFLAGS) inf141329_k.c -o klient

