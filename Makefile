#COMAKE2 edit-mode: -*- Makefile -*-
####################64Bit Mode####################
ifeq ($(shell uname -m),x86_64)
CC=gcc
CXX=g++
CXXFLAGS=-g \
  -pipe \
  -W \
  -Wall \
  -fPIC
CFLAGS=-g \
  -pipe \
  -W \
  -Wall \
  -fPIC
CPPFLAGS=-D_GNU_SOURCE \
  -D__STDC_LIMIT_MACROS \
  -DVERSION=\"0.0.0.1\"
INCPATH=-I. \
  -I./src \
  -I./output/include
DEP_INCPATH=-I../../../public/sofa-pbrpc \
  -I../../../public/sofa-pbrpc/include \
  -I../../../public/sofa-pbrpc/output \
  -I../../../public/sofa-pbrpc/output/include \
  -I../../../third-64/boost \
  -I../../../third-64/boost/include \
  -I../../../third-64/boost/output \
  -I../../../third-64/boost/output/include \
  -I../../../third-64/gflags \
  -I../../../third-64/gflags/include \
  -I../../../third-64/gflags/output \
  -I../../../third-64/gflags/output/include \
  -I../../../third-64/gtest \
  -I../../../third-64/gtest/include \
  -I../../../third-64/gtest/output \
  -I../../../third-64/gtest/output/include \
  -I../../../third-64/protobuf \
  -I../../../third-64/protobuf/include \
  -I../../../third-64/protobuf/output \
  -I../../../third-64/protobuf/output/include \
  -I../../../third-64/snappy \
  -I../../../third-64/snappy/include \
  -I../../../third-64/snappy/output \
  -I../../../third-64/snappy/output/include

#============ CCP vars ============
CCHECK=@ccheck.py
CCHECK_FLAGS=
PCLINT=@pclint
PCLINT_FLAGS=
CCP=@ccp.py
CCP_FLAGS=


#COMAKE UUID
COMAKE_MD5=669ea953df45924325145f40a5db7993  COMAKE


