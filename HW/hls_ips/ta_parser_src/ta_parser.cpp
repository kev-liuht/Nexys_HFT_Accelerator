#include "C:\Users\boris\AppData\Roaming\Xilinx\Vivado\ta_parser\ta_parser.hpp"

ap_fixed<64,32> convert_to_fixed64(const ap_uint<32> &bits) {
    static const ap_fixed<64,32> scaler = 10000.0;
    ap_fixed<64,32> temp = ap_fixed<64,32>(bits);  // Convert to UQ32.32
    return temp / scaler;
}

ap_uint<32> to_uint32(const ap_fixed<64,32> &val) {
    ap_uint<32> bits;
    bits = val.range(47,16);
    return bits;
}

// Top function
extern "C" void ta_parser(
    hls::stream<axis_word_t> &in_stream,      // Input stream (Order Book Data)
    hls::stream<axis_word_t> &out_stream_cov, // Output stream (Covariance Update)
    hls::stream<axis_word_t> &out_stream_og   // Output stream (Order Generation)
) {
#pragma HLS INTERFACE axis port=in_stream
#pragma HLS INTERFACE axis port=out_stream_cov
#pragma HLS INTERFACE axis port=out_stream_og
#pragma HLS INTERFACE ap_ctrl_none port=return
	static unsigned int num_updates = 0;

    axis_word_t price_ask[NUM_STOCKS][5], quantity_ask[NUM_STOCKS][5];
    axis_word_t price_bid[NUM_STOCKS][5], quantity_bid[NUM_STOCKS][5];

    ap_fixed<64,32> market_prices[NUM_STOCKS];

    // Step 1: Read all input data into buffers
    for (int i = 0; i < NUM_STOCKS; i++) {
        for (int j = 0; j < 5; j++) {
            price_ask[i][j] = in_stream.read();
            quantity_ask[i][j] = in_stream.read();
        }
        for (int j = 0; j < 5; j++) {
            price_bid[i][j] = in_stream.read();
            quantity_bid[i][j] = in_stream.read();
        }
    }

    // Step 2: Compute weighted average market prices
    for (int i = 0; i < NUM_STOCKS; i++) {
    	ap_fixed<64,32> ask_prices[5], bid_prices[5], ask_quantities[5], bid_quantities[5];

        // Convert prices and quantities
        for (int j = 0; j < 5; j++) {
            ask_prices[j] = convert_to_fixed64(price_ask[i][j].data);
            bid_prices[j] = convert_to_fixed64(price_bid[i][j].data);
            ask_quantities[j] = ap_fixed<64,32>(quantity_ask[i][j].data);
            bid_quantities[j] = ap_fixed<64,32>(quantity_bid[i][j].data);
        }

        // Compute weighted average market price
        ap_fixed<64,32> total_weight = 0;
        ap_fixed<64,32> weighted_price = 0;

        for (int j = 0; j < 5; j++) {
            total_weight += ask_quantities[j] + bid_quantities[j];
            weighted_price += ask_prices[j] * ask_quantities[j] + bid_prices[j] * bid_quantities[j];
        }

        market_prices[i] = weighted_price / total_weight;
    }

    // Step 3: Stream computed market prices to outputs
    for (int i = 0; i < NUM_STOCKS; i++) {
        axis_word_t temp;
        temp.data = to_uint32(market_prices[i]);
        temp.last = (i == NUM_STOCKS - 1) ? 1 : 0;  // Set last flag on last element

        out_stream_cov.write(temp);
    }

    if (num_updates > 0) {
    	for (int i = 0; i < NUM_STOCKS; i++) {
    		axis_word_t temp;
			temp.data = to_uint32(market_prices[i]);
			temp.last = (i == NUM_STOCKS - 1) ? 1 : 0;  // Set last flag on last element

			out_stream_og.write(temp);
    	}
    }
    num_updates++;
}
