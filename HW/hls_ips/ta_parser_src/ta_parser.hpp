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

float convert_to_float(const ap_uint<32> &bits);

ap_uint<32> float_to_uint32(const float &val);

extern "C" void ta_parser(
    hls::stream<axis_word_t> &in_stream,      // Input stream (Order Book Data)
    hls::stream<axis_word_t> &out_stream_cov, // Output stream (Covariance Update)
    hls::stream<axis_word_t> &out_stream_og   // Output stream (Order Generation)
);
