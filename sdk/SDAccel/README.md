# AWS SDAccel Preview

Software(SW) and Hardware(HW) Emulation modes in SDAccel are now supported.  In addition, FPGA targeted HW builds are now supported.  The AFI generation and runtime flow is coming soon

The quickstart examples require Xilinx 2017.1 SDx tool set.  A pre-released FPGA Developer AMI version (1.3.X) is available and has been shared with your AWS account.  Current AMI ID:  ami-f1365be7

## Get Started on SDAccel

To get started, you will need to clone the GitHub repository where we are privately previewing SDAccel.  Next source the sdk and sdaccel setup scripts.    
```
    $ git clone git@github.com:aws/aws-fpga-preview.git $AWS_FPGA_REPO_DIR  
    $ cd $AWS_FPGA_REPO_DIR                                         
    $ source sdk_setup.sh
    $ cd sdk/SDAccel
    $ source sdaccel_setup.sh
```
Now your ready to start using the [SDAccel examples](./examples/README.md)

    
