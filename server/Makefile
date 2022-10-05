objects = error.o config.o utils.o file.o server.o ftp.o 

server : $(objects)
	gcc -Wall -Werror -Wshadow -Wextra -pthread --std=c11 -o server $(objects) 
	echo "make success\n"

server.o : ftp.h
config.o : file.h config.h error.h utils.h
error.o :  error.h
utils.o : utils.h 
file.o : file.h
ftp.o : ftp.h config.h

.PHONY : clear
clear :
	rm $(objects) 