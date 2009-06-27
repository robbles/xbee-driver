#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

#include <linux/sched.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/interrupt.h> /* mark_bh */
#include <linux/tty.h>


#include <linux/in.h>
#include <linux/netdevice.h>   /* struct device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <linux/tcp.h>         /* struct tcphdr */
#include <linux/skbuff.h>
#include <linux/udp.h>
#include <linux/ip.h>
#include <net/checksum.h>

#include "n_turk.h"

/*
**************Network functions************
*/

    
/*
 * Open and close
 */

int xbee_open(struct net_device *dev)
{
	printk(KERN_ALERT "[NET open called ]\n");
	
	
	/* request_region(), request_irq(), ....  (like fops->open) */

	/* 
	* Assign the hardware address of the board: use "\0XBEEx", where
	* x is 0 or 1. The first byte is '\0' to avoid being a multicast
	* address (the first byte of multicast addrs is odd).
	*/
    //TODO: This should be set to the actual Xbee address somehow, even though it's never used
	memcpy(dev->dev_addr, "\0XBEE111", 8);
	memcpy(dev->perm_addr, "\0XBEE111", 8);
	netif_start_queue(dev);
	return 0;
}

int xbee_release(struct net_device *dev)
{
	printk(KERN_ALERT "[NET release called ]\n");
	
	/* release ports, irq and such -- like fops->close */

	netif_stop_queue(dev); /* can't transmit any more */
	return 0;
}

/*
 * Configuration changes (passed on by ifconfig)
 */
int xbee_config(struct net_device *dev, struct ifmap *map)
{
	//printk(KERN_ALERT "[NET config called ]\n");
	
	if (dev->flags & IFF_UP) /* can't act on a running interface */
		return -EBUSY;

	/* Don't allow changing the I/O address */
	if (map->base_addr != dev->base_addr) {
		printk(KERN_WARNING "xbee: Can't change I/O address\n");
		return -EOPNOTSUPP;
	}

	/* Allow changing the IRQ */
	if (map->irq != dev->irq) {
		dev->irq = map->irq;
		/* request_irq() is delayed to open-time */
	}

	/* ignore other fields */
	return 0;
}


/*
 * Encapsulate a packet with UDP/IP and hand off to upper networking layers
 */
void xbee_rx(struct net_device *dev, unsigned char *data, int len, u16 udp_port) {

    int i;
	struct xbee_priv *priv = netdev_priv(dev);

	//printk(KERN_ALERT "[NET] Encapsulating packet: length %d\n", len);

	struct sk_buff *skb;

	skb = dev_alloc_skb(len + sizeof(struct udphdr) + sizeof(struct iphdr));
	if (!skb) {
		if (printk_ratelimit())
			printk(KERN_NOTICE "[NET] xbee rx: low on mem - packet dropped\n");
		priv->stats.rx_dropped++;
		return;
	}
	skb_reserve (skb, sizeof(struct iphdr) + sizeof(struct udphdr));
	
	// Put all the data into a new socket buffer
	memcpy(skb_put(skb, len), data, len);
	
	// Make a udp header and stick it on, if it isn't a normal TURK packet
	if(udp_port) {
        struct udphdr *uh = (struct udphdr*)skb_push(skb, sizeof(struct udphdr));
        uh->source = udp_port;
        uh->dest = udp_port;
        uh->len = htons(len + 8);
        uh->check = 0x00;
        skb_reset_transport_header(skb);
    }

    //Normally add just a IP header, since TURK packets already include UDP
	struct iphdr *ih = (struct iphdr*)skb_push(skb, sizeof(struct iphdr));
	ih->version = 4;
	ih->ihl = 5;
	ih->tos = 0;
	ih->tot_len = htons(len + sizeof(struct iphdr));
	ih->id = 555;
	ih->frag_off = 0;
	ih->ttl = 64;
	ih->protocol = 17;
	ih->check = 0x0000;
	ih->saddr = 0x00000000L;
	ih->daddr = 0xFFFFFFFFL;
	skb_reset_network_header(skb);
	
	ih->check = ip_fast_csum((unsigned char *)ih, ih->ihl);
	
	skb->dev = dev;
	skb->protocol = ETH_P_IP;
	skb->ip_summed = CHECKSUM_UNNECESSARY; // don't check it (does this make any difference?)
	
	int packet_stat = netif_rx(skb);
	
	if(packet_stat == NET_RX_SUCCESS) {
		printk(KERN_ALERT "[NET] Packet received successfully\n");
	} else if(packet_stat == NET_RX_DROP) {
		printk(KERN_ALERT "[NET] Packet was dropped!\n");
	}
	
	priv->stats.rx_packets++;
	priv->stats.rx_bytes += len;

}




