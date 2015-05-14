#include <gtest/gtest.h>
#include <boost/bind.hpp>
#include <string>
#include <stdlib.h>
#include "binlog.h"

using namespace galaxy::ins;

TEST(BinLogTest, LogEntryDumpLoad) {
    BinLogger bin_logger("/tmp/");
    LogEntry log_entry, log_entry2;
    log_entry.op = kPut;
    log_entry.key = "abc";
    log_entry.value = "123";
    log_entry.term = 1;
    std::string buf;
    bin_logger.DumpLogEntry(log_entry, &buf);
    EXPECT_EQ(buf.size(), 23); //#1+4+3+4+3+8
    std::string buf2 = buf;
    bin_logger.LoadLogEntry(buf2, &log_entry2);
    EXPECT_EQ(log_entry.key, log_entry2.key);
    EXPECT_EQ(log_entry.value, log_entry2.value);
    EXPECT_EQ(log_entry.term, log_entry2.term);
    EXPECT_EQ(log_entry.op, log_entry2.op);
}

TEST(BinLogTest, SlotWriteTest) {
    system("rm /tmp/log.index");
    system("rm /tmp/log.data");
    BinLogger bin_logger("/tmp/");
    char key_buf[1024] = {'\0'};
    char value_buf[1024] = {'\0'};
    for (int i=1; i<=100; i++) {
        LogEntry log_entry;
        snprintf(key_buf, sizeof(key_buf), "key_%d", i);
        snprintf(value_buf, sizeof(value_buf), "value_%d", i);
        log_entry.key = key_buf;
        log_entry.value = value_buf;
        log_entry.term = i;
        log_entry.op = (i % 2 == 0) ? kPut : kDel;
        bin_logger.AppendEntry(log_entry);
    }
}

TEST(BinLogTest, SlotReadTest) {
    BinLogger bin_logger("/tmp/");
    char key_buf[1024] = {'\0'};
    char value_buf[1024] = {'\0'};
    for (int i=1; i<=100; i++) {
        LogEntry log_entry;
        bin_logger.ReadSlot(i-1, &log_entry);
        snprintf(key_buf, sizeof(key_buf), "key_%d", i);
        snprintf(value_buf, sizeof(value_buf), "value_%d", i);
        EXPECT_EQ(log_entry.key, std::string(key_buf));
        EXPECT_EQ(log_entry.value, std::string(value_buf));
        EXPECT_EQ(log_entry.term, i);
        EXPECT_EQ(log_entry.op, (i % 2 == 0) ? kPut : kDel);
    }
}

static int idx = 1;

static void ReadUntilTest(const LogEntry& log_entry) {
    char key_buf[1024] = {'\0'};
    char value_buf[1024] = {'\0'};
    snprintf(key_buf, sizeof(key_buf), "key_%d", idx);
    snprintf(value_buf, sizeof(value_buf), "value_%d", idx);
    EXPECT_EQ(log_entry.key, std::string(key_buf));
    EXPECT_EQ(log_entry.value, std::string(value_buf));
    EXPECT_EQ(log_entry.term, idx);
    EXPECT_EQ(log_entry.op, (idx % 2 == 0) ? kPut : kDel);
    idx++;
}

TEST(BinLogTest, SlotReadUntil) {
    BinLogger bin_logger("/tmp/");
    idx = 1;
    bool not_beyond = bin_logger.ReadUntil(99, boost::bind(ReadUntilTest, _1));
    EXPECT_TRUE(not_beyond);
}

TEST(BinLogTest, SlotReadBeyond) {
    BinLogger bin_logger("/tmp/");
    idx = 1;
    bool not_beyond = bin_logger.ReadUntil(100, boost::bind(ReadUntilTest, _1));
    EXPECT_FALSE(not_beyond);
}

TEST(BinLogTest, SlotTruncate) {
    BinLogger bin_logger("/tmp/");
    idx = 1;
    EXPECT_EQ( bin_logger.GetLength(), 100 );
    bin_logger.Truncate(49);
    EXPECT_EQ( bin_logger.GetLength(), 50);
    bool not_beyond = bin_logger.ReadUntil(49, boost::bind(ReadUntilTest, _1));
    EXPECT_TRUE(not_beyond);
    bin_logger.Truncate(-1);
    EXPECT_EQ(bin_logger.GetLength(), 0);
}

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

