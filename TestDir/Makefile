GMM_PATH := ../cmake-build-release/

.PHONY: run
run : test
	./test

test : test.o print.o
	gcc -o $@ $^


test.o : target.asm
	as -o $@ $^


print.o : print.c
	gcc -c $^


target.asm : test.c
	cp $(GMM_PATH)g-- ./
	./g-- -G ../Grammar/G.txt test.c

.PHONY: clean
clean :
	-rm -f *.txt *.asm *.o test g--
