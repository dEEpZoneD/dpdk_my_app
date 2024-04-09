#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_launch.h>
#include <rte_lcore.h>

static volatile bool force_quit;

static bool promiscuous_on = true;

#define MEMPOOL_CACHE_SIZE 256
#define BURST_SIZE 32

static struct rte_ether_addr port_eth_addr;

#define RX_DESC_DEFAULT 1024
#define TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = RX_DESC_DEFAULT;
static uint16_t nb_txd = TX_DESC_DEFAULT;

struct rte_eth_rxconf rxq_conf;
struct rte_eth_txconf txq_conf;
struct rte_eth_dev_info dev_info;

// struct rte_eth_rss_conf dev_rss_conf;

struct rte_eth_conf eth_dev_conf= {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_NONE,
	},
    .rx_adv_conf = {
        .rss_conf = {
            .rss_key = NULL,
            .rss_hf = 0x0,
        }
    },
    .txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
};

struct rte_mempool * print_pktmbuf_pool = NULL;
unsigned int mbufs_avail;
unsigned int mbufs_inuse;
uint16_t  queue_id[RTE_MAX_LCORE][RTE_MAX_ETHPORTS];

// struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static int main_loop() {
    struct rte_mbuf *rx_pkts[BURST_SIZE];
    unsigned port_id = 0;
    unsigned lcore_id = rte_lcore_id();

    printf("Entering main loop on lcore: %u.\n", lcore_id);
    fflush(stdout);

    struct rte_ether_hdr *eth_hdr;

    while (!force_quit) {
        // Retrieve packets from the receive queue
        RTE_ETH_FOREACH_DEV(port_id){
            unsigned nb_burst = rte_eth_rx_burst(port_id, queue_id[lcore_id][port_id], rx_pkts, BURST_SIZE);
            int pkt_len;
            
            // Process received packets
            
            for (unsigned i = 0; i < nb_burst; i++) {
                struct rte_mbuf *mbuf = rx_pkts[i];
                eth_hdr = rte_pktmbuf_mtod(mbuf ,struct rte_ether_hdr*);
                pkt_len = rte_pktmbuf_pkt_len(mbuf);
                rte_prefetch0(rte_pktmbuf_mtod(mbuf, char *));
                printf("\n\nGot a packet from port: %u lcore: %u Queue: %u\n", port_id, lcore_id, queue_id[lcore_id][port_id]);

                printf("Source MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    eth_hdr->src_addr.addr_bytes[0], eth_hdr->src_addr.addr_bytes[1],
                    eth_hdr->src_addr.addr_bytes[2], eth_hdr->src_addr.addr_bytes[3],
                    eth_hdr->src_addr.addr_bytes[4], eth_hdr->src_addr.addr_bytes[5]);
                printf("Destination MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    eth_hdr->dst_addr.addr_bytes[0], eth_hdr->dst_addr.addr_bytes[1],
                    eth_hdr->dst_addr.addr_bytes[2], eth_hdr->dst_addr.addr_bytes[3],
                    eth_hdr->dst_addr.addr_bytes[4], eth_hdr->dst_addr.addr_bytes[5]);
                printf("Ether type: %04X\n", rte_be_to_cpu_16(eth_hdr->ether_type));

                rte_pktmbuf_dump(stdout, mbuf, pkt_len);
                //rte_pktmbuf_free(mbuf);
                mbufs_inuse = rte_mempool_in_use_count(print_pktmbuf_pool);
                printf("\nmbufs in use: %u out of %u\n", mbufs_inuse, mbufs_avail);
                fflush(stdout);
            }
        }
    }
    return 0;
}

static void signal_handler(int signum) {
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

int main(int argc, char **argv) {
	int ret = 0;
    unsigned int nb_mbufs;
    unsigned int nb_lcores;
    uint16_t nb_ports;
    unsigned lcore_id;    
	
	/* Init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0){
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	}
	argc -= ret;
	argv += ret;

    nb_lcores = rte_lcore_count();
    

    force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	
	 nb_ports = rte_eth_dev_count_avail();
	if (nb_ports < 1) {
		rte_exit(EXIT_FAILURE, "No Ethernet ports available\n");
		return -1;
	}
	
    nb_mbufs = RTE_MAX(nb_ports * (nb_rxd + nb_txd + BURST_SIZE + nb_lcores * MEMPOOL_CACHE_SIZE), 8192U);

    /* Create the mbuf pool. 8< */
	print_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
		MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
		rte_socket_id());
	if (print_pktmbuf_pool == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
    }
    mbufs_avail = rte_mempool_avail_count(print_pktmbuf_pool);

    printf("Total mbufs available: %u\n", mbufs_avail);

    unsigned port_id = 0;
    RTE_ETH_FOREACH_DEV(port_id) {
        ret = rte_eth_dev_is_valid_port(port_id);

        ret = rte_eth_dev_info_get(port_id, &dev_info);
        if (ret < 0) {
            rte_exit(EXIT_FAILURE, "Error during getting device info\n");
        }

        int nb_rx_queues = nb_lcores;

        ret = rte_eth_dev_configure(port_id, nb_rx_queues, 1, &eth_dev_conf);
        if (ret < 0) {
            rte_exit(EXIT_FAILURE, "ETH Devive Configuration Failed\n");
            return -1;
        }

        ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rxd, &nb_txd);
        if (ret < 0) {
            rte_exit(EXIT_FAILURE, "Could not adjust no. of descriptors\n");
            return -1;
        }

        ret = rte_eth_macaddr_get(port_id, &port_eth_addr);
        if (ret < 0) {
            rte_exit(EXIT_FAILURE, "Could not get MAC Address\n");
            return -1;
        }

        fflush(stdout);

        rxq_conf = dev_info.default_rxconf;
        rxq_conf.offloads = eth_dev_conf.rxmode.offloads;
        
        /* RX queue setup. 8< */
        uint16_t  i = 0;
        RTE_LCORE_FOREACH(lcore_id) {
            queue_id[lcore_id][port_id] = i;
            i++;
            printf("Configuring queue: %u for port : %u\n", queue_id[lcore_id][port_id], port_id);
            ret = rte_eth_rx_queue_setup(port_id, queue_id[lcore_id][port_id], nb_rxd,
                                rte_eth_dev_socket_id(port_id),
                                &rxq_conf,
                                print_pktmbuf_pool);
        }

        if (ret < 0) {
            rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup failed\n");
        }

        fflush(stdout);

        ret = rte_eth_dev_set_ptypes(port_id, RTE_PTYPE_UNKNOWN, NULL, 0);
        if (ret < 0)
            printf("Port: Failed to disable Ptype parsing\n");

        ret = rte_eth_dev_start(port_id);

        if (promiscuous_on) {
            ret = rte_eth_promiscuous_enable(port_id);
        }
        if (ret < 0) {
            printf("Port: rte_eth_promiscuous_enable failed\n");
        }
    }

    ret = 0;
    rte_eal_mp_remote_launch(main_loop, NULL, CALL_MAIN);
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0) {
			ret = -1;
			break;
		}
	}

    RTE_ETH_FOREACH_DEV(port_id) {
        printf("Closing port %d...", port_id);
        ret = rte_eth_dev_stop(port_id);
        if (ret != 0) {
            printf("rte_eth_dev_stop failed: err=%d, port=%d\n", ret, port_id);
        }
        rte_eth_dev_close(port_id);
        printf(" Done\n");
    }

    rte_eal_cleanup();

	printf("Success\n");
    return 0;
}
