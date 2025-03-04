#include <ap_int.h>
#include <hls_stream.h>
#include "orderbook_tree_optimized.h"
// -----------------------------------------------------------------------------
// Configuration Constants (Adjust to your needs)
// -----------------------------------------------------------------------------
#define NUM_STOCKS      3           // Number of distinct stocks
#define MAX_ORDER_NUM 		256
#define MAX_LEVELS      64        // Maximum price levels in the segment tree
#define CACHE_SIZE      5           // Cache top-5 levels
#define CMD_ADD_ORDER   0
#define CMD_CANCEL_ORDER 1
#define CMD_GET_TOP_5   2

// For 'side', let's define: 0 => "bid", 1 => "ask"
#define SIDE_BID 0
#define SIDE_ASK 1

// -----------------------------------------------------------------------------
// A small utility to get the "better" price-level index given 'side'.
// For BID side, "better" means larger index. For ASK, "better" means smaller.
// -----------------------------------------------------------------------------
static bool is_better(ap_uint<32> idxA, ap_uint<32> idxB, ap_uint<32> side) {
#pragma HLS inline
    if (side == SIDE_BID) {
        // For bids, "better" = bigger index
        return (idxA > idxB);
    } else {
        // For asks, "better" = smaller index
        return (idxA < idxB);
    }
}

static ap_uint<32> choose_preferred(ap_uint<32> idxA, ap_uint<32> idxB, ap_uint<32> side) {
#pragma HLS inline
    // -1 will be represented with something out of range, e.g. 0xFFFFFFFF
    ap_uint<32> NEG_ONE = 0xFFFFFFFF;

    if (idxA == NEG_ONE) return idxB;
    if (idxB == NEG_ONE) return idxA;

    // For BID: pick the max index
    if (side == SIDE_BID) {
        return (idxA > idxB) ? idxA : idxB;
    } else {
        // For ASK: pick the min index
        return (idxA < idxB) ? idxA : idxB;
    }
}

// -----------------------------------------------------------------------------
// TreeOrderBook Struct
//
// Adapts the Python TreeOrderBook logic into a struct with fixed-size arrays.
// -----------------------------------------------------------------------------

struct OrderList {
	static const int MAX_ORDERS = MAX_ORDER_NUM;
	bool     order_valid[MAX_ORDERS];
	ap_uint<32> order_price_index[MAX_ORDERS];
	ap_uint<32> order_quantity[MAX_ORDERS];
	ap_uint<2> order_ask_bid[MAX_ORDERS];
};


struct TreeOrderBook {
    // Configuration
    ap_uint<32> min_price;
    ap_uint<32> max_price;
    ap_uint<32> tick_size;
    ap_uint<32> cache_size;
    ap_uint<32> side; // 0 => bid, 1 => ask

    // Derived or fixed parameters:
    ap_uint<32> num_levels;          // how many price levels

    // Segment tree storage:
    ap_uint<32> segment_tree[2*MAX_LEVELS];  // each node holds "best index" or 0xFFFFFFFF if empty
    ap_uint<32> price_quantity[MAX_LEVELS];  // quantity per price index
    ap_uint<32> top_quantities[CACHE_SIZE]; // quantity "parked" in cache

    ap_uint<32> top_prices[CACHE_SIZE]; // up to 'cache_size' best indices, stored contiguously
    ap_uint<32> top_count;              // how many entries are truly in top_prices

    // We also need an order_map for quick lookup by order_id => (price_index, quantity).
    // For demonstration, let's keep a small, fixed-size map. In real designs,
    // you might store less or handle collisions differently.


};

