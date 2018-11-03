getcover: getcover.c
	gcc -o getcover getcover.c

install: getcover
	mv ./getcover /usr/local/bin/

clean: getcover
	rm ./getcover
