#include <hls_stream.h>
#include <ap_int.h>
#include <ap_fixed.h>
#include <hls_math.h>

typedef ap_fixed<32,16> fix_t;

struct axis_word_t {
    ap_uint<32> data;
    ap_uint<1>  last;
};

#define N 4

// Conversion routines
ap_uint<32> to_uint32(const fix_t &val) {
    ap_uint<32> bits;
    bits = val.range(31, 0);
    return bits;
}

fix_t to_fixed32(const ap_uint<32> &bits) {
    fix_t val;
    val.range(31,0) = bits;
    return val;
}

// Givens based QR decomposition for the input 4x4 covariance matrix
void givens_qr_4x4_fixed_hls(fix_t A[4][4], fix_t b[4]) {
    for (int i = 0; i < N; i++) {
        for (int j = i+1; j < N; j++) {
#pragma HLS PIPELINE II=1
            fix_t a_val = A[i][i];
            fix_t b_val = A[j][i];
            fix_t r = hls::sqrt(a_val * a_val + b_val * b_val);
            if (r == 0) { continue; }
            fix_t c = a_val / r;
            fix_t s = b_val / r;
            for (int k = i; k < N; k++) {
                fix_t temp = c * A[i][k] + s * A[j][k];
                A[j][k] = -s * A[i][k] + c * A[j][k];
                A[i][k] = temp;
            }
            // Rotate b
            fix_t tmp_b = c * b[i] + s * b[j];
            b[j] = -s * b[i] + c * b[j];
            b[i] = tmp_b;
        }
    }
}

// Back substitution
void back_substitution_4x4_fixed(const fix_t A[4][4], const fix_t b[4], fix_t x[4]) {
    for (int i = N-1; i >= 0; i--) {
        fix_t sum = 0;
        for (int j = i+1; j < N; j++) {
#pragma HLS PIPELINE II=1
            sum += A[i][j] * x[j];
        }
        x[i] = (b[i] - sum) / A[i][i];
    }
}

// Top level function.
/* Reads a 4x4 matrix (row major order) from the input stream,
   sets b = [1,1,1,1], performs QR decomposition and back substitution,
   normalizes the result so that the sum equals 1, and writes out 4 words.*/
extern "C" void qr_decomp_lin_solv_axis(
    hls::stream<axis_word_t> &in_stream,
    hls::stream<axis_word_t> &out_stream
) {
#pragma HLS INTERFACE axis port=in_stream
#pragma HLS INTERFACE axis port=out_stream
#pragma HLS INTERFACE ap_ctrl_hs port=return

    fix_t K[4][4];
    // Read 16 matrix elements from in_stream
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
#pragma HLS PIPELINE II=1
            axis_word_t in_val = in_stream.read();
            K[i][j] = to_fixed32(in_val.data);
        }
    }
    // Define b = [1,1,1,1].
    fix_t b[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    givens_qr_4x4_fixed_hls(K, b);

    // Solve for x using back substitution
    fix_t x[4];
    for (int i = 0; i < 4; i++) { x[i] = 0; }
    back_substitution_4x4_fixed(K, b, x);

    // Normalize x so that the sum equals 1
    fix_t sumx = 0;
    for (int i = 0; i < 4; i++) {
#pragma HLS PIPELINE II=1
        sumx += x[i];
    }
    if (sumx != 0) {
        for (int i = 0; i < 4; i++) {
#pragma HLS PIPELINE II=1
            x[i] = x[i] / sumx;
        }
    }

    // Write out the normalized weights
    for (int i = 0; i < 4; i++) {
#pragma HLS PIPELINE II=1
        axis_word_t out_val;
        out_val.data = to_uint32(x[i]);
        out_val.last = (i == 3) ? ap_uint<1>(1) : ap_uint<1>(0);
        out_stream.write(out_val);
    }
}
