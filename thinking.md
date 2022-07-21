# Lab11: Network

## your job

1. 填写函数e1000_transmit、e1000_recv：

    ```c
    int
    e1000_transmit(struct mbuf *m)
    {
        uint32 index;

        acquire(&e1000_lock);
        index = regs[E1000_TDT];

        if(!(tx_ring[index].status&E1000_TXD_STAT_DD)) {
            release(&e1000_lock);
            return -1;
        }

        if(tx_mbufs[index])
            mbuffree(tx_mbufs[index]);

        tx_mbufs[index] = m;
        tx_ring[index].addr = (uint64)m->head;
        tx_ring[index].length = m->len;
        tx_ring[index].cmd = E1000_TXD_CMD_EOP|E1000_TXD_CMD_EOP;

        regs[E1000_TDT] = (index+1) % TX_RING_SIZE;
        release(&e1000_lock);

        return 0;
    }

    static void
    e1000_recv(void)
    {
        uint32 index;

        while(1) {
            index = (regs[E1000_RDT]+1) % RX_RING_SIZE;
        
            if(!(rx_ring[index].status&E1000_TXD_STAT_DD))
                break;

            rx_mbufs[index]->len = rx_ring[index].length;
            net_rx(rx_mbufs[index]);
            rx_mbufs[index] = mbufalloc(0);
            rx_ring[index].addr = (uint64)rx_mbufs[index]->head;
            rx_ring[index].status = 0;

            regs[E1000_RDT] = index;
        }
    }
    ```
