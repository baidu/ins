# OPT ?= -O2 -DNDEBUG     # (A) Production use (optimized mode)
OPT ?= -g2 -Wall -fPIC         # (B) Debug mode, w/ full line-level debugging symbols
# OPT ?= -O2 -g2 -DNDEBUG # (C) Profiling mode: opt, but w/debugging symbols

# Thirdparty
SNAPPY_PATH=./thirdparty/snappy/
PROTOBUF_PATH=./thirdparty/protobuf/
PROTOC_PATH=./thirdparty/protobuf/bin/
PROTOC=protoc
PBRPC_PATH ?=./thirdparty/sofa-pbrpc/output/
BOOST_PATH=./thirdparty/boost/
GFLAGS_PATH=./thirdparty/gflags/
LEVELDB_PATH=./thirdparty/leveldb/
PREFIX=/usr/local/

INCLUDE_PATH = -I./ -I$(PREFIX)/include -I$(PROTOBUF_PATH)/include \
               -I$(PBRPC_PATH)/include \
               -I$(SNAPPY_PATH)/include \
	       -I$(GFLAGS_PATH)/include \
	       -I$(LEVELDB_PATH)/include \
               -I$(BOOST_PATH)/include

LDFLAGS = -L$(PREFIX)/lib -L$(PROTOBUF_PATH)/lib \
          -L$(PBRPC_PATH)/lib -lsofa-pbrpc -lprotobuf \
          -L$(SNAPPY_PATH)/lib -lsnappy \
          -L$(GFLAGS_PATH)/lib -lgflags \
          -L$(LEVELDB_PATH)/lib -lleveldb \
          -lz -lleveldb -lpthread -luuid

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

SAMPLE_SRC = sdk/sample.cc
SAMPLE_OBJ = $(patsubst %.cc, %.o, $(SAMPLE_SRC))
SAMPLE_HEADER = $(wildcard sdk/*.h)

FLAGS_OBJ = $(patsubst %.cc, %.o, $(wildcard server/flags.cc))
COMMON_OBJ = $(patsubst %.cc, %.o, $(wildcard common/*.cc))
OBJS = $(FLAGS_OBJ) $(COMMON_OBJ) $(PROTO_OBJ)
SDK_OBJ = $(OBJS) $(patsubst %.cc, %.o, sdk/ins_sdk.cc)
BIN = ins ins_cli sample
LIB = libins_sdk.a

all: $(BIN) cp $(LIB)

# Depends
$(INS_OBJ) $(INS_CLI_OBJ): $(PROTO_HEADER)
$(INS_OBJ): $(INS_HEADER)
$(INS_CLI_OBJ): $(INS_CLI_HEADER)
$(SAMPLE_OBJ): $(SAMPLE_HEADER)

# Targets
ins: $(INS_OBJ) $(OBJS)
	$(CXX) $(INS_OBJ) $(OBJS) -o $@ $(LDFLAGS)

ins_cli: $(INS_CLI_OBJ) $(OBJS)
	$(CXX) $(INS_CLI_OBJ) $(OBJS) -o $@ $(LDFLAGS)

sample: $(SAMPLE_OBJ) $(SDK_OBJ) $(LIB)
	$(CXX) $(SAMPLE_OBJ) $(LIB) -o $@ $(LDFLAGS)

$(LIB): $(SDK_OBJ)
	ar -rs $@ $(SDK_OBJ)

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) -c $< -o $@

%.pb.h %.pb.cc: %.proto
	$(PROTOC) --proto_path=./proto/ --proto_path=/usr/local/include --cpp_out=./proto/ $<

clean:
	rm -rf $(BIN)
	rm -rf $(INS_OBJ) $(INS_CLI_OBJ) $(OBJS)
	rm -rf $(PROTO_SRC) $(PROTO_HEADER)

cp: $(BIN) $(LIB)
	mkdir -p output/bin
	mkdir -p output/lib
	mkdir -p output/include
	cp ins output/bin
	cp ins_cli output/bin
	cp sample output/bin
	cp sdk/ins_sdk.h output/include
	cp libins_sdk.a output/lib

sdk: $(LIB)
	mkdir -p output/include
	mkdir -p output/lib
	cp sdk/ins_sdk.h output/include
	cp libins_sdk.a output/lib
	cp sdk/ins_sdk.h $(PREFIX)/include
	cp libins_sdk.a $(PREFIX)/lib

install: $(LIB)
	cp sdk/ins_sdk.h $(PREFIX)/include
	cp $(LIB) $(PREFIX)/lib

.PHONY: test
test:
	echo "Test done"