// -----------------------------------------------------------------------------
// Initialize a TreeOrderBook (called once at reset or start).
// -----------------------------------------------------------------------------
static void init_tree_order_book(TreeOrderBook &ob,
                                 ap_uint<32> min_price,
                                 ap_uint<32> tick_size,
                                 ap_uint<32> side) {
    // Basic config
    ob.min_price = min_price;

    ap_uint<32> max_price = min_price + tick_size * MAX_LEVELS;
    ob.max_price = max_price;
    ob.tick_size = tick_size;
    ob.side      = side;
    ob.cache_size= CACHE_SIZE;

    // Compute num_levels = round((max_price - min_price)/tick_size)
    ap_uint<32> price_diff = max_price - min_price;
    ap_uint<32> levels     = price_diff / tick_size;
    // +1 if you want inclusive range. For safety, clamp to MAX_LEVELS.
    if (levels >= MAX_LEVELS) {
        levels = MAX_LEVELS - 1;
    }
    ob.num_levels = levels + 1; // make it inclusive

    // Clear arrays
    init_loop1: for(int i=0; i<2*MAX_LEVELS; i++){
    #pragma HLS unroll factor=2
    	if (i < CACHE_SIZE) {
    		ob.top_quantities[i] = 0;
    	}
        if (i < MAX_LEVELS) {
            ob.price_quantity[i]       = 0;

        }
        ob.segment_tree[i] = 0xFFFFFFFF; // denote empty
    }

    // Clear top_prices
    init_loop2: for(int i=0; i<CACHE_SIZE; i++){
    #pragma HLS unroll factor=2
        ob.top_prices[i] = 0xFFFFFFFF;
    }
    ob.top_count = 0;


}
static void init_order_list(OrderList&ol) {
	// Clear order map
	init_loop3: for(int i=0; i<OrderList::MAX_ORDERS; i++){
	#pragma HLS unroll factor=2
		ol.order_valid[i]       = false;
		ol.order_price_index[i] = 0;
		ol.order_quantity[i]    = 0;
		ol.order_ask_bid[i]    = 2;
	}
}



// -----------------------------------------------------------------------------
// Utility: Convert price -> index, index -> price
// -----------------------------------------------------------------------------
static ap_uint<32> price_to_index(const TreeOrderBook &ob, ap_uint<32> p) {
#pragma HLS inline
    ap_uint<32> diff  = p - ob.min_price;
    ap_uint<32> idx   = diff / ob.tick_size;
    if (idx >= ob.num_levels) idx = ob.num_levels - 1; // clamp
    return idx;
}
static ap_uint<32> index_to_price(const TreeOrderBook &ob, ap_uint<32> idx) {
#pragma HLS inline
    return ob.min_price + idx * ob.tick_size;
}

// -----------------------------------------------------------------------------
// Segment Tree: bubble-up after we update a leaf
// -----------------------------------------------------------------------------
static void bubble_up(TreeOrderBook &ob, ap_uint<32> leaf_idx) {
#pragma HLS inline off

    // Leaf node = leaf_idx + ob.num_levels in 1-based indexing,
    // but here we effectively do 0-based. Let's do a small offset:
    ap_uint<32> base = ob.num_levels; // # leaves = num_levels
    ap_uint<32> node = leaf_idx + base;

    // Update leaf
    if (ob.price_quantity[leaf_idx] > 0) {
        ob.segment_tree[node] = leaf_idx;
    } else {
        ob.segment_tree[node] = 0xFFFFFFFF;
    }

    // Move upward
    node >>= 1;
    while (node > 0) {
    #pragma HLS loop_tripcount max=10
        ap_uint<32> left  = ob.segment_tree[node << 1];
        ap_uint<32> right = ob.segment_tree[(node << 1) + 1];
        ob.segment_tree[node] = choose_preferred(left, right, ob.side);
        node >>= 1;
    }
}

// -----------------------------------------------------------------------------
// Try to "promote" the best price from the segment tree root into the cache
// -----------------------------------------------------------------------------


