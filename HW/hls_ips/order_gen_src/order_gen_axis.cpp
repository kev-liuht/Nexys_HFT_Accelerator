#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_stream.h"

#define NUM_STOCKS         4
#define ORDER_MSG_WORDS    12
#define ORDER_MSG_BYTES    48

typedef ap_fixed<32,16> fixed16_16;

// Helper function for packaging OUCH 5.0 messages.
void pack_order(
    unsigned int userRefNum,
    char side,
    unsigned int quantity,
    const char symbol[9],
    unsigned int price,
    ap_uint<32> order_msg[ORDER_MSG_WORDS]
)
{
    unsigned char msg[ORDER_MSG_BYTES];
#pragma HLS ARRAY_PARTITION variable=msg complete dim=1

    // Initialize message buffer to 0.
    for (int i = 0; i < ORDER_MSG_BYTES; i++) {
#pragma HLS UNROLL
        msg[i] = 0;
    }

    // Field assignments per OUCH 5.0 spec:
    msg[0] = 'O'; // Type: 'O'
    // UserRefNum (4 bytes, big-endian)
    msg[1] = (userRefNum >> 24) & 0xFF;
    msg[2] = (userRefNum >> 16) & 0xFF;
    msg[3] = (userRefNum >> 8)  & 0xFF;
    msg[4] =  userRefNum        & 0xFF;
    // Side (1 byte)
    msg[5] = side;
    // Quantity (4 bytes, big-endian)
    msg[6] = (quantity >> 24) & 0xFF;
    msg[7] = (quantity >> 16) & 0xFF;
    msg[8] = (quantity >> 8)  & 0xFF;
    msg[9] =  quantity         & 0xFF;
    // Symbol (8 bytes, left-justified, padded with spaces)
    for (int i = 0; i < 8; i++) {
#pragma HLS UNROLL
        msg[10 + i] = symbol[i];
    }
    // Price (8 bytes, numeric, big-endian); high 4 bytes are zero.
    msg[18] = 0;
    msg[19] = 0;
    msg[20] = 0;
    msg[21] = 0;
    msg[22] = (price >> 24) & 0xFF;
    msg[23] = (price >> 16) & 0xFF;
    msg[24] = (price >> 8)  & 0xFF;
    msg[25] =  price         & 0xFF;
    // Dummy values for remaining fields:
    msg[26] = '0';  // Time In Force
    msg[27] = 'Y';  // Display
    msg[28] = 'P';  // Capacity
    msg[29] = 'Y';  // InterMarket Sweep Eligibility
    msg[30] = 'N';  // CrossType
    // ClOrdID (14 bytes): dummy value.
    const char dummyClOrdID[15] = { 'C','L','O','R','D','_','I','D','0','0','1','X','X','X' };
    for (int i = 0; i < 14; i++) {
#pragma HLS UNROLL
        msg[31 + i] = dummyClOrdID[i];
    }
    // Appendage Length (2 bytes) set to 0.
    msg[45] = 0;
    msg[46] = 0;
    // Optional Appendage left as 0.

    // Pack the 48 bytes into 12 32-bit words.
    for (int i = 0; i < ORDER_MSG_WORDS; i++) {
#pragma HLS UNROLL
        int idx = i * 4;
        order_msg[i] = ((ap_uint<32>)msg[idx]   << 24) |
                       ((ap_uint<32>)msg[idx+1] << 16) |
                       ((ap_uint<32>)msg[idx+2] << 8)  |
                       ((ap_uint<32>)msg[idx+3]);
    }
}

// Top-level function: order_gen_axis
/* This function receives new target portfolio weights and stock prices,
   calculates the current portfolio value (cash + market value of holdings),
   then rebalances the portfolio by redistributing the entire portfolio value.
   For each stock the target allocation (in dollars) is computed and converted to a
   target integer number of shares. An order is generated based on the difference between
   the target and current holdings. Finally, the new cash is computed as the leftover value. */
