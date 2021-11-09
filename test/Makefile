cc = gcc
libs = -pthread -lrdmacm -libverbs -lpmem

tg1 = generate
tg2 = write
tg3 = read
tg4 = rdma

all : $(tg1) $(tg2) $(tg3) $(tg4)

$(tg1) : $(tg1)_test.c
	$(cc) -o $(tg1) $(tg1)_test.c

$(tg2) : $(tg2)_test.c
	$(cc) -o $(tg2) $(tg2)_test.c -pthread -lpmem


$(tg3) : $(tg3)_test.c
	$(cc) -o $(tg3) $(tg3)_test.c -pthread -lpmem

$(tg4) : $(tg4)_test.c
	$(cc) -o $(tg4) $(tg4)_test.c $(libs)

clean:
	rm -rf $(tg1) $(tg2) $(tg3) $(tg4) *.o *.out *.test *.bench

zip:
	zip test.zip $(tg1)_test.c $(tg2)_test.c $(tg3)_test.c $(tg4)_test.c Makefile