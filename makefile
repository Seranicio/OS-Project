make:	main.c header.h
	gcc -pthread main.c -o main
	gcc Manager.c -o manager
