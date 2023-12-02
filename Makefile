all: ex1_p1 ex1_p2 clean_text

ex1_p1: ex1_p1.c
	gcc -pthread -g -Wall ex1_p1.c -o ex1_p1 -lrt

ex1_p2: ex1_p2.c
	gcc -pthread -g -Wall ex1_p2.c -o ex1_p2 -lrt

clean_text:
	rm -f *.txt

clean:
	rm -f ex1_p1 ex1_p2 *.txt