static bool try_promote(TreeOrderBook &ob) {
#pragma HLS inline off
    ap_uint<32> root_idx = ob.segment_tree[1];
    if (root_idx == 0xFFFFFFFF) {
        // Tree empty
        return false;
    }

    // If that root index is already in cache, just move all quantity from tree to cache
    for(int i=0; i<5; i++){
		if(ob.top_prices[i] == root_idx){
			ap_uint<32> q_tree  = ob.price_quantity[root_idx];
			if (q_tree > 0) {
				ob.top_quantities[i]+= q_tree;
				ob.price_quantity[root_idx] = 0;
				bubble_up(ob, root_idx);
			}
			return true;
		}
    }
    // If we have space in top_prices
    if (ob.top_count < ob.cache_size) {
        // Move quantity from tree to cache
        ap_uint<32> q_tree = ob.price_quantity[root_idx];

        ob.top_quantities[ob.top_count] += q_tree;
        ob.price_quantity[root_idx] = 0;
        bubble_up(ob, root_idx);

        // Insert root_idx into top_prices
        ob.top_prices[ob.top_count] = root_idx;
        ob.top_count++;

        // Sort top_prices to maintain best->worst order for easy worst check
        // We'll do a simple bubble sort for clarity (small top_count <= 5)
        sort_top: for(int i=0; i< (int)ob.top_count-1; i++){
//        #pragma HLS unroll
            for(int j=i+1; j< (int)ob.top_count; j++){
//            #pragma HLS unroll
                ap_uint<32> x = ob.top_prices[i];
                ap_uint<32> y = ob.top_prices[j];
                ap_uint<32> w = ob.top_quantities[i];
                ap_uint<32> z = ob.top_quantities[j];

                bool swap = false;
                if (ob.side == SIDE_BID) {
                    // descending
                    if (x < y) swap = true;
                } else {
                    // ascending
                    if (x > y) swap = true;
                }
                if (swap) {

                    ob.top_prices[i] = y;
                    ob.top_prices[j] = x;
                    ob.top_quantities[i] = z;
                    ob.top_quantities[j] = w;

                }
            }
        }
        return true;
    }

    // Otherwise, the cache is full => compare with the worst in the cache
    // The "worst" is at index [top_count-1] because we sorted above.
    ap_uint<32> worst_idx = ob.top_prices[ob.top_count-1];
    // If the root is strictly better => swap
    if (is_better(root_idx, worst_idx, ob.side)) {
        // Move worst from cache -> tree
        ap_uint<32> q_cache = ob.top_quantities[ob.top_count-1];
        ob.price_quantity[worst_idx] += q_cache;
        ob.top_quantities[ob.top_count-1] = 0;
        bubble_up(ob, worst_idx);

        // Move root from tree -> cache
        ap_uint<32> q_tree = ob.price_quantity[root_idx];
        ob.top_quantities[ob.top_count-1] += q_tree;
        ob.price_quantity[root_idx] = 0;
        bubble_up(ob, root_idx);

        ob.top_prices[ob.top_count-1] = root_idx;

        // Re-sort
        sort_top2: for(int i=0; i< (int)ob.top_count-1; i++){
//        #pragma HLS unroll
            for(int j=i+1; j< (int)ob.top_count; j++){
//            #pragma HLS unroll
                ap_uint<32> x = ob.top_prices[i];
                ap_uint<32> y = ob.top_prices[j];
                ap_uint<32> w = ob.top_quantities[i];
                ap_uint<32> z = ob.top_quantities[j];
                bool swap = false;
                if (ob.side == SIDE_BID) {
                    // descending
                    if (x < y) swap = true;
                } else {
                    // ascending
                    if (x > y) swap = true;
                }
                if (swap) {
                    ob.top_prices[i] = y;
                    ob.top_prices[j] = x;
                    ob.top_quantities[i] = z;
                    ob.top_quantities[j] = w;

                }
            }
        }
        return true;
    }

    return false;
}

// After updating a single price index, repeatedly try to promote
static void update_and_promote(TreeOrderBook &ob, ap_uint<32> idx) {
#pragma HLS inline off
    bubble_up(ob, idx);
    bool changed = true;
    while(changed) {
    #pragma HLS loop_tripcount max=20
        changed = try_promote(ob);
    }
}

