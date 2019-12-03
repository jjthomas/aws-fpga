module StreamingWrapper(
  input          clock,
  input          reset,
  output [63:0]  io_inputMemAddrs_0,
  output         io_inputMemAddrValids_0,
  output [7:0]   io_inputMemAddrLens_0,
  input          io_inputMemAddrReadys_0,
  input  [511:0] io_inputMemBlocks_0,
  input          io_inputMemBlockValids_0,
  output         io_inputMemBlockReadys_0,
  output [63:0]  io_outputMemAddrs_0,
  output         io_outputMemAddrValids_0,
  output [7:0]   io_outputMemAddrLens_0,
  output [15:0]  io_outputMemAddrIds_0,
  input          io_outputMemAddrReadys_0,
  output [511:0] io_outputMemBlocks_0,
  output         io_outputMemBlockValids_0,
  output         io_outputMemBlockLasts_0,
  input          io_outputMemBlockReadys_0,
  output         io_finished
);
endmodule
