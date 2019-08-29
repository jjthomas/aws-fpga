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

module cl_dram_dma #(parameter NUM_DDR=1) 

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
`include "unused_ddr_a_b_d_template.inc"
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

(* dont_touch = "true" *) logic ocl_slv_sync_rst_n;
lib_pipe #(.WIDTH(1), .STAGES(4)) OCL_SLV_SLC_RST_N (.clk(clk), .rst_n(1'b1), .in_bus(sync_rst_n), .out_bus(ocl_slv_sync_rst_n));
cl_ocl_slv CL_OCL_SLV (

   .clk(clk),
   .sync_rst_n(ocl_slv_sync_rst_n),

   .set_streaming_finished(sw_finished && sw_reset_done),
   .streaming_active(streaming_active),

   .sh_ocl_bus  (sh_ocl_bus)
);

///////////////////////////////////////////////////////////////////////
///////////////// OCL SLAVE module ////////////////////////////////////
///////////////////////////////////////////////////////////////////////

// TODO add register bank for these signals
logic[63:0] sw_cl_sh_ddr_araddr;
logic sw_cl_sh_ddr_arvalid;
logic[8:0] sw_cl_sh_ddr_arlen;
logic sw_cl_sh_ddr_arready;
logic[511:0] sw_cl_sh_ddr_rdata;
logic sw_cl_sh_ddr_rvalid;
logic sw_cl_sh_ddr_rready;
logic[63:0] sw_cl_sh_ddr_awaddr;
logic sw_cl_sh_ddr_awvalid;
logic[8:0] sw_cl_sh_ddr_awlen;
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
    $display("sw_reset_done set!");
    sw_reset <= 0;
    sw_reset_done <= 1'b1;
  end
  else if (streaming_active && !sw_reset_done) begin
    $display("sw_reset set!");
    sw_reset <= 1'b1;
  end

(* DONT_TOUCH = "yes" *) StreamingWrapper streaming_wrapper(
  .clock(clk),
  .reset(sw_reset),
  .io_inputMemAddrs_0(sw_cl_sh_ddr_araddr),
  .io_inputMemAddrValids_0(sw_cl_sh_ddr_arvalid),
  .io_inputMemAddrLens_0(sw_cl_sh_ddr_arlen),
  .io_inputMemAddrReadys_0(sw_cl_sh_ddr_arready),
  .io_inputMemBlocks_0(sw_cl_sh_ddr_rdata),
  .io_inputMemBlockValids_0(sw_cl_sh_ddr_rvalid),
  .io_inputMemBlockReadys_0(sw_cl_sh_ddr_rready),
  .io_outputMemAddrs_0(sw_cl_sh_ddr_awaddr),
  .io_outputMemAddrValids_0(sw_cl_sh_ddr_awvalid),
  .io_outputMemAddrLens_0(sw_cl_sh_ddr_awlen),
  .io_outputMemAddrIds_0(sw_cl_sh_ddr_awid),
  .io_outputMemAddrReadys_0(sw_cl_sh_ddr_awready),
  .io_outputMemBlocks_0(sw_cl_sh_ddr_wdata),
  .io_outputMemBlockValids_0(sw_cl_sh_ddr_wvalid),
  .io_outputMemBlockLasts_0(sw_cl_sh_ddr_wlast),
  .io_outputMemBlockReadys_0(sw_cl_sh_ddr_wready),
  .io_finished(sw_finished)
);

/*
cl_debug_bridge CL_DEBUG_BRIDGE (
     .clk(clk),
     .S_BSCAN_drck(drck),
     .S_BSCAN_shift(shift),
     .S_BSCAN_tdi(tdi),
     .S_BSCAN_update(update),
     .S_BSCAN_sel(sel),
     .S_BSCAN_tdo(tdo),
     .S_BSCAN_tms(tms),
     .S_BSCAN_tck(tck),
     .S_BSCAN_runtest(runtest),
     .S_BSCAN_reset(reset),
     .S_BSCAN_capture(capture),
     .S_BSCAN_bscanid_en(bscanid_en)
  );

ila_1 streaming_wrapper_ila (
                .clk    (clk),
                .probe0 (sw_cl_sh_ddr_awvalid),
                .probe1 (sw_cl_sh_ddr_awaddr),
                .probe2 (2'b0),
                .probe3 (sw_cl_sh_ddr_awready),
                .probe4 (sw_cl_sh_ddr_wvalid),
                .probe5 (64'b0),
                .probe6 (sw_cl_sh_ddr_wlast),
                .probe7 (sw_cl_sh_ddr_wready),
                .probe8 (1'b0),
                .probe9 (1'b0),
                .probe10 (sw_cl_sh_ddr_wdata),
                .probe11 (1'b0),
                .probe12 (sw_cl_sh_ddr_arready),
                .probe13 (2'b0),
                .probe14 (sw_cl_sh_ddr_rdata),
                .probe15 (sw_cl_sh_ddr_araddr),
                .probe16 (sw_cl_sh_ddr_arvalid),
                .probe17 (3'b0),
                .probe18 (3'b0),
                .probe19 (sw_cl_sh_ddr_awid),
                .probe20 (5'b0),
                .probe21 (sw_cl_sh_ddr_awlen),
                .probe22 (1'b0),
                .probe23 (3'b0), 
                .probe24 (2'b0),
                .probe25 (5'b0),
                .probe26 (sw_cl_sh_ddr_rvalid),
                .probe27 (sw_cl_sh_ddr_arlen),
                .probe28 (3'b0),
                .probe29 (2'b0),
                .probe30 (sw_cl_sh_ddr_rready),
                .probe31 (4'b0),
                .probe32 (4'b0),
                .probe33 (4'b0),
                .probe34 (4'b0),
                .probe35 (1'b0),
                .probe36 (4'b0),
                .probe37 (4'b0),
                .probe38 (5'b0),
                .probe39 (1'b0),
                .probe40 (sw_reset),
                .probe41 (sw_finished),
                .probe42 (1'b0),
                .probe43 (1'b0)
                );

ila_0 streaming_wrapper_ila (
                .clk    (clk),
                .probe0 (sw_reset),
                .probe1 (64'b0),
                .probe2 (sw_finished),
                .probe3 (sw_cl_sh_ddr_awready),
                .probe4 (64'b0),
                .probe5 (sw_cl_sh_ddr_awvalid)
);
*/

assign cl_sh_ddr_awid = sw_reset_done ? sw_cl_sh_ddr_awid : sh_cl_dma_pcis_awid;
assign cl_sh_ddr_awaddr = sw_reset_done ? sw_cl_sh_ddr_awaddr : sh_cl_dma_pcis_awaddr;
assign cl_sh_ddr_awlen = sw_reset_done ? sw_cl_sh_ddr_awlen : sh_cl_dma_pcis_awlen;
assign cl_sh_ddr_awsize = sw_reset_done ? 3'b110 : sh_cl_dma_pcis_awsize;
assign cl_sh_ddr_awvalid = sw_reset_done ? sw_cl_sh_ddr_awvalid : sh_cl_dma_pcis_awvalid;
assign cl_sh_dma_pcis_awready = sw_reset_done ? 0 : sh_cl_ddr_awready;
assign sw_cl_sh_ddr_awready = sw_reset_done ? sh_cl_ddr_awready : 0;
assign cl_sh_ddr_wid = 16'b0;
assign cl_sh_ddr_wdata = sw_reset_done ? sw_cl_sh_ddr_wdata : sh_cl_dma_pcis_wdata;
assign cl_sh_ddr_wstrb = sw_reset_done ? 64'hffff_ffff_ffff_ffff : sh_cl_dma_pcis_wstrb;
assign cl_sh_ddr_wlast = sw_reset_done ? sw_cl_sh_ddr_wlast : sh_cl_dma_pcis_wlast;
assign cl_sh_ddr_wvalid = sw_reset_done ? sw_cl_sh_ddr_wvalid : sh_cl_dma_pcis_wvalid;
assign cl_sh_dma_pcis_wready = sw_reset_done ? 0 : sh_cl_ddr_wready;
assign sw_cl_sh_ddr_wready = sw_reset_done ? sh_cl_ddr_wready : 0;
assign cl_sh_dma_pcis_bid = sh_cl_ddr_bid;
assign cl_sh_dma_pcis_bresp = sh_cl_ddr_bresp;
assign cl_sh_dma_pcis_bvalid = sw_reset_done ? 0 : sh_cl_ddr_bvalid;
assign cl_sh_ddr_bready = sw_reset_done ? 1'b1 : sh_cl_dma_pcis_bready;
assign cl_sh_ddr_arid = sw_reset_done ? 16'b0 : sh_cl_dma_pcis_arid;
assign cl_sh_ddr_araddr = sw_reset_done ? sw_cl_sh_ddr_araddr : sh_cl_dma_pcis_araddr;
assign cl_sh_ddr_arlen = sw_reset_done ? sw_cl_sh_ddr_arlen : sh_cl_dma_pcis_arlen;
assign cl_sh_ddr_arsize = sw_reset_done ? 3'b110 : sh_cl_dma_pcis_arsize;
assign cl_sh_ddr_arvalid = sw_reset_done ? sw_cl_sh_ddr_arvalid : sh_cl_dma_pcis_arvalid;
assign cl_sh_dma_pcis_arready = sw_reset_done ? 0 : sh_cl_ddr_arready;
assign sw_cl_sh_ddr_arready = sw_reset_done ? sh_cl_ddr_arready : 0;
assign cl_sh_dma_pcis_rid = sh_cl_ddr_rid;
assign cl_sh_dma_pcis_rresp = sh_cl_ddr_rresp;
assign cl_sh_dma_pcis_rvalid = sw_reset_done ? 0 : sh_cl_ddr_rvalid;
assign sw_cl_sh_ddr_rvalid = sw_reset_done ? sh_cl_ddr_rvalid : 0;
assign cl_sh_dma_pcis_rdata = sh_cl_ddr_rdata;
assign sw_cl_sh_ddr_rdata = sh_cl_ddr_rdata;
assign cl_sh_dma_pcis_rlast = sh_cl_ddr_rlast;
assign cl_sh_ddr_rready = sw_reset_done ? sw_cl_sh_ddr_rready : sh_cl_dma_pcis_rready;

endmodule   