/*
 * Receive a packet from device and send data to the right place
 */
void xbee_receive_packet(struct net_device *dev, unsigned char *data, int len)
{
	
	struct xbee_priv *priv = netdev_priv(dev);

//	printk(KERN_ALERT "[XBEE] receive_packet called ]\n");

	switch(*data) {
		case ZIGBEE_RECEIVE_PACKET:
			printk(KERN_ALERT "[XBEE] Zigbee Receive Packet Frame received\n");
			if(len<13) {
				printk(KERN_ALERT "[XBEE] Packet too short to be a Receive Packet Frame!\n");
				goto out;
			}
            //Shorter than XBEE framing bytes + UDP header
			if(len<21) {
				printk(KERN_ALERT "[XBEE] Not a valid TURK Packet!\n");
				goto out;
			}

			xbee_rx(dev, (data + 12), len - 12, 0);
			goto out;

		case ZIGBEE_TRANSMIT_STATUS:
			//printk(KERN_ALERT "[XBEE] Zigbee Transmit Status Frame received\n");
			if(len != 7) {
				printk(KERN_ALERT "[XBEE] Packet is wrong length for a Transmit Status Packet!\n");
				goto out;
			}

			if(data[5] == TRANSMIT_SUCCESS) {
				printk(KERN_ALERT "[XBEE] Packet delivery reported as success!\n");
                //TODO: report which packet by reading message (and notify super-paranoid drivers?)
			} else {
                if(data[5] == TRANSMIT_INVALID) {
                    printk(KERN_ALERT "[XBEE] Transmit failed: invalid endpoint\n");
                } else if(data[5] == TRANSMIT_ADDRNOTFOUND) {
                    printk(KERN_ALERT "[XBEE] Transmit failed: address not found\n");
                } else {
                    printk(KERN_ALERT "[XBEE] Packet delivery failed with status %d\n", data[5]);
                }
			}

			//Send it to port 50100, some other process can deal with it...
			xbee_rx(dev, data, len, 50100);
			goto out;

		case NODE_IDENTIFICATION_INDICATOR:
			printk(KERN_ALERT "[XBEE] NODE_IDENTIFICATION_INDICATOR Frame received\n");
			break;
		case MODEM_STATUS:
			printk(KERN_ALERT "[XBEE] Zigbee Modem Status Frame received\n");
			break;
		case AT_COMMAND:
			printk(KERN_ALERT "[XBEE] Zigbee AT_COMMAND Frame received\n");
			break;
		case AT_COMMAND_QUEUE_PARAMETER_VALUE:
			printk(KERN_ALERT "[XBEE] AT_COMMAND_QUEUE_PARAMETER_VALUE Frame received\n");
			break;
		case AT_COMMAND_RESPONSE:
			printk(KERN_ALERT "[XBEE] AT_COMMAND_RESPONSE Frame received\n");
			break;
		case REMOTE_COMMAND_REQUEST:
			printk(KERN_ALERT "[XBEE] REMOTE_COMMAND_REQUEST Frame received\n");
			break;
		case REMOTE_COMMAND_RESPONSE:
			printk(KERN_ALERT "[XBEE] REMOTE_COMMAND_RESPONSE Frame received\n");
			break;
		case ZIGBEE_TRANSMIT_REQUEST:
			printk(KERN_ALERT "[XBEE] ZIGBEE_TRANSMIT_REQUEST Frame received\n");
			break;
		case EXPLICIT_ADDRESSING_COMMAND_FRAME:
			printk(KERN_ALERT "[XBEE] EXPLICIT_ADDRESSING_COMMAND_FRAME received\n");
			break;
		case ZIGBEE_EXPLICIT_RX_INDICATOR:
			printk(KERN_ALERT "[XBEE] ZIGBEE_EXPLICIT_RX_INDICATOR Frame received\n");
			break;
		case ZIGBEE_IO_DATA_SAMPLE_RX_INDICATOR:
			printk(KERN_ALERT "[XBEE] ZIGBEE_IO_DATA_SAMPLE_RX_INDICATOR Frame received\n");
			break;
		case XBEE_SENSOR_READ_INDICATOR:
			printk(KERN_ALERT "[XBEE] XBEE_SENSOR_READ_INDICATOR Frame received\n");
			break;
		default:
		printk(KERN_ALERT "[XBEE] UNKNOWN Frame Received : errors %lu\n", priv->stats.rx_dropped++);
	}
	
	//Send unhandled packets to port 50100 for logging or something...
	xbee_rx(dev, data, len, 50100);

	out:
	  return;
}
    




