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

logic [511:0] rdata1;
logic [511:0] rdata2;

   initial begin
      tb.power_up();
      
      $display ("Writing begins...");
      tb.poke_pcis(.addr(64'b0), .data(512'b1), .strb(64'hffff_ffff_ffff_ffff));
      $display ("Write 1 complete...");
      tb.poke_pcis(.addr(64'b0), .data(512'b0), .strb(64'hffff_ffff_ffff_ffff));
      $display ("Write 2 complete...");

      tb.peek_pcis(.addr(64'b0), .data(rdata1));
      $display ("Read 1 0x%x...", rdata1);
      tb.peek_pcis(.addr(64'b0), .data(rdata2));
      $display ("Read 2 0x%x...", rdata2);

      if (rdata1[63:0] == 64'b0 && rdata2[63:0] == 64'b1)
        $display ("Test PASSED");
      else
        $display ("Test FAILED");

      tb.kernel_reset();

      tb.power_down();
      
      $finish;
   end

endmodule // test_hello_world
