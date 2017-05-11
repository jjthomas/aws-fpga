`ifdef RANDOMIZE_GARBAGE_ASSIGN
`define RANDOMIZE
`endif
`ifdef RANDOMIZE_INVALID_ASSIGN
`define RANDOMIZE
`endif
`ifdef RANDOMIZE_REG_INIT
`define RANDOMIZE
`endif
`ifdef RANDOMIZE_MEM_INIT
`define RANDOMIZE
`endif

module SortingNetwork(
  input   clock,
  input   reset,
  input   io_blockValid,
  input  [63:0] io_block,
  input   io_downstreamReady,
  output  io_thisReady,
  output  io_outValid,
  output [63:0] io_out
);
  reg [1:0] numFilled;
  reg [31:0] _GEN_25;
  reg [1:0] numDrained;
  reg [31:0] _GEN_26;
  wire  advance;
  reg [63:0] _T_20_0;
  reg [63:0] _GEN_27;
  reg [63:0] _T_20_1;
  reg [63:0] _GEN_28;
  reg  _T_26;
  reg [31:0] _GEN_29;
  reg [63:0] _T_30_0;
  reg [63:0] _GEN_30;
  reg [63:0] _T_30_1;
  reg [63:0] _GEN_31;
  reg  _T_36;
  reg [31:0] _GEN_32;
  wire  _T_38;
  wire [63:0] _GEN_0;
  wire [63:0] _GEN_2;
  wire  _T_40;
  wire  _GEN_3;
  wire  _T_41;
  wire  _GEN_4;
  wire  _T_44;
  wire  _T_45;
  wire  _T_46;
  wire [63:0] _GEN_1;
  wire [63:0] _GEN_5;
  wire [63:0] _GEN_6;
  wire  _T_49;
  wire [1:0] _GEN_7;
  wire  _GEN_8;
  wire  _T_53;
  wire [2:0] _T_55;
  wire [1:0] _T_56;
  wire [1:0] _GEN_9;
  wire [63:0] _GEN_10;
  wire [63:0] _GEN_11;
  wire [1:0] _GEN_12;
  wire  _GEN_13;
  wire  _T_59;
  wire  _T_61;
  wire [1:0] _GEN_14;
  wire  _GEN_15;
  wire  _T_65;
  wire [2:0] _T_67;
  wire [1:0] _T_68;
  wire [1:0] _GEN_16;
  wire [1:0] _GEN_17;
  wire  _GEN_18;
  wire [20:0] _T_69;
  wire [20:0] _T_70;
  wire  _T_71;
  wire [63:0] _GEN_19;
  wire [63:0] _GEN_20;
  wire  _T_73;
  wire [63:0] _GEN_21;
  wire [63:0] _GEN_22;
  wire [63:0] _GEN_23;
  wire [63:0] _GEN_24;
  assign io_thisReady = _T_44;
  assign io_outValid = _T_36;
  assign io_out = _GEN_0;
  assign advance = _T_40;
  assign _T_38 = numDrained[0];
  assign _GEN_0 = _GEN_2;
  assign _GEN_2 = _T_38 ? _T_30_1 : _T_30_0;
  assign _T_40 = _T_36 == 1'h0;
  assign _GEN_3 = advance ? _T_26 : _T_36;
  assign _T_41 = advance & _T_26;
  assign _GEN_4 = _T_41 ? 1'h0 : _T_26;
  assign _T_44 = _T_26 == 1'h0;
  assign _T_45 = io_blockValid & _T_44;
  assign _T_46 = numFilled[0];
  assign _GEN_1 = io_block;
  assign _GEN_5 = 1'h0 == _T_46 ? _GEN_1 : _T_20_0;
  assign _GEN_6 = _T_46 ? _GEN_1 : _T_20_1;
  assign _T_49 = numFilled == 2'h1;
  assign _GEN_7 = _T_49 ? 2'h0 : numFilled;
  assign _GEN_8 = _T_49 ? 1'h1 : _GEN_4;
  assign _T_53 = _T_49 == 1'h0;
  assign _T_55 = numFilled + 2'h1;
  assign _T_56 = _T_55[1:0];
  assign _GEN_9 = _T_53 ? _T_56 : _GEN_7;
  assign _GEN_10 = _T_45 ? _GEN_5 : _T_20_0;
  assign _GEN_11 = _T_45 ? _GEN_6 : _T_20_1;
  assign _GEN_12 = _T_45 ? _GEN_9 : numFilled;
  assign _GEN_13 = _T_45 ? _GEN_8 : _GEN_4;
  assign _T_59 = io_downstreamReady & _T_36;
  assign _T_61 = numDrained == 2'h1;
  assign _GEN_14 = _T_61 ? 2'h0 : numDrained;
  assign _GEN_15 = _T_61 ? 1'h0 : _GEN_3;
  assign _T_65 = _T_61 == 1'h0;
  assign _T_67 = numDrained + 2'h1;
  assign _T_68 = _T_67[1:0];
  assign _GEN_16 = _T_65 ? _T_68 : _GEN_14;
  assign _GEN_17 = _T_59 ? _GEN_16 : numDrained;
  assign _GEN_18 = _T_59 ? _GEN_15 : _GEN_3;
  assign _T_69 = _T_20_0[20:0];
  assign _T_70 = _T_20_1[20:0];
  assign _T_71 = _T_69 < _T_70;
  assign _GEN_19 = _T_71 ? _T_20_0 : _T_30_0;
  assign _GEN_20 = _T_71 ? _T_20_1 : _T_30_1;
  assign _T_73 = _T_71 == 1'h0;
  assign _GEN_21 = _T_73 ? _T_20_1 : _GEN_19;
  assign _GEN_22 = _T_73 ? _T_20_0 : _GEN_20;
  assign _GEN_23 = advance ? _GEN_21 : _T_30_0;
  assign _GEN_24 = advance ? _GEN_22 : _T_30_1;
