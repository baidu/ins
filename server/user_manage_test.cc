#include <gtest/gtest.h>
#include <string>
#include "server/user_manage.h"
#include "proto/ins_node.pb.h"

using namespace galaxy::ins;

TEST(UserManageTest, CommonUserTest) {
    UserInfo root;
    root.set_username("root");
    root.set_passwd("rootpassword");
    UserManager user_manager("/tmp/user_test1", root);
    // Try to login an inexisting user
    std::string uuid1, uuid2, uuid3, rootid;
    Status ret = user_manager.Login("user1", "123456", &uuid1);
    EXPECT_EQ(ret, kUnknownUser);
    // Try to logout an inexisting user
    ret = user_manager.Logout("someuuid");
    EXPECT_EQ(ret, kUnknownUser);
    // Try to register root user
    ret = user_manager.Register("root", "123456");
    EXPECT_EQ(ret, kUserExists);
    // Register a set of normal users
    ret = user_manager.Register("user1", "123456");
    EXPECT_EQ(ret, kOk);
    EXPECT_TRUE(user_manager.IsValidUser("user1"));
    ret = user_manager.Register("user2", "123456");
    EXPECT_EQ(ret, kOk);
    EXPECT_TRUE(user_manager.IsValidUser("user2"));
    ret = user_manager.Register("user3", "123456");
    EXPECT_EQ(ret, kOk);
    EXPECT_TRUE(user_manager.IsValidUser("user3"));
    // Try to register an existing username
    ret = user_manager.Register("user1", "abcdefg");
    EXPECT_EQ(ret, kUserExists);
    // Login
    ret = user_manager.Login("user1", "123456", &uuid1);
    EXPECT_EQ(ret, kOk);
    EXPECT_TRUE(!uuid1.empty());
    EXPECT_TRUE(user_manager.IsLoggedIn(uuid1));
    // Login again
    ret = user_manager.Login("user1", "123456", &uuid2);
    EXPECT_EQ(ret, kOk);
    // And logout
    ret = user_manager.Logout(uuid2);
    EXPECT_EQ(ret, kOk);
    uuid2 = "";
    // Wrong password
    ret = user_manager.Login("user2", "abcdefg", &uuid2);
    EXPECT_EQ(ret, kPasswordError);
    EXPECT_TRUE(uuid2.empty());
    ret = user_manager.Login("user2", "123456", &uuid2);
    EXPECT_EQ(ret, kOk);
    EXPECT_TRUE(!uuid2.empty());
    EXPECT_TRUE(user_manager.IsLoggedIn(uuid2));
    ret = user_manager.Login("user3", "123456", &uuid3);
    EXPECT_EQ(ret, kOk);
    EXPECT_TRUE(!uuid3.empty());
    EXPECT_TRUE(user_manager.IsLoggedIn(uuid3));
    ret = user_manager.Login("root", "rootpassword", &rootid);
    EXPECT_EQ(ret, kOk);
    EXPECT_TRUE(!rootid.empty());
    EXPECT_TRUE(user_manager.IsLoggedIn(rootid));
    // Logout
    ret = user_manager.Logout(uuid1);
    EXPECT_EQ(ret, kOk);
    // Logout again
    ret = user_manager.Logout(uuid1);
    EXPECT_EQ(ret, kUnknownUser);
    ret = user_manager.Logout(uuid2);
    EXPECT_EQ(ret, kOk);
    ret = user_manager.Logout(uuid3);
    EXPECT_EQ(ret, kOk);
    ret = user_manager.Logout(rootid);
    EXPECT_EQ(ret, kOk);
    ret = user_manager.Login("user1", "123456", &uuid1);
    EXPECT_EQ(ret, kOk);
    ret = user_manager.Login("user2", "123456", &uuid2);
    EXPECT_EQ(ret, kOk);
    // Delete myself
    ret = user_manager.DeleteUser(uuid1, "user1");
    EXPECT_EQ(ret, kOk);
    EXPECT_TRUE(!user_manager.IsValidUser("user1"));
    // Force myself offline
    ret = user_manager.ForceOffline(uuid2, "user2");
    EXPECT_EQ(ret, kOk);
    EXPECT_TRUE(!user_manager.IsLoggedIn(uuid2));
}

