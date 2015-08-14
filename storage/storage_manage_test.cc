#include <gtest/gtest.h>
#include <string>
#include "storage_manage.h"
#include "proto/ins_node.pb.h"

using namespace galaxy::ins;

TEST(StorageManageTest, OpenCloseTest) {
	StorageManager storage_manager("/tmp/test1");
	bool ok = storage_manager.OpenDatabase("user1");
	EXPECT_TRUE(ok);
	ok = storage_manager.OpenDatabase("user2");
	EXPECT_TRUE(ok);
	ok = storage_manager.OpenDatabase("user1");
	EXPECT_TRUE(ok);
	storage_manager.CloseDatabase("user1");
	storage_manager.CloseDatabase("user2");
	storage_manager.CloseDatabase("user3");
	storage_manager.CloseDatabase("user2");
}

TEST(StorageManageTest, GetPutTest) {
	StorageManager storage_manager("/tmp/test2");
	bool ok = storage_manager.OpenDatabase("user1");
	EXPECT_TRUE(ok);
	std::string value;
	DBStatus ret = storage_manager.Put("", "Hello", "World");
	EXPECT_EQ(ret, kOk);
	ret = storage_manager.Put("user1", "Name", "User1");
	EXPECT_EQ(ret, kOk);
	ret = storage_manager.Get("", "Hello", &value);
	EXPECT_EQ(ret, kOk);
	EXPECT_EQ(value, "World");
	ret = storage_manager.Get("user1", "Name", &value);
	EXPECT_EQ(ret, kOk);
	EXPECT_EQ(value, "User1");
	ret = storage_manager.Get("", "Name", &value);
	EXPECT_EQ(ret, kNotFound);
	ret = storage_manager.Get("", "aaa", &value);
	EXPECT_EQ(ret, kNotFound);
	ret = storage_manager.Get("", "Hello", NULL);
	EXPECT_EQ(ret, kError);
	ret = storage_manager.Get("User3", "Hello", &value);
	EXPECT_EQ(ret, kNotFound);
}

int main(int argc, char *argv[]) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