// -----------------------------------------------------------------------------
// Add/Cancel Orders
// For simplicity, we store orders in a small fixed array, indexed by order_id
// directly or with modulo. Real-world designs would handle collisions, etc.
// -----------------------------------------------------------------------------
static void add_order(OrderList &ol,
					  TreeOrderBook &ob,
                      ap_uint<32> order_id,
                      ap_uint<32> price,
                      ap_uint<32> quantity,
					  ap_uint<1>buy_sell)
{
    if (order_id >= OrderList::MAX_ORDERS) return; // naive bound check

    // Convert price -> index
    ap_uint<32> idx = price_to_index(ob, price);
    bool cache_exist = false;
    for(int i=0; i<5; i++){
    		if(ob.top_prices[i] == idx){
    			ob.top_quantities[i] += quantity;
    			cache_exist = true;
    		}
    }

    if (!cache_exist){
        // Add to tree
        ob.price_quantity[idx] += quantity;
        update_and_promote(ob, idx);
    }

    // Record in order map (overwrite)
    ol.order_valid[order_id]       = true;
    ol.order_price_index[order_id] = idx;
    ol.order_quantity[order_id]    = quantity;
    ol.order_ask_bid[order_id]    = buy_sell;
}
static ap_uint<1> check_ask_bid(OrderList &ol,ap_uint<32> order_id){
	if (order_id >= OrderList::MAX_ORDERS) return 2;
	if (!ol.order_valid[order_id]) return 2;
	ap_uint<32> buy_sell = ol.order_ask_bid[order_id];
	return buy_sell;
}
static void cancel_order(OrderList &ol,
						 TreeOrderBook &ob,
                         ap_uint<32> order_id,
                         ap_uint<32> cancel_qty)
{
    if (order_id >= OrderList::MAX_ORDERS) return;
    if (!ol.order_valid[order_id]) return;

    ap_uint<32> idx = ol.order_price_index[order_id];
    ap_uint<32> old_qty = ol.order_quantity[order_id];

    ap_uint<32> remove_qty = (cancel_qty == 0xFFFFFFFF || cancel_qty >= old_qty)
                             ? old_qty
                             : cancel_qty;

    // Decrement from cache or tree

    bool cache_exist = false;
	for(int i=0; i<5; i++){
			if(ob.top_prices[i] == idx){
				ap_uint<32> cached = ob.top_quantities[i];
				if (cached > remove_qty) {
					ob.top_quantities[i] = cached - remove_qty;
				} else {
					ob.top_quantities[i] = 0;
				}
				cache_exist = true;
			}
	}

	if (!cache_exist){
        ap_uint<32> tr = ob.price_quantity[idx];
        if (tr > remove_qty) {
            ob.price_quantity[idx] = tr - remove_qty;
        } else {
            ob.price_quantity[idx] = 0;
        }
    }

    // Update order map
    if (remove_qty == old_qty) {
        // completely remove
    	ol.order_valid[order_id] = false;
    	ol.order_quantity[order_id] = 0;
    	ol.order_ask_bid[order_id] = 3;
    } else {
    	ol.order_quantity[order_id] = old_qty - remove_qty;
    }

    // If that zeroed out the cached quantity, we need to ensure we remove from top_prices
    // if needed. We'll just do a simple check for all top_count entries:
    check_zero: for(int i=0; i<(int)ob.top_count; i++){
//    #pragma HLS unroll
        ap_uint<32> x = ob.top_prices[i];
        if (x != 0xFFFFFFFF && ob.top_quantities[i] == 0) {
            // Remove from array by shifting everything after i
            for(int j=i; j<(int)ob.top_count-1; j++){
                ob.top_prices[j] = ob.top_prices[j+1];
                ob.top_quantities[j] = ob.top_quantities[j+1];
            }
            ob.top_prices[ob.top_count-1] = 0xFFFFFFFF;
            ob.top_quantities[ob.top_count-1] = 0;
            ob.top_count--;
            break; // exit loop
        }
    }

    // Re-bubble up to keep tree consistent
    update_and_promote(ob, idx);
}

// -----------------------------------------------------------------------------
// Retrieve top-5 price/quantity for a given book (like get_top_k_quantity).
// Returns them in arrays of length 5: out_prices[], out_qty[]
// If less than 5 exist, the remainder will be 0xFFFFFFFF or 0.
// -----------------------------------------------------------------------------