/*
 * Adds in the checksum and sends the packet off to the serial driver.
 */
static void xbee_hw_tx(char *frame, int len, struct net_device *dev)
{
	
	int i;
	unsigned char checksum;
	int actual;
	
	struct xbee_priv *priv = netdev_priv(dev);
	
	//printk(KERN_ALERT "[NET hardware_tx called, %d bytes ]\n", len);
	
	//Add start delimiter
	*frame = 0x7E;
	
	//Checksum
	checksum = 0;
	for(i=3; i<(len-1); i++) {
		if(frame[i] == 0x7D) {
			i++;
			checksum += (unsigned char) frame[i] ^ 0x20;
		} else {
			checksum += ((unsigned char)frame[i]);
		}
	}
	checksum = 0xFF - checksum;
	if (checksum == 0x7E || checksum == 0x7D || checksum == 0x11 || checksum == 0x13) {
		frame[len-1] = 0x7D;
		frame[len] = checksum ^ 0x20;
		len++;
	} else {
		frame[len-1] = checksum;
	}
	
	//Send the data to serial driver
	if(main_tty != NULL) {
			
		//printk(KERN_ALERT "Setting DO_WRITE_WAKEUP\n");
		main_tty->flags |= (1 << TTY_DO_WRITE_WAKEUP);
		
		//printk(KERN_ALERT "Writing the data to tty...\n");
		actual = main_tty->driver->write(main_tty, frame, len);
		
		if(actual != len) { 
			printk(KERN_ALERT "Write failed!\n");
		}
	} else {
		printk(KERN_ALERT "No tty is attached!\n");
	}
	
	//printk(KERN_ALERT "Freeing the frame memory\n");
	kfree(frame);
}

/*
 * Copies bytes from one array into another, escaping them if necessary.
 * Returns the size of the new array, including character escapes.
 * The destination array MUST be at least twice the size of the source
 */
static int escape_into(char *dest, const void *srcarray, int len) {
	int i, j=0;
	
	const char *src = srcarray;
	
	for(i=0; i<len; i++) {
		char unesc = *(src + i);
		if( unesc == 0x7D || unesc == 0x7E || unesc == 0x11 || unesc == 0x13) {
			dest[j] = 0x7D;
			dest[j+1] = unesc ^ 0x20;
			j+=2;
		} else {
			dest[j] = unesc;
			j++;
		}	
	}
	return j;
}

/*
 * Called by the networking layers to transmit a new packet.
 * Constructs the XBee packet and hands off to xbee_hw_tx to do 
 * the checksum and serial transmission.
 */
