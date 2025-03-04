#include "C:\Users\boris\AppData\Roaming\Xilinx\Vivado\532_cov_update_1\cov_update.hpp"

fix_t to_fixed32(const ap_uint<32> &bits) {
	fix_t temp;
	temp.range(31,0) = bits;
    return temp;
}

ap_uint<32> to_uint32(const fix_t &val) {
    ap_uint<32> temp;
    temp = val.range(31,0);
    return temp;
}

// Top function for covariance matrix update with AXI-Stream interface
extern "C" void cov_update(
    hls::stream<axis_word_t> &in_stream,  // Input stream
    hls::stream<axis_word_t> &out_stream  // Output stream
) {
#pragma HLS INTERFACE axis port=in_stream
#pragma HLS INTERFACE axis port=out_stream
#pragma HLS INTERFACE ap_ctrl_none port=return

    // Static variables retain values between function calls
    static fix_t last_prices[NUM_STOCKS] = {0};
    static fix_t last_returns[NUM_STOCKS] = {0};
    static fix_t last_second_moment[NUM_STOCKS][NUM_STOCKS] = {0};
    static fix_t cov_matrix[NUM_STOCKS][NUM_STOCKS] = {0};
    static int num_updates = 0;  // Number of updates (needed for incremental averaging)

    fix_t market_prices[NUM_STOCKS]; // Store current market prices
    fix_t returns[NUM_STOCKS] = {0}; // Store computed returns

    // Read market prices from AXI-Stream input
    for (int i = 0; i < NUM_STOCKS; i++) {
        axis_word_t temp = in_stream.read();
        market_prices[i] = to_fixed32(temp.data);
    }

    // First update: Only store prices, no calculations
    if (num_updates == 0) {
        for (int i = 0; i < NUM_STOCKS; i++) {
            last_prices[i] = market_prices[i];
        }
        num_updates++;
        return;
    }

    int N = num_updates + 1; // Increment update counter

    // Compute returns for each stock
    for (int i = 0; i < NUM_STOCKS; i++) {
        returns[i] = (market_prices[i] - last_prices[i]) / last_prices[i] - 1;
        last_returns[i] = (num_updates * last_returns[i] + returns[i]) / N;  // Incremental mean update
    }

    // Update covariance matrix using incremental method
    for (int i = 0; i < NUM_STOCKS; i++) {
        for (int j = 0; j < NUM_STOCKS; j++) {
            last_second_moment[i][j] = (num_updates * last_second_moment[i][j] + returns[i] * returns[j]) / N;
            cov_matrix[i][j] = last_second_moment[i][j] - last_returns[i] * last_returns[j];
        }
    }

    // Update stored last prices
    for (int i = 0; i < NUM_STOCKS; i++) {
        last_prices[i] = market_prices[i];
    }

    num_updates++;

    // Write updated covariance matrix to AXI-Stream output
    for (int i = 0; i < NUM_STOCKS; i++) {
        for (int j = 0; j < NUM_STOCKS; j++) {
            axis_word_t temp;
            temp.data = to_uint32(cov_matrix[i][j]);  // Convert from fixed-point to AXI-Stream format
            temp.last = (i == NUM_STOCKS - 1 && j == NUM_STOCKS - 1) ? 1 : 0;  // Set last flag at the end
            out_stream.write(temp);
        }
    }
}
