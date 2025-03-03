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

    // Define the 4x4 covariance matrix in Q16.16 hex format.
    // Matrix (row-major order):
    // Row0: 4.0,   2.0,   0.6,   1.0
    // Row1: 2.0,   3.0,   0.5,   0.8
    // Row2: 0.6,   0.5,   2.0,   0.7
    // Row3: 1.0,   0.8,   0.7,   3.0
    // Q16.16 representation:
    // 4.0   = 4 * 65536 = 0x00040000
    // 2.0   = 0x00020000
    // 0.6   = 0.6 * 65536 = 39322 = 0x0000999A
    // 1.0   = 0x00010000
    // 3.0   = 0x00030000
    // 0.5   = 0x00008000
    // 0.8   = 0.8 * 65536 = 52429 = 0x0000CCCD
    // 0.7   = 0.7 * 65536 = 45875 = 0x0000B2E3 (or use 0x0000B333 as an approximation)
    unsigned int testK_hex[4][4] = {
        {0x00040000, 0x00020000, 0x0000999A, 0x00010000},
        {0x00020000, 0x00030000, 0x00008000, 0x0000CCCD},
        {0x0000999A, 0x00008000, 0x00020000, 0x0000B333},
        {0x00010000, 0x0000CCCD, 0x0000B333, 0x00030000}
    };

    // Print the input matrix (hex values)
    std::cout << "Input Covariance Matrix (Q16.16 hex):\n";
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            std::cout << "0x" << std::hex << testK_hex[i][j] << " ";
        }
        std::cout << std::dec << "\n";
    }

    // Push the 16 matrix elements into the input stream.
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            axis_word_t val;
            val.data = testK_hex[i][j];
            val.last = (i == 3 && j == 3) ? 1 : 0;
            in_stream.write(val);
        }
    }

    // Call the QR decomposition and linear solver IP.
    qr_decomp_lin_solv_axis(in_stream, out_stream);

    // Read the output: normalized weights (4 words)
    fix_t results[4];
    for (int i = 0; i < 4; i++) {
        axis_word_t out_val = out_stream.read();
        results[i] = to_fixed32(out_val.data);
    }

    // Print computed weights as floating point and as hex.
    std::cout << "Computed Minimum Variance Weights:\n";
    for (int i = 0; i < 4; i++) {
        float weight_f = results[i].to_float();
        ap_uint<32> weight_hex = to_uint32(results[i]);
        std::cout << "w[" << i << "] = " << weight_f
                  << " (hex: 0x" << std::hex << weight_hex << std::dec << ")\n";
    }

    return 0;
}