int xbee_tx(struct sk_buff *skb, struct net_device *dev)
{
	
	int framelen, datalen;
	char *frame;
	
	struct xbee_priv *priv = netdev_priv(dev);
	
	struct xbee_tx_header header = {
		.length = 0x00,
		.api_id = 0x10,
		.frame_id = 0x01,
		.radius = 0x00,
		.options = 0x00
	};
		
	if(main_tty == NULL) {
		printk(KERN_ALERT "No tty attached!\nMake sure ldisc_daemon is running\n");
		return 0;
	}
		
	//printk(KERN_ALERT "[NET tx called, %d bytes in sk_buff]\n", skb->len);
    
    /* If this is a non-UDP/IP packet, drop it
       Also needs to have the XBee HW address in first 8 bytes */
	if(skb->len < (sizeof(struct iphdr) + sizeof(struct udphdr) + 8)) {
        printk(KERN_ALERT "ERROR: Packet is too short to be TURK protocol!\n");
		return 0;
    }
	
	header.address = cpu_to_be64(*((u64*)(skb->data + PACKET_ADDR_OFFSET)));
	header.net_address = cpu_to_be16(0xFFFE);
    header.length = cpu_to_be16(skb->len - PACKET_DATA_OFFSET + 6);
    datalen = skb->len - PACKET_ADDR_OFFSET - 8;

	// Allocate buffer to hold entire serial frame, including start byte and checksum
	frame = kmalloc(sizeof(struct xbee_tx_header) + (2*datalen) + 2, GFP_KERNEL);

	/* Assemble the frame */
    /* Escaping the XBee header */
    //printk(KERN_ALERT "Adding XBee header- length is %u\n", sizeof(header) + 1);
	framelen = 1 + escape_into((frame + 1), &header, sizeof(header));
    
    /* Escaping the UDP header */
    //printk(KERN_ALERT "Adding UDP header - length is %u\n", sizeof(struct udphdr));
	framelen += escape_into((frame + framelen), (skb->data + PACKET_DATA_OFFSET), sizeof(struct udphdr));
	
    /* Escaping the data (not including the hardware address */
    //printk(KERN_ALERT "Adding data - length is %u\n", datalen);
	framelen += escape_into((frame + framelen), (skb->data + PACKET_ADDR_OFFSET + 8), datalen);

	framelen++;
	
	/* save the timestamp */
	dev->trans_start = jiffies; 
	
	//Stop the interface from sending more data, get the spinlock, and send it!
	netif_stop_queue(dev);
	
	spin_lock(&priv->lock);
	
	xbee_hw_tx(frame, framelen, dev);
	
	//Free the skbuff, we've copied all the data out
	dev_kfree_skb(skb);
	
	spin_unlock(&priv->lock);
	
	return 0; 
}


/*
 * Ioctl commands 
 */
int xbee_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	printk(KERN_ALERT "[NET ioctl called, command is %u ]\n", cmd);
	return 0;
}

/*
 * Return statistics to the caller
 */
struct net_device_stats *xbee_stats(struct net_device *dev)
{
	
	struct xbee_priv *priv = netdev_priv(dev);
	
	//printk(KERN_ALERT "[NET device_stats called ]\n");

	return &priv->stats;
}




/*
 * Change the MAC address
 */
static int xbee_mac_addr(struct net_device *dev, void *p) {
	
	struct sockaddr *addr = p;

	//printk(KERN_ALERT "[NET mac_addr called ]\n");

	if (netif_running(dev))
		return -EBUSY;
	
	//TODO: Check if it's a valid address ...somehow...
	
	memcpy(dev->dev_addr, addr->sa_data, 8);
	
	return 0;
}
	

static int xbee_validate_addr(struct net_device *dev) {
//	printk(KERN_ALERT "[NET validate_addr called, addr_len=%u\n", dev->addr_len);
	return 0;
}

/*
 * Initialize the net_device structure
 */
