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

// Givens based QR decomposition for a 4x4 matrix
void givens_qr_float(float A[N][N], float b[N]) {
    for (int i = 0; i < N; i++) {
        for (int j = i+1; j < N; j++) {
#pragma HLS PIPELINE II=1
            float a_val = A[i][i];
            float b_val = A[j][i];
            float r = hls::sqrt(a_val * a_val + b_val * b_val);
            if (r == 0.0f) { continue; }
            float c = a_val / r;
            float s = b_val / r;
            for (int k = i; k < N; k++) {
                float temp = c * A[i][k] + s * A[j][k];
                A[j][k] = -s * A[i][k] + c * A[j][k];
                A[i][k] = temp;
            }
            // Rotate b
            float tmp_b = c * b[i] + s * b[j];
            b[j] = -s * b[i] + c * b[j];
            b[i] = tmp_b;
        }
    }
}

// Back substitution
void back_substitution_float(const float A[N][N], const float b[N], float x[N]) {
    for (int i = N-1; i >= 0; i--) {
        float sum = 0.0f;
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
   normalizes the result so that the sum equals 1, clamps negative weights to zero,
   and redistributes so that the final sum is 1. Then writes out 4 words.*/
extern "C" void qr_decomp_lin_solv_axis(
    hls::stream<axis_word_t> &in_stream,
    hls::stream<axis_word_t> &out_stream
) {
#pragma HLS INTERFACE axis port=in_stream
#pragma HLS INTERFACE axis port=out_stream
#pragma HLS INTERFACE ap_ctrl_none port=return

    // Read input matrix
    float A_f[N][N];
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
#pragma HLS PIPELINE II=1
            axis_word_t in_val = in_stream.read();
            fix_t temp_fixed = to_fixed32(in_val.data);
            A_f[i][j] = (float)temp_fixed;
        }
    }

    // Define b = [1,1,1,1]
    float b_f[N] = {1.0f, 1.0f, 1.0f, 1.0f};

    // Perform QR decomposition + back substitution
    givens_qr_float(A_f, b_f);
    float x_f[N];
    for (int i = 0; i < N; i++) { x_f[i] = 0.0f; }
    back_substitution_float(A_f, b_f, x_f);

    // Normalize x so that the sum equals 1 (first pass)
    float sumx = 0.0f;
    for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
        sumx += x_f[i];
    }
    if (sumx != 0.0f) {
        for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
            x_f[i] = x_f[i] / sumx;
        }
    }

    // Clamp negative weights to zero
    for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
        if (x_f[i] < 0.0f) {
            x_f[i] = 0.0f;
        }
    }

    // Redistribute to ensure final sum = 1 among remaining positive weights
    float sumPos = 0.0f;
    for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
        sumPos += x_f[i];
    }
    if (sumPos != 0.0f) {
        for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
            x_f[i] = x_f[i] / sumPos;
        }
    }

    // Convert float results back to Q16.16 and write out
    for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
        axis_word_t out_val;
        fix_t out_fixed = (fix_t)x_f[i];
        out_val.data = to_uint32(out_fixed);
        out_val.last = (i == 3) ? ap_uint<1>(1) : ap_uint<1>(0);
        out_stream.write(out_val);
    }
}
