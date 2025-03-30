#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_stream.h"
#include <stdint.h>
#include <hls_math.h>

#define NUM_STOCKS         4
#define ORDER_MSG_WORDS    12
#define ORDER_MSG_BYTES    48

struct axis_word_t {
    ap_uint<32> data;
    ap_uint<1>  last;
};

// Convert a float to its 32-bit representation
static inline ap_uint<32> float_to_apuint32(float in) {
    union {
        float f;
        uint32_t u;
    } conv;
    conv.f = in;
    return conv.u;
}

// Convert a 32-bit value to float.
static inline float apuint32_to_float(ap_uint<32> in) {
    union {
        float f;
        uint32_t u;
    } conv;
    conv.u = in.to_uint();
    return conv.f;
}

// Convert a float dollar value to fixed-point (price*10000)
static inline ap_uint<32> float_to_fixedpt(float in) {
    return (ap_uint<32>)(in * 10000.0f);
}

// Pack a 48-byte OUCH message into 12 32-bit words.
void pack_order(
    unsigned int userRefNum,
    char side,
    unsigned int quantity,
    const char symbol[9],
    float price,
    ap_uint<32> order_msg[ORDER_MSG_WORDS]
)
{
    unsigned char msg[ORDER_MSG_BYTES];
#pragma HLS ARRAY_PARTITION variable=msg complete dim=1

    // Initialize message buffer.
    for (int i = 0; i < ORDER_MSG_BYTES; i++) {
#pragma HLS PIPELINE II=1
        msg[i] = 0;
    }
    msg[0] = 'O';
    msg[1] = (userRefNum >> 24) & 0xFF;
    msg[2] = (userRefNum >> 16) & 0xFF;
    msg[3] = (userRefNum >> 8) & 0xFF;
    msg[4] = userRefNum & 0xFF;
    msg[5] = side;
    msg[6] = (quantity >> 24) & 0xFF;
    msg[7] = (quantity >> 16) & 0xFF;
    msg[8] = (quantity >> 8) & 0xFF;
    msg[9] = quantity & 0xFF;
    for (int i = 0; i < 8; i++) {
#pragma HLS PIPELINE II=1
        msg[10+i] = symbol[i];
    }
    // Convert price (float dollars) to fixed-point.
    ap_uint<32> price_val = float_to_fixedpt(price);
    msg[18] = 0; msg[19] = 0; msg[20] = 0; msg[21] = 0;
    msg[22] = (price_val >> 24) & 0xFF;
    msg[23] = (price_val >> 16) & 0xFF;
    msg[24] = (price_val >> 8) & 0xFF;
    msg[25] = price_val & 0xFF;
    msg[26] = '0'; msg[27] = 'Y'; msg[28] = 'P'; msg[29] = 'Y'; msg[30] = 'N';
    const char dummyClOrdID[15] = { 'C','L','O','R','D','_','I','D','0','0','1','X','X','X' };
    for (int i = 0; i < 14; i++) {
#pragma HLS PIPELINE II=1
        msg[31+i] = dummyClOrdID[i];
    }
    msg[45] = 0; msg[46] = 0;
    for (int i = 0; i < ORDER_MSG_WORDS; i++) {
#pragma HLS PIPELINE II=1
        int idx = i * 4;
        order_msg[i] = ((ap_uint<32>)msg[idx] << 24) |
                       ((ap_uint<32>)msg[idx+1] << 16) |
                       ((ap_uint<32>)msg[idx+2] << 8) |
                       ((ap_uint<32>)msg[idx+3]);
    }
}

// Top-level function.
/* Reads new weight and price values from the in-streams. Computes the new portfolio value
   using the latest prices and outputs it as fixed-point. Then, using the new weight vector,
   generates OUCH orders and updates internal state. */
