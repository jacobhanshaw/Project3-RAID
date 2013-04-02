raidsim: main.o disk-array.o disk.o
	gcc main.o disk-array.o disk.o -o raidsim

main.o: main.c
	gcc -Wall -g -c main.c -o main.o

disk-array.o: disk-array.c
	gcc -Wall -g -c disk-array.c -o disk-array.o

disk.o: disk.c
	gcc -Wall -g -c disk.c -o disk.o



clean:
	rm -f *.o raidsim
