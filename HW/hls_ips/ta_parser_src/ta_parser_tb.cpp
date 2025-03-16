#include "C:\Users\boris\AppData\Roaming\Xilinx\Vivado\ta_parser\ta_parser.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

// Helper function to decode the raw 32-bit float as a float value
float decode_float(const ap_uint<32> &bits) {
    union {
        uint32_t u32;
        float f;
    } converter;

    converter.u32 = bits.to_uint();
    return converter.f;
}

int main() {
    hls::stream<axis_word_t> in_stream;
    hls::stream<axis_word_t> out_stream_cov;
    hls::stream<axis_word_t> out_stream_og;

    // Test input data
    axis_word_t input_data[NUM_STOCKS * NUM_LEVELS * 4];

    int index = 0;
    for (int i = 0; i < NUM_STOCKS; i++) {
        input_data[index].data = 1010000; // 101.00
        input_data[index].last = 0;
        index++;

        input_data[index].data = 50; // Quantity 50
        input_data[index].last = 0;
        index++;

        input_data[index].data = 1015000; // 101.50
        input_data[index].last = 0;
        index++;

        input_data[index].data = 40; // Quantity 40
        input_data[index].last = 0;
        index++;

        input_data[index].data = 1020000; // 102.00
        input_data[index].last = 0;
        index++;

        input_data[index].data = 30; // Quantity 30
        input_data[index].last = 0;
        index++;

        input_data[index].data = 1025000; // 102.50
        input_data[index].last = 0;
        index++;

        input_data[index].data = 20; // Quantity 20
        input_data[index].last = 0;
        index++;

        input_data[index].data = 1030000; // 103.00
        input_data[index].last = 0;
        index++;

        input_data[index].data = 10; // Quantity 10
        input_data[index].last = 0;
        index++;

        // Bid prices and quantities
        input_data[index].data = 1000000; // 100.00
        input_data[index].last = 0;
        index++;

        input_data[index].data = 60; // Quantity 60
        input_data[index].last = 0;
        index++;

        input_data[index].data = 995000; // 99.50
        input_data[index].last = 0;
        index++;

        input_data[index].data = 50; // Quantity 50
        input_data[index].last = 0;
        index++;

        input_data[index].data = 990000; // 99.00
        input_data[index].last = 0;
        index++;

        input_data[index].data = 40; // Quantity 40
        input_data[index].last = 0;
        index++;

        input_data[index].data = 985000; // 98.50
        input_data[index].last = 0;
        index++;

        input_data[index].data = 30; // Quantity 30
        input_data[index].last = 0;
        index++;

        input_data[index].data = 980000; // 98.00
        input_data[index].last = 0;
        index++;

        input_data[index].data = 20; // Quantity 20
        input_data[index].last = 0;
        index++;
    }

    input_data[index - 1].last = 1;
    unsigned int num_tests = 2;

    for (int t = 0; t < num_tests; t++) {
        std::cout << "Sending input " << t << "...\n";
        for (int i = 0; i < NUM_STOCKS * NUM_LEVELS * 4; i++) {
            in_stream.write(input_data[i]);
        }

        ta_parser(in_stream, out_stream_cov, out_stream_og);

        // Output Covariance Stream
        std::cout << "Output Covariance Stream:" << std::endl;
        while (!out_stream_cov.empty()) {
            axis_word_t out_data = out_stream_cov.read();

            float decoded_value = decode_float(out_data.data);  // Interpret as float

            std::cout << "Raw Data: " << std::hex << std::setw(8) << std::setfill('0') << out_data.data
                      << " | Float: " << std::fixed << std::setprecision(6) << decoded_value
                      << " | TLAST: " << out_data.last << std::endl;
        }

        // Output Order Generation Stream
        std::cout << "Output Order Generation Stream:" << std::endl;
        while (!out_stream_og.empty()) {
            axis_word_t out_data = out_stream_og.read();

            float decoded_value = decode_float(out_data.data);  // Interpret as float

            std::cout << "Raw Data: " << std::hex << std::setw(8) << std::setfill('0') << out_data.data
                      << " | Float: " << std::fixed << std::setprecision(6) << decoded_value
                      << " | TLAST: " << out_data.last << std::endl;
        }
    }

    return 0;
}
