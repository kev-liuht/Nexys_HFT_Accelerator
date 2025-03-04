// This version of the parser assumes the processor sends 1 byte of data through the 4 byte line.
`timescale 1 ns / 1 ps

	module itch50_parser_v1_0 #
	(
		// Users to add parameters here
		parameter integer STOCK_ID_WIDTH	= 32,
		parameter integer ORDER_REF_NUM_WIDTH	= 32,
		parameter integer NUM_SHARES_WIDTH	= 32,
		parameter integer PRICE_WIDTH	= 32,
		parameter integer ORDER_TYPE_WIDTH	= 4,
		// User parameters ends
		// Do not modify the parameters beyond this line


		// Parameters of Axi Slave Bus Interface S00_AXIS
		parameter integer C_S00_AXIS_TDATA_WIDTH	= 32

		// Parameters of Axi Master Bus Interface M00_AXIS
//		parameter integer C_M00_AXIS_TDATA_WIDTH	= 136,
//		parameter integer C_M00_AXIS_START_COUNT	= 136
	)
	(
		// Users to add ports here

		// User ports ends
		// Do not modify the ports beyond this line


		// Ports of Axi Slave Bus Interface S00_AXIS
		input wire  s00_axis_aclk,
		input wire  s00_axis_aresetn,
		output wire  s00_axis_tready,
		input wire [C_S00_AXIS_TDATA_WIDTH-1 : 0] s00_axis_tdata,
		input wire [(C_S00_AXIS_TDATA_WIDTH/8)-1 : 0] s00_axis_tstrb,
		input wire  s00_axis_tlast,
		input wire  s00_axis_tvalid,

		// Ports of Axi Master Bus Interface M00_AXIS
		input wire  m00_axis_aclk,
		input wire  m00_axis_aresetn,
		output wire  m00_axis_tvalid,
		output wire [136-1 : 0] m00_axis_tdata,
		output wire [(136/8)-1 : 0] m00_axis_tstrb,
		output wire  m00_axis_tlast,
		input wire  m00_axis_tready
	);

	// Add user logic here
    // ------------------ Internal Signals ------------------
    wire reset_parser_n;
    wire [7:0] data_i_parser;
    wire valid_i_parser;
    wire ready_o_parser;
    wire ready_i_parser;

    wire [STOCK_ID_WIDTH-1:0] stock_id_o;
    wire [ORDER_REF_NUM_WIDTH-1:0] order_ref_num_o;
    wire [NUM_SHARES_WIDTH-1:0] num_shares_o;
    wire [PRICE_WIDTH-1:0] price_o;
    wire [ORDER_TYPE_WIDTH-1:0] order_type_o;
    wire buy_sell_o;
    wire valid_o_parser;

    // ------ AXIS Stream M00 internal signals
    reg m00_axis_tvalid_s;
    reg [136-1 : 0] m00_axis_tdata_s;
   
    // ------------------ Input AXI-Stream (S00_AXIS) ------------------
    assign s00_axis_tready = ready_o_parser; // Parser ready dictates S00_AXIS ready
    assign data_i_parser = s00_axis_tdata[7:0]; // Use lower byte
    assign valid_i_parser = s00_axis_tvalid & s00_axis_tstrb[0]; // Input valid from S00_AXIS
    assign reset_parser_n = s00_axis_aresetn;

    // ------------------ Parser Instance ------------------
    parser parser_inst (
        .clk(s00_axis_aclk),
        .reset_n(reset_parser_n),
        .data_i(data_i_parser),
        .valid_i(valid_i_parser),
        .ready_o(ready_o_parser),      // Ready to receive input data (input backpressure)
        .ready_i(ready_i_parser),      // Ready to send output data (output backpressure from M00_AXIS)
        .stock_id_o(stock_id_o),
        .order_ref_num_o(order_ref_num_o),
        .num_shares_o(num_shares_o),
        .price_o(price_o),
        .order_type_o(order_type_o),
        .buy_sell_o(buy_sell_o),
        .valid_o(valid_o_parser)       // Valid output data
    );
    
    // Output AXI-Stream FSM 
    reg ready_i_parser_reg;
    always @(posedge m00_axis_aclk) begin
        if (!m00_axis_aresetn) begin
            m00_axis_tvalid_s <= 1'b0;
            m00_axis_tdata_s <= 0;
            ready_i_parser_reg <= 1;
        end else begin
            if (valid_o_parser && !m00_axis_tvalid_s) begin // Parser has valid data and not currently sending
                m00_axis_tvalid_s <= 1'b1;
                ready_i_parser_reg <= 1'b0; // backpressure until order book consumes the data
                m00_axis_tdata_s[31:0]      <= stock_id_o;
                m00_axis_tdata_s[63:32]     <= order_ref_num_o;
                m00_axis_tdata_s[95:64]     <= num_shares_o;
                m00_axis_tdata_s[127:96]    <= price_o;
                m00_axis_tdata_s[131:128]   <= order_type_o;
                m00_axis_tdata_s[132:132]   <= buy_sell_o;
                m00_axis_tdata_s[135:133]   <= 3'b000;      // unused
            end else if (m00_axis_tvalid_s && m00_axis_tready) begin // Currently sending and downstream is ready
                m00_axis_tvalid_s <= 1'b0; // Deassert valid after successful transfer
                ready_i_parser_reg <= 1'b1;// data consumed, backpressure deasserted
            end else begin
                m00_axis_tvalid_s <= m00_axis_tvalid_s; // Hold current valid state
                ready_i_parser_reg <= ready_i_parser_reg;
            end
        end
    end

    assign m00_axis_tvalid  = m00_axis_tvalid_s;
    assign m00_axis_tdata   = m00_axis_tdata_s;
    assign m00_axis_tstrb  = {(136/8){1'b1}}; // All bytes valid
    assign m00_axis_tlast   = 1'b1; // No TLAST for continuous stream
    assign ready_i_parser = ready_i_parser_reg;
	// User logic ends

endmodule
