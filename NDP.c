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

#include "NDP.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>

#include <linux/if.h>
#include <sys/ioctl.h>



//----------------------------------------------------------------------------//
// Types                                                                      //
//----------------------------------------------------------------------------//

////////////////////////////////////////////////////////////////////////////////
/// <summary> Maximum last recorded value allowed. </summary>
/// <remarks> This value is inclusive [-1, max]. </remarks>

#define NDP_MAX_RECORD 6

////////////////////////////////////////////////////////////////////////////////
/// <summary> Non-reserved IP type for the beacon. </summary>

#define IP_TYPE 0x3900

////////////////////////////////////////////////////////////////////////////////
/// <summary> Represents a single beacon that's sent. </summary>

typedef struct
{
	NDP_Addr TargetAddr;	// Target address
	NDP_Addr SourceAddr;	// Source address
	unsigned short Type;	// IP Type (0x3900)

} Beacon;



//----------------------------------------------------------------------------//
// NDP                                                                        //
//----------------------------------------------------------------------------//

////////////////////////////////////////////////////////////////////////////////
/// <summary> Processes a beacon that has arrived. </summary>

static void ReceiveBeacon (NDP_State* state, const Beacon* beacon)
{
	int i, free = -1;
	for (i = 0; i < NDP_TABLE_LEN; ++i)
	{
		// Track free entry in case we need it
		if (state->Table[i] == NULL)
			free = i;

		// Compare the address with current entry
		else if (memcmp (&state->Table[i]->Addr,
			&beacon->SourceAddr, NDP_ADDR_LEN) == 0)
			{
				state->Table[i]->Arrived = 1;
				return;
			}
	}

	// No entries found, attempt to add
	if (free != -1)
	{
		// Allocate and create an entry
		state->Table[free] = (NDP_Neighbor*)
			malloc (sizeof (NDP_Neighbor));

		state->Table[free]->Addr = beacon->SourceAddr;
		state->Table[free]->Arrived  =  1;
		state->Table[free]->Recorded = -1;
	}
}

////////////////////////////////////////////////////////////////////////////////
/// <summary> Updates the neighbor table. </summary>

static void UpdateTable (NDP_State* state)
{
	int i;
	for (i = 0; i < NDP_TABLE_LEN; ++i)
		if (state->Table[i] != NULL)
		{
			// Check if we recieved a beacon
			if (state->Table[i]->Arrived == 0)
			{
				// Remove out-of-range neighbors
				if (++state->Table[i]->Recorded >= NDP_MAX_RECORD)
				{
					// Deallocate and remove entry
					free (state->Table[i]);
					state->Table[i] = NULL;
				}
			}

			else
			{
				// Reset arrival state
				state->Table[i]->Arrived  = 0;
				state->Table[i]->Recorded = 0;
			}
		}
}



//----------------------------------------------------------------------------//
// Threading                                                                  //
//----------------------------------------------------------------------------//

////////////////////////////////////////////////////////////////////////////////
/// <summary> Thread that handles sending beacon packets. </summary>