.PHONY:all
all:comake2_makefile_check ins ins_cli libins_sdk.a binlog_test 
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mall[0m']"
	@echo "make all done"

.PHONY:comake2_makefile_check
comake2_makefile_check:
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mcomake2_makefile_check[0m']"
	#in case of error, update 'Makefile' by 'comake2'
	@echo "$(COMAKE_MD5)">comake2.md5
	@md5sum -c --status comake2.md5
	@rm -f comake2.md5

.PHONY:ccpclean
ccpclean:
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mccpclean[0m']"
	@echo "make ccpclean done"

.PHONY:clean
clean:ccpclean
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mclean[0m']"
	rm -rf ins
	rm -rf ./output/bin/ins
	rm -rf ins_cli
	rm -rf ./output/bin/ins_cli
	rm -rf libins_sdk.a
	rm -rf ./output/lib/libins_sdk.a
	rm -rf binlog_test
	rm -rf ./output/bin/binlog_test
	rm -rf server/ins_ins_main.o
	rm -rf server/ins_ins_node_impl.o
	rm -rf server/ins_flags.o
	rm -rf storage/ins_meta.o
	rm -rf common/ins_logging.o
	rm -rf storage/ins_binlog.o
	rm -rf proto/ins_node.pb.cc
	rm -rf proto/ins_node.pb.h
	rm -rf proto/ins_ins_node.pb.o
	rm -rf sdk/ins_cli_ins_sdk.o
	rm -rf proto/ins_node.pb.cc
	rm -rf proto/ins_node.pb.h
	rm -rf proto/ins_cli_ins_node.pb.o
	rm -rf common/ins_cli_logging.o
	rm -rf sdk/ins_cli_ins_cli.o
	rm -rf server/ins_cli_flags.o
	rm -rf sdk/ins_sdk_ins_sdk.o
	rm -rf common/ins_sdk_logging.o
	rm -rf proto/ins_node.pb.cc
	rm -rf proto/ins_node.pb.h
	rm -rf proto/ins_sdk_ins_node.pb.o
	rm -rf storage/binlog_test_binlog.o
	rm -rf storage/binlog_test_binlog_test.o
	rm -rf common/binlog_test_logging.o

.PHONY:dist
dist:
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mdist[0m']"
	tar czvf output.tar.gz output
	@echo "make dist done"

.PHONY:distclean
distclean:clean
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mdistclean[0m']"
	rm -f output.tar.gz
	@echo "make distclean done"

.PHONY:love
love:
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mlove[0m']"
	@echo "make love done"

ins:server/ins_ins_main.o \
  server/ins_ins_node_impl.o \
  server/ins_flags.o \
  storage/ins_meta.o \
  common/ins_logging.o \
  storage/ins_binlog.o \
  proto/ins_ins_node.pb.o
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mins[0m']"
	$(CXX) server/ins_ins_main.o \
  server/ins_ins_node_impl.o \
  server/ins_flags.o \
  storage/ins_meta.o \
  common/ins_logging.o \
  storage/ins_binlog.o \
  proto/ins_ins_node.pb.o -Xlinker "-("  ../../../public/sofa-pbrpc/libsofa-pbrpc.a \
  ../../../third-64/boost/lib/libboost_atomic.a \
  ../../../third-64/boost/lib/libboost_chrono.a \
  ../../../third-64/boost/lib/libboost_container.a \
  ../../../third-64/boost/lib/libboost_context.a \
  ../../../third-64/boost/lib/libboost_coroutine.a \
  ../../../third-64/boost/lib/libboost_date_time.a \
  ../../../third-64/boost/lib/libboost_exception.a \
  ../../../third-64/boost/lib/libboost_filesystem.a \
  ../../../third-64/boost/lib/libboost_graph.a \
  ../../../third-64/boost/lib/libboost_locale.a \
  ../../../third-64/boost/lib/libboost_log_setup.a \
  ../../../third-64/boost/lib/libboost_math_c99.a \
  ../../../third-64/boost/lib/libboost_math_c99f.a \
  ../../../third-64/boost/lib/libboost_math_c99l.a \
  ../../../third-64/boost/lib/libboost_math_tr1.a \
  ../../../third-64/boost/lib/libboost_math_tr1f.a \
  ../../../third-64/boost/lib/libboost_math_tr1l.a \
  ../../../third-64/boost/lib/libboost_prg_exec_monitor.a \
  ../../../third-64/boost/lib/libboost_program_options.a \
  ../../../third-64/boost/lib/libboost_python.a \
  ../../../third-64/boost/lib/libboost_random.a \
  ../../../third-64/boost/lib/libboost_regex.a \
  ../../../third-64/boost/lib/libboost_serialization.a \
  ../../../third-64/boost/lib/libboost_signals.a \
  ../../../third-64/boost/lib/libboost_system.a \
  ../../../third-64/boost/lib/libboost_test_exec_monitor.a \
  ../../../third-64/boost/lib/libboost_thread.a \
  ../../../third-64/boost/lib/libboost_timer.a \
  ../../../third-64/boost/lib/libboost_unit_test_framework.a \
  ../../../third-64/boost/lib/libboost_wave.a \
  ../../../third-64/boost/lib/libboost_wserialization.a \
  ../../../third-64/gflags/lib/libgflags.a \
  ../../../third-64/gflags/lib/libgflags_nothreads.a \
  ../../../third-64/gtest/lib/libgtest.a \
  ../../../third-64/gtest/lib/libgtest_main.a \
  ../../../third-64/protobuf/lib/libprotobuf-lite.a \
  ../../../third-64/protobuf/lib/libprotobuf.a \
  ../../../third-64/protobuf/lib/libprotoc.a \
  ../../../third-64/snappy/lib/libsnappy.a -lpthread \
  -lcrypto \
  -lrt \
  -lz -Xlinker "-)" -o ins
	mkdir -p ./output/bin
	cp -f --link ins ./output/bin

ins_cli:sdk/ins_cli_ins_sdk.o \
  proto/ins_cli_ins_node.pb.o \
  common/ins_cli_logging.o \
  sdk/ins_cli_ins_cli.o \
  server/ins_cli_flags.o
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mins_cli[0m']"
	$(CXX) sdk/ins_cli_ins_sdk.o \
  proto/ins_cli_ins_node.pb.o \
  common/ins_cli_logging.o \
  sdk/ins_cli_ins_cli.o \
  server/ins_cli_flags.o -Xlinker "-("  ../../../public/sofa-pbrpc/libsofa-pbrpc.a \
  ../../../third-64/boost/lib/libboost_atomic.a \
  ../../../third-64/boost/lib/libboost_chrono.a \
  ../../../third-64/boost/lib/libboost_container.a \
  ../../../third-64/boost/lib/libboost_context.a \
  ../../../third-64/boost/lib/libboost_coroutine.a \
  ../../../third-64/boost/lib/libboost_date_time.a \
  ../../../third-64/boost/lib/libboost_exception.a \
  ../../../third-64/boost/lib/libboost_filesystem.a \
  ../../../third-64/boost/lib/libboost_graph.a \
  ../../../third-64/boost/lib/libboost_locale.a \
  ../../../third-64/boost/lib/libboost_log_setup.a \
  ../../../third-64/boost/lib/libboost_math_c99.a \
  ../../../third-64/boost/lib/libboost_math_c99f.a \
  ../../../third-64/boost/lib/libboost_math_c99l.a \
  ../../../third-64/boost/lib/libboost_math_tr1.a \
  ../../../third-64/boost/lib/libboost_math_tr1f.a \
  ../../../third-64/boost/lib/libboost_math_tr1l.a \
  ../../../third-64/boost/lib/libboost_prg_exec_monitor.a \
  ../../../third-64/boost/lib/libboost_program_options.a \
  ../../../third-64/boost/lib/libboost_python.a \
  ../../../third-64/boost/lib/libboost_random.a \
  ../../../third-64/boost/lib/libboost_regex.a \
  ../../../third-64/boost/lib/libboost_serialization.a \
  ../../../third-64/boost/lib/libboost_signals.a \
  ../../../third-64/boost/lib/libboost_system.a \
  ../../../third-64/boost/lib/libboost_test_exec_monitor.a \
  ../../../third-64/boost/lib/libboost_thread.a \
  ../../../third-64/boost/lib/libboost_timer.a \
  ../../../third-64/boost/lib/libboost_unit_test_framework.a \
  ../../../third-64/boost/lib/libboost_wave.a \
  ../../../third-64/boost/lib/libboost_wserialization.a \
  ../../../third-64/gflags/lib/libgflags.a \
  ../../../third-64/gflags/lib/libgflags_nothreads.a \
  ../../../third-64/gtest/lib/libgtest.a \
  ../../../third-64/gtest/lib/libgtest_main.a \
  ../../../third-64/protobuf/lib/libprotobuf-lite.a \
  ../../../third-64/protobuf/lib/libprotobuf.a \
  ../../../third-64/protobuf/lib/libprotoc.a \
  ../../../third-64/snappy/lib/libsnappy.a -lpthread \
  -lcrypto \
  -lrt \
  -lz -Xlinker "-)" -o ins_cli
	mkdir -p ./output/bin
	cp -f --link ins_cli ./output/bin

libins_sdk.a:sdk/ins_sdk_ins_sdk.o \
  common/ins_sdk_logging.o \
  proto/ins_sdk_ins_node.pb.o
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mlibins_sdk.a[0m']"
	ar crs libins_sdk.a sdk/ins_sdk_ins_sdk.o \
  common/ins_sdk_logging.o \
  proto/ins_sdk_ins_node.pb.o
	mkdir -p ./output/lib
	cp -f --link libins_sdk.a ./output/lib

binlog_test:storage/binlog_test_binlog.o \
  storage/binlog_test_binlog_test.o \
  common/binlog_test_logging.o
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mbinlog_test[0m']"
	$(CXX) storage/binlog_test_binlog.o \
  storage/binlog_test_binlog_test.o \
  common/binlog_test_logging.o -Xlinker "-("  ../../../public/sofa-pbrpc/libsofa-pbrpc.a \
  ../../../third-64/boost/lib/libboost_atomic.a \
  ../../../third-64/boost/lib/libboost_chrono.a \
  ../../../third-64/boost/lib/libboost_container.a \
  ../../../third-64/boost/lib/libboost_context.a \
  ../../../third-64/boost/lib/libboost_coroutine.a \
  ../../../third-64/boost/lib/libboost_date_time.a \
  ../../../third-64/boost/lib/libboost_exception.a \
  ../../../third-64/boost/lib/libboost_filesystem.a \
  ../../../third-64/boost/lib/libboost_graph.a \
  ../../../third-64/boost/lib/libboost_locale.a \
  ../../../third-64/boost/lib/libboost_log_setup.a \
  ../../../third-64/boost/lib/libboost_math_c99.a \
  ../../../third-64/boost/lib/libboost_math_c99f.a \
  ../../../third-64/boost/lib/libboost_math_c99l.a \
  ../../../third-64/boost/lib/libboost_math_tr1.a \
  ../../../third-64/boost/lib/libboost_math_tr1f.a \
  ../../../third-64/boost/lib/libboost_math_tr1l.a \
  ../../../third-64/boost/lib/libboost_prg_exec_monitor.a \
  ../../../third-64/boost/lib/libboost_program_options.a \
  ../../../third-64/boost/lib/libboost_python.a \
  ../../../third-64/boost/lib/libboost_random.a \
  ../../../third-64/boost/lib/libboost_regex.a \
  ../../../third-64/boost/lib/libboost_serialization.a \
  ../../../third-64/boost/lib/libboost_signals.a \
  ../../../third-64/boost/lib/libboost_system.a \
  ../../../third-64/boost/lib/libboost_test_exec_monitor.a \
  ../../../third-64/boost/lib/libboost_thread.a \
  ../../../third-64/boost/lib/libboost_timer.a \
  ../../../third-64/boost/lib/libboost_unit_test_framework.a \
  ../../../third-64/boost/lib/libboost_wave.a \
  ../../../third-64/boost/lib/libboost_wserialization.a \
  ../../../third-64/gflags/lib/libgflags.a \
  ../../../third-64/gflags/lib/libgflags_nothreads.a \
  ../../../third-64/gtest/lib/libgtest.a \
  ../../../third-64/gtest/lib/libgtest_main.a \
  ../../../third-64/protobuf/lib/libprotobuf-lite.a \
  ../../../third-64/protobuf/lib/libprotobuf.a \
  ../../../third-64/protobuf/lib/libprotoc.a \
  ../../../third-64/snappy/lib/libsnappy.a -lpthread \
  -lcrypto \
  -lrt \
  -lz -Xlinker "-)" -o binlog_test
	mkdir -p ./output/bin
	cp -f --link binlog_test ./output/bin

server/ins_ins_main.o:server/ins_main.cc \
  common/logging.h \
  server/ins_node_impl.h \
  proto/ins_node.pb.h \
  common/mutex.h \
  common/timer.h \
  common/thread_pool.h \
  common/mutex.h \
  rpc/rpc_client.h \
  common/mutex.h \
  common/thread_pool.h \
  common/logging.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mserver/ins_ins_main.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o server/ins_ins_main.o server/ins_main.cc

server/ins_ins_node_impl.o:server/ins_node_impl.cc \
  server/ins_node_impl.h \
  proto/ins_node.pb.h \
  common/mutex.h \
  common/timer.h \
  common/thread_pool.h \
  common/mutex.h \
  rpc/rpc_client.h \
  common/mutex.h \
  common/thread_pool.h \
  common/logging.h \
  storage/meta.h \
  storage/binlog.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mserver/ins_ins_node_impl.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o server/ins_ins_node_impl.o server/ins_node_impl.cc

server/ins_flags.o:server/flags.cc
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mserver/ins_flags.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o server/ins_flags.o server/flags.cc

storage/ins_meta.o:storage/meta.cc \
  storage/meta.h \
  common/logging.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mstorage/ins_meta.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o storage/ins_meta.o storage/meta.cc

common/ins_logging.o:common/logging.cc \
  common/logging.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mcommon/ins_logging.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o common/ins_logging.o common/logging.cc

storage/ins_binlog.o:storage/binlog.cc \
  storage/binlog.h \
  common/asm_atomic.h \
  common/logging.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mstorage/ins_binlog.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o storage/ins_binlog.o storage/binlog.cc

proto/ins_node.pb.cc \
  proto/ins_node.pb.h:proto/ins_node.proto
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mproto/ins_node.pb.cc \
  proto/ins_node.pb.h[0m']"
	../../../third-64/protobuf/bin/protoc --cpp_out=proto --proto_path=proto  proto/ins_node.proto

proto/ins_node.proto:
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mproto/ins_node.proto[0m']"
	@echo "ALREADY BUILT"

proto/ins_ins_node.pb.o:proto/ins_node.pb.cc \
  proto/ins_node.pb.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mproto/ins_ins_node.pb.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o proto/ins_ins_node.pb.o proto/ins_node.pb.cc

sdk/ins_cli_ins_sdk.o:sdk/ins_sdk.cc \
  sdk/ins_sdk.h \
  common/event.h \
  common/mutex.h \
  common/timer.h \
  common/mutex.h \
  rpc/rpc_client.h \
  common/mutex.h \
  common/thread_pool.h \
  common/logging.h \
  proto/ins_node.pb.h \
  common/asm_atomic.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40msdk/ins_cli_ins_sdk.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o sdk/ins_cli_ins_sdk.o sdk/ins_sdk.cc

proto/ins_cli_ins_node.pb.o:proto/ins_node.pb.cc \
  proto/ins_node.pb.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mproto/ins_cli_ins_node.pb.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o proto/ins_cli_ins_node.pb.o proto/ins_node.pb.cc

common/ins_cli_logging.o:common/logging.cc \
  common/logging.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mcommon/ins_cli_logging.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o common/ins_cli_logging.o common/logging.cc

sdk/ins_cli_ins_cli.o:sdk/ins_cli.cc \
  sdk/ins_sdk.h \
  common/event.h \
  common/mutex.h \
  common/timer.h \
  common/mutex.h \
  rpc/rpc_client.h \
  common/mutex.h \
  common/thread_pool.h \
  common/logging.h \
  proto/ins_node.pb.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40msdk/ins_cli_ins_cli.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o sdk/ins_cli_ins_cli.o sdk/ins_cli.cc

server/ins_cli_flags.o:server/flags.cc
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mserver/ins_cli_flags.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o server/ins_cli_flags.o server/flags.cc

sdk/ins_sdk_ins_sdk.o:sdk/ins_sdk.cc \
  sdk/ins_sdk.h \
  common/event.h \
  common/mutex.h \
  common/timer.h \
  common/mutex.h \
  rpc/rpc_client.h \
  common/mutex.h \
  common/thread_pool.h \
  common/logging.h \
  proto/ins_node.pb.h \
  common/asm_atomic.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40msdk/ins_sdk_ins_sdk.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o sdk/ins_sdk_ins_sdk.o sdk/ins_sdk.cc

common/ins_sdk_logging.o:common/logging.cc \
  common/logging.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mcommon/ins_sdk_logging.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o common/ins_sdk_logging.o common/logging.cc

proto/ins_sdk_ins_node.pb.o:proto/ins_node.pb.cc \
  proto/ins_node.pb.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mproto/ins_sdk_ins_node.pb.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o proto/ins_sdk_ins_node.pb.o proto/ins_node.pb.cc

storage/binlog_test_binlog.o:storage/binlog.cc \
  storage/binlog.h \
  common/asm_atomic.h \
  common/logging.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mstorage/binlog_test_binlog.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o storage/binlog_test_binlog.o storage/binlog.cc

storage/binlog_test_binlog_test.o:storage/binlog_test.cc \
  storage/binlog.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mstorage/binlog_test_binlog_test.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o storage/binlog_test_binlog_test.o storage/binlog_test.cc

common/binlog_test_logging.o:common/logging.cc \
  common/logging.h
	@echo "[[1;32;40mCOMAKE:BUILD[0m][Target:'[1;32;40mcommon/binlog_test_logging.o[0m']"
	$(CXX) -c $(INCPATH) $(DEP_INCPATH) $(CPPFLAGS) $(CXXFLAGS)  -o common/binlog_test_logging.o common/logging.cc

endif #ifeq ($(shell uname -m),x86_64)


