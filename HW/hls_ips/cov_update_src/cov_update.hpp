#ifndef COV_UPDATE_HPP
#define COV_UPDATE_HPP

#include <hls_stream.h>
#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <ap_fixed.h>
#include <hls_math.h>

#define NUM_STOCKS 4  // Number of stocks in the system

// 32-bit fixed-point type with 16 integer bits and 16 fractional bits
typedef ap_fixed<32, 16> fix_t;

// AXI-Stream word format
struct axis_word_t {
    ap_uint<32> data; // 32-bit data field
    ap_uint<1> last;  // Last signal (1 if it's the last data in the stream)
};

// Convert fixed-point to 32-bit unsigned integer
ap_uint<32> to_uint32(const fix_t &val);

// Convert 32-bit unsigned integer back to fixed-point
fix_t to_fixed32(const ap_uint<32> &bits);

extern "C" void cov_update(
    hls::stream<axis_word_t> &in_stream,
    hls::stream<axis_word_t> &out_stream
);

#endif // COV_UPDATE_HPP
