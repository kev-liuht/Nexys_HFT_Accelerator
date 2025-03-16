#ifndef ORDERBOOK_TREE_OPTIMIZED_H
#define ORDERBOOK_TREE_OPTIMIZED_H
// For each stock, we have 5 ask prices + 5 ask quantities + 5 bid prices + 5 bid quantities = 20
// Each is 32 bits, so that's 20*32 bits per stock.

#define CACHE_SIZE      5
#define NUM_STOCKS      4
#define MIN_PRICE_INIT {100,100,100,100}
#define TICK_INIT  {2,2,2,2}
#define MAX_ORDER_NUM   1024
#define MAX_LEVELS      64
#define CMD_ADD_ORDER   0
#define CMD_CANCEL_ORDER 1
#define CMD_GET_TOP_5   2
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
