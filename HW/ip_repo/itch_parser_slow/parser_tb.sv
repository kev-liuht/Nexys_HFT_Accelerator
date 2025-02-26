`timescale 1ns / 1ps

module tb_parser;

  localparam CLK_PERIOD = 10;

  // DUT interface signals
  reg         clk;
  reg  [7:0]  data_i;
  reg         valid_i;
  reg         reset_n;
  
  wire [31:0] stock_id_o;
  wire [31:0] order_ref_num_o;
  wire [31:0] num_shares_o;
  wire [31:0] price_o;
  wire [3:0]  order_type_o;
  wire        buy_sell_o;
  wire        valid_o;
  reg         ready_i;              // backpressure from comsumer
  wire        ready_o;              // input backpressure 
  
  // Instantiate the parser module
  parser dut (
    .clk(clk),
    .data_i(data_i),
    .valid_i(valid_i),
    .reset_n(reset_n),
    .stock_id_o(stock_id_o),
    .order_ref_num_o(order_ref_num_o),
    .num_shares_o(num_shares_o),
    .price_o(price_o),
    .order_type_o(order_type_o),
    .buy_sell_o(buy_sell_o),
    .valid_o(valid_o),
    .ready_i(ready_i),
    .ready_o(ready_o)
  );

  // Clock generation
  initial begin
    clk = 0;
    forever #(CLK_PERIOD/2) clk = ~clk;
  end
  
  //--------------------------------------------------------------------------
  // Define a test vector type containing the input message and expected outputs.
  //--------------------------------------------------------------------------

  typedef struct {
    byte message[]; // dynamic array of bytes representing the entire message
    // Expected outputs (adjust according to your message format)
    logic [31:0] expected_stock_id;
    logic [31:0] expected_order_ref_num;
    logic [31:0] expected_num_shares;
    logic [31:0] expected_price;
    logic [3:0]  expected_order_type;
    logic        expected_buy_sell;
  } test_vector_t;
  // Declare a dynamic array of test vectors.
  test_vector_t test_vectors[$];
  test_vector_t tv5;

  //--------------------------------------------------------------------------
  // Task: Load test vectors
  //--------------------------------------------------------------------------
  task load_test_vectors();
    test_vector_t tv;
    
    // Example Test Vector for an "A" message:
    // Assume a message length of 36 bytes, with:
    // - Bytes 0-1: Message Length (0x00, 0x24)
    // - Byte 2: Message Type ('A' = 0x41)
    // - Bytes 3 to 16: Dummy bytes (to reach the first meaningful byte offset)
    // - Then the meaningful fields according to your parsing offsets.
    //
    // You will need to adjust the array elements below to match your expected
    // byte offsets and field meanings.
    
    // Test 0
    tv.message = '{
      8'h00, 8'h24, 8'h41,    // Header: Length high, Length low, Type 'A'
      // Dummy bytes until byte counter reaches first meaningful offset
      8'hAA, 8'hAA, 8'hAA, 8'hAA, 8'hAA, 8'hAA, 8'hAA, 8'hAA,
      8'hAA, 8'hAA, 8'hAA, 8'hAA, 8'hAA, 8'hAA, 
      // Meaningful bytes (assuming the offsets as in your design)
      // Byte offset 0x0F (15): order_ref_num[31:24]
      8'hDE, 
      // Byte offset 0x10: order_ref_num[23:16]
      8'hAD, 
      // Byte offset 0x11: order_ref_num[15:8]
      8'hBE, 
      // Byte offset 0x12: order_ref_num[7:0]
      8'hEF,
      // Byte offset 0x13: buy_sell (0 = Buy)
      8'h00,
      // Byte offset 0x14-0x17: num_shares
      8'h00, 8'h00, 8'h10, 8'h20,
      // Byte offset 0x18-0x1B: stock_id
      8'h00, 8'h00, 8'h03, 8'hE8,
      // Dummy bytes until price offset (if any gap is needed)
      8'hBB, 8'hBB, 8'hBB, 8'hBB,
      // Byte offset 0x20-0x23: price
      8'h00, 8'h00, 8'h27, 8'h10
    };
    // Expected outputs for the above message:
    tv.expected_order_ref_num = 32'hDEADBEEF;
    tv.expected_buy_sell       = 1'b0;
    tv.expected_num_shares     = {8'h00, 8'h00, 8'h10, 8'h20};
    tv.expected_stock_id       = {8'h00, 8'h00, 8'h03, 8'hE8};
    tv.expected_price          = {8'h00, 8'h00, 8'h27, 8'h10};
    tv.expected_order_type     = 4'h1; // Assuming ADD_ORDER corresponds to 4'h1

    test_vectors.push_back(tv);
    
    // Test 1 - "A" message with different values
    tv.message = '{

      8'h00, 8'h24, 8'h41, 8'h01, 8'he4, 8'h00, 8'h00, 8'h0d, 
      8'h18, 8'hc2, 8'heb, 8'h22, 8'h4b, 8'h00, 8'h00, 8'h00, 
      8'h00, 8'h00, 8'h00, 8'h23, 8'h2d, 8'h53, 8'h00, 8'h00, 
      8'h02, 8'h58, 8'h41, 8'h52, 8'h47, 8'h58, 8'h20, 8'h20, 
      8'h20, 8'h20, 8'h00, 8'h10, 8'h5d, 8'hd8

    };

    tv.expected_order_ref_num = 32'd9005;
    tv.expected_buy_sell       = 1'b1;
    tv.expected_num_shares     = 32'd600;
    tv.expected_stock_id       = 32'h41524758; // ASCII "ARGX"
    tv.expected_price          = 32'h00105dD8;

    test_vectors.push_back(tv);

    // Test 2 - "D" message
    tv.message = '{
      8'h00, 8'h13, 8'h44, 8'h20, 8'h7E, 8'h00, 8'h00, 8'h0D, 
      8'h18, 8'hC3, 8'h99, 8'hF5, 8'h14, 8'h00, 8'h00, 8'h00, 
      8'h00, 8'h00, 8'h00, 8'h03, 8'hA8
    };

    tv.expected_order_ref_num = 32'd936;
    tv.expected_buy_sell       = 1'b0;
    tv.expected_num_shares     = 32'd0;
    tv.expected_stock_id       = 32'h0;
    tv.expected_price          = 32'h0;
    tv.expected_order_type     = 4'h8; // Assuming DELETE_ORDER corresponds to 4'h8

    test_vectors.push_back(tv);

    // Test 3 - "F" message
    tv.message = '{
      8'h00, 8'h28, 8'h46, 8'h22, 8'h03, 8'h00, 8'h00, 8'h0D, 
      8'h18, 8'hC4, 8'h4A, 8'h0A, 8'hB2, 8'h00, 8'h00, 8'h00, 
      8'h00, 8'h00, 8'h00, 8'h17, 8'h84, 8'h42, 8'h00, 8'h00, 
      8'h00, 8'h64, 8'h5A, 8'h56, 8'h5A, 8'h5A, 8'h54, 8'h20, 
      8'h20, 8'h20, 8'h00, 8'h01, 8'h5F, 8'h90, 8'h4C, 8'h45, 
      8'h48, 8'h4D
    };

    tv.expected_order_ref_num = 32'd6020;
    tv.expected_buy_sell       = 1'b0;
    tv.expected_num_shares     = 32'd100;
    tv.expected_stock_id       = 32'h5A565a5a; // ASCII "ZVZZ"
    tv.expected_price          = 32'h00015f90;
    tv.expected_order_type     = 4'h1;

    test_vectors.push_back(tv);

    // Test 4 - Invalid message - wrong message type
    tv.message = '{
      8'h00, 8'h24, 8'h00, 8'h01, 8'hE4, 8'h00, 8'h00, 8'h0D, 
      8'h18, 8'hC2, 8'hEB, 8'h22, 8'h4B, 8'h00, 8'h00, 8'h00, 
      8'h00, 8'h00, 8'h00, 8'h23, 8'h2D, 8'h53, 8'h00, 8'h00, 
      8'h02, 8'h58, 8'h41, 8'h52, 8'h47, 8'h58, 8'h20, 8'h20, 
      8'h20, 8'h20, 8'h00, 8'h10, 8'h5D, 8'hD8
    };
    tv.expected_order_ref_num = 32'd0;
    tv.expected_buy_sell       = 1'b0;
    tv.expected_num_shares     = 32'd000;
    tv.expected_stock_id       = 32'd0; // ASCII "ZVZZ"
    tv.expected_price          = 32'h0;
    tv.expected_order_type     = 4'h0; // Assuming INVALID corresponds to 4'h0

    test_vectors.push_back(tv);

 
  endtask
  
  //--------------------------------------------------------------------------
  // Task: Send a complete message byte-by-byte
  //--------------------------------------------------------------------------
  task send_message(input byte message[]);
    int i;
    for (i = 0; i < message.size(); i++) begin
      @(posedge clk);
      if (ready_o == 1'b1)
      begin
          data_i  = message[i];
          valid_i = 1;
      end
//      @(posedge clk);
//      valid_i = 0;
    end
    @(posedge clk);
    valid_i = 0;
  endtask
  
  //--------------------------------------------------------------------------
  // Main test process: Loop through each test vector, send the message, and
  // compare the outputs to the expected results.
  //--------------------------------------------------------------------------
  initial begin
    // Initialize inputs
    reset_n = 0;
    data_i  = 8'h00;
    valid_i = 0;
    ready_i = 0;
    repeat(2) @(posedge clk);
    
    // Release reset
    reset_n = 1;
    ready_i = 1;
    @(posedge clk);
    
    // Load the test vectors into the array
    load_test_vectors();
    
    // Loop through each test vector
    for (int i = 0; i < test_vectors.size(); i++) begin
      
      // Send the message
      send_message(test_vectors[i].message);
      
      // Wait until the parser indicates valid output
//        wait(valid_o == 1);
      @(posedge clk);  // Wait one extra cycle to capture outputs
      
      // Compare DUT outputs with expected values:
      if (order_ref_num_o !== test_vectors[i].expected_order_ref_num)
        $display("Test %0d FAILED: Expected order_ref_num = 0x%08h, got 0x%08h", 
                 i, test_vectors[i].expected_order_ref_num, order_ref_num_o);
      else
        $display("Test %0d: order_ref_num PASSED", i);
      
      if (buy_sell_o !== test_vectors[i].expected_buy_sell)
        $display("Test %0d FAILED: Expected buy_sell = %0d, got %0d", 
                 i, test_vectors[i].expected_buy_sell, buy_sell_o);
      else
        $display("Test %0d: buy_sell PASSED", i);
      
      if (num_shares_o !== test_vectors[i].expected_num_shares)
        $display("Test %0d FAILED: Expected num_shares = 0x%08h, got 0x%08h", 
                 i, test_vectors[i].expected_num_shares, num_shares_o);
      else
        $display("Test %0d: num_shares PASSED", i);
      
      if (stock_id_o !== test_vectors[i].expected_stock_id)
        $display("Test %0d FAILED: Expected stock_id = 0x%08h, got 0x%08h", 
                 i, test_vectors[i].expected_stock_id, stock_id_o);
      else
        $display("Test %0d: stock_id PASSED", i);
      
      if (price_o !== test_vectors[i].expected_price)
        $display("Test %0d FAILED: Expected price = 0x%08h, got 0x%08h", 
                 i, test_vectors[i].expected_price, price_o);
      else
        $display("Test %0d: price PASSED", i);
      
      if (order_type_o !== test_vectors[i].expected_order_type)
        $display("Test %0d FAILED: Expected order_type = 0x%01h, got 0x%01h", 
                 i, test_vectors[i].expected_order_type, order_type_o);
      else
        $display("Test %0d: order_type PASSED", i);
      
      // Optionally wait a few cycles before starting the next test
      repeat(5) @(posedge clk);
    end
    
   // Test 5 - Repeat test 0, but now with backpressure
      tv5.message = '{
      8'h00, 8'h24, 8'h41,    // Header: Length high, Length low, Type 'A'
      // Dummy bytes until byte counter reaches first meaningful offset
      8'hAA, 8'hAA, 8'hAA, 8'hAA, 8'hAA, 8'hAA, 8'hAA, 8'hAA,
      8'hAA, 8'hAA, 8'hAA, 8'hAA, 8'hAA, 8'hAA, 
      // Meaningful bytes (assuming the offsets as in your design)
      // Byte offset 0x0F (15): order_ref_num[31:24]
      8'hDE, 
      // Byte offset 0x10: order_ref_num[23:16]
      8'hAD, 
      // Byte offset 0x11: order_ref_num[15:8]
      8'hBE, 
      // Byte offset 0x12: order_ref_num[7:0]
      8'hEF,
      // Byte offset 0x13: buy_sell (0 = Buy)
      8'h00,
      // Byte offset 0x14-0x17: num_shares
      8'h00, 8'h00, 8'h10, 8'h20,
      // Byte offset 0x18-0x1B: stock_id
      8'h00, 8'h00, 8'h03, 8'hE8,
      // Dummy bytes until price offset (if any gap is needed)
      8'hBB, 8'hBB, 8'hBB, 8'hBB,
      // Byte offset 0x20-0x23: price
      8'h00, 8'h00, 8'h27, 8'h10,
      // extra bit to test if ready_o deasserts
      8'h00
    };
    // Expected outputs for the above message:
    tv5.expected_order_ref_num = 32'hDEADBEEF;
    tv5.expected_buy_sell       = 1'b0;
    tv5.expected_num_shares     = {8'h00, 8'h00, 8'h10, 8'h20};
    tv5.expected_stock_id       = {8'h00, 8'h00, 8'h03, 8'hE8};
    tv5.expected_price          = {8'h00, 8'h00, 8'h27, 8'h10};
    tv5.expected_order_type     = 4'h1; // Assuming ADD_ORDER corresponds to 4'h1
    
    
    ready_i = 0;
    send_message(tv5.message);
    
    @(posedge clk);     // wait for one more cycle
    assert(ready_o == 0);
    
      // Compare DUT outputs with expected values:
  if (order_ref_num_o !== tv5.expected_order_ref_num)
    $display("Test %0d FAILED: Expected order_ref_num = 0x%08h, got 0x%08h", 
             5, tv5.expected_order_ref_num, order_ref_num_o);
  else
    $display("Test %0d: order_ref_num PASSED", 5);
  
  if (buy_sell_o !== tv5.expected_buy_sell)
    $display("Test %0d FAILED: Expected buy_sell = %0d, got %0d", 
             5, tv5.expected_buy_sell, buy_sell_o);
  else
    $display("Test %0d: buy_sell PASSED", 5);
  
  if (num_shares_o !== tv5.expected_num_shares)
    $display("Test %0d FAILED: Expected num_shares = 0x%08h, got 0x%08h", 
             5, tv5.expected_num_shares, num_shares_o);
  else
    $display("Test %0d: num_shares PASSED", 5);
  
  if (stock_id_o !== tv5.expected_stock_id)
    $display("Test %0d FAILED: Expected stock_id = 0x%08h, got 0x%08h", 
             5, tv5.expected_stock_id, stock_id_o);
  else
    $display("Test %0d: stock_id PASSED", 5);
  
  if (price_o !== tv5.expected_price)
    $display("Test %0d FAILED: Expected price = 0x%08h, got 0x%08h", 
             5, tv5.expected_price, price_o);
  else
    $display("Test %0d: price PASSED", 5);
  
  if (order_type_o !== tv5.expected_order_type)
    $display("Test %0d FAILED: Expected order_type = 0x%01h, got 0x%01h", 
             5, tv5.expected_order_type, order_type_o);
  else
    $display("Test %0d: order_type PASSED", 5);
    $display("All tests complete.");
    
    repeat(5) @(posedge clk);
    ready_i = 1; // remove backpressure
    @(posedge clk);
    #5 assert(ready_o == 1);
    repeat(5) @(posedge clk);
    $finish;
  end

endmodule
