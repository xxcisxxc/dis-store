#ifndef _SIM_PAGE_H
#define _SIM_PAGE_H

#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "utils.h"

/* 
 * 1: DISK
 * 2: RAM
 * 3: PMEM
 * 4: REMOTE
 */
#define DEVICE 4
#if DEVICE == 1
	#define DIR "."
#elif DEVICE == 2
	#define DIR "/dev/shm"
#elif DEVICE == 3
	#include <libpmem.h>
	#define DIR "/mnt/pmem"
#elif DEVICE == 4
	#include "setup_ib.h"
#endif /* DEVICE */

static uint32_t PHYS_MEM_SIZE;

static char *phys_mem;

#define PAGE_SIZE 4096

typedef struct __page_entry
{
	uint32_t pframe;
	uint32_t cnext;
	int8_t clear;
	int8_t dirty;
} page_entry;

static page_entry *pgtable;
static page_entry *last_c, *curr_c;

void init_phys_mem(uint32_t phys_mem_size)
{
	PHYS_MEM_SIZE = phys_mem_size;
	phys_mem = (char *)
		mmap(NULL, PHYS_MEM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0);
	check(phys_mem != NULL, "Can't allocate %s", __func__);
}

void destroy_phys_mem()
{
	check(munmap(phys_mem, PHYS_MEM_SIZE) == 0, "Can't free %s", __func__);
}

void init_page_table(uint32_t vir_mem_size, char *server, char *port)
{
	uint32_t num_pages = (uint32_t)(vir_mem_size / PAGE_SIZE);
	pgtable =  (page_entry *)malloc(num_pages * sizeof(page_entry));
	uint32_t i, init_np = PHYS_MEM_SIZE / PAGE_SIZE;

	check(pgtable != NULL, "Can't allocate %s", __func__);

	for (i = 0; i < num_pages; i++) {
		if (i < init_np) {
			pgtable[i].pframe = i;
			pgtable[i].cnext = (i + 1) % init_np;
			pgtable[i].clear = 1;
			pgtable[i].dirty = 0;
		} else {
			pgtable[i].pframe = 0;
			pgtable[i].cnext = 0;
			pgtable[i].clear = -1;
			pgtable[i].dirty = 0;
		}
	}

	last_c = &pgtable[init_np-1];
	curr_c = &pgtable[0];

#if DEVICE == 4
	setup_ib(PAGE_SIZE, 0, server, port);
#endif
}

void destroy_page_table()
{
	free(pgtable);
#if DEVICE <= 3 && DEVICE > 0
	char cmd[64];
	sprintf(cmd, "rm -rf %s/*.swap", DIR);
	system(cmd);
#elif DEVICE == 4
	printf("Hello\n");
	close_ib_connection();
	printf("Byebye\n");
#endif
}

void page_fault(uint32_t);

#define index(addr) (uint32_t)((addr) / PAGE_SIZE)
#define offset(addr) ((addr) % PAGE_SIZE)
#define address(frame) (frame * PAGE_SIZE)

char get(uint32_t vaddr)
{
	uint32_t vframe = index(vaddr);
	page_entry *pe = &pgtable[vframe];

	if (pe->clear == -1)
		page_fault(vframe);

	pe->clear = 1;

	return phys_mem[address(pe->pframe) + offset(vaddr)];
}

void put(uint32_t vaddr, char c)
{
	uint32_t vframe = index(vaddr);
	page_entry *pe = &pgtable[vframe];

	if (pe->clear == -1)
		page_fault(vframe);

	pe->dirty = 1;
	pe->clear = 1;

	phys_mem[address(pe->pframe) + offset(vaddr)] = c;
}

void page_load(uint32_t vframe, uint32_t pframe)
{
	void *buf = (void *)(phys_mem + address(pframe));
	printf("%s\n", __func__);

#if DEVICE <= 3 && DEVICE > 0
	char name[64];
	#if DEVICE == 1 || DEVICE == 2
	int fd;
	#elif DEVICE == 3
	void *pbuf;
	size_t mapped_size;
	int is_pmem;
	#endif

	sprintf(name, "%s/%d.swap", DIR, vframe);

	if (access(name, F_OK) == 0) {
	#if DEVICE == 1 || DEVICE == 2
		fd = open(name, O_RDONLY);
		check(read(fd, buf, PAGE_SIZE) == PAGE_SIZE, "Can't read %s", __func__);
		close(fd);
	#elif DEVICE == 3
		pbuf = pmem_map_file(name, 0, 0, 0644, &mapped_size, &is_pmem);
		check(pbuf != NULL || mapped_size == PAGE_SIZE /*|| is_pmem != 1*/, 
			"Can't allocate %s", __func__);
		memcpy(buf, pbuf, PAGE_SIZE);
		pmem_unmap(pbuf, PAGE_SIZE);
	#endif
	} else
		memset(buf, 0, PAGE_SIZE);
#elif DEVICE == 4
	check(post_read_signaled(address(vframe)) == 0, "Can't read %s", __func__);
	wait_poll(IBV_WC_RDMA_READ);
	memcpy(buf, ib_res.ib_buf, PAGE_SIZE);
#endif
}

void page_flush(uint32_t vframe)
{
	void *buf = (void *)(phys_mem + address(pgtable[vframe].pframe));
	printf("%s\n", __func__);

#if DEVICE <= 3 && DEVICE > 0
	char name[64];
	#if DEVICE == 1 || DEVICE == 2
	int fd;
	#elif DEVICE == 3
	void *pbuf;
	size_t mapped_size;
	int is_pmem;
	#endif

	sprintf(name, "%s/%d.swap", DIR, vframe);

	#if DEVICE == 1 || DEVICE == 2
	fd = open(name, O_WRONLY|O_CREAT, 0644);
	check(write(fd, buf, PAGE_SIZE) == PAGE_SIZE, "Can't write %s", __func__);
	fsync(fd);
	close(fd);
	#elif DEVICE == 3
	pbuf = pmem_map_file(name, PAGE_SIZE, PMEM_FILE_CREATE, 
		0644, &mapped_size, &is_pmem);
	check(pbuf != NULL || mapped_size == PAGE_SIZE /*|| is_pmem != 1*/, 
		"Can't allocate %s", __func__);
	pmem_memcpy_persist(pbuf, buf, PAGE_SIZE);
	pmem_persist(pbuf, PAGE_SIZE);
	pmem_unmap(pbuf, PAGE_SIZE);
	#endif
#elif DEVICE == 4
	memcpy(ib_res.ib_buf, buf, PAGE_SIZE);
	check(post_write_signaled(address(vframe)) == 0, "Can't write %s", __func__);
	wait_poll(IBV_WC_RDMA_WRITE);
#endif
}

void page_fault(uint32_t vframe)
{
	page_entry *pe;

	while (curr_c->clear) {
		curr_c->clear = 0;
		last_c = curr_c;
		curr_c = &pgtable[curr_c->cnext];
	}

	if (curr_c->dirty == 1)
		page_flush(curr_c - pgtable);
	curr_c->clear = -1;
	page_load(vframe, curr_c->pframe);

	pe = &pgtable[vframe];
	last_c->cnext = vframe;
	pe->pframe = curr_c->pframe;
	pe->clear = 1;
	pe->dirty = 0;
	pe->cnext = curr_c->cnext;
	curr_c = pe;
}

#endif /* _SIM_PAGE_H */