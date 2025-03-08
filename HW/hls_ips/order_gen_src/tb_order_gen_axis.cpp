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

static inline float fixedpt_to_float(ap_uint<32> in) {
    return (float)in.to_uint() / 10000.0f;
}

static inline ap_uint<32> float_to_apuint32(float in) {
    union {
        float f;
        uint32_t u;
    } conv;
    conv.f = in;
    return conv.u;
}

extern "C" {
    void order_gen_axis(
        hls::stream<axis_word_t>& in_stream_weights,
        hls::stream<axis_word_t>& in_stream_stock_prices,
        hls::stream<axis_word_t>& out_stream_portfolio,
        hls::stream<axis_word_t>& out_stream_ouch
    );
}

int main(){
    srand(static_cast<unsigned int>(time(0)));
    hls::stream<axis_word_t> in_stream_weights;
    hls::stream<axis_word_t> in_stream_stock_prices;
    hls::stream<axis_word_t> out_stream_portfolio;
    hls::stream<axis_word_t> out_stream_ouch;
    const int num_cycles = 20;
    for (int cycle = 0; cycle < num_cycles; cycle++){
        // Send new stock prices
        // Base prices: AAPL=51.1234, NVDA=12.3456, MSFT=78.9012, INTC=34.5678 dollars.
        float base_prices[4] = {51.1234f, 12.3456f, 78.9012f, 34.5678f};
        for (int i = 0; i < 4; i++){
            float fluctuation = 0.95f + (static_cast<float>(rand())/static_cast<float>(RAND_MAX)) * 0.10f;
            float new_price = base_prices[i] * fluctuation;
            axis_word_t price_word;
            price_word.data = float_to_apuint32(new_price);
            price_word.last = (i == 3) ? 1 : 0;
            in_stream_stock_prices.write(price_word);
        }

        // Send new weight update
        float rnd[4], sum = 0.0f, weight_vals[4];
        for (int i = 0; i < 4; i++){
            rnd[i] = static_cast<float>(rand())/static_cast<float>(RAND_MAX);
            sum += rnd[i];
        }
        cout << "Cycle " << cycle << ": Sent weights (Decimal): ";
        for (int i = 0; i < 4; i++){
            weight_vals[i] = rnd[i] / sum;
            axis_word_t weight_word;
            weight_word.data = float_to_apuint32(weight_vals[i]);
            weight_word.last = (i == 3) ? 1 : 0;
            in_stream_weights.write(weight_word);
            cout << fixed << setprecision(4) << weight_vals[i] << " ";
        }
        cout << endl;
        cout << "Cycle " << cycle << ": Sent weights (Hex): ";
        for (int i = 0; i < 4; i++){
            ap_uint<32> weight_hex = float_to_apuint32(weight_vals[i]);
            cout << setw(8) << setfill('0') << hex << weight_hex.to_uint() << " ";
        }
        cout << dec << endl;

        order_gen_axis(in_stream_weights, in_stream_stock_prices, out_stream_portfolio, out_stream_ouch);

        if(!out_stream_portfolio.empty()){
            axis_word_t port_word = out_stream_portfolio.read();
            cout << "Cycle " << cycle << " Portfolio Value: "
                 << setprecision(6) << fixed << fixedpt_to_float(port_word.data) << endl;
        } else {
            cout << "Cycle " << cycle << " No portfolio update." << endl;
        }
        if(!out_stream_ouch.empty()){
            const int words_per_order = 12;
            for (int order = 0; order < 4; order++){
                cout << "Cycle " << cycle << " Order " << order << ":" << endl;
                for (int j = 0; j < words_per_order; j++){
                    if(!out_stream_ouch.empty()){
                        axis_word_t order_word = out_stream_ouch.read();
                        cout << setw(8) << setfill('0') << hex << order_word.data.to_uint();
                        if(order_word.last == 1)
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
