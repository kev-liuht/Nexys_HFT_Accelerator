#include <iostream>
#include <iomanip>
#include "ap_int.h"
#include "hls_stream.h"

struct axis_word_t {
    ap_uint<32> data;
    ap_uint<1>  last;
};

// Union-based helper to read/write float bits.
static inline float bits_to_float(ap_uint<32> bits) {
    union {
        unsigned int u;
        float f;
    } converter;
    converter.u = (unsigned int)(bits);
    return converter.f;
}

static inline ap_uint<32> float_to_bits(float val) {
    union {
        float f;
        unsigned int u;
    } converter;
    converter.f = val;
    return converter.u;
}

extern "C" void qr_decomp_lin_solv_axis(
    hls::stream<axis_word_t> &in_stream,
    hls::stream<axis_word_t> &out_stream
);

int main() {
    hls::stream<axis_word_t> in_stream("in_stream");
    hls::stream<axis_word_t> out_stream("out_stream");

    // Example 4x4 matrix in float.
    float testK[4][4] = {
        { -0.00428772f,  0.00657654f,  0.00419617f, -0.00871277f },
        {  0.00880432f, -0.01359558f, -0.00871277f,  0.01789856f },
        {  0.00433350f, -0.00671387f, -0.00428772f,  0.00880432f },
        { -0.00671387f,  0.01028442f,  0.00657654f, -0.01359558f }
    };

    // Push the 16 matrix elements into the input stream as float bits
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            axis_word_t val;
            val.data = float_to_bits(testK[i][j]);
            val.last = (i == 3 && j == 3) ? 1 : 0;
            in_stream.write(val);
        }
    }

    // Call the IP
    qr_decomp_lin_solv_axis(in_stream, out_stream);

    // Read the output: normalized weights (4 words)
    float results[4];
    for (int i = 0; i < 4; i++) {
        axis_word_t out_val = out_stream.read();
        results[i] = bits_to_float(out_val.data);
    }

    // Print computed weights
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Computed Weights (clamped & re-normalized):\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "w[" << i << "] = " << results[i] << "\n";
    }

    float sum_of_w = 0.0f;
    for (int i = 0; i < 4; i++) {
        sum_of_w += results[i];
    }
    std::cout << "Sum of w = " << sum_of_w << "\n";

    return 0;
}
