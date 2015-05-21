# OPT ?= -O2 -DNDEBUG     # (A) Production use (optimized mode)
OPT ?= -g2 -Wall          # (B) Debug mode, w/ full line-level debugging symbols
# OPT ?= -O2 -g2 -DNDEBUG # (C) Profiling mode: opt, but w/debugging symbols

# Thirdparty
SNAPPY_PATH=./thirdparty/snappy/
PROTOBUF_PATH=./thirdparty/protobuf/
PROTOC_PATH=
PROTOC=$(PROTOC_PATH)protoc
PBRPC_PATH=./thirdparty/sofa-pbrpc/output/
BOOST_PATH=../boost/

INCLUDE_PATH = -I./ -I$(PROTOBUF_PATH)/include \
               -I$(PBRPC_PATH)/include \
               -I$(SNAPPY_PATH)/include \
               -I$(BOOST_PATH)/include

LDFLAGS = -L$(PROTOBUF_PATH)/lib -lprotobuf \
          -L$(PBRPC_PATH)/lib -lsofa-pbrpc \
          -L$(SNAPPY_PATH)/lib -lsnappy \
          -lpthread -lz -lboost_filesystem -lleveldb -lgflags

CXXFLAGS += $(OPT)

PROTO_FILE = $(wildcard proto/*.proto)
PROTO_SRC = $(patsubst %.proto,%.pb.cc,$(PROTO_FILE))
PROTO_HEADER = $(patsubst %.proto,%.pb.h,$(PROTO_FILE))
PROTO_OBJ = $(patsubst %.proto,%.pb.o,$(PROTO_FILE))

INS_SRC = $(wildcard server/ins_*.cc) storage/binlog.cc storage/meta.cc
INS_OBJ = $(patsubst %.cc, %.o, $(INS_SRC))
INS_HEADER = $(wildcard server/*.h)

INS_CLI_SRC = $(wildcard sdk/ins_*.cc)
INS_CLI_OBJ = $(patsubst %.cc, %.o, $(INS_CLI_SRC))
INS_CLI_HEADER = $(wildcard sdk/*.h)

FLAGS_OBJ = $(patsubst %.cc, %.o, $(wildcard server/flags.cc))
COMMON_OBJ = $(patsubst %.cc, %.o, $(wildcard common/*.cc))
OBJS = $(FLAGS_OBJ) $(COMMON_OBJ) $(PROTO_OBJ)

BIN = ins ins_cli

all: $(BIN)

# Depends
$(INS_OBJ) $(INS_CLI_OBJ): $(PROTO_HEADER)
$(INS_OBJ): $(INS_HEADER)
$(INS_CLI_OBJ): $(INS_CLI_HEADER)

# Targets
ins: $(INS_OBJ) $(OBJS)
	$(CXX) $(INS_OBJ) $(OBJS) -o $@ $(LDFLAGS)

ins_cli: $(INS_CLI_OBJ) $(OBJS)
	$(CXX) $(INS_CLI_OBJ) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) -c $< -o $@

%.pb.h %.pb.cc: %.proto
	$(PROTOC) --proto_path=./proto/ --proto_path=/usr/local/include --cpp_out=./proto/ $<

clean:
	rm -rf $(BIN)
	rm -rf $(INS_OBJ) $(INS_CLI_OBJ) $(OBJS)
	rm -rf $(PROTO_SRC) $(PROTO_HEADER)

.PHONY: test
test:
	echo "Test done"
