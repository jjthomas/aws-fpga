sudo fpga-load-local-image -S 0 -I agfi-05fa0ce28c6fe209b
sudo fpga-describe-local-image -S 0 -R -H
sudo rmmod edma-drv && sudo insmod ~/src/project_data/aws-fpga/sdk/linux_kernel_drivers/edma/edma-drv.ko
sleep 10
sudo fpga-describe-local-image -S 0 -H