static void* SendThread (void* parameters)
{
	int i;

	/// Retrieve the NDP state
	NDP_State* state = (NDP_State*) parameters;

	/// Create a beacon
	Beacon beacon;
	for (i = 0; i < NDP_ADDR_LEN; ++i)
		beacon.TargetAddr.Data[i] = 255;

	beacon.SourceAddr = state->Addr;
	beacon.Type       = htons (IP_TYPE);

	/// Set the destination address
	struct sockaddr_ll to;
	memset (&to, 0, sizeof (to));
	int tolen = sizeof (to);

	to.sll_family  = AF_PACKET;
	to.sll_pkttype = PACKET_BROADCAST;
	to.sll_ifindex = state->IfIndex;

	to.sll_halen = NDP_ADDR_LEN;
	for (i = 0; i < NDP_ADDR_LEN; ++i)
		to.sll_addr[i] = 255;

	/// Broadcast periodically
	unsigned int elapsed = 4000000;

	/// Enter the send loop
	while (state->Active)
	{
		// Use stress test mode
		if (state->Stress != 0)
		{
			// Spoof source address
			beacon.SourceAddr.Data[3] = (unsigned char) (rand() % 256);
			beacon.SourceAddr.Data[4] = (unsigned char) (rand() % 256);
			beacon.SourceAddr.Data[5] = (unsigned char) (rand() % 256);

			// Send beacon
			sendto (state->SocketID, &beacon, sizeof
				(beacon), 0, (struct sockaddr*) &to, tolen);

			// Reset source address
			beacon.SourceAddr = state->Addr;

			usleep (10000);
			continue;
		}

		// Send normally
		if (elapsed > 3000000)
		{
			// Send beacon
			sendto (state->SocketID, &beacon, sizeof
				(beacon), 0, (struct sockaddr*) &to, tolen);

			// Reset timer
			elapsed = 0;
		}

		// Sleep for 100 ms
		usleep (10000);
		elapsed += 10000;
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// <summary> Thread that handles receiving beacon packets. </summary>

static void* RecvThread (void* parameters)
{
	/// Retrieve the NDP state
	NDP_State* state = (NDP_State*) parameters;

	/// Create a beacon
	Beacon beacon;

	/// Set the source address
	struct sockaddr_ll from;
	memset (&from, 0, sizeof (from));
	socklen_t fromlen;

	/// Update the table periodically
	unsigned int elapsed = 0;

	/// Enter the receive loop
	while (state->Active)
	{
		// Reset network values
		fromlen = sizeof (from);
		memset (&beacon, 0, sizeof (beacon));

		// Non blocking receive beacon
		recvfrom (state->SocketID, &beacon, sizeof (beacon),
			MSG_DONTWAIT, (struct sockaddr*) &from, &fromlen);

		// Check for correct protocol type
		if (beacon.Type == htons (IP_TYPE))
		{
			NDP_Lock (state);
			ReceiveBeacon (state, &beacon);
			NDP_Unlock (state);
		}

		// Update the table
		if (elapsed > 5000000)
		{
			NDP_Lock (state);
			UpdateTable (state);
			NDP_Unlock (state);

			// Reset timer
			elapsed = 0;
		}

		// Sleep for 100 ms
		usleep (9000);
		elapsed += 9000;
	}

	return NULL;
}



//----------------------------------------------------------------------------//
// Core                                                                       //
//----------------------------------------------------------------------------//

////////////////////////////////////////////////////////////////////////////////
/// <summary> Creates an NDP state given an interface. </summary>
/// <remarks> The interface is defined in the state. </remarks>

void NDP_Create (NDP_State* state)
{
	/// Reset status variables
	state->Active = 0;
	state->Error  = 0;
	state->Stress = 0;

	memset (state->Table, 0, NDP_TABLE_LEN * sizeof (NDP_Neighbor*));

	/// Create device level socket
	state->SocketID = socket (PF_PACKET, SOCK_RAW, htons (ETH_P_ALL));
		// PF_PACKET - Packet interface on device level
		// SOCK_RAW  - Raw packets including link level header
		// ETH_P_ALL - All frames will be received

	if (state->SocketID < 0)
		{ state->Error = NDP_ERROR_OPEN_SOCK; return; }

	/// Fetch interface information
	struct ifreq ifr;

	// Copy the specified interface name
	strcpy (ifr.ifr_name, state->Interface);

	// Retrieve the interface index
	if (ioctl (state->SocketID, SIOGIFINDEX, &ifr) < 0)
		{ state->Error = NDP_ERROR_GET_IFINDEX; return; }

	state->IfIndex = ifr.ifr_ifindex;

	// Retrieve the hardware address
	if (ioctl (state->SocketID, SIOCGIFHWADDR, &ifr) < 0)
		{ state->Error = NDP_ERROR_GET_ADDRESS; return; }

	memcpy (&state->Addr.Data, &ifr.ifr_hwaddr.sa_data, NDP_ADDR_LEN);

	// Retrieve the maximum transmission unit
	if (ioctl (state->SocketID, SIOCGIFMTU, &ifr) < 0)
		{ state->Error = NDP_ERROR_GET_MTU; return; }

	state->MTU = ifr.ifr_mtu;

	/// Add the promiscuous mode
	struct packet_mreq mr;
	memset (&mr, 0, sizeof (mr));

	mr.mr_ifindex = state->IfIndex;
	mr.mr_type    = PACKET_MR_PROMISC;

	if (setsockopt (state->SocketID, SOL_PACKET,
		PACKET_ADD_MEMBERSHIP, (char*) &mr, sizeof (mr)) < 0)
		{ state->Error = NDP_ERROR_ADD_PROM; return; }

	/// Bind the socket to the interface
	struct sockaddr_ll sll;
	memset (&sll, 0, sizeof (sll));

	sll.sll_family   = AF_PACKET;
	sll.sll_ifindex  = state->IfIndex;
	sll.sll_protocol = htons (ETH_P_ALL);

	if (bind (state->SocketID, (struct sockaddr*) &sll, sizeof (sll)) < 0)
		{ state->Error = NDP_ERROR_BIND_SOCK; return; }
}

////////////////////////////////////////////////////////////////////////////////
/// <summary> Destroys the specified NDP state. </summary>
/// <remarks> This function makes a call to NDP_Stop. </remarks>

void NDP_Destroy (NDP_State* state)
{
	// Close the socket
	if (state->SocketID != -1)
	{
		NDP_Stop (state);
		close (state->SocketID);
	}
}

////////////////////////////////////////////////////////////////////////////////
/// <summary> Creates the threads and starts the NDP protocol. </summary>

void NDP_Start (NDP_State* state)
{
	// Ensure non-active and no errors
	if (state->Error == 0 && state->Active == 0)
	{
		// Create threads
		state->Active = 1;
		pthread_mutex_init (&state->Mutex, NULL);
		pthread_create (&state->SendThread, NULL, SendThread, state);
		pthread_create (&state->RecvThread, NULL, RecvThread, state);
	}
}

////////////////////////////////////////////////////////////////////////////////
/// <summary> Stops the NDP protocol and destroys the threads. </summary>

void NDP_Stop (NDP_State* state)
{
	// Ensure non-active
	if (state->Active != 0)
	{
		int i;

		// Join threads
		state->Active = 0;
		pthread_join (state->SendThread, NULL);
		pthread_join (state->RecvThread, NULL);
		pthread_mutex_destroy (&state->Mutex);

		// Clear neighbor table
		for (i = 0; i < NDP_TABLE_LEN; ++i)
			if (state->Table[i] != NULL)
			{
				free (state->Table[i]);
				state->Table[i] = NULL;
			}
	}
}

////////////////////////////////////////////////////////////////////////////////
/// <summary> Locks access to the neighbor table. </summary>

void NDP_Lock (NDP_State* state)
{
	if (state->Active != 0)
		pthread_mutex_lock (&state->Mutex);
}

////////////////////////////////////////////////////////////////////////////////
/// <summary> Releases access to the neighbor table. </summary>

void NDP_Unlock (NDP_State* state)
{
	if (state->Active != 0)
		pthread_mutex_unlock (&state->Mutex);
}



//----------------------------------------------------------------------------//
// Helpers                                                                    //
//----------------------------------------------------------------------------//

////////////////////////////////////////////////////////////////////////////////
/// <summary> Returns the error string associated with a state. </summary>

const char* NDP_ErrorString (const NDP_State* state)
{
	switch (state->Error)
	{
		case NDP_ERROR_NONE			: return NULL;
		case NDP_ERROR_OPEN_SOCK	: return "Could not open socket, Try running with sudo";
		case NDP_ERROR_GET_IFINDEX	: return "Failed to retrieve the interface index";
		case NDP_ERROR_GET_ADDRESS	: return "Failed to retrieve the hardware address";
		case NDP_ERROR_GET_MTU		: return "Failed to retrieve the maximum transmission unit";
		case NDP_ERROR_ADD_PROM		: return "Failed to add the promiscuous mode";
		case NDP_ERROR_BIND_SOCK	: return "Failed to bind the socket to the interface";
		default						: return "Unknown error occurred";
	}
}

////////////////////////////////////////////////////////////////////////////////
/// <summary> Returns the string representation of an address. </summary>

const char* NDP_AddrString (const NDP_Addr* address)
{
	static char result[32];

	sprintf (result, "%02X:%02X:%02X:%02X:%02X:%02X",
			address->Data[0], address->Data[1], address->Data[2],
			address->Data[3], address->Data[4], address->Data[5]);

	return result;
}
