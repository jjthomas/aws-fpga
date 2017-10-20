#!/bin/bash
if [ -z "$1" ]
then
  echo "timestamp not supplied"
  exit
fi
if [ -z "$2" ]
then
  echo "bitstream name not supplied"
  exit
fi
aws s3 cp $CL_DIR/build/checkpoints/to_aws/$1.Developer_CL.tar s3://fpga-dcp/dcp/
aws ec2 create-fpga-image --region us-east-1 --name $2 --input-storage-location Bucket=fpga-dcp,Key=dcp/$1.Developer_CL.tar --logs-storage-location Bucket=fpga-dcp,Key=logs
rm $CL_DIR/build/checkpoints/to_aws/$1*.dcp
rm $CL_DIR/build/checkpoints/$1*.dcp
rm $CL_DIR/build/checkpoints/to_aws/$1*.tar
rm -rf $CL_DIR/build/scripts/.Xil