static void get_top_5(const TreeOrderBook &ob,
                      ap_uint<32> out_prices[5],
                      ap_uint<32> out_qty[5])
{
#pragma HLS inline off
    // The top-levels are in ob.top_prices[0..top_count-1].
    // We'll use up to 5 of them.
    for(int i=0; i<5; i++){
    #pragma HLS unroll
        if (i < ob.top_count) {
            ap_uint<32> idx = ob.top_prices[i];
            out_prices[i] = index_to_price(ob, idx);
            out_qty[i]    = ob.top_quantities[i];
        } else {
            out_prices[i] = 0xFFFFFFFF;  // indicates no level
            out_qty[i]    = 0;
        }
    }
}

// -----------------------------------------------------------------------------
// The overall order book for multiple stocks
// For demonstration, we statically instantiate "bid" and "ask" book per stock.
// -----------------------------------------------------------------------------
struct StockOrderBook {
    TreeOrderBook bid_books[NUM_STOCKS];
    TreeOrderBook ask_books[NUM_STOCKS];
    OrderList order_list;
};

// Global instance (or static local) - in real designs you may keep it local
static StockOrderBook g_orderbook;

// -----------------------------------------------------------------------------
// Initialize all stock books
// In a real system, you might pass these as parameters or from the input stream.
// Here we use constants for demonstration.
// -----------------------------------------------------------------------------
static void init_all_books() {
    // Example min, max, tick for each of the 2 stocks:
    // (You can adapt to your real scenario.)
    ap_uint<32> min_p[NUM_STOCKS]    = {100, 100, 100};   // just for example
    ap_uint<32> tick [NUM_STOCKS]    = {2, 2, 2};       // just for example

    init_loop: for(int i=0; i<NUM_STOCKS; i++){
    #pragma HLS unroll
        init_tree_order_book(g_orderbook.bid_books[i], min_p[i], tick[i], SIDE_BID);
        init_tree_order_book(g_orderbook.ask_books[i], min_p[i], tick[i], SIDE_ASK);
    }
    init_order_list(g_orderbook.order_list);
}