`ifdef RANDOMIZE
  integer initvar;
  initial begin
    `ifndef verilator
      #0.002 begin end
    `endif
  `ifdef RANDOMIZE_REG_INIT
  _GEN_25 = {1{$random}};
  numFilled = _GEN_25[1:0];
  `endif
  `ifdef RANDOMIZE_REG_INIT
  _GEN_26 = {1{$random}};
  numDrained = _GEN_26[1:0];
  `endif
  `ifdef RANDOMIZE_REG_INIT
  _GEN_27 = {2{$random}};
  _T_20_0 = _GEN_27[63:0];
  `endif
  `ifdef RANDOMIZE_REG_INIT
  _GEN_28 = {2{$random}};
  _T_20_1 = _GEN_28[63:0];
  `endif
  `ifdef RANDOMIZE_REG_INIT
  _GEN_29 = {1{$random}};
  _T_26 = _GEN_29[0:0];
  `endif
  `ifdef RANDOMIZE_REG_INIT
  _GEN_30 = {2{$random}};
  _T_30_0 = _GEN_30[63:0];
  `endif
  `ifdef RANDOMIZE_REG_INIT
  _GEN_31 = {2{$random}};
  _T_30_1 = _GEN_31[63:0];
  `endif
  `ifdef RANDOMIZE_REG_INIT
  _GEN_32 = {1{$random}};
  _T_36 = _GEN_32[0:0];
  `endif
  end
`endif
  always @(posedge clock) begin
    if (reset) begin
      numFilled <= 2'h0;
    end else begin
      if (_T_45) begin
        if (_T_53) begin
          numFilled <= _T_56;
        end else begin
          if (_T_49) begin
            numFilled <= 2'h0;
          end
        end
      end
    end
    if (reset) begin
      numDrained <= 2'h0;
    end else begin
      if (_T_59) begin
        if (_T_65) begin
          numDrained <= _T_68;
        end else begin
          if (_T_61) begin
            numDrained <= 2'h0;
          end
        end
      end
    end
    if (_T_45) begin
      if (1'h0 == _T_46) begin
        _T_20_0 <= _GEN_1;
      end
    end
    if (_T_45) begin
      if (_T_46) begin
        _T_20_1 <= _GEN_1;
      end
    end
    if (reset) begin
      _T_26 <= 1'h0;
    end else begin
      if (_T_45) begin
        if (_T_49) begin
          _T_26 <= 1'h1;
        end else begin
          if (_T_41) begin
            _T_26 <= 1'h0;
          end
        end
      end else begin
        if (_T_41) begin
          _T_26 <= 1'h0;
        end
      end
    end
    if (advance) begin
      if (_T_73) begin
        _T_30_0 <= _T_20_1;
      end else begin
        if (_T_71) begin
          _T_30_0 <= _T_20_0;
        end
      end
    end
    if (advance) begin
      if (_T_73) begin
        _T_30_1 <= _T_20_0;
      end else begin
        if (_T_71) begin
          _T_30_1 <= _T_20_1;
        end
      end
    end
    if (reset) begin
      _T_36 <= 1'h0;
    end else begin
      if (_T_59) begin
        if (_T_61) begin
          _T_36 <= 1'h0;
        end else begin
          if (advance) begin
            _T_36 <= _T_26;
          end
        end
      end else begin
        if (advance) begin
          _T_36 <= _T_26;
        end
      end
    end
  end
endmodule
