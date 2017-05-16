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


module test_hello_world();

   import tb_type_defines_pkg::*;
   `include "cl_common_defines.vh"
   
    int            error_count;
    int            timeout_count;
    int            fail;
    logic    status;
    int            len0 = 128;

    initial begin

       logic [63:0] host_memory_buffer_address;
       

       tb.power_up(.clk_recipe_a(ClockRecipe::A1), 
                   .clk_recipe_b(ClockRecipe::B0), 
                   .clk_recipe_c(ClockRecipe::C0));

       tb.nsec_delay(500);

       // allow memory to initialize
       tb.nsec_delay(25000);

       // issuing flr
       tb.issue_flr();

       $display("[%t] : Initializing buffers", $realtime);

       host_memory_buffer_address = 64'h0;

       //Queue data to be transfered to CL DDR
       tb.que_buffer_to_cl(.chan(0), .src_addr(host_memory_buffer_address), .cl_addr(64'h0), .len(len0) );  // move buffer to DDR 0

       // Put test pattern in host memory       
       for (int i = 0 ; i < len0 ; i++) begin
          tb.hm_put_byte(.addr(host_memory_buffer_address), .d(8'hAA));
          host_memory_buffer_address++;
       end
       
       $display("[%t] : starting H2C DMA channels ", $realtime);
       
       //Start transfers of data to CL DDR
       tb.start_que_to_cl(.chan(0));   

       // wait for dma transfers to complete
       timeout_count = 0;       
       do begin
          status = tb.is_dma_to_cl_done(.chan(0));
          #10ns;
          timeout_count++;
       end while ((status != 1'b1) && (timeout_count < 500));
       
       if (timeout_count >= 500) begin
          $display("[%t] : *** ERROR *** Timeout waiting for dma transfers from cl", $realtime);
          error_count++;
       end

       $display("[%t] : starting C2H DMA channels ", $realtime);

       //Start transfers of data from CL DDR
       tb.start_que_to_buffer(.chan(0));

       // read the data from cl and put it in the host memory 
       host_memory_buffer_address = 64'h0_0001_0800;
       tb.que_cl_to_buffer(.chan(0), .dst_addr(host_memory_buffer_address), .cl_addr(64'h0), .len(len0) );  // move DDR0 to buffer
                                                                                                                                            
       // wait for dma transfers to complete
       timeout_count = 0;       
       do begin
          status = tb.is_dma_to_buffer_done(.chan(0));
          #10ns;
          timeout_count++;          
       end while ((status != 1'b1) && (timeout_count < 500));
       
       if (timeout_count >= 500) begin
          $display("[%t] : *** ERROR *** Timeout waiting for dma transfers from cl", $realtime);
          error_count++;
       end


       // DDR 0
       // Compare the data in host memory with the expected data
       $display("[%t] : DMA buffer from DDR 0", $realtime);

       host_memory_buffer_address = 64'h0_0001_0800;
       for (int i = 0 ; i<len0; i+=64) begin
         for (int j = i ; j<i+8 ; j++) begin
           if (tb.hm_get_byte(.addr(host_memory_buffer_address + j)) !== 8'hAA) begin
             $display("[%t] : *** ERROR *** DDR0 Data mismatch, addr:%0x read data is: %0x", 
                              $realtime, (host_memory_buffer_address + j), tb.hm_get_byte(.addr(host_memory_buffer_address + j)));
             error_count++;
           end
         end
       end
       
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

endmodule // test_hello_world
 
