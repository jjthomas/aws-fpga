#!/bin/bash

#
# Copyright 2015-2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License"). You may
# not use this file except in compliance with the License. A copy of the
# License is located at
#
#     http://aws.amazon.com/apache2.0/
#
# or in the "license" file accompanying this file. This file is distributed
# on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
# express or implied. See the License for the specific language governing
# permissions and limitations under the License.
#
script=${BASH_SOURCE[0]}
if [ $script == $0 ]; then
  echo "ERROR: You must source this script"
  exit 2
fi
full_script=$(readlink -f $script)
script_name=$(basename $full_script)
script_dir=$(dirname $full_script)

debug=0

function info_msg {
  echo -e "AWS FPGA-SDK-INFO: $1"
}

function debug_msg {
  if [[ $debug == 0 ]]; then
    return
  fi
  echo -e "AWS FPGA-SDK-DEBUG: $1"
}

function err_msg {
  echo -e >&2 "AWS FPGA-SDK-ERROR: $1"
}

function usage {
  echo -e "USAGE: source [\$AWS_FPGA_REPO_DIR/]$script_name [-d|-debug] [-h|-help]"
}

function help {
  info_msg "$script_name"
  info_msg " "
  info_msg "Sets up the environment for AWS FPGA SDAccel tools."
  info_msg " "
  info_msg "sdaccel_setup.sh script will:"
  info_msg "  (1) check if Xilinx's vivado is installed,"
  echo " "
  usage
}

# Process command line args
args=( "$@" )
for (( i = 0; i < ${#args[@]}; i++ )); do
  arg=${args[$i]}
  case $arg in
    -d|-debug)
      debug=1
    ;;
    -h|-help)
      help
      return 0
    ;;
    *)
      err_msg "Invalid option: $arg\n"
      usage
      return 1
  esac
done

# Make sure that AWS_FPGA_REPO_DIR is set to the location of this script.
if [[ ":$AWS_FPGA_REPO_DIR" == ':' ]]; then
  debug_msg "AWS_FPGA_REPO_DIR not set so setting to $script_dir"
  export AWS_FPGA_REPO_DIR=$script_dir
elif [[ $AWS_FPGA_REPO_DIR != $script_dir ]]; then
  info_msg "Changing AWS_FPGA_REPO_DIR from $AWS_FPGA_REPO_DIR to $script_dir"
  export AWS_FPGA_REPO_DIR=$script_dir
else
  debug_msg "AWS_FPGA_REPO_DIR=$AWS_FPGA_REPO_DIR"
fi

info_msg "Setting up environment variables"
module unload
module load sdx
export XILINX_SDX=/opt/Xilinx/SDx/2017.1.op
export PATH=$XILINX_SDX/runtime/bin/:$PATH
export LD_LIBRARY_PATH=$XILINX_SDX/runtime/lib/x86_64/:$LD_LIBRARY_PATH
source $XILINX_SDX/settings64.sh
if grep -q 'libxilinxopencl.so' /etc/OpenCL/vendors/xilinx.icd; then
  echo "Found 'libxilinxopencl.so"
else
  err_msg "Failed find to libxilinxopencl.so in /etc/OpenCL/vendors/xilinx.icd"
  cat /etc/OpenCL/vendors/xilinx.icd
  return 2
fi
echo "Done setting environment variables."


# Download correct DSA
#TODO DSA Version:  info_msg "Using HDK shell version $hdk_shell_version"
#TODO DSA Version:  debug_msg "Checking HDK shell's checkpoint version"
dsa_dir=$SDK_DIR/SDAccel/aws_platform/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0/hw/
sdk_dsa=$dsa_dir/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0.dsa
sdk_dsa_s3_bucket=aws-fpga-hdk-resources
s3_sdk_dsa=$sdk_dsa_s3_bucket/sdk/SDAccel/dsa_v04151701_0524/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0.dsa
# Download the sha256
if [ ! -e $dsa_dir ]; then
	mkdir -p $dsa_dir || { err_msg "Failed to create $dsa_dir"; return 2; }
fi
# Use curl instead of AWS CLI so that credentials aren't required.
curl -s https://s3.amazonaws.com/$s3_sdk_dsa.sha256 -o $sdk_dsa.sha256 || { err_msg "Failed to download DSA checkpoint version from $s3_sdk_dsa.sha256 -o $sdk_dsa.sha256"; return 2; }
if grep -q '<?xml version' $sdk_dsa.sha256; then
  err_msg "Failed to downlonad SDK DSA checkpoint version from $s3_sdk_dsa.sha256"
  cat sdk_dsa.sha256
  return 2
fi
exp_sha256=$(cat $sdk_dsa.sha256)
debug_msg "  latest   version=$exp_sha256"
# If DSA already downloaded check its sha256
if [ -e $sdk_dsa ]; then
  act_sha256=$( sha256sum $sdk_dsa | awk '{ print $1 }' )
  debug_msg "  existing version=$act_sha256"
  if [[ $act_sha256 != $exp_sha256 ]]; then
    info_msg "SDK dsa checkpoint version is incorrect"
    info_msg "  Saving old checkpoint to $sdk_dsa.back"
    mv $sdk_dsa $sdk_dsa.back
  fi
else
  info_msg "HDK dsa checkpoint hasn't been downloaded yet."
fi
if [ ! -e $sdk_dsa ]; then
  info_msg "Downloading latest SDK dsa checkpoint from $s3_sdk_dsa"
  # Use curl instead of AWS CLI so that credentials aren't required.
  curl -s https://s3.amazonaws.com/$s3_sdk_dsa -o $sdk_dsa || { err_msg "SDK dsa checkpoint download failed"; return 2; }
fi
# Check sha256
act_sha256=$( sha256sum $sdk_dsa | awk '{ print $1 }' )
if [[ $act_sha256 != $exp_sha256 ]]; then
  err_msg "Incorrect SDK dsa checkpoint version:"
  err_msg "  expected version=$exp_sha256"
  err_msg "  actual   version=$act_sha256"
  err_msg "  There may be an issue with the uploaded checkpoint or the download failed."
  return 2
fi
info_msg "SDK DSA is up-to-date"

