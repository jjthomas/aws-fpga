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


module test_db();

   import tb_type_defines_pkg::*;

   int error_count;
   int timeout_count;
   int fail;
   logic status;
   logic[31:0] read_data;

   initial begin

      logic [63:0] host_memory_buffer_address;


      tb.power_up(.clk_recipe_a(ClockRecipe::A0),
                  .clk_recipe_b(ClockRecipe::B0),
                  .clk_recipe_c(ClockRecipe::C0));

      tb.nsec_delay(1000);
      tb.poke_stat(.addr(8'h0c), .ddr_idx(0), .data(32'h0000_0000));
      tb.poke_stat(.addr(8'h0c), .ddr_idx(1), .data(32'h0000_0000));
      tb.poke_stat(.addr(8'h0c), .ddr_idx(2), .data(32'h0000_0000));

      // AXI_MEMORY_MODEL is used to bypass DDR micron models and run with AXI memory models. More information can be found in the readme.md
      
`ifndef AXI_MEMORY_MODEL      
      // allow memory to initialize
      tb.nsec_delay(27000);
`endif
      
      $display("[%t] : Initializing buffers", $realtime);

      host_memory_buffer_address = 64'h0;

      //Queue data to be transfered to CL DDR
      tb.que_buffer_to_cl(.chan(0), .src_addr(host_memory_buffer_address), .cl_addr(64'h0000_0000_0000), .len(64) );

      // Put test pattern in host memory
      tb.hm_put_byte(.addr(host_memory_buffer_address), .d(8'h01));
      for (int i = 1; i < 64; i++) begin
         tb.hm_put_byte(.addr(host_memory_buffer_address + i), .d(8'h00));
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
      end while ((status != 4'hf) && (timeout_count < 200));

      if (timeout_count >= 200) begin
         $display("[%t] : *** ERROR *** Timeout waiting for dma transfers to cl", $realtime);
         error_count++;
      end

      tb.poke_ocl(.addr(64'h800), .data(1'b1));
      tb.poke_ocl(.addr(64'h600), .data(1'b1));

      timeout_count = 0;
      do begin
         tb.peek_ocl(.addr(64'h600), .data(read_data));
         #10ns;
         timeout_count++;
      end while ((read_data != 0) && (timeout_count < 1000));

      if (timeout_count >= 1000) begin
         $display("[%t] : *** ERROR *** Timeout waiting for user logic to complete", $realtime);
         error_count++;
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

      if (fail || (tb.chk_prot_err_stat())) begin
         $display("[%t] : *** TEST FAILED ***", $realtime);
      end else begin
         $display("[%t] : *** TEST PASSED ***", $realtime);
      end

      $finish;
   end // initial begin

endmodule // test_db
