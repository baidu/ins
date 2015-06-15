#include "ins_sdk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <algorithm>
#include <vector>
#include <iostream>

using namespace galaxy::ins::sdk;

void my_watch_cb(const WatchParam& param, SDKError error) {
    InsSDK* sdk = static_cast<InsSDK*>(param.context);
    printf("key: %s\n", param.key.c_str());
    printf("value: %s\n", param.value.c_str());
    printf("deleted: %s\n", param.deleted ?"true":"false");
    printf("error code: %d\n", static_cast<int>(error));
    if (sdk) {
        printf("watch again\n");
        SDKError er;
        sdk->Watch(param.key, my_watch_cb, (void*)sdk, &er);
    }
}

int main(int argc, char* argv[]) {
    std::vector<std::string> members;
    if (argc < 2) {
        fprintf(stderr, "./sample [read|write]\n");
        return 1;
    }
    if (strcmp("write", argv[1]) == 0) {
        fprintf(stderr, "write test\n");
        InsSDK::ParseFlagFromArgs(argc, argv, &members);
        InsSDK sdk(members);
        char key_buf[1024] = {'\0'};
        char value_buf[1024] = {'\0'};
        SDKError err;
        for (int i=1; i<=100000; i++) {
            snprintf(key_buf, sizeof(key_buf), "key_%d", i);
            snprintf(value_buf, sizeof(value_buf), "value_%d", i);
            sdk.Put(key_buf, value_buf, &err);
            if (err == kClusterDown) {
                i--;
                printf("try put again: %s\n", key_buf);
                sleep(2);
                continue;
            }
            printf("%s\n", key_buf);
            fflush(stdout);
        }
    } else if(strcmp("read", argv[1]) == 0) {
        fprintf(stderr, "read test\n");
        InsSDK::ParseFlagFromArgs(argc, argv, &members);
        InsSDK sdk(members);
        char key_buf[1024] = {'\0'};
        char value_buf[1024] = {'\0'};
        SDKError err;
        for (int i=1; i<=100000; i++) {
            snprintf(key_buf, sizeof(key_buf), "key_%d", i);
            snprintf(value_buf, sizeof(value_buf), "value_%d", i);
            std::string value;
            sdk.Get(key_buf, &value, &err);
            if (err == kClusterDown) {
                i--;
                printf("try get again: %s", key_buf);
                sleep(2);
                continue;
            }
            if (err == kOK) {
                printf("%s\n", value.c_str());
            } else if (err == kNoSuchKey) {
                printf("NOT FOUND\n");
            }
            fflush(stdout);
        }
    } else if(strcmp("watch", argv[1]) == 0) {
        fprintf(stderr, "watch test\n");
        InsSDK::ParseFlagFromArgs(argc, argv, &members);
        InsSDK sdk(members);
        char key_buf[1024] = {'\0'};
        SDKError err;
        for (int i=1; i<=1000; i++) {
            snprintf(key_buf, sizeof(key_buf), "key_%d", i);
            sdk.Watch(key_buf, my_watch_cb, &sdk, &err);
        }
        while (true) {
            sleep(1);
        }
    } 
    return 0;
}

