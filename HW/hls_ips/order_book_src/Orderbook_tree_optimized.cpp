#include <ap_int.h>
#include <hls_stream.h>
#include "orderbook_tree_optimized.h"

#ifndef ORDERBOOK_TREE_OPTIMIZED_H
#define ORDERBOOK_TREE_OPTIMIZED_H

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

#endif

struct TreeOrderBook {
    ap_uint<32> min_price;
    ap_uint<32> max_price;
    ap_uint<32> tick_size;
    ap_uint<32> side;
    ap_uint<32> num_levels;
    ap_uint<32> segment_tree[2 * MAX_LEVELS];
    ap_uint<32> price_quantity[MAX_LEVELS];
};

struct OrderList {
    static const int MAX_ORDERS = MAX_ORDER_NUM;
    bool order_valid[MAX_ORDERS];
    ap_uint<32> order_price_index[MAX_ORDERS];
    ap_uint<32> order_quantity[MAX_ORDERS];
    ap_uint<2> order_ask_bid[MAX_ORDERS];
};

struct StockOrderBook {
    TreeOrderBook bid_books[NUM_STOCKS];
    TreeOrderBook ask_books[NUM_STOCKS];
    OrderList order_list;
};

static StockOrderBook g_orderbook;

static ap_uint<32> choose_preferred(ap_uint<32> idxA, ap_uint<32> idxB, ap_uint<32> side) {
    ap_uint<32> NEG_ONE = 0xFFFFFFFF;
    if (idxA == NEG_ONE) return idxB;
    if (idxB == NEG_ONE) return idxA;
    return (side == SIDE_BID) ? ((idxA > idxB) ? idxA : idxB) : ((idxA < idxB) ? idxA : idxB);
}

static void init_tree_order_book(TreeOrderBook &ob, ap_uint<32> min_price, ap_uint<32> tick_size, ap_uint<32> side) {
    ob.min_price = min_price;
    ob.max_price = min_price + tick_size * (MAX_LEVELS - 1);
    ob.tick_size = tick_size;
    ob.side = side;
    ob.num_levels = MAX_LEVELS;

    // Pipelined initialization
    for (int i = 0; i < 2 * MAX_LEVELS; i++) {
        #pragma HLS pipeline II=1
        ob.segment_tree[i] = 0xFFFFFFFF;
    }
    for (int i = 0; i < MAX_LEVELS; i++) {
        #pragma HLS pipeline II=1
        ob.price_quantity[i] = 0;
    }
}

static ap_uint<32> price_to_index(const TreeOrderBook &ob, ap_uint<32> p) {
    ap_uint<32> diff = p - ob.min_price;
    ap_uint<32> idx = diff / ob.tick_size;
    return (idx >= MAX_LEVELS) ? ap_uint<32>(MAX_LEVELS - 1) : idx;
}

static ap_uint<32> index_to_price(const TreeOrderBook &ob, ap_uint<32> idx) {
    return ob.min_price + idx * ob.tick_size;
}

static void bubble_up(TreeOrderBook &ob, ap_uint<32> leaf_idx) {
    ap_uint<32> node = leaf_idx + MAX_LEVELS;
    ob.segment_tree[node] = (ob.price_quantity[leaf_idx] > 0) ? leaf_idx : ap_uint<32>(0xFFFFFFFF);

    node >>= 1;
    while (node > 0) {
        ap_uint<32> left = ob.segment_tree[node << 1];
        ap_uint<32> right = ob.segment_tree[(node << 1) + 1];
        ob.segment_tree[node] = choose_preferred(left, right, ob.side);
        node >>= 1;
    }
}

static void add_order(OrderList &ol, TreeOrderBook &ob, ap_uint<32> order_id, ap_uint<32> price, ap_uint<32> quantity, ap_uint<1> buy_sell) {
    if (order_id >= OrderList::MAX_ORDERS) return;
    ap_uint<32> idx = price_to_index(ob, price);
    ob.price_quantity[idx] += quantity;
    bubble_up(ob, idx);

    ol.order_valid[order_id] = true;
    ol.order_price_index[order_id] = idx;
    ol.order_quantity[order_id] = quantity;
    ol.order_ask_bid[order_id] = buy_sell;
}

