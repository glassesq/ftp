objects = file.o error.o config.o utils.o server.o ftp.o ftpreply.o

server : $(objects)
	gcc -Wall -Werror -Wshadow -Wextra -pthread --std=c11 -o server $(objects) -lm
	echo "make success\n"

CFLAGS := -Wall -Werror -Wshadow -Wextra -pthread -lm

file.o : file.h 
server.o : ftp.h
config.o : file.h config.h error.h utils.h
error.o :  error.h
utils.o : utils.h 
ftp.o : ftp.h config.h
ftpreply.o : ftp.h error.h


.PHONY : clear
clear :
	rm $(objects) 