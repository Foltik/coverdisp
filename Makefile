all: build

CC = gcc

build:
	$(CC) coverdisp.c -o coverdisp -lmpdclient

install:
	sudo cp coverdisp /usr/local/bin/coverdisp
