#ifndef _SETUP_IB
#define _SETUP_IB

#include <infiniband/verbs.h>
#include <inttypes.h>
#include <endian.h>
#include <byteswap.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#ifdef USE_PMEM
#include <libpmem.h>
#endif

#define IB_PORT 1

struct IBRes {
    struct ibv_context          *ctx;
    struct ibv_pd               *pd;
    struct ibv_mr               *mr;
    struct ibv_cq               *cq;
    struct ibv_qp               *qp;
    struct ibv_port_attr         port_attr;
    struct ibv_device_attr       dev_attr;

    void   *ib_buf;
    size_t  ib_buf_size;

    uint32_t rkey;
    uint64_t raddr;
} ib_res;

struct QPInfo {
    uint16_t lid;
    uint32_t qp_num;
    uint32_t rkey;
    uint64_t raddr;
}__attribute__ ((packed));

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll (uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll (uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll (uint64_t x) { return x; }
static inline uint64_t ntohll (uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

inline void die(char *msg, int type)
{
	if (type)
		perror(msg);
	else
		fprintf(stderr, "%s\n", msg);
	exit(1);
}

void print_buf()
{
    char *buf = (char *)ib_res.ib_buf;
    int i, size = ib_res.ib_buf_size / sizeof(char);
    for (i = 0; i < size; i++)
        printf("%c, ", buf[i]);
    printf("\n");
}

ssize_t sock_read(int sock_fd, void *buffer, size_t len)
{
    ssize_t nr, tot_read;
    char *buf = buffer; // avoid pointer arithmetic on void pointer                                    
    tot_read = 0;

    while (len !=0 && (nr = read(sock_fd, buf, len)) != 0) {
        if (nr < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        }
        len -= nr;
        buf += nr;
        tot_read += nr;
    }

    return tot_read;
}

ssize_t sock_write(int sock_fd, void *buffer, size_t len)
{
    ssize_t nw, tot_written;
    const char *buf = buffer;  // avoid pointer arithmetic on void pointer                             

    for (tot_written = 0; tot_written < len; ) {
        nw = write(sock_fd, buf, len-tot_written);

        if (nw <= 0) {
            if (nw == -1 && errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        }

        tot_written += nw;
        buf += nw;
    }
    return tot_written;
}

int sock_create_bind(char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sock_fd = -1, ret = 0;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    ret = getaddrinfo(NULL, port, &hints, &result);
    if (ret != 0)
        die("Getaddrinfo bind error", 1);

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock_fd < 0) {
            continue;
        }

        ret = bind(sock_fd, rp->ai_addr, rp->ai_addrlen);
        if (ret == 0) {
            /* bind success */
            break;
        }

        close(sock_fd);
        sock_fd = -1;
    }

    if (rp == NULL) 
        die("Creating socket error", 0);

    freeaddrinfo(result);
    return sock_fd;
}

int sock_create_connect(char *server_name, char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sock_fd = -1, ret = 0;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    ret = getaddrinfo(server_name, port, &hints, &result);
    if (ret != 0)
        die("Getaddrinfo create error", 1);

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock_fd == -1) {
            continue;
        }

        ret = connect(sock_fd, rp->ai_addr, rp->ai_addrlen);
        if (ret == 0) {
            /* connection success */
            break;
        }

        close(sock_fd);
        sock_fd = -1;
    }

    if (rp == NULL)
        die("Could not connect", 1);

    freeaddrinfo(result);
    return sock_fd;
}

int sock_set_qp_info(int sock_fd, struct QPInfo *qp_info)
{
    int n;
    struct QPInfo tmp_qp_info;

    tmp_qp_info.lid       = htons(qp_info->lid);
    tmp_qp_info.qp_num    = htonl(qp_info->qp_num);
    tmp_qp_info.rkey      = htonl(qp_info->rkey);
    tmp_qp_info.raddr     = htonll(qp_info->raddr);

    n = sock_write(sock_fd, (char *)&tmp_qp_info, sizeof(struct QPInfo));
    if (n != sizeof(struct QPInfo))
        die("write qp_info to socket", 0);

    return 0;
}

