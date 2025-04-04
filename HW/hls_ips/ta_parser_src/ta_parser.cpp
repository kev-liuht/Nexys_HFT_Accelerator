#include <iostream>
#include <cmath>
#include <hls_stream.h>
#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_math.h>

#define NUM_STOCKS 4
#define NUM_LEVELS 5

struct axis_word_t {
    ap_uint<32> data;
    ap_uint<1> last;  // Last signal (1 if it's the last data in the stream)
};

// Converts 32-bit data to float
float convert_to_float(const ap_uint<32> &bits) {
    return static_cast<float>(bits.to_uint());
}

// Converts float to 32-bit data using bitwise casting
ap_uint<32> float_to_uint32(const float &val) {
    union {
        float f;
        uint32_t u32;
    } converter;

    converter.f = val;
    return converter.u32;
}

// Top function
extern "C" void ta_parser(
    hls::stream<axis_word_t> &in_stream,
    hls::stream<axis_word_t> &out_stream_cov,
    hls::stream<axis_word_t> &out_stream_og
) {
#pragma HLS INTERFACE axis port=in_stream
#pragma HLS INTERFACE axis port=out_stream_cov
#pragma HLS INTERFACE axis port=out_stream_og
#pragma HLS INTERFACE ap_ctrl_none port=return

    static unsigned int num_updates = 0;

    axis_word_t price_ask[NUM_STOCKS][NUM_LEVELS], quantity_ask[NUM_STOCKS][NUM_LEVELS];
    axis_word_t price_bid[NUM_STOCKS][NUM_LEVELS], quantity_bid[NUM_STOCKS][NUM_LEVELS];

    float market_prices[NUM_STOCKS];

    // Step 1: Read all input data into buffers
    for (int i = 0; i < NUM_STOCKS; i++) {
        for (int j = 0; j < NUM_LEVELS; j++) {
#pragma HLS PIPELINE II=1
            price_ask[i][j] = in_stream.read();
            quantity_ask[i][j] = in_stream.read();
        }
        for (int j = 0; j < NUM_LEVELS; j++) {
#pragma HLS PIPELINE II=1
            price_bid[i][j] = in_stream.read();
            quantity_bid[i][j] = in_stream.read();
        }
    }

    // Step 2: Compute weighted average market prices
    for (int i = 0; i < NUM_STOCKS; i++) {
        float ask_prices[NUM_LEVELS], bid_prices[NUM_LEVELS], ask_quantities[NUM_LEVELS], bid_quantities[NUM_LEVELS];

        // Convert prices and quantities
        for (int j = 0; j < NUM_LEVELS; j++) {
#pragma HLS PIPELINE II=1
            ask_prices[j] = convert_to_float(price_ask[i][j].data) / 10000.0f;
            bid_prices[j] = convert_to_float(price_bid[i][j].data) / 10000.0f;
            ask_quantities[j] = convert_to_float(quantity_ask[i][j].data);
            bid_quantities[j] = convert_to_float(quantity_bid[i][j].data);
        }

        // Compute weighted average market price
        float total_weight = 0.0f;
        float weighted_price = 0.0f;

        for (int j = 0; j < NUM_LEVELS; j++) {
#pragma HLS PIPELINE II=1
            total_weight += ask_quantities[j] + bid_quantities[j];
            weighted_price += ask_prices[j] * ask_quantities[j] + bid_prices[j] * bid_quantities[j];
        }

        market_prices[i] = (total_weight > 0) ? (weighted_price / total_weight) : 0.0f;
    }

    // Step 3: Stream computed market prices to outputs
    for (int i = 0; i < NUM_STOCKS; i++) {
#pragma HLS PIPELINE II=1
        axis_word_t temp;
        temp.data = float_to_uint32(market_prices[i]);
        temp.last = (i == NUM_STOCKS - 1) ? 1 : 0;  // Set last flag on last element

        out_stream_cov.write(temp);
    }

    if (num_updates > 0) {
        for (int i = 0; i < NUM_STOCKS; i++) {
#pragma HLS PIPELINE II=1
            axis_word_t temp;
            temp.data = float_to_uint32(market_prices[i]);
            temp.last = (i == NUM_STOCKS - 1) ? 1 : 0;  // Set last flag on last element

            out_stream_og.write(temp);
        }
    }

    num_updates++;
}
