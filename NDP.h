////////////////////////////////////////////////////////////////////////////////
// -------------------------------------------------------------------------- //
//                                                                            //
//                          Copyright (C) 2012-2013                           //
//                            github.com/dkrutsko                             //
//                            github.com/Harrold                              //
//                            github.com/AbsMechanik                          //
//                                                                            //
//                        See LICENSE.md for copyright                        //
//                                                                            //
// -------------------------------------------------------------------------- //
////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------//
// Prefaces                                                                   //
//----------------------------------------------------------------------------//

#ifndef NDP_PROTOCOL_H
#define NDP_PROTOCOL_H

#include <pthread.h>



//----------------------------------------------------------------------------//
// Types                                                                      //
//----------------------------------------------------------------------------//

////////////////////////////////////////////////////////////////////////////////
/// <summary> Maximum length for the name of the interface. </summary>
/// <remarks> This value includes the null terminator (\0). </remarks>

#define NDP_IFNAME_LEN	16

////////////////////////////////////////////////////////////////////////////////
/// <summary> Maximum number of neighbors that can be discovered. </summary>

#define NDP_TABLE_LEN	32

////////////////////////////////////////////////////////////////////////////////
/// <summary> Maximum length of a WLAN address. </summary>

#define NDP_ADDR_LEN	6

////////////////////////////////////////////////////////////////////////////////
/// <summary> Represents an NDP address type. </summary>

typedef struct
{
	// Address value
	unsigned char Data[NDP_ADDR_LEN];

} NDP_Addr;

////////////////////////////////////////////////////////////////////////////////
/// <summary> Represents a single neighbor entry. </summary>

typedef struct
{
	NDP_Addr Addr;	// Neighbor address
	char Arrived;	// Has arrived
	char Recorded;	// Last recorded

} NDP_Neighbor;

////////////////////////////////////////////////////////////////////////////////
/// <summary> Represents a single state of the NDP protocol. </summary>

typedef struct
{
	int Error;				// Index of the error

	int SocketID;			// Socket descriptor
	int IfIndex;			// Interface index
	NDP_Addr Addr;			// Local MAC address
	int MTU;				// Maximum transmission unit

	volatile char Active;	// Currently active
	volatile char Stress;	// Stress test mode

	pthread_t SendThread;	// Send thread ID
	pthread_t RecvThread;	// Recv thread ID
	pthread_mutex_t Mutex;	// Synchronization

	// Represents an interface to use
	char Interface[NDP_IFNAME_LEN];
		// Must be set before calling NDP_Create

	// Represents a table of neighbors
	NDP_Neighbor* Table[NDP_TABLE_LEN];
		// A call to NDP_Lock must be made before accessing
		// this variable. When finished, call NDP_Unlock.

} NDP_State;



//----------------------------------------------------------------------------//
// Errors                                                                     //
//----------------------------------------------------------------------------//

enum
{
	NDP_ERROR_NONE = 0,
	NDP_ERROR_OPEN_SOCK,
	NDP_ERROR_GET_IFINDEX,
	NDP_ERROR_GET_ADDRESS,
	NDP_ERROR_GET_MTU,
	NDP_ERROR_ADD_PROM,
	NDP_ERROR_BIND_SOCK,
};



//----------------------------------------------------------------------------//
// Prototypes                                                                 //
//----------------------------------------------------------------------------//

// Core
void NDP_Create  (NDP_State* state);
void NDP_Destroy (NDP_State* state);

void NDP_Start   (NDP_State* state);
void NDP_Stop    (NDP_State* state);

void NDP_Lock    (NDP_State* state);
void NDP_Unlock  (NDP_State* state);

// Helpers
const char* NDP_ErrorString (const NDP_State* state  );
const char* NDP_AddrString  (const NDP_Addr*  address);

#endif // NDP_PROTOCOL_H