void xbee_init(struct net_device *dev)
{
	struct xbee_priv *priv;

	dev->set_mac_address 	= xbee_mac_addr;
	dev->validate_addr	= xbee_validate_addr;
	
	dev->mtu		= XBEE_DATA_MTU;
	dev->addr_len		= 8;
	dev->tx_queue_len	= 1000;	

	memset(dev->broadcast, 0xFF, 8);

	dev->open            = xbee_open;
	
	dev->stop            = xbee_release;
	dev->hard_start_xmit = xbee_tx;
	dev->do_ioctl        = xbee_ioctl;
	dev->get_stats       = xbee_stats;
	
#ifdef XBEE_CHECK_TRANSMIT
	dev->tx_timeout      = xbee_tx_timeout;
	dev->watchdog_timeo	= 20*HZ;
#endif
	
	/* keep the default flags, just add NOARP */
	dev->flags           |= IFF_NOARP;
	dev->features        |= NETIF_F_NO_CSUM;

	/*
	* Then, initialize the priv field. This encloses the statistics
	* and a few private fields.
	*/
	priv = netdev_priv(dev);
	memset(priv, 0, sizeof(struct xbee_priv));
	spin_lock_init(&priv->lock);
}



/*
***************End Network Functions*************
*/




/*
***************LDISC Functions*************
*/


/*
* The following routines are called from above (user space/tty).
*/
static int n_turk_open(struct tty_struct *tty) {
	
	struct xbee_priv *priv = netdev_priv(xbee_dev);
	
	//printk(KERN_ALERT "OPEN CALLED\n");
	
	// Don't allow more than one tty to be attached
	if( main_tty != NULL )
		return -EPERM;
	
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;
	
	main_tty = tty;
	printk(KERN_ALERT "Attached to a tty!\n\n");
	
	priv->rbuff = kmalloc(XBEE_MAXFRAME, GFP_KERNEL);
	priv->rcount = 0;
	priv->frame_status = UNFRAMED;
	priv->frame_len = 0;
	
	return 0;
}

static void n_turk_close(struct tty_struct *tty) {
	
	struct xbee_priv *priv = netdev_priv(xbee_dev);
	
	printk(KERN_ALERT "CLOSE CALLED\n");

	module_put(THIS_MODULE);
	
	main_tty = NULL;
	
	kfree(priv->rbuff);
	
}

static void n_turk_flush_buffer(struct tty_struct *tty) {
	//printk(KERN_ALERT "FLUSH_BUFFER CALLED\n");

}

static ssize_t	n_turk_chars_in_buffer(struct tty_struct *tty) {
	//printk(KERN_ALERT "CHARS_IN_BUFFER CALLED\n");

	return 0;
}

static int n_turk_ioctl(struct tty_struct * tty, struct file * file, unsigned int cmd, unsigned long arg) {
	printk(KERN_ALERT "IOCTL CALLED\n");

	return -EPERM;
}


static unsigned int n_turk_poll(struct tty_struct *tty, struct file *file, struct poll_table_struct *poll) {
	printk(KERN_ALERT "POLL CALLED\n");

	return 0;
}
	
/*
* The following routines are called from below (serial port driver).
*/




static char checksum_validate(unsigned char *buffer, int len, unsigned char checksum) {
	int i = 0;
	while(len--) {
		checksum += buffer[i++];
	}
	if(checksum == 0xFF)
		return true;
	printk(KERN_ALERT "[XBEE] CSUM should be 0xFF, is 0x%x\n", checksum);
	return false;
}

/*
 * Called from the serial driver when new received data is ready
 */
