objects = server.o

server : $(objects)
	gcc -Wall -Werror -Wshadow -Wextra --std=c11 -o server $(objects) 

server.o :

.PHONY : clear
clear :
	-del $(objects)