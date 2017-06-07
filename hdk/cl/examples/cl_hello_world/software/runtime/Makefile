# Amazon FPGA Hardware Development Kit
#
# Copyright 2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Licensed under the Amazon Software License (the "License"). You may not use
# this file except in compliance with the License. A copy of the License is
# located at
#
#    http://aws.amazon.com/asl/
#
# or in the "license" file accompanying this file. This file is distributed on
# an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, express or
# implied. See the License for the specific language governing permissions and
# limitations under the License.


INCLUDES = -I$(SDK_DIR)/userspace/include

CC = gcc
CFLAGS = -DCONFIG_LOGLEVEL=4 -std=gnu99 -g -Wall -Werror $(INCLUDES)

LDLIBS = -lfpga_mgmt -lrt -lpthread

SRC1 = test_hello_world.c
OBJ1 = $(SRC1:.c=.o)
BIN1 = test_hello_world

SRC2 = test_hello_world_pcie.c
OBJ2 = $(SRC2:.c=.o)
BIN2 = test_hello_world_pcie

SRC3 = sa-mm-fpga.c
OBJ3 = $(SRC3:.c=.o)
BIN3 = sa-mm-fpga 

all: check_env $(BIN1) $(BIN2)

$(BIN1): $(OBJ1)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(BIN2): $(OBJ2)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(BIN3): $(OBJ3)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f *.o $(BIN1) $(BIN2) $(BIN3)

check_env:
ifndef SDK_DIR
    $(error SDK_DIR is undefined. Try "source sdk_setup.sh" to set the software environment)
endif
