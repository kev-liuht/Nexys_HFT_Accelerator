#include "C:\Users\boris\AppData\Roaming\Xilinx\Vivado\532_cov_update_1\cov_update.hpp"
#include <iostream>
#include <iomanip>

#define NUM_TESTS 5

// Helper function to decode 32-bit data as float
float decode_float(const ap_uint<32> &bits) {
    union {
        uint32_t u32;
        float f;
    } converter;

    converter.u32 = bits.to_uint();
    return converter.f;
}

int main() {
    std::cout << "Running Testbench...\n";

    hls::stream<axis_word_t> in_stream;
    hls::stream<axis_word_t> out_stream;

    // Test input data in float format
    float test_inputs[NUM_TESTS][NUM_STOCKS] = {
        {146.8f, 2780.47f, 303.835f, 108.34f},
        {147.28f, 2780.72f, 303.985f, 108.38f},
        {147.21f, 2780.99f, 303.83f, 108.545f},
        {145.45f, 2781.19f, 303.905f, 108.65f},
        {147.745f, 2981.145f, 303.78f, 108.56f}
    };

    for (int test = 0; test < NUM_TESTS; test++) {
        // Send input burst
        for (int i = 0; i < NUM_STOCKS; i++) {
            axis_word_t temp;
            temp.data = float_to_uint32(test_inputs[test][i]);
            temp.last = (i == NUM_STOCKS - 1) ? 1 : 0;
            in_stream.write(temp);
        }

        cov_update(in_stream, out_stream);

        // Display the results
        std::cout << "Output after burst " << test + 1 << ": \n";
        while (!out_stream.empty()) {
            axis_word_t temp = out_stream.read();
            float decoded_value = decode_float(temp.data);

            std::cout << "Raw Data: " << std::hex << std::setw(8) << std::setfill('0') << temp.data
                      << " | Decoded Value (float): " << std::fixed << std::setprecision(10)
                      << decoded_value << " ";

            if (temp.last) std::cout << "\n";
        }
    }

    return 0;
}
