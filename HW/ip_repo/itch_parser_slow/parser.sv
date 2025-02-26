`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Module Name: parser
// Design Name: ITCH 5.0 Message Parser
// Project Name: Nexys_Video_HFT
// Description:
//   Parses a subset of ITCH 5.0 messages (A, F, E, C, X, D) to extract key order information
//   including stock ID, order reference number, number of shares, price, order type, and buy/sell side.
//   Implements a state machine to handle message length and type decoding.
//   Handles irrelevant message types by skipping them.
//
// Inputs:
//   clk       : Clock signal.
//   data_i    : 8-bit input data stream carrying ITCH 5.0 message bytes.
//   valid_i   : Input valid signal indicating data_i is valid.
//   reset_n   : Active-low asynchronous reset.
//
// Outputs:
//   stock_id_o      : 32-bit Stock ID.
//   order_ref_num_o : 32-bit Order Reference Number.
//   num_shares_o    : 32-bit Number of Shares.
//   price_o         : 32-bit Price.
//   order_type_o    : 4-bit Order Type (ADD_ORDER, CANCEL_ORDER, etc.).
//   buy_sell_o      : Buy/Sell indicator (1 = Sell, 0 = Buy).
//   valid_o         : Output valid signal, asserted for one clock cycle when output data is valid.
//
// Dependencies: None
//
// Revision:
// Revision 0.01 - File Created - Initial parsing for A, F, E, C, X, D messages
//
//////////////////////////////////////////////////////////////////////////////////


module parser(
    input clk,
    input [7:0] data_i, // Renamed to data_i to avoid confusion with SystemVerilog keyword 'data'
    input valid_i,
    input reset_n,
    output logic [31:0] stock_id_o, // Changed to logic and _o suffix for clarity
    output logic [31:0] order_ref_num_o,
    output logic [31:0] num_shares_o,
    output logic [31:0] price_o,
    output logic [3:0] order_type_o,
    output logic buy_sell_o,
    output logic valid_o // Optional output to indicate when output data is valid
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

        // Irrelevant message state. Skips parsing messages we don't care about.
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
    logic [7:0] byte_counter_reg;   // Counter to track bytes received for current message
    logic [7:0] data_reg;          // Register to hold the incoming data byte

    // --- Output Registers ---
    logic [31:0] stock_id_reg;
    logic [31:0] order_ref_num_reg;
    logic [31:0] num_shares_reg;
    logic [31:0] price_reg;
    order_type_t order_type_reg;
    logic buy_sell_reg;
    logic valid_o_reg;

    // next state logic
    logic [31:0] stock_id_next;
    logic [31:0] order_ref_num_next;
    logic [31:0] num_shares_next;
    logic [31:0] price_next;
    order_type_t order_type_next;
    logic buy_sell_next;
    logic valid_o_next;
    logic [7:0] byte_counter_next;
    logic [7:0] message_length_next;
    logic [7:0]  message_type_next;

    // localparam definitions
    localparam MSG_C_NON_PRINTABLE_BYTE_INDEX = 16'h18;
    localparam NON_PRINTABLE_BYTE_VALUE = 8'h4E;


    // --- FSM State Transition Logic (Combinational) ---
    always_comb begin
        next_state                  = current_state; // Default: stay in the current state
        data_reg                    = data_i;         // Latch input data for current cycle. Important for synchronous logic.
        message_length_next         = message_length_reg;
        message_type_next           = message_type_reg;
        stock_id_next               = stock_id_reg;
        order_ref_num_next          = order_ref_num_reg;
        num_shares_next             = num_shares_reg;
        price_next                  = price_reg;
        order_type_next             = order_type_reg;
        buy_sell_next               = buy_sell_reg;
        valid_o_next                = 1'b0;
        byte_counter_next           = byte_counter_reg;

        case (current_state)
            GET_LENGTH: begin
                if (valid_i) begin
                    message_length_next = data_reg;  // Capture entire 8-bit message length
                    if (data_reg == 8'h0) begin
                        next_state = GET_LENGTH; // Remain in GET_LENGTH if length is 0
                    end else begin
                        next_state = GET_MSG_TYPE;
                        byte_counter_next = 16'h0; // Reset byte counter for new message
                    end
                end
            end

            GET_MSG_TYPE: begin
                if (valid_i) begin
                    message_type_next = data_reg;      // Store message type
                    byte_counter_next = 16'h1; // Received length and type bytes
                    case (data_reg)
                        MESSAGE_TYPE_A: next_state = PARSE_MSG_A;
                        MESSAGE_TYPE_F: next_state = PARSE_MSG_F; 
                        MESSAGE_TYPE_E: next_state = PARSE_MSG_E;
                        MESSAGE_TYPE_C: next_state = PARSE_MSG_C;
                        MESSAGE_TYPE_X: next_state = PARSE_MSG_X;
                        MESSAGE_TYPE_D: next_state = PARSE_MSG_D;
                        default: next_state = PARSE_MSG_IRRELEVANT;
                    endcase
                end
            end

            PARSE_MSG_A: begin
                order_type_next = ADD_ORDER;
                if (valid_i) begin
                    case (byte_counter_reg)
                        16'h0F: order_ref_num_next[31:24] = data_reg;
                        16'h10: order_ref_num_next[23:16] = data_reg;
                        16'h11: order_ref_num_next[15:8] = data_reg;
                        16'h12: order_ref_num_next[7:0] = data_reg;
                        16'h13: buy_sell_next = data_reg[0];         // 0 = Buy, 1 = Sell. 'B' has 0 for LSB, 'S' has 1
                        16'h14: num_shares_next[31:24] = data_reg;
                        16'h15: num_shares_next[23:16] = data_reg;
                        16'h16: num_shares_next[15:8] = data_reg;
                        16'h17: num_shares_next[7:0] = data_reg;
                        16'h18: stock_id_next[31:24] = data_reg;
                        16'h19: stock_id_next[23:16] = data_reg;
                        16'h1A: stock_id_next[15:8] = data_reg;
                        16'h1B: stock_id_next[7:0] = data_reg;
                        16'h20: price_next[31:24] = data_reg;
                        16'h21: price_next[23:16] = data_reg;
                        16'h22: price_next[15:8] = data_reg;
                        16'h23: price_next[7:0] = data_reg;

                        default: ; // Do nothing
                    endcase
                    byte_counter_next = byte_counter_reg + 1;
                    if (byte_counter_next == message_length_reg) begin
                        next_state = GET_LENGTH;
                        valid_o_next = 1'b1; // Output is valid after all data is received
                    end
                end
            end

            PARSE_MSG_F: begin
                order_type_next = ADD_ORDER;
                if (valid_i) begin
                    case (byte_counter_reg)
                        16'h0F: order_ref_num_next[31:24] = data_reg;
                        16'h10: order_ref_num_next[23:16] = data_reg;
                        16'h11: order_ref_num_next[15:8] = data_reg;
                        16'h12: order_ref_num_next[7:0] = data_reg;
                        16'h13: buy_sell_next = data_reg[0];         // 0 = Buy, 1 = Sell. 'B' has 0 for LSB, 'S' has 1
                        16'h14: num_shares_next[31:24] = data_reg;
                        16'h15: num_shares_next[23:16] = data_reg;
                        16'h16: num_shares_next[15:8] = data_reg;
                        16'h17: num_shares_next[7:0] = data_reg;
                        16'h18: stock_id_next[31:24] = data_reg;
                        16'h19: stock_id_next[23:16] = data_reg;
                        16'h1A: stock_id_next[15:8] = data_reg;
                        16'h1B: stock_id_next[7:0] = data_reg;
                        16'h20: price_next[31:24] = data_reg;
                        16'h21: price_next[23:16] = data_reg;
                        16'h22: price_next[15:8] = data_reg;
                        16'h23: price_next[7:0] = data_reg;

                        default: ; // Do nothing
                    endcase
                    byte_counter_next = byte_counter_reg + 1;
                    if (byte_counter_next == message_length_reg) begin
                        next_state = GET_LENGTH;
                        valid_o_next = 1'b1; // Output is valid after all data is received
                    end
                end
            end

            PARSE_MSG_E: begin
                order_type_next = EXECUTE_ORDER;
                if (valid_i) begin
                    case (byte_counter_reg)
                        16'h0F: order_ref_num_next[31:24] = data_reg;
                        16'h10: order_ref_num_next[23:16] = data_reg;
                        16'h11: order_ref_num_next[15:8] = data_reg;
                        16'h12: order_ref_num_next[7:0] = data_reg;
                        16'h13: num_shares_next[31:24] = data_reg;
                        16'h14: num_shares_next[23:16] = data_reg;
                        16'h15: num_shares_next[15:8] = data_reg;
                        16'h16: num_shares_next[7:0] = data_reg;

                        default: ; // Do nothing
                    endcase
                    byte_counter_next = byte_counter_reg + 1;
                    if (byte_counter_next == message_length_reg ) begin
                        next_state = GET_LENGTH;
                        valid_o_next = 1'b1; // Output is valid after all data is received
                    end
                end
            end

            PARSE_MSG_C: begin
                order_type_next = EXECUTE_ORDER;
                if (valid_i) begin
                    case (byte_counter_reg)
                        16'h0F: order_ref_num_next[31:24] = data_reg;
                        16'h10: order_ref_num_next[23:16] = data_reg;
                        16'h11: order_ref_num_next[15:8] = data_reg;
                        16'h12: order_ref_num_next[7:0] = data_reg;
                        16'h13: num_shares_next[31:24] = data_reg;
                        16'h14: num_shares_next[23:16] = data_reg;
                        16'h15: num_shares_next[15:8] = data_reg;
                        16'h16: num_shares_next[7:0] = data_reg;
                        16'h20: price_next[31:24] = data_reg;
                        16'h21: price_next[23:16] = data_reg;
                        16'h22: price_next[15:8] = data_reg;
                        16'h23: price_next[7:0] = data_reg;

                        default: ; // Do nothing
                    endcase
                    byte_counter_next = byte_counter_reg + 1;
                    if (byte_counter_next == message_length_reg ) begin
                        next_state = GET_LENGTH;
                        valid_o_next = 1'b1; // Output is valid after all data is received
                    end
                    // check for non-printable messages (when data_reg = 'N'). If found, go to irrelevant state to avoid double counting
                    if (byte_counter_reg == MSG_C_NON_PRINTABLE_BYTE_INDEX  && data_reg == NON_PRINTABLE_BYTE_VALUE ) begin
                        next_state = PARSE_MSG_IRRELEVANT;
                    end
                end
            end

            PARSE_MSG_X: begin
                order_type_next = CANCEL_ORDER;
                if (valid_i) begin
                    case (byte_counter_reg)
                        16'h0F: order_ref_num_next[31:24] = data_reg;
                        16'h10: order_ref_num_next[23:16] = data_reg;
                        16'h11: order_ref_num_next[15:8] = data_reg;
                        16'h12: order_ref_num_next[7:0] = data_reg;
                        16'h13: num_shares_next[31:24] = data_reg;
                        16'h14: num_shares_next[23:16] = data_reg;
                        16'h15: num_shares_next[15:8] = data_reg;
                        16'h16: num_shares_next[7:0] = data_reg;

                        default: ; // Do nothing
                    endcase
                    byte_counter_next = byte_counter_reg + 1;
                    if (byte_counter_next == message_length_reg ) begin
                        next_state = GET_LENGTH;
                        valid_o_next = 1'b1; // Output is valid after all data is received
                    end
                end
            end

            PARSE_MSG_D: begin
                order_type_next = DELETE_ORDER;
                if (valid_i) begin
                    case (byte_counter_reg)
                        16'h0F: order_ref_num_next[31:24] = data_reg;
                        16'h10: order_ref_num_next[23:16] = data_reg;
                        16'h11: order_ref_num_next[15:8] = data_reg;
                        16'h12: order_ref_num_next[7:0] = data_reg;

                        default: ; // Do nothing
                    endcase
                    byte_counter_next = byte_counter_reg + 1;
                    if (byte_counter_next == message_length_reg ) begin
                        next_state = GET_LENGTH;
                        valid_o_next = 1'b1; // Output is valid after all data is received
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
                if (valid_i) begin
                    byte_counter_next = byte_counter_reg + 1;
                    if (byte_counter_next == message_length_reg) begin
                        next_state = GET_LENGTH;
                    end
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
        end else begin
            current_state     <= next_state;
            stock_id_reg      <= stock_id_next; // Hold output if not valid, else update
            order_ref_num_reg <= order_ref_num_next;
            num_shares_reg    <= num_shares_next;
            price_reg         <= price_next;
            order_type_reg    <= order_type_next;
            buy_sell_reg      <= buy_sell_next;
            valid_o_reg       <= valid_o_next; // Keep valid_o asserted for one cycle when data is ready
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

endmodule