// -----------------------------------------------------------------------------
// The Top Function: orderbook_wrapper
//
// AXI4-Stream protocol usage (simplified):
//   - We'll read commands from 'inStream' one at a time:
//       word0 = command (0=add, 1=cancel, 2=get_top_5)
//     If command=0 (add):
//       next 4 words: stock_id, side, order_id, price, quantity
//     If command=1 (cancel):
//       next 4 words: stock_id, side, order_id, cancel_qty
//     If command=2 (get_top_5):
//       next 1 word: stock_id (or, if you want "all stocks", we can ignore this
//       and do all). The code below retrieves top-5 (ask,bid) for each stock
//       and writes them out.
// -----------------------------------------------------------------------------
void Orderbook_wrapper(
	hls::stream<ap_uint<136> >& inStream_pars,
	hls::stream<ap_uint<1> >& inStream_algo,
    hls::stream<ap_uint<32> >& outStream_algo
) {
#pragma HLS interface ap_ctrl_none port=return
#pragma HLS interface axis port=inStream_pars
#pragma HLS interface axis port=inStream_algo
#pragma HLS interface axis port=outStream_algo

    // We assume "initialization" is done once at the start.
    // You might want to guard it behind a static bool or reset logic in real hardware.
    static bool initialized = false;
    if (!initialized) {
        init_all_books();
        initialized = true;
    }

    // -------------------------------------------------------------------------
    // Main loop: read commands from input. Because the problem states
    // "add/cancel/get_all_stocks_top_5_prices_quantity cannot execute at the same time",
    // we assume we read exactly one command at a time, do it fully, produce output,
    // then return or wait for next command.
    // -------------------------------------------------------------------------

        if (!inStream_pars.empty()) {
			// 1) Read one 136-bit wide word from the input
			ap_uint<136> in_data = inStream_pars.read();

			// 2) Parse out each field (matching your 136-bit format)
			//    According to your table:
			//
			//    Offset (bits) | Width | Field Name
			//    0..31         | 32    | stock_id
			//    32..63        | 32    | order_ref_num
			//    64..95        | 32    | num_shares
			//    96..127       | 32    | price
			//    128..131      | 4     | order_type
			//    132           | 1     | buy_sell
			//    133..135      | 3     | unused
			//
			ap_uint<32> stock_id     = in_data.range( 31,  0);
			ap_uint<32> order_ref_num= in_data.range( 63, 32);
			ap_uint<32> num_shares   = in_data.range( 95, 64);
			ap_uint<32> price        = in_data.range(127, 96);
			ap_uint<4>  order_type   = in_data.range(131,128);
			ap_uint<1>  buy_sell     = in_data[132];
			// The last 3 bits are unused:
			ap_uint<3>  unused_bits  = in_data.range(135,133);


			switch (order_type) {
			case 0x1:  // Add Order
				// Use the same logic you had for CMD_ADD_ORDER:
				if (stock_id < NUM_STOCKS) {
					if (buy_sell == SIDE_BID) {
						add_order(g_orderbook.order_list,g_orderbook.bid_books[stock_id],
								  order_ref_num, price, num_shares,buy_sell);
					} else {
						add_order(g_orderbook.order_list,g_orderbook.ask_books[stock_id],
								  order_ref_num, price, num_shares,buy_sell);
					}
				}
				break;

			case 0x2:  // Cancel Order
				// Use the same logic you had for CMD_CANCEL_ORDER
				if (stock_id < NUM_STOCKS) {
					ap_uint<2> buy_sell = check_ask_bid(g_orderbook.order_list,order_ref_num);
					if(buy_sell == 2){
						break;
					}
					if (buy_sell == SIDE_BID) {
						cancel_order(g_orderbook.order_list,g_orderbook.bid_books[stock_id],
									 order_ref_num, /*cancel_qty=*/0xFFFFFFFF);
					} else {
						cancel_order(g_orderbook.order_list,g_orderbook.ask_books[stock_id],
									 order_ref_num, /*cancel_qty=*/0xFFFFFFFF);
					}
				}
				break;

			case 0x4:  // Execute Order (if you implement it)
				// Implement your ¡°execute¡± logic here
				// Use the same logic you had for CMD_CANCEL_ORDER
				if (stock_id < NUM_STOCKS) {
					ap_uint<2> buy_sell = check_ask_bid(g_orderbook.order_list,order_ref_num);
					if(buy_sell == 2){
						break;
					}
					if (buy_sell == SIDE_BID) {
						cancel_order(g_orderbook.order_list,g_orderbook.bid_books[stock_id],
									 order_ref_num, /*cancel_qty=*/0xFFFFFFFF);
					} else {
						cancel_order(g_orderbook.order_list,g_orderbook.ask_books[stock_id],
									 order_ref_num, /*cancel_qty=*/0xFFFFFFFF);
					}
				}
				break;

			case 0x8:  // Delete Order (if you implement it)
				// Implement your ¡°delete¡± logic here
				break;

			default:
				// Possibly handle an unrecognized order_type
				break;
			}

			for(int s=0; s<(NUM_STOCKS-1); s++){
			        							// ask side top-5
			        							ap_uint<32> ask_prices[5], ask_qty[5];
			        							get_top_5(g_orderbook.ask_books[s], ask_prices, ask_qty);
			        							for(int i=0; i<5; i++){
			        								outStream_algo.write(ask_prices[i]);
			        							}
			        							for(int i=0; i<5; i++){
			        								outStream_algo.write(ask_qty[i]);
			        							}
			        							// bid side top-5
			        							ap_uint<32> bid_prices[5], bid_qty[5];
			        							get_top_5(g_orderbook.bid_books[s], bid_prices, bid_qty);
			        							for(int i=0; i<5; i++){
			        								outStream_algo.write(bid_prices[i]);
			        							}
			        							for(int i=0; i<5; i++){
			        								outStream_algo.write(bid_qty[i]);
			        							}
			        						}

		}




}
