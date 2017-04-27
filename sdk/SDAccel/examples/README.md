
# AWS EC2 SDAccel Quickstart

Both Software(SW) and Hardware(HW) Emulation modes in SDAccel are now supported.  

The quickstart examples require Xilinx 2016.4 SDx tool set, which are included in the [AWS FPGA Developer AMI available from AWS Marketplace](https://aws.amazon.com/marketplace/pp/B06VVYBLZZ).  

The SDAccel build flow targeting F1 FPGA is coming soon.

## Get Started on Early Preview of SDAccel

```
    $ git clone git@github.com:aws/aws-fpga.git $AWS_FPGA_REPO_DIR  # Clone Repo (if you haven't already)
    $ cd $AWS_FPGA_REPO_DIR                                         
    $ git checkout kristopk_preview                     # checkout early access branch
    $ source sdk_setup.sh
    $ cd sdk/SDAccel
    $ source sdaccel_setup.sh
```
## AWS EC2 SDAccel Example Software (SW) Emulation

The main goal of SW emulation is to ensure functional correctness and to partition the application into kernels vs host.  For CPU-based (SW) emulation, both the host code and the kernel(s) code are compiled to run on an x86 processor. The SW Emulation enables developer to iterate and refine the algorithms through fast compile, and iteration time is similar to software compile and run cycle on CPU. 

```
    $ cd $SDK_DIR/SDAccel/examples/xilinx/getting_started/basic/hello/             # Start using an SDAccel example
    $ make clean                                                                   # clean up before you start
    $ emconfigutil -f $SDK_DIR/SDAccel/platforms/xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3/hw/xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3.dsa --nd 1                                                                 # Create emulation config file
    $ make TARGETS=sw_emu DEVICES=$SDK_DIR/SDAccel/platforms/xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3/xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3.xpfm all      # Compile using xocc.  "hw_emu" is another option
    $ ./hello xclbin/krnl_hello.sw_emu.xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3.xclbin # Run using sw emulation
```

## AWS EC2 SDAccel Example Hardware(HW) Emulation

The SDAccel hardware emulation flow enables the developer to check the correctness of the logic generated for the custom kernels. This emulation flow invokes the hardware simulator in the SDAccel environment to test the functionality that will be executed on FPGA Custom Logic. 

The instructions below describe how to get started on SDAccel development using the HW Emulation: 

```
    $ cd $SDK_DIR/SDAccel/examples/xilinx/getting_started/basic/hello/             # Start using an SDAccel example
    $ make clean                                                                   # clean up before you start
    $ emconfigutil -f $SDK_DIR/SDAccel/platforms/xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3/hw/xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3.dsa --nd 1                                                                 # Create emulation config file
    $ make TARGETS=hw_emu DEVICES=$SDK_DIR/SDAccel/platforms/xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3/xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3.xpfm all      # Compile using xocc.  "hw_emu" is another option
    $ ./hello xclbin/krnl_hello.sw_emu.xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3.xclbin # Run using hw emulation
```
