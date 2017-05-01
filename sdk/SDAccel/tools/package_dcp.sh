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
#if [ $script == $0 ]; then
#  echo "ERROR: You must source this script"
#  exit 2
#fi
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
  echo -e "USAGE: $script_name [-d|-debug] [-h|-help] [-k|-kernal <kernal_name>]"
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
    -k|-kernel)
      kernel=${args[$i + 1]}
      echo $kernel
      break
    ;;
    *)
      err_msg "Invalid option: $arg\n"
      usage
      exit 1
  esac
done

if [[ -x "$kernel" ]]
then
    echo "File '$kernel' is executable"
else
    echo "File '$kernel' is not executable or found"
    exit 1
fi

timestamp=$(date +"%y_%m_%d-%H%M%S")

mkdir to_aws

#split xcp file
$XILINX_SDX/runtime/bin/xclbinsplit -o ${kernel} xclbin/krnl_${kernel}.hw.xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3.xclbin

#OLD LOCATION of XCP file
#$XILINX_SDX/runtime/bin/xclbinsplit -o ${kernel} _xocc_link_krnl_${kernel}.hw.xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3_krnl_${kernel}.hw.xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3.dir/impl/build/system/krnl_${kernel}.hw.xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3/bitstream/krnl_${kernel}.hw.xilinx_minotaur-vu9p-f1_4ddr-xpr_3_3.xcp

#rename .bit to .dcp
mv ${kernel}-primary.bit to_aws/${kernel}_${timestamp}_SH_CL_routed.dcp

#generate manifest
strategy=DEFAULT
clock_recipe_a=A0
clock_recipe_b=B0
clock_recipe_c=C0

# Check that strategy is valid
if [[ $strategy != @(BASIC|DEFAULT|EXPLORE|TIMING|CONGESTION) ]] 
then
  echo "ERROR: $strategy isn't a valid strategy. Valid strategies are BASIC, DEFAULT, EXPLORE, TIMING and CONGESTION." 
  exit 1
fi

# Check that clock_recipe_a is valid
if [[ $clock_recipe_a != @(A0|A1|A2) ]] 
then
  echo "ERROR: $clock_recipe_a isn't a valid Clock Group A recipe. Valid Clock Group A recipes are A0, A1, and A2." 
  exit 1
fi

# Check that clock_recipe_b is valid
if [[ $clock_recipe_b != @(B0|B1) ]] 
then
  echo "ERROR: $clock_recipe_b isn't a valid Clock Group B recipe. Valid Clock Group B recipes are B0 and B1." 
  exit 1
fi

# Check that clock_recipe_c is valid
if [[ $clock_recipe_c != @(C0|C1) ]] 
then
  echo "ERROR: $clock_recipe_c isn't a valid Clock Group C recipe. Valid Clock Group C recipes are C0 and C1." 
  exit 1
fi

# Get the HDK Version
hdk_version=1.2.0
#hdk_version=$(grep 'HDK_VERSION' $HDK_DIR/hdk_version.txt | sed 's/=/ /g' | awk '{print $2}')

# Get the Shell Version
shell_version=0x04151701
#shell_version=$(grep 'SHELL_VERSION' $HDK_SHELL_DIR/shell_version.txt | sed 's/=/ /g' | awk '{print $2}')

# Get the PCIe Device & Vendor ID from ID0
#id0_version=
device_id=0xF000
vendor_id=0x1D0F
#id0_version=$(grep 'CL_SH_ID0' $CL_DIR/design/cl_id_defines.vh | grep 'define' | sed 's/_//g' | awk -F "h" '{print $2}')
#device_id="0x${id0_version:0:4}";
#vendor_id="0x${id0_version:4:4}";

# Get the PCIe Subsystem & Subsystem Vendor ID from ID1
#id1_version=$(grep 'CL_SH_ID1' $CL_DIR/design/cl_id_defines.vh | grep 'define' | sed 's/_//g' | awk -F "h" '{print $2}')
#subsystem_id="0x${id1_version:0:4}";
#subsystem_vendor_id="0x${id1_version:4:4}";
#id1_version=FIXME
subsystem_id=0x1D51
subsystem_vendor_id=0xFEDD

#convert to bash
hash=$( sha256sum  to_aws/${kernel}_${timestamp}_SH_CL_routed.dcp | awk '{ print $1 }' )
FILENAME="to_aws/${kernel}_${timestamp}_manifest.txt"
exec 3<>$FILENAME
echo "manifest_format_version=1" >&3
echo "pci_vendor_id=$vendor_id" >&3
echo "pci_device_id=$device_id" >&3
echo "pci_subsystem_id=$subsystem_id" >&3
echo "pci_subsystem_vendor_id=$subsystem_vendor_id" >&3
echo "dcp_hash=$hash" >&3
echo "shell_version=$shell_version" >&3
echo "dcp_file_name=${kernel}_${timestamp}_SH_CL_routed.dcp" >&3
echo "hdk_version=$hdk_version" >&3
echo "date=$timestamp" >&3
echo "clock_recipe_a=$clock_recipe_a" >&3
echo "clock_recipe_b=$clock_recipe_b" >&3
echo "clock_recipe_c=$clock_recipe_c" >&3
exec 3>&-

#tar .dcp and manifest
tar -cf ${kernel}_${timestamp}_Developer_CL.tar to_aws/${kernel}_${timestamp}_SH_CL_routed.dcp to_aws/${kernel}_${timestamp}_manifest.txt

#Create .awsxclbin
cat ${kernel}-meta.xml >> ${kernel}_${timestamp}.awsxclbin
echo ${kernel}_afi_id.txt >> ${kernel}_${timestamp}.awsxclbin
  
