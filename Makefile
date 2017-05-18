# modify prefix to specify the installation target
PREFIX=./output

# switch OPT for different purpose
# OPT ?= -O2 -DNDEBUG     # (A) Production use (optimized mode)
OPT ?= -g2 -Wall          # (B) Debug mode, w/ full line-level debugging symbols
# OPT ?= -O2 -g2 -DNDEBUG # (C) Profiling mode: opt, but w/debugging symbols

# Thirdparty
PROTOC ?= ./depends/bin/protoc
SNAPPY_PATH ?= ./depends
BOOST_PATH ?=./depends/boost_1_57_0/boost
GFLAGS_PATH ?= ./depends
PROTOBUF_PATH ?= ./depends
PBRPC_PATH ?=./depends
GTEST_PATH ?= ./depends
NEXUS_LDB_PATH = ./thirdparty/leveldb/

# Compiler related
INCPATH = -I./ -I./src -I$(NEXUS_LDB_PATH)/include \
          -I$(SNAPPY_PATH)/include \
          -I$(BOOST_PATH) \
          -I$(GFLAGS_PATH)/include \
          -I$(PROTOBUF_PATH)/include \
          -I$(PBRPC_PATH)/include

LDFLAGS += -L$(PROTOBUF_PATH)/lib -L$(PBRPC_PATH)/lib -lsofa-pbrpc -lprotobuf \
           -L$(GFLAGS_PATH)/lib -lgflags \
           -L$(SNAPPY_PATH)/lib -lsnappy \
           -lrt -lz -lpthread

LDFLAGS_SO = -L$(PROTOBUF_PATH)/lib -L$(PBRPC_PATH)/lib \
             -Wl,--whole-archive -lsofa-pbrpc -lprotobuf -Wl,--no-whole-archive \
             -L$(GFLAGS_PATH)/lib -Wl,--whole-archive -lgflags -Wl,--no-whole-archive \
             -L$(SNAPPY_PATH)/lib -lsnappy \
             -lz -lpthread

NEXUS_LDB_FLAGS = -L$(NEXUS_LDB_PATH)/lib -lleveldb

TESTFLAGS = -L$(GTEST_PATH)/lib -lgtest

CXXFLAGS += $(OPT) -pipe -MMD -W -Wall -fPIC -DHAVE_SNAPPY