extern "C" {
void order_gen_axis(
    hls::stream<ap_uint<32> >& in_stream_weights,       // 4 words; Q16.16 fixed-point weights
    hls::stream<ap_uint<32> >& in_stream_stock_prices,    // 4 words; price*10000 format
    hls::stream<ap_uint<32> >& out_stream_portfolio,      // 1 word; portfolio value (price*10000)
    hls::stream<ap_uint<32> >& out_stream_ouch,           // 12 words per order message
    ap_uint<1> ap_clk,
    ap_uint<1> ap_rst_n
)
{
#pragma HLS INTERFACE axis port=in_stream_weights
#pragma HLS INTERFACE axis port=in_stream_stock_prices
#pragma HLS INTERFACE axis port=out_stream_portfolio
#pragma HLS INTERFACE axis port=out_stream_ouch
#pragma HLS INTERFACE ap_ctrl_none port=return

    // Persistent internal state: holdings for each stock and available cash.
    static unsigned int holdings[NUM_STOCKS] = {0, 0, 0, 0};
    static unsigned int cash = 100000000;   // Initial portfolio value
    static unsigned int userRefNum = 1;   // Unique order identifier starting point

    if (!in_stream_weights.empty() && !in_stream_stock_prices.empty()) {
        // Read target weights.
        ap_uint<32> weight_words[NUM_STOCKS];
        for (int i = 0; i < NUM_STOCKS; i++) {
#pragma HLS UNROLL
            weight_words[i] = in_stream_weights.read();
        }
        // Read stock prices.
        ap_uint<32> price_words[NUM_STOCKS];
        for (int i = 0; i < NUM_STOCKS; i++) {
#pragma HLS UNROLL
            price_words[i] = in_stream_stock_prices.read();
        }

        float target_weights[NUM_STOCKS];
        unsigned int stock_prices[NUM_STOCKS];
        for (int i = 0; i < NUM_STOCKS; i++) {
#pragma HLS UNROLL
            target_weights[i] = (float)weight_words[i] / 65536.0;
            stock_prices[i] = price_words[i];
        }

        // Compute current portfolio value: cash + sum(holdings * stock_price)
        unsigned int portfolio_value = cash;
        for (int i = 0; i < NUM_STOCKS; i++) {
#pragma HLS UNROLL
            portfolio_value += holdings[i] * stock_prices[i];
        }
        // Output current portfolio value.
        out_stream_portfolio.write(portfolio_value);

        // Compute new target holdings based on the entire portfolio value.
        unsigned int new_holdings[NUM_STOCKS];
        unsigned int total_cost = 0;
        // Stock symbols.
        const char symbols[NUM_STOCKS][9] = {"AAPL    ",
        									 "NVDA    ",
											 "MSFT    ",
											 "INTC    "};

        for (int i = 0; i < NUM_STOCKS; i++) {
#pragma HLS UNROLL
            // Desired allocation in currency units.
            float desired_alloc = target_weights[i] * (float)portfolio_value;
            // Calculate target shares as the maximum whole shares that can be purchased.
            unsigned int target_shares = (unsigned int)(desired_alloc / (float)stock_prices[i]);
            new_holdings[i] = target_shares;
            total_cost += target_shares * stock_prices[i];

            // Compute the order delta (target shares - current holdings).
            int delta = (int)target_shares - (int)holdings[i];
            char side;
            unsigned int quantity;
            if (delta > 0) {
                side = 'B'; // BUY order
                quantity = (unsigned int)delta;
            } else if (delta < 0) {
                side = 'S'; // SELL order
                quantity = (unsigned int)(-delta);
            } else {
                // (Just in case) No change needed; output a dummy order.
                side = 'N';
                quantity = 0;
            }

            // Pack the order into a 48-byte (12-word) OUCH message.
            ap_uint<32> order_msg[ORDER_MSG_WORDS];
            pack_order(userRefNum, side, quantity, symbols[i], stock_prices[i], order_msg);
            userRefNum++; // Increment order identifier
            for (int j = 0; j < ORDER_MSG_WORDS; j++) {
#pragma HLS UNROLL
                out_stream_ouch.write(order_msg[j]);
            }
        }

        // Update portfolio state: set holdings to the new target and update free cash.
        for (int i = 0; i < NUM_STOCKS; i++) {
#pragma HLS UNROLL
            holdings[i] = new_holdings[i];
        }
        cash = portfolio_value - total_cost;
    }
}
}
