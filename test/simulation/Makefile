cc = gcc
libs = -pthread -lrdmacm -libverbs -lpmem

tg1 = client
tg2 = server

all : $(tg1) $(tg2)

$(tg1) : $(tg1).c setup_ib.h sim_page.h utils.h
	$(cc) -o $(tg1) $^ $(libs)

$(tg2) : $(tg2).c setup_ib.h utils.h
	$(cc) -o $(tg2) $^ $(libs)