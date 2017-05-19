#include <gtest/gtest.h>
#include <gflags/gflags.h>
#include <cstdlib>
#include "server/performance_center.h"

DECLARE_int32(performance_interval);

using namespace galaxy::ins;

// Following testcases need carefully consideration
#if 0
TEST(PerformanceCenterTest, BasicFunctionTest) {
    PerformanceCenter pc(10);
    for (int i = 0; i < 100; ++i) {
        pc.Put();
    }
    EXPECT_EQ(pc.CurrentPut(), 100);
    for (int i = 0; i < 200; ++i) {
        pc.Get();
    }
    EXPECT_EQ(pc.CurrentGet(), 200);
    for (int i = 0; i < 300; ++i) {
        pc.Delete();
    }
    EXPECT_EQ(pc.CurrentDelete(), 300);
    for (int i = 0; i < 400; ++i) {
        pc.Scan();
    }
    EXPECT_EQ(pc.CurrentScan(), 400);
    for (int i = 0; i < 500; ++i) {
        pc.KeepAlive();
    }
    EXPECT_EQ(pc.CurrentKeepAlive(), 500);
    for (int i = 0; i < 600; ++i) {
        pc.Lock();
    }
    EXPECT_EQ(pc.CurrentLock(), 600);
    for (int i = 0; i < 700; ++i) {
        pc.Unlock();
    }
    EXPECT_EQ(pc.CurrentUnlock(), 700);
    for (int i = 0; i < 800; ++i) {
        pc.Watch();
    }
    EXPECT_EQ(pc.CurrentWatch(), 800);
}

TEST(PerformanceCenterTest, HistoryAverageTest) {
    PerformanceCenter pc(10);
    for (int i = 0; i < 100; ++i) {
        pc.Put();
    }
    EXPECT_EQ(pc.CurrentPut(), 100);
    usleep(FLAGS_performance_interval * 1000);
    EXPECT_EQ(pc.CurrentPut(), 0);
    for (int i = 0; i < 200; ++i) {
        pc.Put();
    }
    EXPECT_EQ(pc.CurrentPut(), 200);
    usleep(FLAGS_performance_interval * 1000);
    EXPECT_EQ(pc.CurrentPut(), 0);
    EXPECT_EQ(pc.AveragePut(), 150);
}
#endif

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

