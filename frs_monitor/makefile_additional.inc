# -*- Makefile -*-

# ------- OPTIONS -------------
# Enable ZeroMQ (Remote monitoring) code - requires extra libraries (not default at GSI)
# libzmq libzmqpp (ZeroMQ) and Google Protobuf
USE_ZEROMQ=y

# -------- END OF OPTIONS -------

USE_LMD_INPUT=1

OBJS += frs_monitor_user.o

CXXFLAGS += -DCONTROL_INCLUDE -Wno-conversion -Wno-float-conversion
CXXFLAGS += -DFRS_EXPERIMENT_H="\"frs_experiment_s160.h\""
#CXXFLAGS = -DZMQ_PORT="\"5000\""

ifeq ($(USE_ZEROMQ),y)
OBJS += ucesb.pb.o

#PROTOC=/u/nhubbard/Build/protobuf-3.13.0/deb10/bin/protoc
PROTOC=protoc
#export PKG_CONFIG_PATH:=/u/nhubbard/Build/protobuf-3.13.0/deb10/lib/pkgconfig:$(PKG_CONFIG_PATH)

ucesb.pb.h ucesb.pb.cc: $(UCESB_BASE_DIR)/frs_monitor/ucesb.proto
	$(PROTOC) --cpp_out=$(UCESB_BASE_DIR)/frs_monitor ucesb.proto
CXXFLAGS += -DZEROMQ $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags libzmq libzmqpp protobuf)
CXXLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs libzmq libzmqpp protobuf)
CXXLIBS += -Wl,-rpath,'$$ORIGIN'
endif