TEST(UserManageTest, SuperUserTest) {
    UserInfo root;
    root.set_username("root");
    root.set_passwd("rootpassword");
    UserManager user_manager("/tmp/user_test2", root);
    // Login root
    std::string rootid;
    Status ret = user_manager.Login("root", "rootpassword", &rootid);
    EXPECT_EQ(ret, kOk);
    // Create some targets
    ret = user_manager.Register("user1", "123456");
    EXPECT_EQ(ret, kOk);
    ret = user_manager.Register("user2", "123456");
    EXPECT_EQ(ret, kOk);
    ret = user_manager.Register("user3", "123456");
    EXPECT_EQ(ret, kOk);
    std::string uuid1, uuid2;
    ret = user_manager.Login("user1", "123456", &uuid1);
    EXPECT_EQ(ret, kOk);
    ret = user_manager.Login("user2", "123456", &uuid2);
    EXPECT_EQ(ret, kOk);
    // Permission denied test
    ret = user_manager.ForceOffline(uuid1, "user2");
    EXPECT_EQ(ret, kPermissionDenied);
    ret = user_manager.DeleteUser(uuid1, "user2");
    EXPECT_EQ(ret, kPermissionDenied);
    ret = user_manager.TruncateOnlineUsers(uuid1);
    EXPECT_EQ(ret, kPermissionDenied);
    ret = user_manager.TruncateAllUsers(uuid1);
    EXPECT_EQ(ret, kPermissionDenied);
    // Kill a user
    ret = user_manager.DeleteUser(rootid, "user2");
    EXPECT_EQ(ret, kOk);
    EXPECT_TRUE(!user_manager.IsLoggedIn(uuid2));
    EXPECT_TRUE(!user_manager.IsValidUser("user2"));
    // Kill an inexisting users
    ret = user_manager.DeleteUser(rootid, "whoop");
    EXPECT_EQ(ret, kNotFound);
    // Truncate online user
    ret = user_manager.TruncateOnlineUsers(rootid);
    EXPECT_EQ(ret, kOk);
    EXPECT_TRUE(!user_manager.IsLoggedIn(uuid1));
    ret = user_manager.Login("user1", "123456", &uuid1);
    EXPECT_EQ(ret, kOk);
    // Force user offline
    ret = user_manager.ForceOffline(rootid, "user1");
    EXPECT_EQ(ret, kOk);
    // Force some unlogged user offline(will return ok)
    ret = user_manager.ForceOffline(rootid, "user3");
    EXPECT_EQ(ret, kOk);
    // Force some inexisting user offline
    ret = user_manager.ForceOffline(rootid, "user2");
    EXPECT_EQ(ret, kUnknownUser);
    // Truncate all users
    ret = user_manager.TruncateAllUsers(rootid);
    EXPECT_EQ(ret, kOk);
    EXPECT_TRUE(user_manager.IsValidUser("root"));
    EXPECT_TRUE(user_manager.IsLoggedIn(rootid));
    EXPECT_TRUE(!user_manager.IsValidUser("user1"));
    EXPECT_TRUE(!user_manager.IsValidUser("user3"));
}

TEST(UserManageTest, ToolFunctionTest) {
    UserInfo root;
    root.set_username("root");
    root.set_passwd("rootpassword");
    UserManager user_manager("/tmp/user_test3", root);
    std::string uuid1, uuid2;
    Status ret = user_manager.Register("user1", "123456");
    EXPECT_EQ(ret, kOk);
    ret = user_manager.Register("user2", "123456");
    EXPECT_EQ(ret, kOk);
    // Test CalcUuid
    uuid1 = UserManager::CalcUuid("user1");
    ret = user_manager.Login("user1", "123456", &uuid2);
    EXPECT_EQ(ret, kOk);
    EXPECT_EQ(UserManager::CalcName(uuid1), UserManager::CalcName(uuid2));
    // Test user_manager.IsLoggedIn
    EXPECT_TRUE(user_manager.IsLoggedIn(uuid2));
    EXPECT_TRUE(!user_manager.IsLoggedIn("user3"));
    // Test user_manager.IsValidUser
    EXPECT_TRUE(user_manager.IsValidUser("user1"));
    EXPECT_TRUE(user_manager.IsValidUser("user2"));
    EXPECT_TRUE(!user_manager.IsValidUser("user3"));
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