static void	n_turk_receive_buf(struct tty_struct *tty, const unsigned char *cp, char *fp, int count) {
	
	unsigned char temp;
	printk(KERN_ALERT "RECEIVE_BUF CALLED  %d chars received\n", count);

	struct xbee_priv *priv = netdev_priv(xbee_dev);
	unsigned char checksum;
	
	while(count--) {
		
		temp = *cp++;

		//Escape characters after 0x7D byte
		if(temp == 0x7D) {
			temp = (*cp++) ^ 0x20;
		}
		
		//Start a new frame if 0x7E received, even if we seem to be in the middle of one
		if(temp == 0x7E) {
			priv->frame_status = FRAME_LEN_HIGH;
			priv->frame_len = 0;
			priv->rcount = 0;
			continue;
		}

		
		switch(priv->frame_status) {

            //FIXME: All the Xbee packets seem to have two extra bytes on the end. why...?
			case UNFRAMED:
				//printk(KERN_ALERT "Unframed data received: 0x%x\n", temp);
				break;
				
			case FRAME_LEN_HIGH:
				priv->frame_len = (temp) << 8;
				priv->frame_status = FRAME_LEN_LOW;
				break;
				
			case FRAME_LEN_LOW:
				priv->frame_len |= (temp);
				priv->frame_status = FRAME_DATA;
				priv->rcount = 0;
				break;
				
			case FRAME_DATA:
				//Add to the frame buffer
				priv->rbuff[priv->rcount++] = (temp);
				
				//Is the frame done?
				if(priv->rcount == (priv->frame_len)) {
					priv->frame_status = FRAME_CHECKSUM;
				} 
				//Is the frame FUBAR?
				if(priv->rcount > XBEE_MAXFRAME || priv->rcount > priv->frame_len || priv->frame_len > XBEE_MAXFRAME) {
					priv->rcount = 0;
					priv->frame_status = UNFRAMED;
					printk(KERN_ALERT "[XBEE] A frame got pwned!! : %lu errors\n", priv->stats.rx_dropped++);
				} 
				break;
				
			case FRAME_CHECKSUM:
				checksum = temp;
				if(checksum_validate(priv->rbuff, priv->frame_len, checksum)) {
					
					//Hand off to xbee_receive_packet for networking/packet ID
					xbee_receive_packet(xbee_dev, priv->rbuff, priv->frame_len);
				} else {
					printk(KERN_ALERT "[XBEE] CHECKSUM FAIL : %lu errors\n", priv->stats.rx_dropped++);
				}
				priv->frame_status = UNFRAMED;
				priv->frame_len = 0;
				priv->rcount = 0;

				break;
				
			default:
				printk(KERN_ALERT "[XBEE] WARNING - Undefined Frame Status!!\n");
				priv->frame_status = UNFRAMED;
		}
		
		
	}
	
}

static void	n_turk_write_wakeup(struct tty_struct *tty) {

	main_tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
	
	netif_wake_queue(xbee_dev);
}


struct tty_ldisc n_turk_ldisc = {
	.magic           = TTY_LDISC_MAGIC,
	.name            = "n_turk",
	.open            = n_turk_open, /* TTY */
	.close           = n_turk_close, /* TTY */
	.flush_buffer    = n_turk_flush_buffer, /* TTY */
	.chars_in_buffer = n_turk_chars_in_buffer, /* TTY */
	.read            = NULL, /* TTY */
	.write           = NULL, /* TTY */
	.ioctl           = n_turk_ioctl, /* TTY */
	.set_termios     = NULL, /* FIXME no support for termios setting yet */
	.poll            = n_turk_poll,
	.receive_buf     = n_turk_receive_buf, /* Serial driver */
	.write_wakeup    = n_turk_write_wakeup /* Serial driver */
};



/*
*************** End of LDISC Functions*************
*/



void xbee_cleanup(void)
{
	
	if (xbee_dev) {
		unregister_netdev(xbee_dev);
		
		free_netdev(xbee_dev);
	}
	
	return;
}


int xbee_init_module(void)
{
	int result, ret=0;

	//Register the line discipline
	result = tty_register_ldisc(N_XBEE, &n_turk_ldisc);
	if(result) {
		printk(KERN_ALERT "Registering the line discipline failed with error %d\n", result);
		return result;
	}
	
	/* Allocate the devices */
	xbee_dev = alloc_netdev(sizeof(struct xbee_priv), "zigbee%d",
				     xbee_init);
	
	
	if (xbee_dev == NULL)
		goto out;

	ret = -ENODEV;
	
	if ((result = register_netdev(xbee_dev)))
		printk("xbee: error %i registering device \"%s\"\n", result, xbee_dev->name);
	else
		ret = 0;
	
	
out:
	if (ret) 
		xbee_cleanup();
	return ret;
}





module_init(xbee_init_module);
module_exit(xbee_cleanup);

MODULE_LICENSE("GPL");

