`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company:
// Engineer:
//
// Create Date: 02/17/2025 01:57:58 PM
// Design Name: ITCH 5.0 parser
// Module Name: parser
// Project Name: Nexys_Video_HFT
// Target Devices:
// Tool Versions:
// Description:
//   Parses a subset of ITCH 5.0 messages (A, F, E, C, X, D) to extract key order information
//   including stock ID, order reference number, number of shares, price, order type, and buy/sell side.
//   Implements a state machine with both input and output backpressure.
//   Handles irrelevant message types by skipping them.
//
// Inputs:
//   clk       : Clock signal.
//   data_i    : 8-bit input data stream carrying ITCH 5.0 message bytes.
//   valid_i   : Input valid signal indicating data_i is valid.
//   reset_n   : Active-low asynchronous reset.
//   ready_i   : Input ready signal from consumer (output backpressure).
//
// Outputs:
//   stock_id_o      : 32-bit Stock ID.
//   order_ref_num_o : 32-bit Order Reference Number.
//   num_shares_o    : 32-bit Number of Shares.
//   price_o         : 32-bit Price.
//   order_type_o    : 4-bit Order Type (ADD_ORDER, CANCEL_ORDER, etc.).
//   buy_sell_o      : Buy/Sell indicator (1 = Sell, 0 = Buy).
//   valid_o         : Output valid signal, asserted for one clock cycle when output data is valid.
//   ready_o         : Output ready signal to upstream data provider (input backpressure).
//
// Dependencies: None
//
// Revision:
// Revision 0.03 - Fixed ready_o_next logic to prevent parser from getting stuck.
// Revision 0.02 - Added input and output backpressure mechanisms.
// Revision 0.01 - File Created - Initial parsing for A, F, E, C, X, D messages
//
//////////////////////////////////////////////////////////////////////////////////


module parser(
    input clk,
    input [7:0] data_i, // Renamed to data_i to avoid confusion with SystemVerilog keyword 'data'
    input valid_i,
    input reset_n,
    input ready_i,         // <--- Output Backpressure from Consumer
    output logic [31:0] stock_id_o,
    output logic [31:0] order_ref_num_o,
    output logic [31:0] num_shares_o,
    output logic [31:0] price_o,
    output logic [3:0] order_type_o,
    output logic buy_sell_o,
    output logic valid_o,
    output logic ready_o      // <--- Input Backpressure to Upstream
    );

    // --- State Machine Definition ---
    typedef enum logic [3:0] {
        GET_LENGTH,
        GET_MSG_TYPE,

        // "A" message states - Add order - No MPID Atribution
        PARSE_MSG_A,

        // "F" message states - Add order - MPID Atribution
        PARSE_MSG_F,

        // "E" message states - Order Executed
        PARSE_MSG_E,

        // "C" message states - Order Executed with Price
        PARSE_MSG_C,

        // "X" message states - Order Cancel
        PARSE_MSG_X,

        // "D" message states - Order Delete
        PARSE_MSG_D,

        // Irrelevant message state
        PARSE_MSG_IRRELEVANT
    } parser_state_t;

    // --- Message Type Definitions ---
    localparam MESSAGE_TYPE_A = 8'h41;
    localparam MESSAGE_TYPE_F = 8'h46;
    localparam MESSAGE_TYPE_E = 8'h45;
    localparam MESSAGE_TYPE_C = 8'h43;
    localparam MESSAGE_TYPE_X = 8'h58;
    localparam MESSAGE_TYPE_D = 8'h44;

    // --- Order Type Definitions ---
    typedef enum logic [3:0] {
        ERROR = 4'h0,
        ADD_ORDER = 4'h1,
        CANCEL_ORDER = 4'h2,
        EXECUTE_ORDER = 4'h4,
        DELETE_ORDER = 4'h8
    } order_type_t;

    parser_state_t current_state, next_state;

    // --- Internal Registers and Signals ---
    logic [7:0] message_length_reg; // Register to store the message length
    logic [7:0] message_type_reg;   // Register to store the message type
    logic [15:0] byte_counter_reg;   // Counter to track bytes received for current message

    // --- Output Registers ---
    logic [31:0] stock_id_reg;
    logic [31:0] order_ref_num_reg;
    logic [31:0] num_shares_reg;
    logic [31:0] price_reg;
    order_type_t order_type_reg;
    logic buy_sell_reg;
    logic valid_o_reg;
    logic ready_o_reg;       // Register for ready_o output

    // --- Next State Logic Signals ---
    logic [31:0] stock_id_next;
    logic [31:0] order_ref_num_next;
    logic [31:0] num_shares_next;
    logic [31:0] price_next;
    order_type_t order_type_next;
    logic buy_sell_next;
    logic valid_o_next;
    logic ready_o_next;       // Next value for ready_o
    logic [15:0] byte_counter_next;
    logic [7:0] message_length_next;
    logic [7:0]  message_type_next;


    // --- FSM State Transition Logic (Combinational) ---
    always_comb begin
        next_state                  = current_state;
        message_length_next         = message_length_reg;
        message_type_next           = message_type_reg;
        stock_id_next               = stock_id_reg;
        order_ref_num_next          = order_ref_num_reg;
        num_shares_next             = num_shares_reg;
        price_next                  = price_reg;
        order_type_next             = order_type_reg;
        buy_sell_next               = buy_sell_reg;
        valid_o_next                = 1'b0;
        ready_o_next                = 1'b1;       // Default: Parser is ready to receive data

        case (current_state)
            GET_LENGTH: begin
                // reset the output signals as it has been consumed
                buy_sell_next = 0;
                order_type_next = ERROR;
                num_shares_next = '0;
                order_ref_num_next = '0;
                stock_id_next = '0;
                message_type_next = '0;
                price_next = '0;
                if (valid_i && ready_o_reg) begin // Consume data only if valid and parser is ready (from previous cycle)
                    message_length_next = data_i;  // Capture entire 8-bit message length
                    if (data_i == 8'h0) begin
                        next_state = GET_LENGTH; // Remain in GET_LENGTH if length is 0 (consider error handling)
                    end else begin
                        next_state = GET_MSG_TYPE;
                        byte_counter_next = 16'h0; // Reset byte counter for new message
                    end
                end else begin
                    next_state = GET_LENGTH; // Stay in GET_LENGTH if not valid or not ready
                end
            end

            GET_MSG_TYPE: begin
                if (valid_i && ready_o_reg) begin // Consume data only if valid and parser is ready (from previous cycle)
                    message_type_next = data_i;      // Store message type
                    byte_counter_next = 16'h1; // Received length and type bytes
                    case (data_i)
                        MESSAGE_TYPE_A: next_state = PARSE_MSG_A;
                        MESSAGE_TYPE_F: next_state = PARSE_MSG_F;
                        MESSAGE_TYPE_E: next_state = PARSE_MSG_E;
                        MESSAGE_TYPE_C: next_state = PARSE_MSG_C;
                        MESSAGE_TYPE_X: next_state = PARSE_MSG_X;
                        MESSAGE_TYPE_D: next_state = PARSE_MSG_D;
                        default: next_state = PARSE_MSG_IRRELEVANT;
                    endcase
                end else begin
                    next_state = GET_MSG_TYPE; // Stay in GET_MSG_TYPE if not valid or not ready
                end
            end

            PARSE_MSG_A: begin
                order_type_next = ADD_ORDER;
                if (valid_i && ready_o_reg) begin // Consume data only if valid and parser is ready (from previous cycle)
                    case (byte_counter_reg)
                        16'h0F: order_ref_num_next[31:24] = data_i;
                        16'h10: order_ref_num_next[23:16] = data_i;
                        16'h11: order_ref_num_next[15:8] = data_i;
                        16'h12: order_ref_num_next[7:0] = data_i;
                        16'h13: buy_sell_next = data_i[0];         // 0 = Buy, 1 = Sell. 'B' has 0 for LSB, 'S' has 1
                        16'h14: num_shares_next[31:24] = data_i;
                        16'h15: num_shares_next[23:16] = data_i;
                        16'h16: num_shares_next[15:8] = data_i;
                        16'h17: num_shares_next[7:0] = data_i;
                        16'h18: stock_id_next[31:24] = data_i;
                        16'h19: stock_id_next[23:16] = data_i;
                        16'h1A: stock_id_next[15:8] = data_i;
                        16'h1B: stock_id_next[7:0] = data_i;
                        16'h20: price_next[31:24] = data_i;
                        16'h21: price_next[23:16] = data_i;
                        16'h22: price_next[15:8] = data_i;
                        16'h23: price_next[7:0] = data_i;

                        default: ; // Do nothing
                    endcase
                    if (byte_counter_next < message_length_reg)
                    begin
                            byte_counter_next = byte_counter_reg + 1;
                    end
                end 
                if (byte_counter_next == message_length_reg) begin
                    if (ready_i) begin      // Output backpressure check
                        next_state = GET_LENGTH;
                        valid_o_next = 1'b1; // Output is valid only if consumer is ready
                    end else begin
                        next_state = PARSE_MSG_A; // Stay in PARSE_MSG_A due to output backpressure
                        ready_o_next = 1'b0;     // <--- Set ready_o to 0 due to output backpressure
                        valid_o_next = 1'b0;
                    end
                end
            end

            PARSE_MSG_F: begin // Similar to PARSE_MSG_A - apply same ready_o logic
                order_type_next = ADD_ORDER;
                if (valid_i && ready_o_reg) begin
                    case (byte_counter_reg)
                        16'h0F: order_ref_num_next[31:24] = data_i;
                        16'h10: order_ref_num_next[23:16] = data_i;
                        16'h11: order_ref_num_next[15:8] = data_i;
                        16'h12: order_ref_num_next[7:0] = data_i;
                        16'h13: buy_sell_next = data_i[0];         // 0 = Buy, 1 = Sell. 'B' has 0 for LSB, 'S' has 1
                        16'h14: num_shares_next[31:24] = data_i;
                        16'h15: num_shares_next[23:16] = data_i;
                        16'h16: num_shares_next[15:8] = data_i;
                        16'h17: num_shares_next[7:0] = data_i;
                        16'h18: stock_id_next[31:24] = data_i;
                        16'h19: stock_id_next[23:16] = data_i;
                        16'h1A: stock_id_next[15:8] = data_i;
                        16'h1B: stock_id_next[7:0] = data_i;
                        16'h20: price_next[31:24] = data_i;
                        16'h21: price_next[23:16] = data_i;
                        16'h22: price_next[15:8] = data_i;
                        16'h23: price_next[7:0] = data_i;

                        default: ; // Do nothing
                    endcase
                    if (byte_counter_next < message_length_reg)
                    begin
                            byte_counter_next = byte_counter_reg + 1;
                    end
                end
                if (byte_counter_next == message_length_reg) begin
                    if (ready_i) begin
                        next_state = GET_LENGTH;
                        valid_o_next = 1'b1;
                    end else begin
                        next_state = PARSE_MSG_F;
                        ready_o_next = 1'b0;     // <--- Set ready_o to 0 due to output backpressure
                        valid_o_next = 1'b0;
                    end
                end
            end

            PARSE_MSG_E: begin // Similar to PARSE_MSG_A - apply same ready_o logic
                order_type_next = EXECUTE_ORDER;
                if (valid_i && ready_o_reg) begin
                    case (byte_counter_reg)
                        16'h0F: order_ref_num_next[31:24] = data_i;
                        16'h10: order_ref_num_next[23:16] = data_i;
                        16'h11: order_ref_num_next[15:8] = data_i;
                        16'h12: order_ref_num_next[7:0] = data_i;
                        16'h13: num_shares_next[31:24] = data_i;
                        16'h14: num_shares_next[23:16] = data_i;
                        16'h15: num_shares_next[15:8] = data_i;
                        16'h16: num_shares_next[7:0] = data_i;

                        default: ; // Do nothing
                    endcase
                    if (byte_counter_next < message_length_reg)
                    begin
                            byte_counter_next = byte_counter_reg + 1;
                    end
                end
                if (byte_counter_next == message_length_reg ) begin
                    if (ready_i) begin
                        next_state = GET_LENGTH;
                        valid_o_next = 1'b1;
                    end else begin
                        next_state = PARSE_MSG_E;
                        ready_o_next = 1'b0;     // <--- Set ready_o to 0 due to output backpressure
                        valid_o_next = 1'b0;
                    end
                end
            end

            PARSE_MSG_C: begin // Similar to PARSE_MSG_A - apply same ready_o logic
                order_type_next = EXECUTE_ORDER;
                if (valid_i && ready_o_reg) begin
                    case (byte_counter_reg)
                        16'h0F: order_ref_num_next[31:24] = data_i;
                        16'h10: order_ref_num_next[23:16] = data_i;
                        16'h11: order_ref_num_next[15:8] = data_i;
                        16'h12: order_ref_num_next[7:0] = data_i;
                        16'h13: num_shares_next[31:24] = data_i;
                        16'h14: num_shares_next[23:16] = data_i;
                        16'h15: num_shares_next[15:8] = data_i;
                        16'h16: num_shares_next[7:0] = data_i;
                        16'h20: price_next[31:24] = data_i;
                        16'h21: price_next[23:16] = data_i;
                        16'h22: price_next[15:8] = data_i;
                        16'h23: price_next[7:0] = data_i;

                        default: ; // Do nothing
                    endcase
                    if (byte_counter_next < message_length_reg)
                    begin
                            byte_counter_next = byte_counter_reg + 1;
                    end
                end
                if (byte_counter_next == message_length_reg ) begin
                    if (ready_i) begin
                        next_state = GET_LENGTH;
                        valid_o_next = 1'b1;
                    end else begin
                        next_state = PARSE_MSG_C;
                        ready_o_next = 1'b0;     // <--- Set ready_o to 0 due to output backpressure
                        valid_o_next = 1'b0;
                    end
                end
                // check for non-printable messages. If found, go to irrelevant state to avoid double counting
                if (byte_counter_reg == 16'h19 - 1 && data_i == 8'h4E) begin
                    next_state = PARSE_MSG_IRRELEVANT;
                end
            end

            PARSE_MSG_X: begin // Similar to PARSE_MSG_A - apply same ready_o logic
                order_type_next = CANCEL_ORDER;
                if (valid_i && ready_o_reg) begin
                    case (byte_counter_reg)
                        16'h0F: order_ref_num_next[31:24] = data_i;
                        16'h10: order_ref_num_next[23:16] = data_i;
                        16'h11: order_ref_num_next[15:8] = data_i;
                        16'h12: order_ref_num_next[7:0] = data_i;
                        16'h13: num_shares_next[31:24] = data_i;
                        16'h14: num_shares_next[23:16] = data_i;
                        16'h15: num_shares_next[15:8] = data_i;
                        16'h16: num_shares_next[7:0] = data_i;

                        default: ; // Do nothing
                    endcase
                    if (byte_counter_next < message_length_reg)
                    begin
                            byte_counter_next = byte_counter_reg + 1;
                    end
                end
                if (byte_counter_next == message_length_reg ) begin
                    if (ready_i) begin
                        next_state = GET_LENGTH;
                        valid_o_next = 1'b1;
                    end else begin
                        next_state = PARSE_MSG_X;
                        ready_o_next = 1'b0;     // <--- Set ready_o to 0 due to output backpressure
                        valid_o_next = 1'b0;
                    end
                end
            end

            PARSE_MSG_D: begin // Similar to PARSE_MSG_A - apply same ready_o logic
                order_type_next = DELETE_ORDER;
                if (valid_i && ready_o_reg) begin
                    case (byte_counter_reg)
                        16'h0F: order_ref_num_next[31:24] = data_i;
                        16'h10: order_ref_num_next[23:16] = data_i;
                        16'h11: order_ref_num_next[15:8] = data_i;
                        16'h12: order_ref_num_next[7:0] = data_i;

                        default: ; // Do nothing
                    endcase
                    if (byte_counter_next < message_length_reg)
                    begin
                            byte_counter_next = byte_counter_reg + 1;
                    end
                end
                if (byte_counter_next == message_length_reg ) begin
                    if (ready_i) begin
                        next_state = GET_LENGTH;
                        valid_o_next = 1'b1;
                    end else begin
                        next_state = PARSE_MSG_D;
                        ready_o_next = 1'b0;     // <--- Set ready_o to 0 due to output backpressure
                        valid_o_next = 1'b0;
                    end
                end
            end

            PARSE_MSG_IRRELEVANT: begin
                // reset output registers
                stock_id_next = 32'h0;
                order_ref_num_next = 32'h0;
                num_shares_next = 32'h0;
                price_next = 32'h0;
                order_type_next = ERROR;
                buy_sell_next = 1'b0;
                valid_o_next = 1'b0;
                // check for end of message
                if (valid_i && ready_o_reg) begin // Consume data only if valid and parser is ready
                    byte_counter_next = byte_counter_reg + 1;
                    if (byte_counter_next == message_length_reg) begin
                        next_state = GET_LENGTH;
                    end
                end else begin
                    next_state = PARSE_MSG_IRRELEVANT; // Stay in IRRELEVANT if not valid or not ready
                end
            end

            default: next_state = GET_LENGTH; // Should not reach here
        endcase
    end

    // --- FSM State and Output Register Update (Sequential) ---
    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            current_state     <= GET_LENGTH;
            message_length_reg  <= 16'h0;
            message_type_reg    <= 8'h0;
            byte_counter_reg    <= 16'h0;
            stock_id_reg      <= 32'h0;
            order_ref_num_reg <= 32'h0;
            num_shares_reg    <= 32'h0;
            price_reg         <= 32'h0;
            order_type_reg    <= ERROR;
            buy_sell_reg      <= 1'b0;
            valid_o_reg       <= 1'b0;
            ready_o_reg       <= 1'b1; // Initialize ready_o to high (ready) on reset
        end else begin
            current_state     <= next_state;
            stock_id_reg      <= stock_id_next;
            order_ref_num_reg <= order_ref_num_next;
            num_shares_reg    <= num_shares_next;
            price_reg         <= price_next;
            order_type_reg    <= order_type_next;
            buy_sell_reg      <= buy_sell_next;
            valid_o_reg       <= valid_o_next;
            ready_o_reg       <= ready_o_next; // Update ready_o register
            byte_counter_reg  <= byte_counter_next;
            message_length_reg <= message_length_next;
            message_type_reg   <= message_type_next;
        end
    end

    // --- Output Assignments ---
    assign stock_id_o      = stock_id_reg;
    assign order_ref_num_o = order_ref_num_reg;
    assign num_shares_o    = num_shares_reg;
    assign price_o         = price_reg;
    assign order_type_o    = order_type_reg;
    assign buy_sell_o      = buy_sell_reg;
    assign valid_o         = valid_o_reg;
    assign ready_o         = ready_o_reg; // Assign ready_o output

endmodule