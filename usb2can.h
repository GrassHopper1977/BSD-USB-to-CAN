#ifndef __USB2CAN_H__
#define __USB2CAN_H__

// Max payload & DLC definitions accroding to ISO 11898-1
#define CAN_MAX_DLEN	8
#define CAN_MAX_DLC		8 	// Note DLC is NOT the same as DLEN. CANFD uses short codes to define lengths of 

// Special address flags for the CAN_ID
#define CAN_EFF_FLAG	0x80000000U	// EFF/SFF (extended frame format or standard frame format)
#define CAN_RTR_FLAG	0x40000000U	// Remote Transmission Request
#define CAN_ERR_FLAG	0x20000000U	// Error message frame

// Valid bits in the CAN_ID for frame formats
#define CAN_SFF_MASK	0x000007FFU	// Standard Frame Format (SFF)
#define CAN_EFF_MASK	0x1FFFFFFFU	// Extended Frame Format (EFF)
#define CAN_ERR_MASK	0x1FFFFFFFU	// Error Message Frame

#define CAN_ERR_DLC		8  // DLC for error message frames.

// Error class mask in can_id
#define CAN_ERR_TX_TIMEOUT 0x00000001U   // TX timeout (by netdevice driver)
#define CAN_ERR_LOSTARB	   0x00000002U   // lost arbitration    / data[0]   
#define CAN_ERR_CRTL	   0x00000004U   // controller problems / data[1]   
#define CAN_ERR_PROT	   0x00000008U   // protocol violations / data[2..3]
#define CAN_ERR_TRX		   0x00000010U   // transceiver status  / data[4]
#define CAN_ERR_ACK		   0x00000020U   // received no ACK on transmission
#define CAN_ERR_BUSOFF	   0x00000040U   // bus off
#define CAN_ERR_BUSERROR   0x00000080U   // bus error (may flood!)
#define CAN_ERR_RESTARTED  0x00000100U   // controller restarted

// CAN Identifier structure
// bit 0-28	: CAN identifier (11/29 bit)
// bit 29	: error message frame flag (0 = data frame, 1 = error message)
// bit 30	: Remote Transmission Request ( 1 = RTR frame)
// bit 31	: Frame Format Flag (0 = Standard Frame Format, 1 = Extended Frame Format)
typedef uint32_t canid_t;

struct can_frame {
	canid_t	can_id;	// 32-bit CAN_ID + EFF/RTR/ERR flags
	uint8_t	len;	// frame payload length in bytes (0 to CAN_MAX_DLEN)
	uint8_t	__pad;	// padding
	uint8_t	__res0;	// reserved / padding
	uint8_t	__res1;	// reserved / padding
	uint8_t	data[CAN_MAX_DLEN] __attribute__((aligned(8)));
};

#endif