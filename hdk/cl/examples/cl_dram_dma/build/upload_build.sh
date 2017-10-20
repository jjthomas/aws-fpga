#!/bin/bash
aws s3 cp $CL_DIR/build/checkpoints/to_aws/$1 s3://fpga-dcp/dcp/
aws ec2 create-fpga-image --region us-east-1 --name $2 --input-storage-location Bucket=fpga-dcp,Key=dcp/$1 --logs-storage-location Bucket=fpga-dcp,Key=logs
rm $CL_DIR/build/checkpoints/to_aws/*.dcp
rm $CL_DIR/build/checkpoints/*.dcp
rm $CL_DIR/build/checkpoints/to_aws/*.tar
