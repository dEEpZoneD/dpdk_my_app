# dpdk_print_pkt

Uses DPDK to poll NIC ports for packets and print the packets.
Uses multi-queue approach and assigns one queue per lcore on every port to avoid locks.

## Download and complie DPDK in your system.

Reference: [DPDK Documentation](https://doc.dpdk.org/guides/index.html)

```
sudo apt-get update && sudo apt-get upgrade -y
sudo apt install gcc g++ build-essential pkg-config python3 python3-pip meson ninja-build libnuma-dev python3-pyelftools
wget https://fast.dpdk.org/rel/dpdk-23.11.tar.xz
tar -xJf dpdk-23.11.tar.xz
cd dpdk-23.11
meson setup build
cd build
ninja
sudo meson install
sudo ldconfig
```

## Running the application

- Clone the repo: `git clone https://github.com/dEEpZoneD/dpdk_print_pkt.git`
- Setup hugepages `echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages`
- Run the following commands inside the cloned repo
`make`
`sudo ./build/print_pkt -l 0-1 -n 2 --vdev=net_af_packet0, iface=lo,qpair=2 --vdev=net_af_packet1, iface=lo,qpair=2`
**Assign as many qpairs as lcores (eg. if `-l 0-5` then `qpairs=6`)**
