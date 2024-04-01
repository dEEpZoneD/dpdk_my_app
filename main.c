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
// #include <rte_prefetch.h>
// #include <rte_malloc.h>
// #include <rte_cycles.h>

static volatile bool force_quit;

// Ports set in promiscuous mode on by default
static bool promiscuous_on = true;

#define MEMPOOL_CACHE_SIZE 256
#define BURST_SIZE 32  // Number of packets to retrieve per burst

static struct rte_ether_addr port_eth_addr;

#define RX_DESC_DEFAULT 1024
#define TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = RX_DESC_DEFAULT;
static uint16_t nb_txd = TX_DESC_DEFAULT;

// static unsigned int l2fwd_rx_queue_per_lcore = 1; maybe

// #define MAX_RX_QUEUE_PER_LCORE 16
// #define MAX_TX_QUEUE_PER_PORT 16
/* List of queues to be polled for a given lcore. 8< */
// struct lcore_queue_conf {     maybe
// 	unsigned n_rx_port;
// 	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
// } __rte_cache_aligned;
// struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];


struct rte_eth_rxconf rxq_conf;
struct rte_eth_txconf txq_conf;
struct rte_eth_dev_info dev_info;

struct rte_eth_conf eth_dev_conf= {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_NONE,
	},
    .txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
};

static int main_loop() {
    struct rte_mbuf *rx_pkts[BURST_SIZE];
    unsigned port_id = 0;

    printf("Entering main loop on lcore: %u.\n", rte_lcore_id());

    while (!force_quit) {
        // Retrieve packets from the receive queue
        unsigned nb_burst = rte_eth_rx_burst(port_id, 0, rx_pkts, BURST_SIZE);
        // Process received packets
        for (unsigned i = 0; i < nb_burst; i++) {
            struct rte_mbuf *mbuf = rx_pkts[i];
            
        }
    }
    return 0;
}

struct rte_mempool * print_pktmbuf_pool = NULL;

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
    unsigned int nb_lcores = 0;
    unsigned lcore_id, rx_lcore_id;
    // struct lcore_queue_conf *qconf;
    // maybe
    // lcore_id = rte_lcore_id();
	// qconf = &lcore_queue_conf[lcore_id];
	
	/* Init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0){
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	}
	argc -= ret;
	argv += ret;

    force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	
	uint16_t nb_ports = rte_eth_dev_count_avail();
	if (nb_ports < 1) {
		rte_exit(EXIT_FAILURE, "No Ethernet ports available\n");
		return -1;
	}
	
	uint16_t port_id = 0; 
	
	const uint16_t nb_rx_desc = BURST_SIZE;
    ret = rte_eth_dev_is_valid_port(port_id);

    rx_lcore_id = 0;
	// qconf = NULL;

    nb_mbufs = RTE_MAX(nb_ports * (nb_rxd + nb_txd + BURST_SIZE +
		nb_lcores * MEMPOOL_CACHE_SIZE), 8192U);

	/* Create the mbuf pool. 8< */
	print_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
		MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
		rte_socket_id());
	if (print_pktmbuf_pool == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
    }



    ret = rte_eth_dev_info_get(port_id, &dev_info);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Error during getting device info\n");
    }

    ret = rte_eth_dev_configure(port_id, 1, 1, &eth_dev_conf);
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
    ret = rte_eth_rx_queue_setup(port_id, 0, nb_rxd,
                        rte_eth_dev_socket_id(port_id),
                        &rxq_conf,
                        print_pktmbuf_pool);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup failed\n");
    }

    fflush(stdout);

    ret = rte_eth_dev_set_ptypes(port_id, RTE_PTYPE_UNKNOWN, NULL, 0);
    if (ret < 0)
        printf("Port: Failed to disable Ptype parsing\n");

    ret = rte_eth_dev_start(port_id);
    // if (ret < 0)
    //     rte_exit(EXIT_FAILURE, "rte_eth_dev_start failed\n");
    //     return -1;

    if (promiscuous_on) {
        ret = rte_eth_promiscuous_enable(port_id);
    }
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "rte_eth_promiscuous_enable failed\n");
    }

    rte_eal_mp_remote_launch(main_loop, NULL, CALL_MAIN);
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0) {
			ret = -1;
			break;
		}
	}

	printf("Success mfs\n");
    //int rte_eth_rx_queue_setup (port_id, uint16_t rx_queue_id, uint16_t nb_rx_desc, unsigned int socket_id, const struct rte_eth_rxconf *rx_conf, struct rte_mempool *mb_pool);
    return 0;
	
    // uint16_t port_id = 0;  // Use the first port by defaukt

    // // Port configuration
    // const uint16_t nb_rx_desc = BURST_SIZE; // Number of receive descriptors
    // const uint16_t nb_tx_desc = BURST_SIZE; // Number of transmit descriptors (not used in this example)
    // struct rte_eth_conf dev_conf = RTE_ETH_CONF_DEFAULT();
    // dev_conf.rxmode.mq_mode = RTE_ETH_MQ_MODE_NONE; // No multi-queue mode
    // ret = rte_eth_dev_configure(port_id, nb_rx_desc, nb_tx_desc, &dev_conf);
    // if (ret < 0) {
    //     RTE_LOG(RTE_LOG_ERR, "Error: rte_eth_dev_configure failed: %d\n", ret);
    //     return -1;
    // }

    // // Receive queue setup
    // ret = rte_eth_rx_queue_setup(port_id, 0, nb_rx_desc, 0, NULL, NULL);
    // if (ret < 0) {
    //     RTE_LOG(RTE_LOG_ERR, "Error: rte_eth_rx_queue_setup failed: %d\n", ret);
    //     return -1;
    // }

    // // Start the port
    // ret = rte_eth_dev_start(port_id);
    // if (ret < 0) {
    //     RTE_LOG(RTE_LOG_ERR, "Error: rte_eth_dev_start failed: %d\n", ret);
    //     return -1;
    // }

    // // Main loop to poll for packets
    // struct rte_mbuf *rx_pkts[BURST_SIZE];
    // uint64_t prev_tsc = rte_rdtsc(); // Timestamp for rate calculation
    // uint32_t nb_rx = 0;
    // while (force_quit) {
    //     // Retrieve packets from the receive queue
    //     unsigned nb_burst = rte_eth_rx_burst(port_id, 0, rx_pkts, BURST_SIZE);

    //     // Process received packets
    //     for (unsigned i = 0; i < nb_burst; i++) {
    //         struct rte_mbuf *mbuf = rx_pkts[i];
    //         const void *data = rte_pktmbuf_get_contiguous_data(mbuf, 0); // Get packet data pointer

    //         // Print packet data (replace with your processing logic)
    //         printf("Packet %u: Length %u, Data: %.*s\n", nb_rx, mbuf->pkt_len,
    //                (int)(mbuf->data_len - mbuf->data_off), (const char *)data);

    //         // Free the packet (optional, packets might be recycled)
    //         rte_pktmbuf_free(mbuf);
    //         nb_rx++;
    //     }

    //     // Calculate and print packet receive rate (optional)
    //     uint64_t curr_tsc = rte_rdtsc();
    //     uint64_t diff_tsc = curr_tsc - prev_tsc;
    //     if (diff_tsc > (RTE_GET_TSC_FREQ() * 5)) { // Print rate every 5 seconds
    //         double hz = RTE_GET_TSC_FREQ();
    //         printf("Packets received: %u, Rate: %.2);
    //     }
    // }
}
