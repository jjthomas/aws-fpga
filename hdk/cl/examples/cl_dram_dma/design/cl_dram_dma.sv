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

module cl_dram_dma #(parameter NUM_DDR=2)

(
   `include "cl_ports.vh"

);

`include "cl_common_defines.vh"      // CL Defines for all examples
`include "cl_id_defines.vh"          // Defines for ID0 and ID1 (PCI ID's)
`include "cl_dram_dma_defines.vh"

// TIE OFF ALL UNUSED INTERFACES
// Including all the unused interface to tie off
// This list is put in the top of the fie to remind
// developers to remve the specific interfaces
// that the CL will use

`include "unused_sh_bar1_template.inc"
`include "unused_apppf_irq_template.inc"
`include "unused_cl_sda_template.inc"
`include "unused_flr_template.inc"
`include "unused_pcim_template.inc"

//---------------------------- 
// Internal signals
//---------------------------- 

axi_bus_t sh_ocl_bus();

logic clk;
(* dont_touch = "true" *) logic pipe_rst_n;
logic pre_sync_rst_n;
(* dont_touch = "true" *) logic sync_rst_n;

//---------------------------- 
// End Internal signals
//----------------------------

// Unused 'full' signals
assign cl_sh_dma_rd_full  = 1'b0;
assign cl_sh_dma_wr_full  = 1'b0;

// Unused *burst signals
assign cl_sh_ddr_arburst[1:0] = 2'h0;
assign cl_sh_ddr_awburst[1:0] = 2'h0;


assign clk = clk_main_a0;

//reset synchronizer
lib_pipe #(.WIDTH(1), .STAGES(4)) PIPE_RST_N (.clk(clk), .rst_n(1'b1), .in_bus(rst_main_n), .out_bus(pipe_rst_n));
   
always_ff @(negedge pipe_rst_n or posedge clk)
   if (!pipe_rst_n)
   begin
      pre_sync_rst_n <= 0;
      sync_rst_n <= 0;
   end
   else
   begin
      pre_sync_rst_n <= 1;
      sync_rst_n <= pre_sync_rst_n;
   end

// The functionality for these signals is TBD so they can can be tied-off.
assign cl_sh_status0 = 32'h0;
assign cl_sh_status1 = 32'h0;

assign cl_sh_id0[31:0] = `CL_SH_ID0; 
assign cl_sh_id1[31:0] = `CL_SH_ID1;

///////////////////////////////////////////////////////////////////////
///////////////// OCL SLAVE module ////////////////////////////////////
///////////////////////////////////////////////////////////////////////

logic sw_finished;
logic streaming_active;
logic sw_reset;
logic sw_reset_done;
logic sw_ddr_c;

assign sh_ocl_bus.awvalid = sh_ocl_awvalid;
assign sh_ocl_bus.awaddr[31:0] = sh_ocl_awaddr;
assign ocl_sh_awready = sh_ocl_bus.awready;
assign sh_ocl_bus.wvalid = sh_ocl_wvalid;
assign sh_ocl_bus.wdata[31:0] = sh_ocl_wdata;
assign sh_ocl_bus.wstrb[3:0] = sh_ocl_wstrb;
assign ocl_sh_wready = sh_ocl_bus.wready;
assign ocl_sh_bvalid = sh_ocl_bus.bvalid;
assign ocl_sh_bresp = sh_ocl_bus.bresp;
assign sh_ocl_bus.bready = sh_ocl_bready;
assign sh_ocl_bus.arvalid = sh_ocl_arvalid;
assign sh_ocl_bus.araddr[31:0] = sh_ocl_araddr;
assign ocl_sh_arready = sh_ocl_bus.arready;
assign ocl_sh_rvalid = sh_ocl_bus.rvalid;
assign ocl_sh_rresp = sh_ocl_bus.rresp;
assign ocl_sh_rdata = sh_ocl_bus.rdata[31:0];
assign sh_ocl_bus.rready = sh_ocl_rready;

cl_ocl_slv CL_OCL_SLV (

   .clk(clk),
   .sync_rst_n(sync_rst_n),

   .set_streaming_finished(sw_finished && sw_reset_done),
   .streaming_active(streaming_active),
   .sw_ddr_c(sw_ddr_c),

   .sh_ocl_bus  (sh_ocl_bus)
);

///////////////////////////////////////////////////////////////////////
///////////////// OCL SLAVE module ////////////////////////////////////
///////////////////////////////////////////////////////////////////////

// TODO add register bank for these signals
logic[63:0] sw_cl_sh_ddr_araddr;
logic sw_cl_sh_ddr_arvalid;
logic[7:0] sw_cl_sh_ddr_arlen;
logic sw_cl_sh_ddr_arready;
logic[511:0] sw_cl_sh_ddr_rdata;
logic sw_cl_sh_ddr_rvalid;
logic sw_cl_sh_ddr_rready;
logic[63:0] sw_cl_sh_ddr_awaddr;
logic sw_cl_sh_ddr_awvalid;
logic[7:0] sw_cl_sh_ddr_awlen;
logic[15:0] sw_cl_sh_ddr_awid;
logic sw_cl_sh_ddr_awready;
logic[511:0] sw_cl_sh_ddr_wdata;
logic sw_cl_sh_ddr_wvalid;
logic sw_cl_sh_ddr_wlast;
logic sw_cl_sh_ddr_wready;

always_ff @(negedge sync_rst_n or posedge clk)
  if (!sync_rst_n) begin
    sw_reset <= 0;
    sw_reset_done <= 0;
  end
  else if (sw_reset_done && sw_finished) begin
    $display("sw_reset_done back to 0!");
    sw_reset_done <= 0;
  end
  else if (sw_reset) begin
    sw_reset <= 0;
    $display("sw_reset_done set!");
    sw_reset_done <= 1'b1;
  end
  else if (streaming_active && !sw_reset_done) begin
    $display("sw_reset set!");
    sw_reset <= 1'b1;
  end

StreamingWrapper streaming_wrapper(
  .clock(clk),
  .reset(sw_reset),
  .io_inputMemAddr(sw_cl_sh_ddr_araddr),
  .io_inputMemAddrValid(sw_cl_sh_ddr_arvalid),
  .io_inputMemAddrLen(sw_cl_sh_ddr_arlen),
  .io_inputMemAddrReady(sw_cl_sh_ddr_arready),
  .io_inputMemBlock(sw_cl_sh_ddr_rdata),
  .io_inputMemBlockValid(sw_cl_sh_ddr_rvalid),
  .io_inputMemBlockReady(sw_cl_sh_ddr_rready),
  .io_outputMemAddr(sw_cl_sh_ddr_awaddr),
  .io_outputMemAddrValid(sw_cl_sh_ddr_awvalid),
  .io_outputMemAddrLen(sw_cl_sh_ddr_awlen),
  .io_outputMemAddrId(sw_cl_sh_ddr_awid),
  .io_outputMemAddrReady(sw_cl_sh_ddr_awready),
  .io_outputMemBlock(sw_cl_sh_ddr_wdata),
  .io_outputMemBlockValid(sw_cl_sh_ddr_wvalid),
  .io_outputMemBlockLast(sw_cl_sh_ddr_wlast),
  .io_outputMemBlockReady(sw_cl_sh_ddr_wready),
  .io_finished(sw_finished)
);

logic[15:0] cl_sh_ddr_awid_2d[2:0];
logic[63:0] cl_sh_ddr_awaddr_2d[2:0];
logic[7:0] cl_sh_ddr_awlen_2d[2:0];
logic[2:0] cl_sh_ddr_awsize_2d[2:0];
logic[1:0] cl_sh_ddr_awburst_2d[2:0];
logic cl_sh_ddr_awvalid_2d [2:0];
logic[2:0] sh_cl_ddr_awready_2d;

logic[15:0] cl_sh_ddr_wid_2d[2:0];
logic[511:0] cl_sh_ddr_wdata_2d[2:0];
logic[63:0] cl_sh_ddr_wstrb_2d[2:0];
logic[2:0] cl_sh_ddr_wlast_2d;
logic[2:0] cl_sh_ddr_wvalid_2d;
logic[2:0] sh_cl_ddr_wready_2d;

logic[15:0] sh_cl_ddr_bid_2d[2:0];
logic[1:0] sh_cl_ddr_bresp_2d[2:0];
logic[2:0] sh_cl_ddr_bvalid_2d;
logic[2:0] cl_sh_ddr_bready_2d;

logic[15:0] cl_sh_ddr_arid_2d[2:0];
logic[63:0] cl_sh_ddr_araddr_2d[2:0];
logic[7:0] cl_sh_ddr_arlen_2d[2:0];
logic[2:0] cl_sh_ddr_arsize_2d[2:0];
logic[1:0] cl_sh_ddr_arburst_2d[2:0];
logic[2:0] cl_sh_ddr_arvalid_2d;
logic[2:0] sh_cl_ddr_arready_2d;

logic[15:0] sh_cl_ddr_rid_2d[2:0];
logic[511:0] sh_cl_ddr_rdata_2d[2:0];
logic[1:0] sh_cl_ddr_rresp_2d[2:0];
logic[2:0] sh_cl_ddr_rlast_2d;
logic[2:0] sh_cl_ddr_rvalid_2d;
logic[2:0] cl_sh_ddr_rready_2d;

sh_ddr #(
         .DDR_A_PRESENT(0),
         .DDR_B_PRESENT(1),
         .DDR_D_PRESENT(0)
   ) SH_DDR
   (
   .clk(clk),
   .rst_n(sync_rst_n),

   .stat_clk(clk),
   .stat_rst_n(sync_rst_n),


   .CLK_300M_DIMM0_DP(CLK_300M_DIMM0_DP),
   .CLK_300M_DIMM0_DN(CLK_300M_DIMM0_DN),
   .M_A_ACT_N(M_A_ACT_N),
   .M_A_MA(M_A_MA),
   .M_A_BA(M_A_BA),
   .M_A_BG(M_A_BG),
   .M_A_CKE(M_A_CKE),
   .M_A_ODT(M_A_ODT),
   .M_A_CS_N(M_A_CS_N),
   .M_A_CLK_DN(M_A_CLK_DN),
   .M_A_CLK_DP(M_A_CLK_DP),
   .M_A_PAR(M_A_PAR),
   .M_A_DQ(M_A_DQ),
   .M_A_ECC(M_A_ECC),
   .M_A_DQS_DP(M_A_DQS_DP),
   .M_A_DQS_DN(M_A_DQS_DN),
   .cl_RST_DIMM_A_N(cl_RST_DIMM_A_N),


   .CLK_300M_DIMM1_DP(CLK_300M_DIMM1_DP),
   .CLK_300M_DIMM1_DN(CLK_300M_DIMM1_DN),
   .M_B_ACT_N(M_B_ACT_N),
   .M_B_MA(M_B_MA),
   .M_B_BA(M_B_BA),
   .M_B_BG(M_B_BG),
   .M_B_CKE(M_B_CKE),
   .M_B_ODT(M_B_ODT),
   .M_B_CS_N(M_B_CS_N),
   .M_B_CLK_DN(M_B_CLK_DN),
   .M_B_CLK_DP(M_B_CLK_DP),
   .M_B_PAR(M_B_PAR),
   .M_B_DQ(M_B_DQ),
   .M_B_ECC(M_B_ECC),
   .M_B_DQS_DP(M_B_DQS_DP),
   .M_B_DQS_DN(M_B_DQS_DN),
   .cl_RST_DIMM_B_N(cl_RST_DIMM_B_N),

   .CLK_300M_DIMM3_DP(CLK_300M_DIMM3_DP),
   .CLK_300M_DIMM3_DN(CLK_300M_DIMM3_DN),
   .M_D_ACT_N(M_D_ACT_N),
   .M_D_MA(M_D_MA),
   .M_D_BA(M_D_BA),
   .M_D_BG(M_D_BG),
   .M_D_CKE(M_D_CKE),
   .M_D_ODT(M_D_ODT),
   .M_D_CS_N(M_D_CS_N),
   .M_D_CLK_DN(M_D_CLK_DN),
   .M_D_CLK_DP(M_D_CLK_DP),
   .M_D_PAR(M_D_PAR),
   .M_D_DQ(M_D_DQ),
   .M_D_ECC(M_D_ECC),
   .M_D_DQS_DP(M_D_DQS_DP),
   .M_D_DQS_DN(M_D_DQS_DN),
   .cl_RST_DIMM_D_N(cl_RST_DIMM_D_N),

   //------------------------------------------------------
   // DDR-4 Interface from CL (AXI-4)
   //------------------------------------------------------
   .cl_sh_ddr_awid(cl_sh_ddr_awid_2d),
   .cl_sh_ddr_awaddr(cl_sh_ddr_awaddr_2d),
   .cl_sh_ddr_awlen(cl_sh_ddr_awlen_2d),
   .cl_sh_ddr_awsize(cl_sh_ddr_awsize_2d),
   .cl_sh_ddr_awvalid(cl_sh_ddr_awvalid_2d),
   .cl_sh_ddr_awburst(cl_sh_ddr_awburst_2d),
   .sh_cl_ddr_awready(sh_cl_ddr_awready_2d),

   .cl_sh_ddr_wid(cl_sh_ddr_wid_2d),
   .cl_sh_ddr_wdata(cl_sh_ddr_wdata_2d),
   .cl_sh_ddr_wstrb(cl_sh_ddr_wstrb_2d),
   .cl_sh_ddr_wlast(cl_sh_ddr_wlast_2d),
   .cl_sh_ddr_wvalid(cl_sh_ddr_wvalid_2d),
   .sh_cl_ddr_wready(sh_cl_ddr_wready_2d),

   .sh_cl_ddr_bid(sh_cl_ddr_bid_2d),
   .sh_cl_ddr_bresp(sh_cl_ddr_bresp_2d),
   .sh_cl_ddr_bvalid(sh_cl_ddr_bvalid_2d),
   .cl_sh_ddr_bready(cl_sh_ddr_bready_2d),

   .cl_sh_ddr_arid(cl_sh_ddr_arid_2d),
   .cl_sh_ddr_araddr(cl_sh_ddr_araddr_2d),
   .cl_sh_ddr_arlen(cl_sh_ddr_arlen_2d),
   .cl_sh_ddr_arsize(cl_sh_ddr_arsize_2d),
   .cl_sh_ddr_arvalid(cl_sh_ddr_arvalid_2d),
   .cl_sh_ddr_arburst(cl_sh_ddr_arburst_2d),
   .sh_cl_ddr_arready(sh_cl_ddr_arready_2d),

   .sh_cl_ddr_rid(sh_cl_ddr_rid_2d),
   .sh_cl_ddr_rdata(sh_cl_ddr_rdata_2d),
   .sh_cl_ddr_rresp(sh_cl_ddr_rresp_2d),
   .sh_cl_ddr_rlast(sh_cl_ddr_rlast_2d),
   .sh_cl_ddr_rvalid(sh_cl_ddr_rvalid_2d),
   .cl_sh_ddr_rready(cl_sh_ddr_rready_2d),

   .sh_cl_ddr_is_ready(),

   .sh_ddr_stat_addr0  (sh_ddr_stat_addr0) ,
   .sh_ddr_stat_wr0    (sh_ddr_stat_wr0     ) ,
   .sh_ddr_stat_rd0    (sh_ddr_stat_rd0     ) ,
   .sh_ddr_stat_wdata0 (sh_ddr_stat_wdata0  ) ,
   .ddr_sh_stat_ack0   (ddr_sh_stat_ack0    ) ,
   .ddr_sh_stat_rdata0 (ddr_sh_stat_rdata0  ),
   .ddr_sh_stat_int0   (ddr_sh_stat_int0    ),

   .sh_ddr_stat_addr1  (sh_ddr_stat_addr1) ,
   .sh_ddr_stat_wr1    (sh_ddr_stat_wr1     ) ,
   .sh_ddr_stat_rd1    (sh_ddr_stat_rd1     ) ,
   .sh_ddr_stat_wdata1 (sh_ddr_stat_wdata1  ) ,
   .ddr_sh_stat_ack1   (ddr_sh_stat_ack1    ) ,
   .ddr_sh_stat_rdata1 (ddr_sh_stat_rdata1  ),
   .ddr_sh_stat_int1   (ddr_sh_stat_int1    ),

   .sh_ddr_stat_addr2  (sh_ddr_stat_addr2) ,
   .sh_ddr_stat_wr2    (sh_ddr_stat_wr2     ) ,
   .sh_ddr_stat_rd2    (sh_ddr_stat_rd2     ) ,
   .sh_ddr_stat_wdata2 (sh_ddr_stat_wdata2  ) ,
   .ddr_sh_stat_ack2   (ddr_sh_stat_ack2    ) ,
   .ddr_sh_stat_rdata2 (ddr_sh_stat_rdata2  ),
   .ddr_sh_stat_int2   (ddr_sh_stat_int2    )
   );

assign cl_sh_ddr_awid = sw_ddr_c ? sw_cl_sh_ddr_awid : sh_cl_dma_pcis_awid;
assign cl_sh_ddr_awid_2d[1] = !sw_ddr_c ? sw_cl_sh_ddr_awid : sh_cl_dma_pcis_awid;
assign cl_sh_ddr_awaddr = sw_ddr_c ? sw_cl_sh_ddr_awaddr : sh_cl_dma_pcis_awaddr;
assign cl_sh_ddr_awaddr_2d[1] = !sw_ddr_c ? sw_cl_sh_ddr_awaddr : sh_cl_dma_pcis_awaddr;
assign cl_sh_ddr_awlen = sw_ddr_c ? sw_cl_sh_ddr_awlen : sh_cl_dma_pcis_awlen;
assign cl_sh_ddr_awlen_2d[1] = !sw_ddr_c ? sw_cl_sh_ddr_awlen : sh_cl_dma_pcis_awlen;
assign cl_sh_ddr_awsize = sw_ddr_c ? 3'b110 : sh_cl_dma_pcis_awsize;
assign cl_sh_ddr_awsize_2d[1] = !sw_ddr_c ? 3'b110 : sh_cl_dma_pcis_awsize;
assign cl_sh_ddr_awvalid = sw_ddr_c ? (sw_reset_done ? sw_cl_sh_ddr_awvalid : 0) : sh_cl_dma_pcis_awvalid;
assign cl_sh_ddr_awvalid_2d[1] = !sw_ddr_c ? (sw_reset_done ? sw_cl_sh_ddr_awvalid : 0) : sh_cl_dma_pcis_awvalid;
assign cl_sh_dma_pcis_awready = sw_ddr_c ? sh_cl_ddr_awready_2d[1] : sh_cl_ddr_awready;
assign sw_cl_sh_ddr_awready = !sw_ddr_c ? sh_cl_ddr_awready_2d[1] : sh_cl_ddr_awready;
assign cl_sh_ddr_wid = 16'b0;
assign cl_sh_ddr_wid_2d[1] = 16'b0;
assign cl_sh_ddr_wdata = sw_ddr_c ? sw_cl_sh_ddr_wdata : sh_cl_dma_pcis_wdata;
assign cl_sh_ddr_wdata_2d[1] = !sw_ddr_c ? sw_cl_sh_ddr_wdata : sh_cl_dma_pcis_wdata;
assign cl_sh_ddr_wstrb = sw_ddr_c ? 64'hffff_ffff_ffff_ffff : sh_cl_dma_pcis_wstrb;
assign cl_sh_ddr_wstrb_2d[1] = !sw_ddr_c ? 64'hffff_ffff_ffff_ffff : sh_cl_dma_pcis_wstrb;
assign cl_sh_ddr_wlast = sw_ddr_c ? sw_cl_sh_ddr_wlast : sh_cl_dma_pcis_wlast;
assign cl_sh_ddr_wlast_2d[1] = !sw_ddr_c ? sw_cl_sh_ddr_wlast : sh_cl_dma_pcis_wlast;
assign cl_sh_ddr_wvalid = sw_ddr_c ? (sw_reset_done ? sw_cl_sh_ddr_wvalid : 0) : sh_cl_dma_pcis_wvalid;
assign cl_sh_ddr_wvalid_2d[1] = !sw_ddr_c ? (sw_reset_done ? sw_cl_sh_ddr_wvalid : 0) : sh_cl_dma_pcis_wvalid;
assign cl_sh_dma_pcis_wready = sw_ddr_c ? sh_cl_ddr_wready_2d[1] : sh_cl_ddr_wready;
assign sw_cl_sh_ddr_wready = !sw_ddr_c ? sh_cl_ddr_wready_2d[1] : sh_cl_ddr_wready;
assign cl_sh_dma_pcis_bid = sw_ddr_c ? sh_cl_ddr_bid_2d[1] : sh_cl_ddr_bid;
assign cl_sh_dma_pcis_bresp = sw_ddr_c ? sh_cl_ddr_bresp_2d[1] : sh_cl_ddr_bresp;
assign cl_sh_dma_pcis_bvalid = sw_ddr_c ? sh_cl_ddr_bvalid_2d[1] : sh_cl_ddr_bvalid;
assign cl_sh_ddr_bready = sw_ddr_c ? 1'b1 : sh_cl_dma_pcis_bready;
assign cl_sh_ddr_bready_2d[1] = !sw_ddr_c ? 1'b1 : sh_cl_dma_pcis_bready;
assign cl_sh_ddr_arid = sw_ddr_c ? 16'b0 : sh_cl_dma_pcis_arid;
assign cl_sh_ddr_arid_2d[1] = !sw_ddr_c ? 16'b0 : sh_cl_dma_pcis_arid;
assign cl_sh_ddr_araddr = sw_ddr_c ? sw_cl_sh_ddr_araddr : sh_cl_dma_pcis_araddr;
assign cl_sh_ddr_araddr_2d[1] = !sw_ddr_c ? sw_cl_sh_ddr_araddr : sh_cl_dma_pcis_araddr;
assign cl_sh_ddr_arlen = sw_ddr_c ? sw_cl_sh_ddr_arlen : sh_cl_dma_pcis_arlen;
assign cl_sh_ddr_arlen_2d[1] = !sw_ddr_c ? sw_cl_sh_ddr_arlen : sh_cl_dma_pcis_arlen;
assign cl_sh_ddr_arsize = sw_ddr_c ? 3'b110 : sh_cl_dma_pcis_arsize;
assign cl_sh_ddr_arsize_2d[1] = !sw_ddr_c ? 3'b110 : sh_cl_dma_pcis_arsize;
assign cl_sh_ddr_arvalid = sw_ddr_c ? (sw_reset_done ? sw_cl_sh_ddr_arvalid : 0) : sh_cl_dma_pcis_arvalid;
assign cl_sh_ddr_arvalid_2d[1] = !sw_ddr_c ? (sw_reset_done ? sw_cl_sh_ddr_arvalid : 0) : sh_cl_dma_pcis_arvalid;
assign cl_sh_dma_pcis_arready = sw_ddr_c ? sh_cl_ddr_arready_2d[1] : sh_cl_ddr_arready;
assign sw_cl_sh_ddr_arready = !sw_ddr_c ? sh_cl_ddr_arready_2d[1] : sh_cl_ddr_arready;
assign cl_sh_dma_pcis_rid = sw_ddr_c ? sh_cl_ddr_rid_2d[1] : sh_cl_ddr_rid;
assign cl_sh_dma_pcis_rresp = sw_ddr_c ? sh_cl_ddr_rresp_2d[1] : sh_cl_ddr_rresp;
assign cl_sh_dma_pcis_rvalid = sw_ddr_c ? sh_cl_ddr_rvalid_2d[1] : sh_cl_ddr_rvalid;
assign sw_cl_sh_ddr_rvalid = !sw_ddr_c ? sh_cl_ddr_rvalid_2d[1] : sh_cl_ddr_rvalid;
assign cl_sh_dma_pcis_rdata = sw_ddr_c ? sh_cl_ddr_rdata_2d[1] : sh_cl_ddr_rdata;
assign sw_cl_sh_ddr_rdata = !sw_ddr_c ? sh_cl_ddr_rdata_2d[1] : sh_cl_ddr_rdata;
assign cl_sh_dma_pcis_rlast = sw_ddr_c ? sh_cl_ddr_rlast_2d[1] : sh_cl_ddr_rlast;
assign cl_sh_ddr_rready = sw_ddr_c ? (sw_reset_done ? sw_cl_sh_ddr_rready : 0) : sh_cl_dma_pcis_rready;
assign cl_sh_ddr_rready_2d[1] = !sw_ddr_c ? (sw_reset_done ? sw_cl_sh_ddr_rready : 0) : sh_cl_dma_pcis_rready;

endmodule   
