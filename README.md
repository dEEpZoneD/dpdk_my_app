# dpdk_print_pkt

Uses DPDK to poll a NIC port for packets and print the packets.

## Download and complie DPDK in your system.

Reference: [DPDK Documentation](https://doc.dpdk.org/guides/index.html)

## Running the application

- Clone the repo: `git clone https://github.com/dEEpZoneD/dpdk_print_pkt.git`
- Run the following commands inside the cloned repo
`make`
`sudo ./build/print_pkt -l 0 -n 1 --vdev=net_af_packet0, iface=lo`