#include <gtest/gtest.h>
#include <string>
#include <set>
#include <boost/lexical_cast.hpp>
#include "storage/storage_manage.h"
#include "proto/ins_node.pb.h"

using namespace galaxy::ins;

TEST(StorageManageTest, OpenCloseTest) {
    StorageManager storage_manager("/tmp/storage_test1");
    bool ok = storage_manager.OpenDatabase("user1");
    EXPECT_TRUE(ok);
    ok = storage_manager.OpenDatabase("user2");
    EXPECT_TRUE(ok);
    // Open again, should be true
    ok = storage_manager.OpenDatabase("user1");
    EXPECT_TRUE(ok);
    storage_manager.CloseDatabase("user1");
    storage_manager.CloseDatabase("user2");
    // Close an inexist database
    storage_manager.CloseDatabase("user3");
    // Close again
    storage_manager.CloseDatabase("user2");
}

TEST(StorageManageTest, GetPutDeleteTest) {
    StorageManager storage_manager("/tmp/storage_test2");
    bool ok = storage_manager.OpenDatabase("user1");
    EXPECT_TRUE(ok);
    std::string value;
    // Put in default user
    Status ret = storage_manager.Put("", "Hello", "World");
    EXPECT_EQ(ret, kOk);
    ret = storage_manager.Put("user1", "Name", "User1");
    EXPECT_EQ(ret, kOk);
    ret = storage_manager.Get("", "Hello", &value);
    EXPECT_EQ(ret, kOk);
    EXPECT_EQ(value, "World");
    ret = storage_manager.Get("user1", "Name", &value);
    EXPECT_EQ(ret, kOk);
    EXPECT_EQ(value, "User1");
    // Get another user's key, check namespace
    ret = storage_manager.Get("", "Name", &value);
    EXPECT_EQ(ret, kNotFound);
    // Get inexist key
    ret = storage_manager.Get("", "aaa", &value);
    EXPECT_EQ(ret, kNotFound);
    // Give null pointer
    ret = storage_manager.Get("", "Hello", NULL);
    EXPECT_EQ(ret, kError);
    // Get key from unlogged user
    ret = storage_manager.Get("User3", "Hello", &value);
    EXPECT_EQ(ret, kUnknownUser);
    // Delete inexist key
    ret = storage_manager.Delete("", "This");
    EXPECT_EQ(ret, kNotFound);
    ret = storage_manager.Delete("", "Hello");
    EXPECT_EQ(ret, kOk);
    // Check if deleted
    ret = storage_manager.Get("", "Hello", &value);
    EXPECT_EQ(ret, kNotFound);
    // Delete another user's key
    ret = storage_manager.Delete("", "Name");
    EXPECT_EQ(ret, kOk);
    ret = storage_manager.Delete("user1", "Aha");
    EXPECT_EQ(ret, kNotFound);
    // Delete key from unlogged user
    ret = storage_manager.Delete("user2", "Aha");
    EXPECT_EQ(ret, kUnknownUser);
    ret = storage_manager.Delete("user1", "Name");
    EXPECT_EQ(ret, kOk);
    ret = storage_manager.Get("user1", "Name", &value);
    EXPECT_EQ(ret, kNotFound);
    storage_manager.CloseDatabase("user1");
}

TEST(StorageManageTest, IteratorTest) {
    StorageManager storage_manager("/tmp/storage_test3");
    bool ok = storage_manager.OpenDatabase("user1");
    EXPECT_TRUE(ok);
    const std::string value_str = "value";
    std::set<std::string> default_value;
    std::set<std::string> user1_value;
    Status ret = kOk;
    for (int i = 0; i < 10; ++i) {
        std::string value = value_str + boost::lexical_cast<std::string>(i);
        default_value.insert(value);
        ret = storage_manager.Put("", boost::lexical_cast<std::string>(i), value);
        EXPECT_EQ(ret, kOk);
    }
    for (int i = 0; i < 10; ++i) {
        std::string value = value_str + boost::lexical_cast<std::string>(i);
        user1_value.insert(value);
        ret = storage_manager.Put("user1", boost::lexical_cast<std::string>(i), value);
        EXPECT_EQ(ret, kOk);
    }
    StorageManager::Iterator* it = storage_manager.NewIterator("");
    for (it->Seek("0"); it->Valid(); it->Next()) {
        EXPECT_EQ(it->status(), kOk);
        default_value.erase(it->value());
    }
    EXPECT_TRUE(default_value.empty());
    delete it;
    it = storage_manager.NewIterator("user1");
    for (it->Seek("100"); it->Valid(); it->Next()) {
        EXPECT_EQ(it->status(), kOk);
        user1_value.erase(it->value());
    }
    EXPECT_TRUE(user1_value.empty());
    delete it;
    storage_manager.CloseDatabase("user1");
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

