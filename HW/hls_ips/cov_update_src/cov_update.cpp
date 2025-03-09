#include <iostream>
#include <cmath>
#include <hls_stream.h>
#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_math.h>

#define NUM_STOCKS 4  // Number of stocks in the system

struct axis_word_t {
    ap_uint<32> data;
    ap_uint<1> last;  // Last signal (1 if it's the last data in the stream)
};

// Converts 32-bit data to float using bitwise casting
inline float convert_to_float(const ap_uint<32> &bits) {
    union {
        uint32_t u32;
        float f;
    } converter;

    converter.u32 = bits.to_uint();
    return converter.f;
}

// Converts float to 32-bit data using bitwise casting
inline ap_uint<32> float_to_uint32(const float &val) {
    union {
        float f;
        uint32_t u32;
    } converter;

    converter.f = val;
    return converter.u32;
}

// Top function
extern "C" void cov_update(
    hls::stream<axis_word_t> &in_stream,  // Input stream
    hls::stream<axis_word_t> &out_stream  // Output stream
) {
#pragma HLS INTERFACE axis port=in_stream
#pragma HLS INTERFACE axis port=out_stream
#pragma HLS INTERFACE ap_ctrl_none port=return

    // Static variables retain values between function calls
    static float last_prices[NUM_STOCKS] = {0.0f};
    static float last_returns[NUM_STOCKS] = {0.0f};
    static float last_second_moment[NUM_STOCKS][NUM_STOCKS] = {0.0f};
    static float cov_matrix[NUM_STOCKS][NUM_STOCKS] = {0.0f};
    static unsigned int num_updates = 0;  // Number of updates (needed for incremental averaging)

    float market_prices[NUM_STOCKS]; // Store current market prices
    float returns[NUM_STOCKS] = {0.0f}; // Store computed returns

    // Read market prices from AXI-Stream input
    for (int i = 0; i < NUM_STOCKS; i++) {
#pragma HLS PIPELINE II=1
        axis_word_t temp = in_stream.read();
        market_prices[i] = convert_to_float(temp.data);
    }

    // First update: Only store prices, no calculations
    if (num_updates == 0) {
        for (int i = 0; i < NUM_STOCKS; i++) {
#pragma HLS PIPELINE II=1
            last_prices[i] = market_prices[i];
        }
        num_updates++;
        return;
    }

    unsigned int N = num_updates + 1; // Increment update counter

    // Compute returns for each stock
    for (int i = 0; i < NUM_STOCKS; i++) {
#pragma HLS UNROLL factor=2
        returns[i] = (market_prices[i] - last_prices[i]) / last_prices[i];
        last_returns[i] = (float(num_updates) * last_returns[i] + returns[i]) / float(N);  // Incremental mean update
    }

    // Update covariance matrix using incremental method
    for (int i = 0; i < NUM_STOCKS; i++) {
        for (int j = 0; j < NUM_STOCKS; j++) {
#pragma HLS UNROLL factor=2
            last_second_moment[i][j] = (float(num_updates) * last_second_moment[i][j] +
                                        returns[i] * returns[j]) / float(N);
            cov_matrix[i][j] = last_second_moment[i][j] - last_returns[i] * last_returns[j];
        }
    }

    // Update stored last prices
    for (int i = 0; i < NUM_STOCKS; i++) {
#pragma HLS PIPELINE II=1
        last_prices[i] = market_prices[i];
    }

    num_updates++;

    // Write updated covariance matrix to AXI-Stream output
    for (int i = 0; i < NUM_STOCKS; i++) {
        for (int j = 0; j < NUM_STOCKS; j++) {
#pragma HLS PIPELINE II=1
            axis_word_t temp;
            temp.data = float_to_uint32(cov_matrix[i][j]);  // Convert float to AXI-Stream format
            temp.last = (i == NUM_STOCKS - 1 && j == NUM_STOCKS - 1) ? 1 : 0;  // Set last flag at the end
            out_stream.write(temp);
        }
    }
}
