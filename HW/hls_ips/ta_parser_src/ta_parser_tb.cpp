#include "C:\Users\boris\AppData\Roaming\Xilinx\Vivado\ta_parser\ta_parser.hpp"

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

    for (int i = 0; i < NUM_STOCKS * NUM_LEVELS * 4; i++) {
        in_stream.write(input_data[i]);
    }

    ta_parser(in_stream, out_stream_cov, out_stream_og);

    std::cout << "Output Covariance Stream:" << std::endl;
    while (!out_stream_cov.empty()) {
        axis_word_t out_data = out_stream_cov.read();

        ap_fixed<32,16> decoded_value;
        decoded_value.range(31,0) = out_data.data;

        std::cout << "Data: " << std::hex << out_data.data
                  << ", Numerical Value: " << std::dec << decoded_value
                  << ", TLAST: " << out_data.last << std::endl;
    }

    // Read output data from the order generation stream
    std::cout << "Output Order Generation Stream:" << std::endl;
    while (!out_stream_og.empty()) {
        axis_word_t out_data = out_stream_og.read();

        ap_fixed<32,16> decoded_value;
        decoded_value.range(31,0) = out_data.data;

        std::cout << "Data: " << std::hex << out_data.data
                  << ", Numerical Value: " << std::dec << decoded_value
                  << ", TLAST: " << out_data.last << std::endl;
    }

    return 0;
}
