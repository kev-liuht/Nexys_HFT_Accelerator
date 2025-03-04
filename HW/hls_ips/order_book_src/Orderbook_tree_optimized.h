#ifndef ORDERBOOK_TREE_OPTIMIZED_H
#define ORDERBOOK_TREE_OPTIMIZED_H
// For each stock, we have 5 ask prices + 5 ask quantities + 5 bid prices + 5 bid quantities = 20
// Each is 32 bits, so that's 20*32 bits per stock.

#define NUM_STOCKS      2           // Number of distinct stocks
#define MAX_ORDER_NUM 		32
#define MAX_LEVELS      64        // Maximum price levels in the segment tree
#define CACHE_SIZE      5           // Cache top-5 levels
#define CMD_ADD_ORDER   0
#define CMD_CANCEL_ORDER 1
#define CMD_GET_TOP_5   2


// For 'side', let's define: 0 => "bid", 1 => "ask"
#define SIDE_BID 0
#define SIDE_ASK 1


#include <hls_stream.h>
#include <ap_int.h>

// Declare your top-level function
void Orderbook_wrapper(
		hls::stream<ap_uint<136> >& inStream_pars,
		hls::stream<ap_uint<1> >& inStream_algo,
		hls::stream<ap_uint<32> >& outStream_algo
);

// You can also put other struct / function declarations here

#endif