static void init_order_list(OrderList &ol) {
    for (int i = 0; i < OrderList::MAX_ORDERS; i++) {
        #pragma HLS pipeline II=1
        ol.order_valid[i] = false;
        ol.order_price_index[i] = 0;
        ol.order_quantity[i] = 0;
        ol.order_ask_bid[i] = 2;
    }
}

static ap_uint<1> check_ask_bid(OrderList &ol, ap_uint<32> order_id) {
    if (order_id >= OrderList::MAX_ORDERS || !ol.order_valid[order_id]) return 2;
    return ol.order_ask_bid[order_id];
}

static void cancel_order(OrderList &ol, TreeOrderBook &ob, ap_uint<32> order_id, ap_uint<32> cancel_qty) {
    if (order_id >= OrderList::MAX_ORDERS || !ol.order_valid[order_id]) return;

    ap_uint<32> idx = ol.order_price_index[order_id];
    ap_uint<32> old_qty = ol.order_quantity[order_id];
    ap_uint<32> remove_qty = (cancel_qty == 0xFFFFFFFF) ? old_qty : ((cancel_qty > old_qty) ? old_qty : cancel_qty);

    ob.price_quantity[idx] -= remove_qty;
    bubble_up(ob, idx);

    if (remove_qty == old_qty) {
        ol.order_valid[order_id] = false;
        ol.order_quantity[order_id] = 0;
        ol.order_ask_bid[order_id] = 3;
    } else {
        ol.order_quantity[order_id] = old_qty - remove_qty;
    }
}

static void get_top_5(TreeOrderBook &ob, ap_uint<32> out_prices[5], ap_uint<32> out_qty[5]) {
    ap_uint<32> saved_indices[5];
    ap_uint<32> saved_qty[5];
    ap_uint<32> count = 0;
    for (int i = 0; i < 5; i++) {
        ap_uint<32> best_idx = ob.segment_tree[1];
        if (best_idx == 0xFFFFFFFF) break;
        saved_indices[count] = best_idx;
        saved_qty[count] = ob.price_quantity[best_idx];
        count++;
        ob.price_quantity[best_idx] = 0;
        bubble_up(ob, best_idx);
    }

    for (int i = 0; i < count; i++) {
        ob.price_quantity[saved_indices[i]] = saved_qty[i];
        bubble_up(ob, saved_indices[i]);
    }

    for (int i = 0; i < 5; i++) {
        ap_uint<32> worst_price;
        if (ob.side == SIDE_BID)
            worst_price = index_to_price(ob, 0);
        else
            worst_price = index_to_price(ob, MAX_LEVELS - 1);
        out_prices[i] = (i < count) ? index_to_price(ob, saved_indices[i]) : worst_price;
        out_qty[i] = (i < count) ? saved_qty[i] : ap_uint<32>(1);
    }
}

static void init_all_books() {
    ap_uint<32> min_p[NUM_STOCKS] = MIN_PRICE_INIT;
    ap_uint<32> tick[NUM_STOCKS]  = TICK_INIT;
    for (int i = 0; i < NUM_STOCKS; i++) {
        #pragma HLS unroll
        init_tree_order_book(g_orderbook.bid_books[i], min_p[i], tick[i], SIDE_BID);
        init_tree_order_book(g_orderbook.ask_books[i], min_p[i], tick[i], SIDE_ASK);
    }
    init_order_list(g_orderbook.order_list);
}

static void publish(hls::stream<axis_word_t> &outStream_algo) {
    for (int s = 0; s < NUM_STOCKS; s++) {
        // Process top-5 for ask side
        ap_uint<32> ask_prices[5], ask_qty[5];
        get_top_5(g_orderbook.ask_books[s], ask_prices, ask_qty);
        axis_word_t temp;
        for (int i = 0; i < 5; i++) {
            temp.data = ask_prices[i];
            temp.last = 0;
            outStream_algo.write(temp);
            temp.data = ask_qty[i];
            temp.last = 0;
            outStream_algo.write(temp);
        }
        // Process top-5 for bid side
        ap_uint<32> bid_prices[5], bid_qty[5];
        get_top_5(g_orderbook.bid_books[s], bid_prices, bid_qty);
        for (int i = 0; i < 5; i++) {
            temp.data = bid_prices[i];
            temp.last = 0;
            outStream_algo.write(temp);
            temp.data = bid_qty[i];
            // Fixed the last flag here as well
            temp.last = (i == 4) ? 1 : 0;
            outStream_algo.write(temp);
        }
    }
}

