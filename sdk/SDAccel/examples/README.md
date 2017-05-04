
# AWS SDAccel Example Software (SW) Emulation

The main goal of SW emulation is to ensure functional correctness and to partition the application into kernels vs host.  For CPU-based (SW) emulation, both the host code and the kernel(s) code are compiled to run on an x86 processor. The SW Emulation enables developer to iterate and refine the algorithms through fast compile, and iteration time is similar to software compile and run cycle on CPU. 

```
    $ cd $SDK_DIR/SDAccel/examples/xilinx/getting_started/basic/hello/             # Start using an SDAccel example
    $ make clean                                                                   # clean up before you start
    $ export XCL_EMULATION_MODE=sw_emu
    $ emconfigutil -f $SDK_DIR/SDAccel/aws_platform/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0/hw/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0.dsa --nd 1                                                                 # Create emulation config file
    $ make TARGETS=sw_emu DEVICES=$SDK_DIR/SDAccel/aws_platform/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0.xpfm all      # Compile using xocc.  "hw_emu" is another option
    $ ./hello xclbin/krnl_hello.sw_emu.xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0.xclbin # Run using sw emulation
```

# AWS SDAccel Example Hardware(HW) Emulation

The SDAccel hardware emulation flow enables the developer to check the correctness of the logic generated for the custom kernels. This emulation flow invokes the hardware simulator in the SDAccel environment to test the functionality that will be executed on FPGA Custom Logic. 

The instructions below describe how to get started on SDAccel development using the HW Emulation: 

```
    $ cd $SDK_DIR/SDAccel/examples/xilinx/getting_started/basic/hello/             # Start using an SDAccel example
    $ make clean                                                                   # clean up before you start
    $ export XCL_EMULATION_MODE=hw_emu
    $ emconfigutil -f $SDK_DIR/SDAccel/aws_platform/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0/hw/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0.dsa --nd 1                                                                 # Create emulation config file
    $ make TARGETS=hw_emu DEVICES=$SDK_DIR/SDAccel/aws_platform/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0.xpfm all      # Compile using xocc.  "hw_emu" is another option
    $ ./hello xclbin/krnl_hello.hw_emu.xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0.xclbin # Run using hw emulation
```
# AWS SDAccel Example FPGA Hardware Build 

The SDAccel build flow enables the developer to compile custom kernels.  

The instructions below describe how to build for FPGA Hardware: 

```
    $ cd $SDK_DIR/SDAccel/examples/xilinx/getting_started/basic/hello/             # Start using an SDAccel example
    $ make clean                                                                   # clean up before you start
    $ export XCL_EMULATION_MODE=
    $ make DEVICES=$SDK_DIR/SDAccel/aws_platform/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0.xpfm CLFLAGS="--xp param:compiler.worstNegativeSlack=-0.025" all  # Compile using xocc.  
    # Run is not supported
```
