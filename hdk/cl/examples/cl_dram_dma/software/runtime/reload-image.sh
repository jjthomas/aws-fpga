sudo fpga-clear-local-image  -S 0
sudo fpga-load-local-image -S 0 -I agfi-0d7757aa2b8b448dd
sudo fpga-describe-local-image -S 0 -R -H
sudo rmmod edma-drv && sudo insmod ~/src/project_data/aws-fpga/sdk/linux_kernel_drivers/edma/edma-drv.ko
