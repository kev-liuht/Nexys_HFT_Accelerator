#include <iostream>
#include "hls_stream.h"
#include "ap_int.h"
#include "ap_axi_sdata.h"

typedef ap_axiu<32,1,1,1> axis_word_t;

extern "C" void burst_generator(ap_uint<1> start, hls::stream<axis_word_t> &out_stream);

int main() {
    hls::stream<axis_word_t> out_stream;
    ap_uint<1> start = 0;

    std::cout << "==== Starting Test Bench ====" << std::endl;

    burst_generator(start, out_stream);
    if (!out_stream.empty()) {
        std::cerr << "Error: Unexpected data output at cycle 0" << std::endl;
        return 1;
    } else {
        std::cout << "Cycle 0: No output, as expected (start=0)" << std::endl;
    }

    start = 1;
    burst_generator(start, out_stream);

    int total_words = 0;
    for (int cycle = 2; cycle < 100; cycle++) {
        burst_generator(start, out_stream);
        while (!out_stream.empty()) {
            axis_word_t w = out_stream.read();
            std::cout << "Cycle " << cycle << " -> Word " << total_words
                      << " : " << w.data.to_uint();
            if (w.last) {
                std::cout << " [tlast]";
            }
            std::cout << std::endl;
            total_words++;
        }
    }
    std::cout << "Sent total of " << total_words << " words in the burst." << std::endl;

    burst_generator(start, out_stream);
    if (!out_stream.empty()) {
        std::cerr << "Error: Got new data even though start was never toggled low" << std::endl;
        return 1;
    } else {
        std::cout << "No new data, as expected (still start=1, burst already sent)" << std::endl;
    }

    start = 0;
    burst_generator(start, out_stream);
    start = 1;
    burst_generator(start, out_stream);

    int second_burst_count = 0;
    for (int cycle = 0; cycle < 100; cycle++) {
        burst_generator(start, out_stream);
        while (!out_stream.empty()) {
            axis_word_t w = out_stream.read();
            std::cout << "Second Burst - Word " << second_burst_count
                      << " : " << w.data.to_uint();
            if (w.last) {
                std::cout << " [tlast]";
            }
            std::cout << std::endl;
            second_burst_count++;
        }
    }
    std::cout << "Sent total of " << second_burst_count << " words in second burst." << std::endl;

    std::cout << "==== Test Bench Complete ====" << std::endl;
    return 0;
}
