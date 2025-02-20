#include <iostream>
#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_stream.h"

typedef ap_fixed<32,16> fix_t;

struct axis_word_t {
    ap_uint<32> data;
    ap_uint<1>  last;
};

extern ap_uint<32> to_uint32(const fix_t &val);
extern fix_t to_fixed32(const ap_uint<32> &bits);

extern "C" void qr_decomp_lin_solv_axis(
    hls::stream<axis_word_t> &in_stream,
    hls::stream<axis_word_t> &out_stream
);

int main() {
    hls::stream<axis_word_t> in_stream("in_stream");
    hls::stream<axis_word_t> out_stream("out_stream");

    // Example 4x4 matrix (row major order).
    fix_t testK[4][4] = {
        {4.0, 2.0, 0.6, 1.0},
        {2.0, 3.0, 0.5, 0.8},
        {0.6, 0.5, 2.0, 0.7},
        {1.0, 0.8, 0.7, 3.0}
    };

    // Push the 16 matrix elements onto in_stream
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            axis_word_t val;
            val.data = to_uint32(testK[i][j]);
            // For inputs, mark all words with last = 0
            val.last = 0;
            in_stream.write(val);
        }
    }

    // Call DUT
    qr_decomp_lin_solv_axis(in_stream, out_stream);

    // Read output
    fix_t results[4];
    for (int i = 0; i < 4; i++) {
        axis_word_t val_out = out_stream.read();
        results[i] = to_fixed32(val_out.data);
    }

    // Print computed minimum variance weights
    std::cout << "Computed min var weights:" << std::endl;
    for (int i = 0; i < 4; i++) {
        std::cout << "  w[" << i << "] = " << results[i].to_float() << std::endl;
    }
    return 0;
}
