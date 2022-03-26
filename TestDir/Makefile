GMM_PATH := ../cmake-build-release/
CC := clang

.PHONY: run_gmm
run_gmm : test
	time ./test

.PHONY: run_gcc
run_gcc : test_gcc
	time ./test_gcc

test : test.o print.o
	$(CC) -o $@ $^


test.o : target.s
	as -o $@ $^


print.o : print.c
	$(CC) -c $^


target.asm : test.c
	cp $(GMM_PATH)g-- ./
	-./g-- -G ../Grammar/G.txt test.c


test_gcc : test_gcc.o print.o
	$(CC) -o $@ $^

test_gcc.o : test.c
	$(CC) -o $@ -c $^

.PHONY: clean
clean :
	-rm -f *.txt *.asm *.o test g-- test_gcc
