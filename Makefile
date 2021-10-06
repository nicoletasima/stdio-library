build:
	gcc -fPIC -c so_stdio.c -Wall -Wextra -g
	gcc -shared so_stdio.o -o libso_stdio.so
clean:
	rm *.o
