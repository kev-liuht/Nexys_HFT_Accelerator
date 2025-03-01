`timescale 1ns / 1ps

import axi4stream_vip_pkg::*;
import design_3_axi4stream_vip_0_0_pkg::*; // Master VIP package
import design_3_axi4stream_vip_1_0_pkg::*; // Slave VIP package

module parser_ip_slow_tb_2();
    // Interface Agent Declarations
    design_3_axi4stream_vip_0_0_mst_t       mst_agent;
    design_3_axi4stream_vip_1_0_slv_t       slv_agent;


    // Clock and Reset signals
    bit clk;
    bit resetn;

    // DUT instantiation
    design_3_wrapper DUT (
        .resetn(resetn),
        .clk(clk)
    );
    
    // Clock generation
    always #10 clk = ~clk;

    initial begin
        clk = 1'b0;
        resetn <= 1'b0;

        // Initialize agents
        mst_agent = new("master vip agent", DUT.design_3_i.axi4stream_vip_0.inst.IF);
        mst_agent.vif_proxy.set_dummy_drive_type(XIL_AXI4STREAM_VIF_DRIVE_NONE);        // needed to stop VIP spewing random garbage data
        mst_agent.start_master();

        slv_agent = new("slave vip agent", DUT.design_3_i.axi4stream_vip_1.inst.IF);
        slv_agent.start_slave();

        // Reset sequence
        #250 resetn <= 1'b1;
        #100; // Wait for slave to be ready after reset
        slv_gen_tready();
//        send_data_to_dut();
        send_stream_data_msg_A();
        send_stream_data_msg_F();
        send_stream_data_msg_E();
        send_stream_data_msg_C_printable();
        send_stream_data_msg_C_nonprintable();
        send_stream_data_msg_X();
        send_stream_data_msg_D();
        send_last_packet();
        //send_a_packet(32);
        repeat(20) @(posedge clk);
        $display("Simulation finished successfully.");
        $finish;
    end
    
    task test_data_send();
        axi4stream_transaction wr_transaction;
        axi4stream_transaction                  wr_transactionc;
        automatic bit [31:0] packed_data = 32'hdeadbeef;
        wr_transaction = mst_agent.driver.create_transaction($sformatf("Test Transaction"));
        wr_transactionc = mst_agent.driver.create_transaction("Master VIP write transactionc");
        wr_transaction.set_driver_return_item_policy(XIL_AXI4STREAM_AT_ACCEPT_RETURN );
        wr_transaction.set_data_beat(packed_data);
        wr_transaction.set_strb('{0,0,0,1});
        mst_agent.driver.send(wr_transaction);
        mst_agent.driver.seq_item_port.get_next_rsp(wr_transactionc);
        
//        mst_agent.wait_drivers_idle();
    endtask

    task automatic send_stream_data_msg_A();
        axi4stream_transaction wr_transaction;
        // Define the stream data as an array of bytes.
        automatic byte data_bytes[] = '{
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
        
        // expected output: 0x010000271000001020deadbeef000003e8
    
        int total_bytes = data_bytes.size();
        // Here we're treating each byte as one beat.
        int total_beats = (total_bytes+3)/4;
        bit [31:0] data_beat;
        bit [3:0]  tstrb;
    
        // Loop over each beat and pack up to 4 bytes per beat
        for (int beat = 0; beat < total_beats; beat++) begin
            data_beat = 32'h0;
            tstrb = 4'b0;
            // Pack bytes into the 32-bit word and set the corresponding strobe bits.
            for (int i = 0; i < 4; i++) begin
                int byte_index = beat * 4 + i;
                if (byte_index < total_bytes) begin
                    data_beat[i*8 +: 8] = data_bytes[byte_index];
                    tstrb[i] = 1'b1;
                end else begin
                    data_beat[i*8 +: 8] = 8'b0;
                    tstrb[i] = 1'b1;
                end
            end
                
            // Create a transaction for this beat.
            wr_transaction = mst_agent.driver.create_transaction($sformatf("Stream Beat %0d", beat));
            wr_transaction.set_data_beat(data_beat);
            wr_transaction.set_strb({tstrb[0], tstrb[1], tstrb[2], tstrb[3]});
            // Mark the last beat appropriately.
            wr_transaction.set_last((beat == total_beats - 1) ? 1'b1 : 1'b0);
            
            // Optional: Display the beat details.
            $display("Sending Beat %0d: Data = %h, STRB = %b, LAST = %b", 
                     beat, data_beat, tstrb, wr_transaction.get_last());
            
            // Send the transaction.
            mst_agent.driver.send(wr_transaction);
         end

        $display("End task - send_stream_data_msg_a()");
    endtask
    
    task automatic send_stream_data_msg_F();
        axi4stream_transaction wr_transaction;
        // Define the stream data as an array of bytes.
        automatic byte data_bytes[] = '{
          8'h00, 8'h28, 8'h46, 8'h22, 8'h03, 8'h00, 8'h00, 8'h0D, 
          8'h18, 8'hC4, 8'h4A, 8'h0A, 8'hB2, 8'h00, 8'h00, 8'h00, 
          8'h00, 8'h00, 8'h00, 8'h17, 8'h84, 8'h42, 8'h00, 8'h00, 
          8'h00, 8'h64, 8'h5A, 8'h56, 8'h5A, 8'h5A, 8'h54, 8'h20, 
          8'h20, 8'h20, 8'h00, 8'h01, 8'h5F, 8'h90, 8'h4C, 8'h45, 
          8'h48, 8'h4D
        };
        // Expected output: 0x0100015f9000000064000017845a565a5a
    
        int total_bytes = data_bytes.size();
        // Here we're treating each byte as one beat.
        int total_beats = (total_bytes+3)/4;
        bit [31:0] data_beat;
        bit [3:0]  tstrb;
    
        // Loop over each beat and pack up to 4 bytes per beat
        for (int beat = 0; beat < total_beats; beat++) begin
            data_beat = 32'h0;
            tstrb = 4'b0;
            // Pack bytes into the 32-bit word and set the corresponding strobe bits.
            for (int i = 0; i < 4; i++) begin
                int byte_index = beat * 4 + i;
                if (byte_index < total_bytes) begin
                    data_beat[i*8 +: 8] = data_bytes[byte_index];
                    tstrb[i] = 1'b1;
                end else begin
                    data_beat[i*8 +: 8] = 8'b0;
                    tstrb[i] = 1'b1;
                end
            end
                
            // Create a transaction for this beat.
            wr_transaction = mst_agent.driver.create_transaction($sformatf("Stream Beat %0d", beat));
            wr_transaction.set_data_beat(data_beat);
            wr_transaction.set_strb({tstrb[0], tstrb[1], tstrb[2], tstrb[3]});
            // Mark the last beat appropriately.
            wr_transaction.set_last((beat == total_beats - 1) ? 1'b1 : 1'b0);
            
            // Optional: Display the beat details.
            $display("Sending Beat %0d: Data = %h, STRB = %b, LAST = %b", 
                     beat, data_beat, tstrb, wr_transaction.get_last());
            
            // Send the transaction.
            mst_agent.driver.send(wr_transaction);
         end

        $display("End task - send_stream_data_msg_F()");
    endtask
    
    task automatic send_stream_data_msg_E();
        axi4stream_transaction wr_transaction;
        // Define the stream data as an array of bytes.
        automatic byte data_bytes[] = '{
          8'h00, 8'h1F, 8'h45, 8'hAA, 8'hFF, 8'hAA, 8'hFF, 8'hAA, 
          8'hFF, 8'hAA, 8'hFF, 8'hAA, 8'hFF, 8'hAA, 8'h00, 8'hBB, 
          8'hCC, 8'hCA, 8'hFE, 8'hBA, 8'hBE, 8'h00, 8'h10, 8'h01, 
          8'h20, 8'h64, 8'h5A, 8'h56, 8'h5A, 8'h5A, 8'h54, 8'h20, 
          8'h20
        };
        // Mesasge E, ORDER REF NUM = CAFEBABE, Shares = 00100120
        // Expected output: 040000000000100120cafebabe00000000
    
        int total_bytes = data_bytes.size();
        // Here we're treating each byte as one beat.
        int total_beats = (total_bytes+3)/4;
        bit [31:0] data_beat;
        bit [3:0]  tstrb;
    
        // Loop over each beat and pack up to 4 bytes per beat
        for (int beat = 0; beat < total_beats; beat++) begin
            data_beat = 32'h0;
            tstrb = 4'b0;
            // Pack bytes into the 32-bit word and set the corresponding strobe bits.
            for (int i = 0; i < 4; i++) begin
                int byte_index = beat * 4 + i;
                if (byte_index < total_bytes) begin
                    data_beat[i*8 +: 8] = data_bytes[byte_index];
                    tstrb[i] = 1'b1;
                end else begin
                    data_beat[i*8 +: 8] = 8'b0;
                    tstrb[i] = 1'b1;
                end
            end
                
            // Create a transaction for this beat.
            wr_transaction = mst_agent.driver.create_transaction($sformatf("Stream Beat %0d", beat));
            wr_transaction.set_data_beat(data_beat);
            wr_transaction.set_strb({tstrb[0], tstrb[1], tstrb[2], tstrb[3]});
            // Mark the last beat appropriately.
            wr_transaction.set_last((beat == total_beats - 1) ? 1'b1 : 1'b0);
            
            // Optional: Display the beat details.
            $display("Sending Beat %0d: Data = %h, STRB = %b, LAST = %b", 
                     beat, data_beat, tstrb, wr_transaction.get_last());
            
            // Send the transaction.
            mst_agent.driver.send(wr_transaction);
         end

        $display("End task - send_stream_data_msg_E()");
    endtask
    
    task automatic send_stream_data_msg_C_printable();
        axi4stream_transaction wr_transaction;
        // Define the stream data as an array of bytes.
        automatic byte data_bytes[] = '{
          8'h00, 8'h24, 8'h43, 8'hAA, 8'hFF, 8'hAA, 8'hFF, 8'hAA, 
          8'hFF, 8'hAA, 8'hFF, 8'hAA, 8'hFF, 8'hAA, 8'h00, 8'hBB, 
          8'hCC, 8'h10, 8'hDB, 8'h52, 8'h32, 8'h08, 8'h30, 8'h21, 
          8'h20, 8'h64, 8'h5A, 8'h56, 8'h5A, 8'h5A, 8'h54, 8'h20, 
          8'h20, 8'h59, 8'h5A, 8'h56, 8'h5A, 8'h5A
        };
        // Mesasge C, ORDER REF NUM = 10DB5232, # Shares = 08302120, printable, executed price: 8'h5A, 8'h56, 8'h5A, 8'h5A
        // Expected output: 025a565a5a0830212010db523200000000
    
        int total_bytes = data_bytes.size();
        // Here we're treating each byte as one beat.
        int total_beats = (total_bytes+3)/4;
        bit [31:0] data_beat;
        bit [3:0]  tstrb;
    
        // Loop over each beat and pack up to 4 bytes per beat
        for (int beat = 0; beat < total_beats; beat++) begin
            data_beat = 32'h0;
            tstrb = 4'b0;
            // Pack bytes into the 32-bit word and set the corresponding strobe bits.
            for (int i = 0; i < 4; i++) begin
                int byte_index = beat * 4 + i;
                if (byte_index < total_bytes) begin
                    data_beat[i*8 +: 8] = data_bytes[byte_index];
                    tstrb[i] = 1'b1;
                end
            end
                
            // Create a transaction for this beat.
            wr_transaction = mst_agent.driver.create_transaction($sformatf("Stream Beat %0d", beat));
            wr_transaction.set_data_beat(data_beat);
            wr_transaction.set_strb({tstrb[0], tstrb[1], tstrb[2], tstrb[3]});
            // Mark the last beat appropriately.
            wr_transaction.set_last((beat == total_beats - 1) ? 1'b1 : 1'b0);
            
            // Optional: Display the beat details.
            $display("Sending Beat %0d: Data = %h, STRB = %b, LAST = %b", 
                     beat, data_beat, tstrb, wr_transaction.get_last());
            
            // Send the transaction.
            mst_agent.driver.send(wr_transaction);
         end

        $display("End task - send_stream_data_msg_C_printable()");
    endtask
    task automatic send_stream_data_msg_C_nonprintable();
        axi4stream_transaction wr_transaction;
        // Define the stream data as an array of bytes.
        automatic byte data_bytes[] = '{
          8'h00, 8'h24, 8'h43, 8'hAA, 8'hFF, 8'hAA, 8'hFF, 8'hAA, 
          8'hFF, 8'hAA, 8'hFF, 8'hAA, 8'hFF, 8'hAA, 8'h00, 8'hBB,
          8'h08, 8'h10, 8'hDB, 8'h52, 8'h32, 8'hCC, 8'h30, 8'h21, 
          8'h20, 8'h64, 8'h5A, 8'h56, 8'h5A, 8'h5A, 8'h54, 8'h20, 
          8'h20, 8'h4E, 8'h5A, 8'h56, 8'h5A, 8'h5A
        };
        // Mesasge C, ORDER REF NUM = 10DB5232, # Shares = 08302120, nonprintable, executed price: 8'h5A, 8'h56, 8'h5A, 8'h5A
        // Expected output: NA
    
        int total_bytes = data_bytes.size();
        // Here we're treating each byte as one beat.
        int total_beats = (total_bytes+3)/4;
        bit [31:0] data_beat;
        bit [3:0]  tstrb;
    
        // Loop over each beat and pack up to 4 bytes per beat
        for (int beat = 0; beat < total_beats; beat++) begin
            data_beat = 32'h0;
            tstrb = 4'b0;
            // Pack bytes into the 32-bit word and set the corresponding strobe bits.
            for (int i = 0; i < 4; i++) begin
                int byte_index = beat * 4 + i;
                if (byte_index < total_bytes) begin
                    data_beat[i*8 +: 8] = data_bytes[byte_index];
                    tstrb[i] = 1'b1;
                end
            end
                
            // Create a transaction for this beat.
            wr_transaction = mst_agent.driver.create_transaction($sformatf("Stream Beat %0d", beat));
            wr_transaction.set_data_beat(data_beat);
            wr_transaction.set_strb({tstrb[0], tstrb[1], tstrb[2], tstrb[3]});
            // Mark the last beat appropriately.
            wr_transaction.set_last((beat == total_beats - 1) ? 1'b1 : 1'b0);
            
            // Optional: Display the beat details.
            $display("Sending Beat %0d: Data = %h, STRB = %b, LAST = %b", 
                     beat, data_beat, tstrb, wr_transaction.get_last());
            
            // Send the transaction.
            mst_agent.driver.send(wr_transaction);
         end

        $display("End task - send_stream_data_msg_C_nonprintable()");
    endtask
    
    task automatic send_stream_data_msg_X();
        axi4stream_transaction wr_transaction;
        // Define the stream data as an array of bytes.
        automatic byte data_bytes[] = '{
          8'h00, 8'h17, 8'h58, 8'hAA, 8'hFF, 8'hAA, 8'hFF, 8'hAA, 
          8'hFF, 8'hAA, 8'hFF, 8'hAA, 8'hFF, 8'h00, 8'hBB, 8'hCC,
          8'h08, 8'h75, 8'h35, 8'h99, 8'h32, 8'hAA, 8'h30, 8'h21, 
          8'h20
        };
        // Mesasge X, ORDER REF NUM = 75359932, # Shares = AA302120
        // Expected output: 0200000000aa3021207535993200000000
    
        int total_bytes = data_bytes.size();
        // Here we're treating each byte as one beat.
        int total_beats = (total_bytes+3)/4;
        bit [31:0] data_beat;
        bit [3:0]  tstrb;
    
        // Loop over each beat and pack up to 4 bytes per beat
        for (int beat = 0; beat < total_beats; beat++) begin
            data_beat = 32'h0;
            tstrb = 4'b0;
            // Pack bytes into the 32-bit word and set the corresponding strobe bits.
            for (int i = 0; i < 4; i++) begin
                int byte_index = beat * 4 + i;
                if (byte_index < total_bytes) begin
                    data_beat[i*8 +: 8] = data_bytes[byte_index];
                    tstrb[i] = 1'b1;
                end else begin
                    data_beat[i*8 +: 8] = 8'b0;
                    tstrb[i] = 1'b1;
                end
            end
                
            // Create a transaction for this beat.
            wr_transaction = mst_agent.driver.create_transaction($sformatf("Stream Beat %0d", beat));
            wr_transaction.set_data_beat(data_beat);
            wr_transaction.set_strb({tstrb[0], tstrb[1], tstrb[2], tstrb[3]});
            // Mark the last beat appropriately.
            wr_transaction.set_last((beat == total_beats - 1) ? 1'b1 : 1'b0);
            
            // Optional: Display the beat details.
            $display("Sending Beat %0d: Data = %h, STRB = %b, LAST = %b", 
                     beat, data_beat, tstrb, wr_transaction.get_last());
            
            // Send the transaction.
            mst_agent.driver.send(wr_transaction);
         end

        $display("End task - send_stream_data_msg_a()");
    endtask
    
    task automatic send_stream_data_msg_D();
        axi4stream_transaction wr_transaction;
        // Define the stream data as an array of bytes.
        automatic byte data_bytes[] = '{
            8'h00, 8'h13, 8'h44, 8'h20, 8'h7E, 8'h00, 8'h00, 8'h0D, 
            8'h18, 8'hC3, 8'h99, 8'hF5, 8'h14, 8'h00, 8'h00, 8'h00, 
            8'h00, 8'h00, 8'h00, 8'h03, 8'hA8
        };
        // Mesasge C, ORDER REF NUM = d936, order type: h8
        // Expected output: 080000000000000000000003a800000000
    
        int total_bytes = data_bytes.size();
        // Here we're treating each byte as one beat.
        int total_beats = (total_bytes+3)/4;
        bit [31:0] data_beat;
        bit [3:0]  tstrb;
    
        // Loop over each beat and pack up to 4 bytes per beat
        for (int beat = 0; beat < total_beats; beat++) begin
            data_beat = 32'h0;
            tstrb = 4'b0;
            // Pack bytes into the 32-bit word and set the corresponding strobe bits.
            for (int i = 0; i < 4; i++) begin
                int byte_index = beat * 4 + i;
                if (byte_index < total_bytes) begin
                    data_beat[i*8 +: 8] = data_bytes[byte_index];
                    tstrb[i] = 1'b1;
                end else begin
                    data_beat[i*8 +: 8] = 8'b0;
                    tstrb[i] = 1'b1;
                end
            end
                
            // Create a transaction for this beat.
            wr_transaction = mst_agent.driver.create_transaction($sformatf("Stream Beat %0d", beat));
            wr_transaction.set_data_beat(data_beat);
            wr_transaction.set_strb({tstrb[0], tstrb[1], tstrb[2], tstrb[3]});
            // Mark the last beat appropriately.
            wr_transaction.set_last((beat == total_beats - 1) ? 1'b1 : 1'b0);
            
            // Optional: Display the beat details.
            $display("Sending Beat %0d: Data = %h, STRB = %b, LAST = %b", 
                     beat, data_beat, tstrb, wr_transaction.get_last());
            
            // Send the transaction.
            mst_agent.driver.send(wr_transaction);
         end

        $display("End task - send_stream_data_msg_a()");
    endtask
  
  // somehow axis master doesn't send the last byte, so we have to explicitly send it here
  task automatic send_last_packet();
    axi4stream_transaction wr_transaction;
    bit [31:0] data_beat = '0;
    wr_transaction = mst_agent.driver.create_transaction($sformatf("End packet"));
    wr_transaction.set_data_beat(data_beat);
    wr_transaction.set_strb({1'b0, 1'b0, 1'b0, 1'b0});
    mst_agent.driver.send(wr_transaction);
  endtask :send_last_packet
  
  // Generate slave tready signal. Right now its a 1:9 duty cycle
  task slv_gen_tready();
    axi4stream_ready_gen                           ready_gen;
    ready_gen = slv_agent.driver.create_ready("ready_gen");
    ready_gen.set_ready_policy(XIL_AXI4STREAM_READY_GEN_OSC);
    ready_gen.set_low_time(9);
    ready_gen.set_high_time(1);
    slv_agent.driver.send_tready(ready_gen);
  endtask :slv_gen_tready

endmodule