#include "C:\Users\boris\AppData\Roaming\Xilinx\Vivado\532_cov_update_1\cov_update.hpp"
#include <iostream>

#define NUM_TESTS 5

int main() {
    std::cout << "Running Testbench...\n";

    hls::stream<axis_word_t> in_stream;
    hls::stream<axis_word_t> out_stream;

    fix_t test_inputs[5][NUM_STOCKS] = {
    	{146.8, 2780.47, 303.835, 108.34},
		{147.28, 2780.72, 303.985, 108.38},
		{147.21, 2780.99, 303.83, 108.545},
		{145.45, 2781.19, 303.905, 108.65},
		{147.745, 2981.145, 303.78, 108.56}
    };

    for (int test = 0; test < NUM_TESTS; test++) {
    	// Send input burst
    	for (int i = 0; i < NUM_STOCKS; i++) {
			axis_word_t temp;
			temp.data = to_uint32(test_inputs[test][i]);
			temp.last = (i == NUM_STOCKS - 1) ? 1 : 0;
			in_stream.write(temp);
    	}

		cov_update(in_stream, out_stream);
		std::cout << "Output after burst " << test+1 << ": \n";
		while (!out_stream.empty()) {
			axis_word_t temp = out_stream.read();
			fix_t val = to_fixed32(temp.data);
			std::cout << val.to_float() << " ";
			if (temp.last) std::cout << "\n";
		}
    }
    return 0;
}
