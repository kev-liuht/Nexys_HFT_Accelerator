#include "hls_stream.h"
#include "ap_int.h"
#include "ap_axi_sdata.h"

typedef ap_axiu<32,1,1,1> axi_word;

static ap_uint<32> prng_seed = 0x12345678;
unsigned int xorshift_rand() {
    prng_seed = (prng_seed << 13) ^ (prng_seed >> 17) ^ prng_seed;
    return prng_seed;
}

void burst_generator(ap_uint<1> start, hls::stream<axi_word>& out_stream) {
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS INTERFACE axis        port=out_stream

    static bool sent = false;

    if (start == 1 && !sent) {
        const int SIZE = 80;
        unsigned int base_data[SIZE] = {
             800342,    12,   1215000,    13,    210500,    65,   2125000,    87,
            1030023,    34,   1000234,    42,    995000,    45,    900234,    48,
             885000,     5,    800234,    52,    410400,    35,   1325000,    89,
             400342,    30,   1025034,    63,   1233400,    57,   1110123,    23,
             295000,    12,    370046,    64,    885023,    86,    802300,    53,
            1210034,    43,   5015067,     7,   1022300,    97,   4324000,    56,
             330034,    56,   1602344,    52,    995012,    12,    900012,    35,
             885034,    78,    560456,    12,    710023,    43,   1713000,    65,
            1520004,    75,   6025000,    32,   1130065,    54,    912400,    58,
             995034,    56,    700424,    42,    885045,    65,    802000,    18
        };

        for (int i = 0; i < SIZE; i++) {
#pragma HLS PIPELINE II=1
			unsigned int base = base_data[i];
			unsigned int variation = (base * 20) / 100;
			if (variation == 0 && base > 0) {
				variation = 1;
			}

			unsigned int rnd = xorshift_rand();
			bool sign_bit = ((rnd & 1) == 1);

			unsigned int new_val;
			if (sign_bit) {
				new_val = base + variation;
			} else {
				new_val = (base > variation) ? (base - variation) : 0;
			}

			axi_word w;
			w.data = new_val;
			w.keep = 0xF;
			w.last = (i == SIZE - 1) ? 1 : 0;
			out_stream.write(w);
		}
        sent = true;
    }
    else if (start == 0) {
        sent = false;
    }
}
