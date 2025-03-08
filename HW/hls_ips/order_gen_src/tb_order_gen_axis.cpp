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
    // Seed random number generator.
    srand(static_cast<unsigned int>(time(0)));

    hls::stream<ap_uint<32> > in_stream_weights;
    hls::stream<ap_uint<32> > in_stream_stock_prices;
    hls::stream<axis_word_t> out_stream_portfolio;
    hls::stream<axis_word_t> out_stream_ouch;

    const int num_cycles = 20;
    for (int cycle = 0; cycle < num_cycles; cycle++) {
        // With ~33% chance, send a new weight update.
        bool update_weights = (rand() % 3 == 0);
        if (update_weights) {
            // Generate 4 random weights that sum to 1.
            float rnd[4];
            float sum = 0.0f;
            for (int i = 0; i < 4; i++) {
                rnd[i] = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                sum += rnd[i];
            }
            for (int i = 0; i < 4; i++) {
                float weight = rnd[i] / sum;
                // Convert to Q16.16 fixed-point.
                ap_uint<32> weight_q16 = (ap_uint<32>)(weight * 65536);
                in_stream_weights.write(weight_q16);
            }
            cout << "Cycle " << cycle << ": Weight update." << endl;
        } else {
            cout << "Cycle " << cycle << ": No weight update." << endl;
        }

        // Always send new price updates.
        // Base prices: AAPL=511234, NVDA=123456, MSFT=789012, INTC=345678.
        ap_uint<32> base_prices[4] = {511234, 123456, 789012, 345678};
        for (int i = 0; i < 4; i++) {
            float fluctuation = 0.95f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.10f;
            float new_price = base_prices[i] * fluctuation;
            ap_uint<32> price_val = (ap_uint<32>)(new_price);
            in_stream_stock_prices.write(price_val);
        }

        // Call the DUT.
        order_gen_axis(in_stream_weights, in_stream_stock_prices, out_stream_portfolio, out_stream_ouch);

        // Portfolio update
        if (!out_stream_portfolio.empty()) {
            axis_word_t port_word = out_stream_portfolio.read();
            cout << "Cycle " << cycle << " Portfolio Value: " << port_word.data.to_uint() << endl;
        } else {
            cout << "Cycle " << cycle << " No portfolio update." << endl;
        }

        // Orders are generated only if a new weight update was provided.
        if (!out_stream_ouch.empty()) {
            const int words_per_order = 12;
            for (int order = 0; order < 4; order++) {
                cout << "Cycle " << cycle << " Order " << order << ":" << endl;
                for (int j = 0; j < words_per_order; j++) {
                    if (!out_stream_ouch.empty()) {
                        axis_word_t order_word = out_stream_ouch.read();
                        cout << setw(8) << setfill('0') << hex << order_word.data.to_uint();
                        if (order_word.last == 1)
                            cout << " (last)";
                        cout << " ";
                    }
                }
                cout << dec << endl;
            }
        } else {
            cout << "Cycle " << cycle << " No orders generated." << endl;
        }
        cout << "-------------------------------------" << endl;
    }

    return 0;
}
