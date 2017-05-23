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


module test_hello_world_pcie();

import tb_type_defines_pkg::*;


   initial begin
      tb.power_up();
      
      for (int i = 0; i < 2; i++) begin
        tb.poke_pcis(.addr(64'b0), .data(512'b1), .strb(64'hffff_ffff_ffff_ffff));
      end

      for (int i = 0; i < 2; i++) begin
        logic [511:0] rdata;
        tb.peek_pcis(.addr(64'b0), .data(rdata));
        if (rdata != 512'b1)
          $display ("Read %d: 0x%x...", i, rdata);
      end
       
      $display ("Test PASSED");

      tb.kernel_reset();

      tb.power_down();
      
      $finish;
   end

endmodule // test_hello_world_pcie