int sock_get_qp_info(int sock_fd, struct QPInfo *qp_info)
{
    int n;
    struct QPInfo  tmp_qp_info;

    n = sock_read(sock_fd, (char *)&tmp_qp_info, sizeof(struct QPInfo));
    if (n !=sizeof(struct QPInfo))
        die("read qp_info from socket", 0);

    qp_info->lid       = ntohs(tmp_qp_info.lid);
    qp_info->qp_num    = ntohl(tmp_qp_info.qp_num);
    qp_info->rkey      = ntohl(tmp_qp_info.rkey);
    qp_info->raddr     = ntohll(tmp_qp_info.raddr);
    
    return 0;
}

#define IB_MTU IBV_MTU_4096
#define IB_PORT 1
#define IB_SL 0

int modify_qp_to_rts(struct ibv_qp *qp, uint32_t target_qp_num, uint16_t target_lid)
{
    int ret = 0;

    /* change QP state to INIT */
    {
        struct ibv_qp_attr qp_attr = {
            .qp_state        = IBV_QPS_INIT,
            .pkey_index      = 0,
            .port_num        = IB_PORT,
            .qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                               IBV_ACCESS_REMOTE_READ |
                               IBV_ACCESS_REMOTE_ATOMIC |
                               IBV_ACCESS_REMOTE_WRITE,
        };

        ret = ibv_modify_qp(qp, &qp_attr,
                         IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                         IBV_QP_PORT  | IBV_QP_ACCESS_FLAGS);
        if (ret != 0)
            die("Failed to modify qp to INIT", 1);
    }

    /* Change QP state to RTR */
    {
        struct ibv_qp_attr  qp_attr = {
            .qp_state           = IBV_QPS_RTR,
            .path_mtu           = IB_MTU,
            .dest_qp_num        = target_qp_num,
            .rq_psn             = 0,
            .max_dest_rd_atomic = 1,
            .min_rnr_timer      = 12,
            .ah_attr.is_global  = 0,
            .ah_attr.dlid       = target_lid,
            .ah_attr.sl         = IB_SL,
            .ah_attr.src_path_bits = 0,
            .ah_attr.port_num      = IB_PORT,
        };

        ret = ibv_modify_qp(qp, &qp_attr,
                            IBV_QP_STATE | IBV_QP_AV |
                            IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                            IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                            IBV_QP_MIN_RNR_TIMER);
        if (ret != 0)
            die("Failed to change qp to rtr", 1);
    }
    
    /* Change QP state to RTS */
    {
        struct ibv_qp_attr qp_attr = {
            .qp_state      = IBV_QPS_RTS,
            .timeout       = 14,
            .retry_cnt     = 7,
            .rnr_retry     = 7,
            .sq_psn        = 0,
            .max_rd_atomic = 1,
        };

        ret = ibv_modify_qp(qp, &qp_attr,
                            IBV_QP_STATE | IBV_QP_TIMEOUT |
                            IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                            IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
        if (ret != 0)
            die("Failed to modify qp to RTS", 1);
    }

    return 0;
}

#define SOCK_SYNC_MSG "sync"

int connect_qp_server(char *sock_port)
{
    int	ret	= 0, n = 0;
    int	sockfd = 0;
    int	peer_sockfd = 0;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(struct sockaddr_in);
    char sock_buf[64] = {'\0'};
    struct QPInfo local_qp_info, remote_qp_info;

    sockfd = sock_create_bind(sock_port);
    if (sockfd <= 0) 
        die("Failed to create server socket", 1);
    listen(sockfd, 5);

    peer_sockfd = accept(sockfd, (struct sockaddr *)&peer_addr,
			        &peer_addr_len);
    if (peer_sockfd <= 0)
        die("Failed to accept peer_sockfd", 1);

    /* init local qp_info */
    local_qp_info.lid	 = ib_res.port_attr.lid; 
    local_qp_info.qp_num = ib_res.qp->qp_num;
    local_qp_info.rkey   = ib_res.mr->rkey;
    local_qp_info.raddr  = (uintptr_t) ib_res.ib_buf;

    /* get qp_info from client */
    ret = sock_get_qp_info(peer_sockfd, &remote_qp_info);
    if (ret != 0) 
        die("Failed to get qp_info from client", 1);
    
    /* send qp_info to client */
    ret = sock_set_qp_info(peer_sockfd, &local_qp_info);
    if (ret != 0) 
        die("Failed to send qp_info to client", 1);

    /* store rkey and raddr info */
    ib_res.rkey  = remote_qp_info.rkey;
    ib_res.raddr = remote_qp_info.raddr;

    /* change send QP state to RTS */
    ret = modify_qp_to_rts(ib_res.qp, remote_qp_info.qp_num, 
			    remote_qp_info.lid);
    if (ret != 0) 
        die("Failed to modify qp to rts", 1);

    /* sync with clients */
    n = sock_read(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    if (n != sizeof(SOCK_SYNC_MSG))
        die("Failed to receive sync from client", 1);
    
    n = sock_write(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    if (n != sizeof(SOCK_SYNC_MSG))
        die("Failed to write sync to client", 1);
	
    close(peer_sockfd);
    close(sockfd);
    return 0;
}

int connect_qp_client(char *server_name, char *sock_port)
{
    int ret	= 0, n = 0;
    int peer_sockfd   = 0;
    char sock_buf[64] = {'\0'};

    struct QPInfo local_qp_info, remote_qp_info;

    peer_sockfd = sock_create_connect(server_name, sock_port);
    if (peer_sockfd <= 0)
        die("Failed to create peer_sockfd", 1);

    local_qp_info.lid     = ib_res.port_attr.lid; 
    local_qp_info.qp_num  = ib_res.qp->qp_num; 
    local_qp_info.rkey    = ib_res.mr->rkey;
    local_qp_info.raddr   = (uintptr_t) ib_res.ib_buf;
   
    /* send qp_info to server */
    ret = sock_set_qp_info(peer_sockfd, &local_qp_info);
    if (ret != 0)
        die("Failed to send qp_info to server", 1);

    /* get qp_info from server */
    ret = sock_get_qp_info(peer_sockfd, &remote_qp_info);
    if (ret != 0)
        die("Failed to get qp_info from server", 1);
    
    /* store rkey and raddr info */
    ib_res.rkey  = remote_qp_info.rkey;
    ib_res.raddr = remote_qp_info.raddr;
    
    /* change QP state to RTS */
    ret = modify_qp_to_rts(ib_res.qp, remote_qp_info.qp_num, 
			    remote_qp_info.lid);
    if (ret != 0) 
        die("Failed to modify qp to rts", 1);

    /* sync with server */
    n = sock_write(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    if (n != sizeof(SOCK_SYNC_MSG))
        die("Failed to write sync to client", 1);
    
    n = sock_read(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    if (n != sizeof(SOCK_SYNC_MSG))
        die("Failed to receive sync from client", 1);

    close(peer_sockfd);
    return 0;
}

int setup_ib(int fd, size_t buf_size, int is_server, char *server_name, char *sock_port)
{
    int ret;
    
    /* Create an Infiniband context */
    struct ibv_device **dev_list = ibv_get_device_list(NULL);;
    if (dev_list == NULL)
        die("Can't get device list", 1);

    ib_res.ctx = ibv_open_device(*dev_list);
    if (ib_res.ctx == NULL)
        die("Can't open the device", 1);

    ibv_free_device_list(dev_list);

    /* Allocate protect domain */
    ib_res.pd = ibv_alloc_pd(ib_res.ctx);

    ret = ibv_query_port(ib_res.ctx, IB_PORT, &ib_res.port_attr);
    if (ret != 0)
        die("Can't query port", 1);
    ret = ibv_query_device(ib_res.ctx, &ib_res.dev_attr);
    if (ret != 0)
        die("Can't query device", 1);
    
    /* Create complete queue */
    ib_res.cq = ibv_create_cq(ib_res.ctx, ib_res.dev_attr.max_cqe,
                                NULL, NULL, 0);
    if (ib_res.cq == NULL)
        die("Can't create complete queue", 1);
    
    /* Create queue pair */
    struct ibv_qp_init_attr qp_init_attr = {
        .send_cq = ib_res.cq,
        .recv_cq = ib_res.cq,
        .cap = {
            .max_send_wr = ib_res.dev_attr.max_qp_wr,
            .max_recv_wr = ib_res.dev_attr.max_qp_wr,
            .max_send_sge = 1,
            .max_recv_sge = 1,
        },
        .qp_type = IBV_QPT_RC,
    };

    ib_res.qp = ibv_create_qp(ib_res.pd, &qp_init_attr);
    if (ib_res.qp == NULL)
        die("Can't create queue pair", 1);

    /* Register Memory */
    ib_res.ib_buf_size = buf_size;
    if (!is_server) {
        ib_res.ib_buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    } else {
#ifndef USE_PMEM
        ib_res.ib_buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
#else
        ib_res.ib_buf = pmem_map_file(USE_PMEM, buf_size, PMEM_FILE_CREATE, 0666, NULL, NULL);
#endif
    }
    if (ib_res.ib_buf == NULL)
        die("Failed to allocate ib_buf", 1);

    ib_res.mr = ibv_reg_mr(ib_res.pd, ib_res.ib_buf,
                            ib_res.ib_buf_size,
                            IBV_ACCESS_LOCAL_WRITE |
                            IBV_ACCESS_REMOTE_READ |
                            IBV_ACCESS_REMOTE_WRITE);
    if (ib_res.mr == NULL)
        die("Failed to register mr", 1);


    if (is_server)
        connect_qp_server(sock_port);
    else
        connect_qp_client(server_name, sock_port);
}

void close_ib_connection()
{
    if (ib_res.qp != NULL) {
	    ibv_destroy_qp(ib_res.qp);
    }

    if (ib_res.cq != NULL) {
	    ibv_destroy_cq(ib_res.cq);
    }

    if (ib_res.mr != NULL) {
	    ibv_dereg_mr(ib_res.mr);
    }

    if (ib_res.pd != NULL) {
        ibv_dealloc_pd(ib_res.pd);
    }

    if (ib_res.ctx != NULL) {
        ibv_close_device(ib_res.ctx);
    }

    if (ib_res.ib_buf != NULL) {
#ifndef USE_PMEM
	    munmap(ib_res.ib_buf, ib_res.ib_buf_size);
#else
        pmem_unmap(ib_res.ib_buf, ib_res.ib_buf_size);
#endif
    }
}

int post_write_signaled()
{
    uint32_t lkey = ib_res.mr->lkey;
    uint64_t wr_id = 100;
    struct ibv_qp *qp = ib_res.qp;
    void *buf = ib_res.ib_buf;
    uint32_t rkey = ib_res.rkey;
    uint64_t raddr = ib_res.raddr;
    
    int ret = 0;
    struct ibv_send_wr *bad_send_wr;

    struct ibv_sge list = {
        .addr   = (uintptr_t)buf,
        .length = ib_res.ib_buf_size,
        .lkey   = lkey
    };

    struct ibv_send_wr send_wr = {
        .wr_id      = wr_id,
        .sg_list    = &list,
	    .num_sge    = 1,
        .opcode     = IBV_WR_RDMA_WRITE,
        //.send_flags = IBV_SEND_SIGNALED,
	    .wr.rdma.remote_addr = raddr,
        .wr.rdma.rkey        = rkey,
    };

    ret = ibv_post_send(qp, &send_wr, &bad_send_wr);
    return ret;
}

int post_read_signaled()
{
    uint32_t lkey = ib_res.mr->lkey;
    uint64_t wr_id = 200;
    struct ibv_qp *qp = ib_res.qp;
    void *buf = ib_res.ib_buf;
    uint32_t rkey = ib_res.rkey;
    uint64_t raddr = ib_res.raddr;
    
    int ret = 0;
    struct ibv_send_wr *bad_send_wr;

    struct ibv_sge list = {
        .addr   = (uintptr_t)buf,
        .length = ib_res.ib_buf_size,
        .lkey   = lkey
    };

    struct ibv_send_wr send_wr = {
        .wr_id      = wr_id,
        .sg_list    = &list,
	    .num_sge    = 1,
        .opcode     = IBV_WR_RDMA_READ,
        //.send_flags = IBV_SEND_SIGNALED,
	    .wr.rdma.remote_addr = raddr,
        .wr.rdma.rkey        = rkey,
    };

    ret = ibv_post_send(qp, &send_wr, &bad_send_wr);
    return ret;
}

int wait_poll()
{
    struct ibv_wc wc;
    int result;
    do {
        result = ibv_poll_cq(ib_res.cq, 1, &wc);
    } while (0);
    printf("%d\n", result);
    if (result < 0 || wc.status != IBV_WC_SUCCESS) {
        printf("%d\n", result);
        die("Wait Failed", 1);
    }
    return result;
}

#endif /* _SETUP_IB */
