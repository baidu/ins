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
    } 
    return 0;
}

