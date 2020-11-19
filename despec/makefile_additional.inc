# -*- Makefile -*-

USE_LMD_INPUT=1

OBJS += despec_user.o ucesb.pb.o

ucesb.pb.h ucesb.pb.cc: $(UCESB_BASE_DIR)/despec/ucesb.proto
	protoc --cpp_out=$(UCESB_BASE_DIR)/despec ucesb.proto
	
CXXFLAGS += -DCONTROL_INCLUDE -DZEROMQ $(shell pkg-config --cflags libzmq libzmqpp protobuf) -Wno-conversion -Wno-float-conversion
CXXLIBS += $(shell pkg-config --libs libzmq libzmqpp protobuf)
CXXLIBS += -Wl,-rpath,'$$ORIGIN'

