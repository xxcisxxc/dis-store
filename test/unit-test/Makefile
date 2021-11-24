cc = gcc
libs1 = 
libs2 = -pthread -lpmem
libs3 = -pthread -lrdmacm -libverbs -lpmem

tg1 = generate
tg2 = local
tg3 = rdma

all : $(tg1) $(tg2) $(tg3) $(tg4)

$(tg1) : $(tg1)_test.c
	$(cc) -o $(tg1) $(tg1)_test.c $(libs1)

$(tg2) : $(tg2)_test.c
	$(cc) -o $(tg2) $(tg2)_test.c $(libs2)

$(tg3) : $(tg3)_test.c setup_ib.h
	$(cc) -o $(tg3) $(tg3)_test.c $(libs3)

dir1 = 
dir2 = /mnt/pmem
dir3 = /dev/shm

clean_test:
	@sudo rm -rf *.test *.bench $(dir2)/*.test $(dir3)/*.test

clean:
	sudo rm -rf $(tg1) $(tg2) $(tg3) $(tg4) *.o *.out *.test *.bench $(dir2)/*.test $(dir3)/*.test

zip:
	zip test.zip $(tg1)_test.c $(tg2)_test.c $(tg3)_test.c setup_ib.h Makefile