void Orderbook_wrapper(
    hls::stream<ap_uint<136> > &inStream_pars,
    hls::stream<axis_word_t> &outStream_algo
) {
    #pragma HLS interface ap_ctrl_none port=return
    #pragma HLS interface axis port=inStream_pars
    #pragma HLS interface axis port=outStream_algo
    #pragma HLS array_partition variable=g_orderbook.bid_books complete dim=1
    #pragma HLS array_partition variable=g_orderbook.ask_books complete dim=1

    static bool initialized = false;
    static ap_uint<32> num_new_order = 0;

    // Initialization Block (do this once)
    if (!initialized) {
        init_all_books();
        initialized = true;
    }

    // Publish order snapshot every 20 new orders
    if (num_new_order >= 20) {
        num_new_order = 0;
        publish(outStream_algo);
    }

    // Parser Data Intake Block
    if (!inStream_pars.empty()) {
        ap_uint<136> in_data = inStream_pars.read();
        ap_uint<32> stock_id = in_data.range(31, 0);
        ap_uint<32> order_ref_num = in_data.range(63, 32);
        ap_uint<32> num_shares = in_data.range(95, 64);
        ap_uint<32> price = in_data.range(127, 96);
        ap_uint<4> order_type = in_data.range(131, 128);
        ap_uint<1> buy_sell = in_data[132];

        switch (order_type) {
            case 0x1:
                if (stock_id < NUM_STOCKS) {
                    if (buy_sell == SIDE_BID) {
                        add_order(g_orderbook.order_list, g_orderbook.bid_books[stock_id],
                                  order_ref_num, price, num_shares, buy_sell);
                    } else {
                        add_order(g_orderbook.order_list, g_orderbook.ask_books[stock_id],
                                  order_ref_num, price, num_shares, buy_sell);
                    }
                }
                break;
            case 0x2:
                if (stock_id < NUM_STOCKS) {
                    ap_uint<2> side_flag = check_ask_bid(g_orderbook.order_list, order_ref_num);
                    if (side_flag == 2) break;
                    if (side_flag == SIDE_BID) {
                        cancel_order(g_orderbook.order_list, g_orderbook.bid_books[stock_id],
                                     order_ref_num, num_shares);
                    } else {
                        cancel_order(g_orderbook.order_list, g_orderbook.ask_books[stock_id],
                                     order_ref_num, num_shares);
                    }
                }
                break;
            case 0x4:
                if (stock_id < NUM_STOCKS) {
                    ap_uint<2> side_flag = check_ask_bid(g_orderbook.order_list, order_ref_num);
                    if (side_flag == 2) break;
                    if (side_flag == SIDE_BID) {
                        cancel_order(g_orderbook.order_list, g_orderbook.bid_books[stock_id],
                                     order_ref_num, num_shares);
                    } else {
                        cancel_order(g_orderbook.order_list, g_orderbook.ask_books[stock_id],
                                     order_ref_num, num_shares);
                    }
                }
                break;
            case 0x8:
                if (stock_id < NUM_STOCKS) {
                    ap_uint<2> side_flag = check_ask_bid(g_orderbook.order_list, order_ref_num);
                    if (side_flag == 2) break;
                    if (side_flag == SIDE_BID) {
                        cancel_order(g_orderbook.order_list, g_orderbook.bid_books[stock_id],
                                     order_ref_num, 0xFFFFFFFF);
                    } else {
                        cancel_order(g_orderbook.order_list, g_orderbook.ask_books[stock_id],
                                     order_ref_num, 0xFFFFFFFF);
                    }
                }
                break;
            default:
                break;
        }
        num_new_order++;
    }
}
