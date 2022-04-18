GMM_PATH := ../cmake-build-release/
CC := clang
TB_FILE := test.c

.PHONY: run_gmm
run_gmm : test
	time ./test

.PHONY: run_clang
run_clang : test_clang
	time ./test_clang

test : test.o print.o
	$(CC) -o $@ $^


test.o : target.s
	as -o $@ $^


print.o : print.c
	$(CC) -c $^


target.s : $(TB_FILE)
	cp $(GMM_PATH)g-- ./
	-./g-- $^ -o target.s


test_clang : test_clang.o print.o
	$(CC) -o $@ $^

test_clang.o : $(TB_FILE)
	$(CC) -o $@ -c $^

.PHONY : run_clang3
run_clang3 : test_clang3
	time ./test_clang3

test_clang3 : test_clang3.o print.o
	$(CC) -o $@ $^

test_clang3.o : $(TB_FILE)
	$(CC) -O3 -o $@ -c $^

.PHONY: clean
clean :
	-rm -f *.txt *.s *.o test g-- test_clang test_clang3
