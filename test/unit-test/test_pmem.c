#include <libpmem.h>
#include <stdio.h>

#define DIRPM "/mnt/pmem/test"

int main()
{
    int is_pmem = -1;
    void *pmemaddr = pmem_map_file(DIRPM, 1024,
				PMEM_FILE_CREATE,
				0666, NULL, &is_pmem);
    printf("%d, %d\n", pmemaddr != NULL, is_pmem);
    remove(DIRPM);
    return 1;
}
