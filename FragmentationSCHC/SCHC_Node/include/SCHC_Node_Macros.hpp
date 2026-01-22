#ifndef SCHC_Node_Macros_hpp
#define SCHC_Node_Macros_hpp




#define _SESSION_POOL_SIZE 3   // Sessions numbers

#define _LORAWAN_BUFFER_SIZE 300         // buffer size (in bytes) for storing the received lorawan message

/* Fragmentation Rule ID*/
#define SCHC_FRAG_UPDIR_RULE_ID 20
#define SCHC_FRAG_DOWNDIR_RULE_ID 21  

/* Fragmentation traffic direction */
#define SCHC_FRAG_UP 1
#define SCHC_FRAG_DOWN 2

/* L2 Protocols */
#define SCHC_FRAG_LORAWAN 1
#define SCHC_FRAG_NBIOT 2
#define SCHC_FRAG_SIGFOX 3

/* Machine States */
#define STATE_TX_INIT 0
#define STATE_TX_SEND 1
#define STATE_TX_WAIT_x_ACK 2
#define STATE_TX_RESEND_MISSING_FRAG 3
#define STATE_TX_ERROR 4
#define STATE_TX_END 5

/* SCHC Messages */
#define SCHC_REGULAR_FRAGMENT_MSG 0
#define SCHC_ALL1_FRAGMENT_MSG 1
#define SCHC_ACK_MSG 2
#define SCHC_ACK_REQ_MSG 3
#define SCHC_SENDER_ABORT_MSG 4
#define SCHC_RECEIVER_ABORT_MSG 5 
#define SCHC_COMPOUND_ACK 6
#define SCHC_ACK_RESIDUAL_MSG 7

/* ACK Modes */
#define ACK_MODE_ACK_END_WIN 1
#define ACK_MODE_ACK_END_SES 2
#define ACK_MODE_COMPOUND_ACK 3
#define ACK_MODE  ACK_MODE_ACK_END_WIN  

#endif