# Source releated constants
PROTO_FILE = $(wildcard src/proto/*.proto)
PROTO_SRC = $(patsubst %.proto, %.pb.cc, $(PROTO_FILE))
PROTO_HEADER = $(patsubst %.proto, %.pb.h, $(PROTO_FILE))
PROTO_OBJ = $(patsubst %.cc, %.o, $(PROTO_SRC))

COMMON_OBJ = $(patsubst %.cc, %.o, $(wildcard src/common/*.cc))

NEXUS_NODE_SRC = $(wildcard src/server/*.cc) $(wildcard src/storage/*.cc)
NEXUS_NODE_OBJ = $(patsubst %.cc, %.o, $(NEXUS_NODE_SRC))

INS_CLI_OBJ = $(patsubst %.cc, %.o, src/client/ins_cli.cc)

SAMPLE_OBJ = $(patsubst %.cc, %.o, src/client/sample.cc)

CXX_SDK_SRC = src/sdk/ins_sdk.cc src/server/flags.cc
CXX_SDK_OBJ = $(patsubst %.cc, %.o, $(CXX_SDK_SRC))

PYTHON_SDK_SRC = src/sdk/ins_wrapper.cc
PYTHON_SDK_OBJ = $(patsubst %.cc, %.o, $(PYTHON_SDK_SRC))
PYTHON_SDK_PY = src/sdk/ins_sdk.py

TEST_BINLOG_SRC = src/test/binlog_test.cc src/storage/binlog.cc
TEST_BINLOG_OBJ = $(patsubst %.cc, %.o, $(TEST_BINLOG_SRC))

TEST_PERFORMANCE_CENTER_SRC = src/test/performance_center_test.cc src/server/performance_center.cc
TEST_PERFORMANCE_CENTER_OBJ = $(patsubst %.cc, %.o, $(TEST_PERFORMANCE_CENTER_SRC))

TEST_STORAGE_MANAGER_SRC = src/test/storage_manager_test.cc src/storage/storage_manage.cc
TEST_STORAGE_MANAGER_OBJ = $(patsubst %.cc, %.o, $(TEST_STORAGE_MANAGER_SRC))

TEST_USER_MANAGER_SRC = src/test/user_manage_test.cc src/server/user_manage.cc
TEST_USER_MANAGER_OBJ = $(patsubst %.cc, %.o, $(TEST_USER_MANAGER_SRC))

OBJS = $(PROTO_OBJ) $(COMMON_OBJ) $(NEXUS_NODE_OBJ) $(INS_CLI_OBJ) $(SAMPLE_OBJ) \
       $(CXX_SDK_OBJ) $(PYTHON_SDK_OBJ) $(TEST_BINLOG_OBJ) $(TEST_PERFORMANCE_OBJ) \
       $(TEST_STORAGE_MANAGER_OBJ) $(TEST_USER_MANAGER_OBJ)
DEPS = $(patsubst %.o, %.d, $(OBJS))
TESTS = test_binlog test_performance_center test_storage_manager test_user_manager
BIN = nexus ins_cli sample
LIB = libins_sdk.a
PYTHON_LIB = libins_py.so

# Build binaries and libs by default
default: $(BIN) $(LIB) $(PYTHON_LIB)
	mkdir -p output/bin
	mkdir -p output/lib
	cp -f $(BIN) ./output/bin
	cp -f $(LIB) $(PYTHON_LIB) $(PYTHON_LIB_PY) ./output/lib
	@echo 'make done'

# Dependencies
$(OBJS): $(PROTO_HEADER)
-include $(DEPS)

# Building targets
%.pb.h %.pb.cc: %.proto
	$(PROTOC) --proto_path=./proto/ --proto_path=/usr/local/include --cpp_out=./proto/ $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) -c $< -o $@

nexus: $(NEXUS_NODE_OBJ) $(COMMON_OBJ) $(PROTO_OBJ) nexus_ldb
	$(CXX) $(NEXUS_NODE_OBJ) $(COMMON_OBJ) $(PROTO_OBJ) -o $@ $(LDFLAGS) $(NEXUS_LDB_FLAGS)

ins_cli: $(INS_CLI_OBJ) libins_sdk.a
	$(CXX) $(INS_CLI_OBJ) -o $@ -L. -lins_sdk $(LDFLAGS)

sample: $(SAMPLE_OBJ) libins_sdk.a
	$(CXX) $(SAMPLE_OBJ) -o $@ -L. -lins_sdk $(LDFLAGS)

libins_sdk.a: $(CXX_SDK_OBJ) $(COMMON_OBJ) $(PROTO_OBJ)
	ar -rs $@ $^

libins_py.so: $(PYTHON_SDK_OBJ) $(CXX_SDK_OBJ) $(COMMON_OBJ) $(PROTO_OBJ)
	$(CXX) -shared -fPIC -Wl,-soname,$@ -o $@ $(LDFLAGS_SO) $^

test_binlog: $(TEST_BINLOG_OBJ) $(COMMON_OBJ) $(PROTO_OBJ)
	$(CXX) $^ -o $@ $(LDFLAGS) $(TESTFLAGS)

test_performance_center: $(TEST_PERFORMANCE_CENTER_OBJ) $(COMMON_OBJ)
	$(CXX) $^ -o $@ $(LDFLAGS) $(TESTFLAGS)

test_storage_manager: $(TEST_STORAGE_MANAGER_OBJ) $(COMMON_OBJ) $(PROTO_OBJ)
	$(CXX) $^ -o $@ $(LDFLAGS) $(TESTFLAGS)

test_user_manager: $(TEST_USER_MANAGER_OBJ) $(COMMON_OBJ) $(PROTO_OBJ)
	$(CXX) $^ -o $@ $(LDFLAGS) $(TESTFLAGS)

# Phony targets
.PHONY: nexus_ldb all test sdk python install install_sdk uninstall clean
nexus_ldb: 
	make -C ./thirdparty/leveldb

all: $(BIN) $(LIB) $(PYTHON_LIB) $(TESTS)
	mkdir -p output/bin
	mkdir -p output/lib
	mkdir -p output/test
	cp -f $(BIN) ./output/bin
	cp -f $(LIB) $(PYTHON_LIB) ./output/lib
	cp -f $(TESTS) ./output/test
	@echo 'make all done'

test: $(TESTS)
	./test_binlog
	./test_performance_center
	./test_storage_manager
	./test_user_manager
	@echo 'all tests done'

sdk: $(LIB) $(PYTHON_LIB)
	mkdir -p output/lib
	cp -f $^ $(PYTHON_SDK_PY) ./output/lib
	@echo 'make sdk done'

python: $(PYTHON_LIB)
	mkdir -p output/lib
	cp -f $^ $(PYTHON_SDK_PY) ./output/lib
	@echo 'make python sdk done'

install: $(BIN) install_sdk
	mkdir -p $(PREFIX)/bin
	cp -f $(BIN) $(PREFIX)/bin
	@echo 'make install done'

install_sdk: $(LIB) $(PYTHON_LIB)
	mkdir -p $(PREFIX)/lib
	cp -f $(LIB) $(PYTHON_LIB) $(PYTHON_SDK_PY) $(PREFIX)/lib
	@echo 'make install sdk done'

uninstall:
	rm -f $(addprefix $(PREFIX)/bin/, $(BIN))
	rm -f $(addprefix $(PREFIX)/lib/, $(LIB) $(PYTHON_LIB))
	@echo 'make uninstall done'

clean:
	rm -rf $(BIN) $(LIB) $(PYTHON_LIB) $(TESTS) $(OBJS) $(DEPS)
	rm -rf $(PROTO_SRC) $(PROTO_HEADER)
	rm -rf output/
	@echo 'make clean done'

