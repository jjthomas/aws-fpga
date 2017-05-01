EXENAME := awssak

CXX_EXT := cpp
CL_EXT := cl

AR := ar
CXX := g++

CXXFLAGS := -Wall -Werror -std=c++11

ROOT = $(SDK_DIR)/SDAccel
HAL_INC := -I../../include -I$(ROOT)/userspace/include

CXXFLAGS += $(HAL_INC)

ifeq ($(ec2),1)
AWSBM_HAL_LIBNAME := $(ROOT)/userspace/src/libawsxcldrv.a
else
AWSBM_HAL_LIBNAME := $(ROOT)/userspace/src/libawsbmdrv.a
endif

ifeq ($(debug),1)
	CXXFLAGS += -g -D_DEBUG
else
	CXXFLAGS += -O2 -DNDEBUG
endif

SRCS := $(wildcard *.$(CXX_EXT))
OBJS := $(patsubst %.$(CXX_EXT), %.o, $(SRCS))

-include $(OBJS:.o=.d)

LDFLAGS :=

#$(error $(ODIR))
#$(error $(DIR))
#$(error $(ROOT))

all : $(EXENAME)

%.o: %.$(CXX_EXT)
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) $(MYCXXFLAGS) -c $< -o $@
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) $(MYCXXFLAGS) -c -MM $< -o $(patsubst %.o, %.d, $@)

$(EXENAME): $(OBJS) $(AWSBM_HAL_LIBNAME)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS) $(AWSBM_HAL_LIBNAME)

clean:
	rm -rf *.o *.d $(EXENAME)

.PHONY: all

.DEFAULT_GOAL := all
