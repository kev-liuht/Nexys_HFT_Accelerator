#ifndef COV_UPDATE_HPP
#define COV_UPDATE_HPP

#include <hls_stream.h>
#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_math.h>

#define NUM_STOCKS 4  // Number of stocks in the system

// AXI-Stream word format
struct axis_word_t {
    ap_uint<32> data; // 32-bit data field
    ap_uint<1> last;  // Last signal (1 if it's the last data in the stream)
};

// Convert 32-bit unsigned integer back to float (bitwise cast, this is different from the ta_parser convert_to_float!!)
float convert_to_float(const ap_uint<32> &bits);

// Convert float to 32-bit unsigned integer (bitwise cast)
ap_uint<32> float_to_uint32(const float &val);

extern "C" void cov_update(
    hls::stream<axis_word_t> &in_stream,
    hls::stream<axis_word_t> &out_stream
);

#endif // COV_UPDATE_HPP