extern "C" {
void order_gen_axis(
    hls::stream<axis_word_t>& in_stream_weights,            // 4 words; float weights (0-1)
    hls::stream<axis_word_t>& in_stream_stock_prices,       // 4 words; float prices
    hls::stream<axis_word_t>& out_stream_portfolio_ouch     // 1 word; fixed-point portfolio value (price*10000)
															// 12 words; ouch order message
)
{
#pragma HLS INTERFACE axis port=in_stream_weights
#pragma HLS INTERFACE axis port=in_stream_stock_prices
#pragma HLS INTERFACE axis port=out_stream_portfolio_ouch
#pragma HLS INTERFACE ap_ctrl_none port=return

    // Internal state: holdings (shares) and cash (dollars)
    static float holdings[NUM_STOCKS] = {0.0f, 0.0f, 0.0f, 0.0f};
    static float cash = 10000.0f; // initially 10,000 dollars
    static unsigned int userRefNum = 1;
    static float latched_weights[NUM_STOCKS] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Read new stock prices
    float price_vals[NUM_STOCKS];
#pragma HLS ARRAY_PARTITION variable=price_vals complete dim=1
    for (int i = 0; i < NUM_STOCKS; i++){
#pragma HLS PIPELINE II=1
        axis_word_t in_price_val = in_stream_stock_prices.read();
        price_vals[i] = apuint32_to_float(in_price_val.data);
    }

    // Read new weight vector
    float weight_vals[NUM_STOCKS];
#pragma HLS ARRAY_PARTITION variable=weight_vals complete dim=1
    for (int i = 0; i < NUM_STOCKS; i++){
#pragma HLS PIPELINE II=1
        axis_word_t in_weight_val = in_stream_weights.read();
        float val = apuint32_to_float(in_weight_val.data);
        //weight_vals[i] = hls::isnan(val) ? latched_weights[i] : val;
        if (val > 1 || val < 0) {
        	return; // return if in_stream weights are invalid, skip cycle
        }
        weight_vals[i] = val;
        latched_weights[i] = weight_vals[i];
    }

    // Compute portfolio value = cash + sum(holdings * price)
    float portfolio_value = cash;
    for (int i = 0; i < NUM_STOCKS; i++){
#pragma HLS PIPELINE II=1
        portfolio_value += holdings[i] * price_vals[i];
    }
    axis_word_t port_word;
    // Convert portfolio value to fixed-point (multiply by 10000) and send
    port_word.data = float_to_fixedpt(portfolio_value);
    out_stream_portfolio_ouch.write(port_word);

    // Generate orders using the new weight vector.
    float new_holdings[NUM_STOCKS];
    float total_cost = 0.0f;
    const char symbols[NUM_STOCKS][9] = {"AMD_    ", "JPM_    ", "CUST    ", "PG__    "};
    for (int i = 0; i < NUM_STOCKS; i++){
#pragma HLS PIPELINE II=1
        // Desired allocation in dollars = weight * portfolio_value
        float desired_alloc = latched_weights[i] * portfolio_value;
        unsigned int target_shares = (unsigned int)(desired_alloc / price_vals[i]);
        new_holdings[i] = (float)target_shares;
        total_cost += target_shares * price_vals[i];
        int delta = (int)target_shares - (int)holdings[i];
        char side;
        unsigned int quantity;
        if(delta > 0){
            side = 'B';
            quantity = (unsigned int)delta;
        } else if(delta < 0){
            side = 'S';
            quantity = (unsigned int)(-delta);
        } else{
            side = 'N';
            quantity = 0;
        }
        // Send OUCH
        ap_uint<32> order_msg[ORDER_MSG_WORDS];
        pack_order(userRefNum, side, quantity, symbols[i], price_vals[i], order_msg);
        userRefNum++;
        for (int j = 0; j < ORDER_MSG_WORDS; j++){
#pragma HLS PIPELINE II=1
            axis_word_t order_word;
            order_word.data = order_msg[j];
            order_word.last = (i == (NUM_STOCKS - 1) && j == (ORDER_MSG_WORDS - 1)) ? 1 : 0;
            out_stream_portfolio_ouch.write(order_word);
        }
    }
    // Update internal state
    for (int i = 0; i < NUM_STOCKS; i++){
#pragma HLS PIPELINE II=1
        holdings[i] = new_holdings[i];
    }
    cash = portfolio_value - total_cost;
}
}
