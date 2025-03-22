#include <iostream>
#include <iomanip>
#include <hls_stream.h>
#include <ap_int.h>

// Include your header with Orderbook_wrapper here:
#include "orderbook_tree_optimized.h"

// Helper to pack command fields into a 136-bit word
static ap_uint<136> make_input_word(
    ap_uint<32> stock_id,
    ap_uint<32> order_ref_num,
    ap_uint<32> num_shares,
    ap_uint<32> price,
    ap_uint<4>  order_type,
    ap_uint<1>  buy_sell
)
{
    // The input 136 bits are mapped as follows:
    //   stock_id    : in_data.range(31, 0)
    //   order_ref   : in_data.range(63, 32)
    //   num_shares  : in_data.range(95, 64)
    //   price       : in_data.range(127,96)
    //   order_type  : in_data.range(131,128)
    //   buy_sell    : in_data[132]
    //   bits 133..135 are unused
    ap_uint<136> value = 0;
    value.range(31, 0)     = stock_id;
    value.range(63, 32)    = order_ref_num;
    value.range(95, 64)    = num_shares;
    value.range(127, 96)   = price;
    value.range(131, 128)  = order_type;
    value[132]             = buy_sell;
    return value;
}



int main()
{
    hls::stream<ap_uint<136> > inStream_pars("inPars");
//    ap_uint<1> auto_publish_order;
//    ap_uint<1> manual_publish_order;
    hls::stream<axis_word_t >  outStream_algo("outAlgo");

    // We rely on #defines in "orderbook_tree_optimized.h" for:
    //   NUM_STOCKS, SIDE_BID, SIDE_ASK, etc.
    // Ensure you do NOT redefine them here.

    // We want each stock/side to place 64 orders,
        // giving a total of 64 * 2 sides * 2 stocks = 256.
        const int ORDERS_PER_SIDE = 128;
        const int NUM_TOP = 5;

        // Prices range from 100..228 in steps of 2 (i.e., 64 levels).
        //   PRICE = 100 + i * 2, for i=0..63
        // This matches the 64 levels in your design (MAX_LEVELS=64).
        ap_uint<32> min_p[NUM_STOCKS] = MIN_PRICE_INIT;
        ap_uint<32> tick[NUM_STOCKS]  = TICK_INIT;

        // 1) Inject add orders for each (stock, side). Each combination has 64 orders.
        for (int stock_id = 0; stock_id < NUM_STOCKS; ++stock_id) {
            for (int side = 0; side <= 1; ++side) {
                for (int i = 0; i < ORDERS_PER_SIDE; ++i) {
                    // Order ID range (0..255), split up among the 4 (stock, side) combos:
                    //   Stock0-BID: 0..63    Stock0-ASK: 64..127
                    //   Stock1-BID: 128..191 Stock1-ASK: 192..255
                    ap_uint<32> order_ref_num = (stock_id * ORDERS_PER_SIDE * 2) + (side * ORDERS_PER_SIDE) + i;

                    // Price i-th level => 100 + 2*i
                    ap_uint<32> price = min_p[stock_id] + (tick[stock_id] * i);

                    // For example, 10 shares each
                    ap_uint<32> quantity = 10;

                    // "Add" order command has order_type = 0x1
                    ap_uint<4> order_type = 0x1;

                    // side=0 => BID, side=1 => ASK
                    ap_uint<1> buy_sell = side;

                    ap_uint<136> in_data = make_input_word(
                        stock_id,
                        order_ref_num,
                        quantity,
                        price,
                        order_type,
                        buy_sell
                    );

                    // Write add command into inStream_pars and call the wrapper
                    inStream_pars.write(in_data);
//                    Orderbook_wrapper(inStream_pars, auto_publish_order,manual_publish_order, outStream_algo);
                    Orderbook_wrapper(inStream_pars, outStream_algo);
                }
            }
        }

        // 2) Request a top-5 snapshot for ALL stocks by putting '1' on inStream_algo.
//        ap_uint<1> temp_1 = 1;
//        manual_publish_order = temp_1;
//        Orderbook_wrapper(inStream_pars, auto_publish_order,manual_publish_order, outStream_algo);

        // 3) For each stock, read and print top-5 Asks and top-5 Bids (before cancel)
        std::cout << "\n=== TOP-5 SNAPSHOT (POST ADD) ===\n";

        for (int s = 0; s < NUM_STOCKS; s++) {
            std::cout << "\nStock " << s << ":\n";

            // Ask side: read top-5 asks (each ask has a price and a quantity)
            std::cout << "  Top-5 Asks:\n";
            ap_uint<32> ask_prices[5], ask_qty[5];
            axis_word_t temp;
            for (int i = 0; i < 5; i++) {
                // Read ask price
                temp = outStream_algo.read();
                ask_prices[i] = temp.data;
                // Read ask quantity
                temp = outStream_algo.read();
                ask_qty[i] = temp.data;
            }
            for (int i = 0; i < 5; i++) {
//                if (ask_prices[i] == 0xFFFFFFFF)
//                    std::cout << "    Ask " << i << ": <Empty>\n";
//                else
                    std::cout << "    Ask " << i << ": Price = " << ask_prices[i]
                              << ", Qty = " << ask_qty[i] << "\n";
            }

            // Bid side: read top-5 bids (each bid has a price and a quantity)
            std::cout << "  Top-5 Bids:\n";
            ap_uint<32> bid_prices[5], bid_qty[5];
            for (int i = 0; i < 5; i++) {
                // Read bid price
                temp = outStream_algo.read();
                bid_prices[i] = temp.data;
                // Read bid quantity
                temp = outStream_algo.read();
                bid_qty[i] = temp.data;
            }
            for (int i = 0; i < 5; i++) {
//                if (bid_prices[i] == 0xFFFFFFFF)
//                    std::cout << "    Bid " << i << ": <Empty>\n";
//                else

                    std::cout << "    Bid " << i << ": Price = " << bid_prices[i]
                              << ", Qty = " << bid_qty[i] << "\n";
            }
        }

//        // 4) Cancel some top orders.
//        // For each stock, cancel the best (top) ask and the best bid orders.
//        // For ASK, the best order is the one at the lowest price level (i = 0).
//        // For BID, the best order is the one at the highest price level (i = 63).
//        // Cancellation is done using order_type = 0x8 (which cancels the full quantity).
//        for (int s = 0; s < NUM_STOCKS; s++) {
//            // Cancel best ASK: For ask side (side=1), i=0 => order_ref = (s*128) + 64 + 0.
//            ap_uint<32> order_ref_ask = (s * ORDERS_PER_SIDE * 2) + (1 * ORDERS_PER_SIDE) + 0;
//            // Price is same as min price for level 0
//            ap_uint<32> price_ask = min_p[s];
//            // Use quantity of 10 (the field is not used in cancellation for type 0x8)
//            ap_uint<32> quantity = 5;
//            ap_uint<4> order_type_cancel = 0x8; // cancellation command (delete)
//            ap_uint<1> buy_sell_ask = 1; // ASK side
//            ap_uint<136> in_data_cancel_ask = make_input_word(
//                s,
//                order_ref_ask,
//                quantity,
//                price_ask,
//                order_type_cancel,
//                buy_sell_ask
//            );
//            inStream_pars.write(in_data_cancel_ask);
//            Orderbook_wrapper(inStream_pars, inStream_algo, outStream_algo);
//
//            // Cancel best BID: For bid side (side=0), i=63 => order_ref = (s*128) + 0*64 + 63.
//            ap_uint<32> order_ref_bid = (s * ORDERS_PER_SIDE * 2) + (0 * ORDERS_PER_SIDE)  + ORDERS_PER_SIDE - 1;
//            // Price for bid at level 63: min_p[s] + tick[s]*63
//            ap_uint<32> price_bid = min_p[s] + tick[s] * ORDERS_PER_SIDE;
//            ap_uint<1> buy_sell_bid = 0; // BID side
//            ap_uint<136> in_data_cancel_bid = make_input_word(
//                s,
//                order_ref_bid,
//                quantity,
//                price_bid,
//                order_type_cancel,
//                buy_sell_bid
//            );
//            inStream_pars.write(in_data_cancel_bid);
//            Orderbook_wrapper(inStream_pars, inStream_algo, outStream_algo);
//        }
//
//        // 5) Request a top-5 snapshot again to check the cancellation effect.
//        inStream_algo.write(temp_1);
//        Orderbook_wrapper(inStream_pars, inStream_algo, outStream_algo);
//
//        std::cout << "\n=== TOP-5 SNAPSHOT (POST CANCEL) ===\n";
//
//        for (int s = 0; s < NUM_STOCKS; s++) {
//            std::cout << "\nStock " << s << ":\n";
//
//            // Ask side: read top-5 asks (each ask has a price and a quantity)
//            std::cout << "  Top-5 Asks:\n";
//            ap_uint<32> ask_prices[5], ask_qty[5];
//            axis_word_t temp;
//            for (int i = 0; i < 5; i++) {
//                // Read ask price
//                temp = outStream_algo.read();
//                ask_prices[i] = temp.data;
//                // Read ask quantity
//                temp = outStream_algo.read();
//                ask_qty[i] = temp.data;
//            }
//            for (int i = 0; i < 5; i++) {
//                if (ask_prices[i] == 0xFFFFFFFF)
//                    std::cout << "    Ask " << i << ": <Empty>\n";
//                else
//                    std::cout << "    Ask " << i << ": Price = " << ask_prices[i]
//                              << ", Qty = " << ask_qty[i] << "\n";
//            }
//
//            // Bid side: read top-5 bids (each bid has a price and a quantity)
//            std::cout << "  Top-5 Bids:\n";
//            ap_uint<32> bid_prices[5], bid_qty[5];
//            for (int i = 0; i < 5; i++) {
//                // Read bid price
//                temp = outStream_algo.read();
//                bid_prices[i] = temp.data;
//                // Read bid quantity
//                temp = outStream_algo.read();
//                bid_qty[i] = temp.data;
//            }
//            for (int i = 0; i < 5; i++) {
//                if (bid_prices[i] == 0xFFFFFFFF)
//                    std::cout << "    Bid " << i << ": <Empty>\n";
//                else
//                    std::cout << "    Bid " << i << ": Price = " << bid_prices[i]
//                              << ", Qty = " << bid_qty[i] << "\n";
//            }
//        }






    std::cout << "\n=== Test Completed ===\n";
    return 0;
}




