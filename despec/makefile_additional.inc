# -*- Makefile -*-

# ------- OPTIONS -------------
# Enable ZeroMQ (Remote monitoring) code - requires extra libraries (not default at GSI)
# libzmq libzmqpp (ZeroMQ) and Google Protobuf
USE_ZEROMQ=y
# Enable bPlast broken f_user correction - only use if f_user was broken!
#BPLAST_FIX=y
# Enable FATIMA TAMEX broken f_user correction - only use if f_user was broken!
#FATIMA_FIX=y

# -------- END OF OPTIONS -------

USE_LMD_INPUT=1

OBJS += despec_user.o

CXXFLAGS += -DCONTROL_INCLUDE -Wno-conversion -Wno-float-conversion
CXXFLAGS += -DDESPEC_EXPERIMENT_H="\"despec_experiment_s100.h\""
#CXXFLAGS = -DZMQ_PORT="\"5000\""

ifeq ($(BPLAST_FIX),y)
CXXFLAGS += -DBPLAST_DELAY_FIX=1
endif

ifeq ($(FATIMA_FIX),y)
CXXFLAGS += -DFATIMA_DELAY_FIX=1
endif

ifeq ($(USE_ZEROMQ),y)
OBJS += ucesb.pb.o

PROTOC=/u/nhubbard/Build/protobuf-3.13.0/deb10/bin/protoc
export PKG_CONFIG_PATH:=/u/nhubbard/Build/protobuf-3.13.0/deb10/lib/pkgconfig:$(PKG_CONFIG_PATH)

ucesb.pb.h ucesb.pb.cc: $(UCESB_BASE_DIR)/despec/ucesb.proto
	$(PROTOC) --cpp_out=$(UCESB_BASE_DIR)/despec ucesb.proto
CXXFLAGS += -DZEROMQ $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags libzmq libzmqpp protobuf)
CXXLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs libzmq libzmqpp protobuf)
CXXLIBS += -Wl,-rpath,'$$ORIGIN'
endif
