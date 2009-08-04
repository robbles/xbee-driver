#ifndef _N_XBEE_H
#define N_XBEE_H

#include <linux/skbuff.h>

#define XBEE_DATA_MTU 72
//Maximum possible size of serial frame (hopefully)
#define XBEE_MAXFRAME (XBEE_DATA_MTU*2 + 16)

#define N_XBEE 17

int pool_size = 8;

//Frame Types
#define MODEM_STATUS 					0X8A
#define AT_COMMAND 					0X08
#define AT_COMMAND_QUEUE_PARAMETER_VALUE 		0X09
#define AT_COMMAND_RESPONSE 				0X88
#define REMOTE_COMMAND_REQUEST				0X17
#define REMOTE_COMMAND_RESPONSE 			0X97
#define ZIGBEE_TRANSMIT_REQUEST 			0X10
#define EXPLICIT_ADDRESSING_COMMAND_FRAME 		0X11
#define ZIGBEE_TRANSMIT_STATUS 				0X8B
#define ZIGBEE_RECEIVE_PACKET 				0X90
#define ZIGBEE_EXPLICIT_RX_INDICATOR 			0X91
#define ZIGBEE_IO_DATA_SAMPLE_RX_INDICATOR 		0X92
#define XBEE_SENSOR_READ_INDICATOR 			0X94
#define NODE_IDENTIFICATION_INDICATOR 			0X95

//Transmit Status
#define TRANSMIT_SUCCESS 0x00
#define TRANSMIT_CCA_FAILURE 0x02
#define TRANSMIT_INVALID 0x15
#define TRANSMIT_NETACKFAIL 0x21
#define TRANSMIT_NOTJOINED 0x22
#define TRANSMIT_SELFADDRESSED 0x23
#define TRANSMIT_ADDRNOTFOUND 0x24
#define TRANSMIT_ROUTENOTFOUND 0x25

//Packet data offsets for frame construction
#define PACKET_DATA_OFFSET (sizeof(struct iphdr))
#define PACKET_ADDR_OFFSET (sizeof(struct iphdr) + sizeof(struct udphdr))

struct xbee_priv {
	struct net_device_stats stats;
	struct tty_struct *tty;
	
	//Buffer for frame data bytes
	unsigned char *rbuff;
	//Number of received data bytes (after escapes)
	int rcount;
	//Receive state machine variable
	unsigned char frame_status;
#define UNFRAMED 	0	// No Start Delimiter received
#define FRAME_LEN_HIGH 	1	// Ready to receive high byte of length field
#define FRAME_LEN_LOW 	2	// Ready to receive low byte of length field
#define FRAME_DATA 	3	// Receiving the frame data
#define FRAME_DATA_ESC  4
#define FRAME_CHECKSUM	5	// Receiving the checksum byte
	//The length of the frame being received (according to packet)	
	unsigned short frame_len;
	
	//Main read/write lock
	spinlock_t lock;
};

//Outgoing packet data header (not including data)
struct xbee_tx_header {
	u16 length;
	u8 api_id;
	u8 frame_id;
	u64 address;
	u16 net_address;
	u8 radius;
	u8 options;
} __attribute__((packed));


// Only allow the module to be attached to one serial line at a time
struct tty_struct *main_tty;

struct net_device *xbee_dev;




#endif /* N_XBEE_H */

