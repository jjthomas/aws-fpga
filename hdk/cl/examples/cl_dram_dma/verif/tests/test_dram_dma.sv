// Amazon FPGA Hardware Development Kit
//
// Copyright 2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Amazon Software License (the "License"). You may not use
// this file except in compliance with the License. A copy of the License is
// located at
//
//    http://aws.amazon.com/asl/
//
// or in the "license" file accompanying this file. This file is distributed on
// an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, express or
// implied. See the License for the specific language governing permissions and
// limitations under the License.


module test_dram_dma();

   import tb_type_defines_pkg::*;
   
    int            error_count;
    int            timeout_count;
    int            fail;
    logic [3:0]    status;

    initial begin

       logic [63:0] host_memory_buffer_address;
       logic [511:0] input_metadata;
       input_metadata[63:0] = 64'd64; // input addr
       input_metadata[127:64] = 64'd512; // input length in bits
       input_metadata[191:128] = 64'd0; // output addr
       logic [511:0] output_metadata;
       output_metadata[63:0] = 64'd512;
       

       tb.power_up(.clk_recipe_a(ClockRecipe::A1), 
                   .clk_recipe_b(ClockRecipe::B0), 
                   .clk_recipe_c(ClockRecipe::C0));

       tb.nsec_delay(1000);
       tb.poke_stat(.addr(8'h0c), .ddr_idx(0), .data(32'h0000_0000));
       tb.poke_stat(.addr(8'h0c), .ddr_idx(1), .data(32'h0000_0000));
       tb.poke_stat(.addr(8'h0c), .ddr_idx(2), .data(32'h0000_0000));

       // de-select the ATG hardware
       
       tb.poke_ocl(.addr(64'h130), .data(0));
       tb.poke_ocl(.addr(64'h230), .data(0));
       tb.poke_ocl(.addr(64'h330), .data(0));
       tb.poke_ocl(.addr(64'h430), .data(0));

       // allow memory to initialize
       tb.nsec_delay(25000);

       // issuing flr
       tb.issue_flr();

       $display("[%t] : Initializing buffers", $realtime);

       host_memory_buffer_address = 64'h0;

       //Queue data to be transfered to CL DDR
       tb.que_buffer_to_cl(.chan(0), .src_addr(host_memory_buffer_address), .cl_addr(64'h0), .len(128) );  // move buffer to DDR 0

       // Put test pattern in host memory       
       for (int i = 0; i < 64; i++) begin
          tb.hm_put_byte(.addr(host_memory_buffer_address), .d(input_metadata[(i+1)*8-1:i*8]));
          host_memory_buffer_address++;
       end
       for (int i = 0; i < 64; i++) begin
          tb.hm_put_byte(.addr(host_memory_buffer_address), .d(8'hAA));
          host_memory_buffer_address++;
       end
       
       host_memory_buffer_address = 64'h0_0000_3000;
       tb.que_buffer_to_cl(.chan(1), .src_addr(host_memory_buffer_address), .cl_addr(64'h0), .len(128) );  // move buffer to DDR 1

       // Put test pattern in host memory       
       for (int i = 0; i < 64; i++) begin
          tb.hm_put_byte(.addr(host_memory_buffer_address), .d(input_metadata[(i+1)*8-1:i*8]));
          host_memory_buffer_address++;
       end
       for (int i = 0; i < 64; i++) begin
          tb.hm_put_byte(.addr(host_memory_buffer_address), .d(8'hBB));
          host_memory_buffer_address++;
       end

       $display("[%t] : starting H2C DMA channels ", $realtime);
       
       //Start transfers of data to CL DDR
       tb.start_que_to_cl(.chan(0));   
       tb.start_que_to_cl(.chan(1));   

       // wait for dma transfers to complete
       timeout_count = 0;       
       do begin
          status[0] = tb.is_dma_to_cl_done(.chan(0));
          status[1] = tb.is_dma_to_cl_done(.chan(1));
          status[2] = 1'b1;
          status[3] = 1'b1;
          #10ns;
          timeout_count++;
       end while ((status != 4'hf) && (timeout_count < 2000));
       
       if (timeout_count >= 2000) begin
          $display("[%t] : *** ERROR *** Timeout waiting for dma transfers to cl", $realtime);
          error_count++;
       end

       poke_ocl(.addr(64'h500), .data(32'h1));

       logic[31:0] streaming_active;
       timeout_count = 0;
       do begin
          peek_ocl(.addr(64'h500), .data(streaming_active));
          #10ns;
          timeout_count++;
       end while ((streaming_active != 32'h0) && (timeout_count < 10000));
       if (timeout_count >= 10000) begin
          $display("[%t] : *** ERROR *** Timeout waiting for streaming to complete", $realtime);
          error_count++;
       end

       $display("[%t] : starting C2H DMA channels ", $realtime);

       // read the data from cl and put it in the host memory 
       host_memory_buffer_address = 64'h0_0003_2800;                                                                                        
       tb.que_cl_to_buffer(.chan(2), .dst_addr(host_memory_buffer_address), .cl_addr(64'h0), .len(128) );  // move DDR2 to buffer
                                                                                                                                            
       host_memory_buffer_address = 64'h0_0004_3800;                                                                                        
       tb.que_cl_to_buffer(.chan(3), .dst_addr(host_memory_buffer_address), .cl_addr(64'h0), .len(128) );  // move DDR3 to buffer
       
       //Start transfers of data from CL DDR
       tb.start_que_to_buffer(.chan(2));   
       tb.start_que_to_buffer(.chan(3));   

       // wait for dma transfers to complete
       timeout_count = 0;       
       do begin
          status[0] = 1'b1; 
          status[1] = 1'b1;
          status[2] = tb.is_dma_to_buffer_done(.chan(2));
          status[3] = tb.is_dma_to_buffer_done(.chan(3));
          #10ns;
          timeout_count++;          
       end while ((status != 4'hf) && (timeout_count < 1000));
       
       if (timeout_count >= 1000) begin
          $display("[%t] : *** ERROR *** Timeout waiting for dma transfers from cl", $realtime);
          error_count++;
       end

       #1us;
       
       // DDR 2
       // Compare the data in host memory with the expected data
       $display("[%t] : DMA buffer from DDR 2", $realtime);
   
       host_memory_buffer_address = 64'h0_0003_2800;
       for (int i = 0; i < 8; i++) begin
         if (tb.hm_get_byte(.addr(host_memory_buffer_address + i)) !== output_metadata[(i+1)*8-1:i*8]) begin
           $display("[%t] : *** ERROR *** DDR2 Data mismatch, addr:%0x read data is: %0x", 
                            $realtime, (host_memory_buffer_address + i), tb.hm_get_byte(.addr(host_memory_buffer_address + i)));
           error_count++;
         end    
       end
       host_memory_buffer_address = host_memory_buffer_address + 64'd64;
       for (int i = 0; i < 64; i++) begin
         if (tb.hm_get_byte(.addr(host_memory_buffer_address + i)) !== 8'hAA) begin
           $display("[%t] : *** ERROR *** DDR2 Data mismatch, addr:%0x read data is: %0x", 
                            $realtime, (host_memory_buffer_address + i), tb.hm_get_byte(.addr(host_memory_buffer_address + i)));
           error_count++;
         end    
       end
       
       // DDR 3
       // Compare the data in host memory with the expected data
       $display("[%t] : DMA buffer from DDR 3", $realtime);

       host_memory_buffer_address = 64'h0_0004_3800;
       for (int i = 0; i < 8; i++) begin
         if (tb.hm_get_byte(.addr(host_memory_buffer_address + i)) !== output_metadata[(i+1)*8-1:i*8]) begin
           $display("[%t] : *** ERROR *** DDR3 Data mismatch, addr:%0x read data is: %0x", 
                            $realtime, (host_memory_buffer_address + i), tb.hm_get_byte(.addr(host_memory_buffer_address + i)));
           error_count++;
         end    
       end
       host_memory_buffer_address = host_memory_buffer_address + 64'd64;
       for (int i = 0; i < 64; i++) begin
         if (tb.hm_get_byte(.addr(host_memory_buffer_address + i)) !== 8'hBB) begin
           $display("[%t] : *** ERROR *** DDR3 Data mismatch, addr:%0x read data is: %0x", 
                            $realtime, (host_memory_buffer_address + i), tb.hm_get_byte(.addr(host_memory_buffer_address + i)));
           error_count++;
         end    
       end

       logic[31:0] cycle_count;
       peek_ocl(.addr(64'h600), .data(cycle_count));
       $display("[%t] : streaming cycle count is %0x", $realtime, cycle_count);

       
       // Power down
       #500ns;
       tb.power_down();

       //---------------------------
       // Report pass/fail status
       //---------------------------
       $display("[%t] : Checking total error count...", $realtime);
       if (error_count > 0) begin
         fail = 1;
       end
       $display("[%t] : Detected %3d errors during this test", $realtime, error_count);

       if (fail) begin
         $display("[%t] : *** TEST FAILED ***", $realtime);
       end else begin
         $display("[%t] : *** TEST PASSED ***", $realtime);
       end

       $finish;
    end // initial begin

endmodule // test_dram_dma
   
