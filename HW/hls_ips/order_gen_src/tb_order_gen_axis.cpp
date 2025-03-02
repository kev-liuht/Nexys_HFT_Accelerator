#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_stream.h"

using namespace std;

struct axis_word_t {
    ap_uint<32> data;
    ap_uint<1>  last;
};

extern "C" {
    void order_gen_axis(
        hls::stream<ap_uint<32> >& in_stream_weights,
        hls::stream<ap_uint<32> >& in_stream_stock_prices,
        hls::stream<axis_word_t>& out_stream_portfolio,
        hls::stream<axis_word_t>& out_stream_ouch
    );
}

int main() {
    // Seed for random number generator.
    srand(static_cast<unsigned int>(time(0)));

    hls::stream<ap_uint<32> > in_stream_weights;
    hls::stream<ap_uint<32> > in_stream_stock_prices;
    hls::stream<axis_word_t> out_stream_portfolio;
    hls::stream<axis_word_t> out_stream_ouch;

    const int num_cycles = 10;
    for (int cycle = 0; cycle < num_cycles; cycle++) {
        // Generate 4 random weights (normalized to sum to 1).
        float rnd[4];
        float sum = 0.0f;
        for (int i = 0; i < 4; i++) {
            rnd[i] = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            sum += rnd[i];
        }
        for (int i = 0; i < 4; i++) {
            float weight = rnd[i] / sum;
            // Convert to Q16.16 fixed-point: weight * 65536.
            ap_uint<32> weight_q16 = (ap_uint<32>)(weight * 65536);
            in_stream_weights.write(weight_q16);
        }

        // Generate new stock prices with a random fluctuation.
        // Base prices: AAPL=511234, NVDA=123456, MSFT=789012, GOOG=345678.
        ap_uint<32> base_prices[4] = {511234, 123456, 789012, 345678};
        for (int i = 0; i < 4; i++) {
            // Random fluctuation between 95% and 105%.
            float fluctuation = 0.95f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.10f;
            float new_price = base_prices[i] * fluctuation;
            ap_uint<32> price_val = (ap_uint<32>)(new_price);
            in_stream_stock_prices.write(price_val);
        }

        // Call the DUT.
        order_gen_axis(in_stream_weights, in_stream_stock_prices, out_stream_portfolio, out_stream_ouch);

        // Print current portfolio value.
        if (!out_stream_portfolio.empty()) {
            axis_word_t portfolio_word = out_stream_portfolio.read();
            cout << "Cycle " << cycle << " Portfolio Value (price*10000 format): "
                 << portfolio_word.data.to_uint() << endl;
        }

        // There should be 4 orders (one per stock), each composed of 12 words.
        const int words_per_order = 12;
        for (int order = 0; order < 4; order++) {
            cout << "Cycle " << cycle << " Order " << order << " (12 words):" << endl;
            for (int j = 0; j < words_per_order; j++) {
                if (!out_stream_ouch.empty()) {
                    axis_word_t order_word = out_stream_ouch.read();
                    // Print each 32-bit word as an 8-digit hex number.
                    cout << std::setw(8) << std::setfill('0') << std::hex
                         << order_word.data.to_uint();
                    if (order_word.last == 1)
                        cout << " (last)";
                    cout << " ";
                }
            }
            cout << std::dec << endl;
        }
        cout << "-------------------------------------" << endl;
    }

    return 0;
}
