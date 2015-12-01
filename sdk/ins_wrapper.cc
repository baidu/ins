#include "ins_sdk.h"

#include <stdlib.h>
#include <string.h>
#include <algorithm>

using namespace galaxy::ins::sdk;

extern "C" {

typedef void (*SessionTimeoutCallback)(void*);
typedef void (*CWatchCallback)(WatchParam*, long, SDKError);
typedef void (*CTimeoutCallback)(long, void*);
typedef void (*AbstractFunc)();

struct CallbackPack {
    AbstractFunc callback_wrapper;
    long callback_id;
    void* ctx;
};

void WatchCallbackWrapper(const WatchParam& param, SDKError error) {
    CallbackPack* pack = static_cast<CallbackPack*>(param.context);
    CWatchCallback cb = (CWatchCallback)pack->callback_wrapper;
    long callback_id = pack->callback_id;
    void* ctx = pack->ctx;
    delete pack;
    WatchParam p = param;
    p.context = ctx;
    cb(&p, callback_id, error);
}

void TimeoutWrapper(void* ctx) {
    CallbackPack* pack = static_cast<CallbackPack*>(ctx);
    CTimeoutCallback cb = (CTimeoutCallback)pack->callback_wrapper;
    long callback_id = pack->callback_id;
    void* context = pack->ctx;
    delete pack;
    cb(callback_id, context);
}

// ----- InsSDK Wrappers -----
InsSDK* GetSDK(const char* server_list) {
    return new InsSDK(server_list);
}

InsSDK* GetSDKFromArray(int count, const char* members[]) {
    std::vector<std::string> member_vec;
    for (int i = 0; i < count; ++i) {
        member_vec.push_back(members[i]);
    }
    return new InsSDK(member_vec);
}

void DeleteSDK(InsSDK* sdk) {
    if (sdk != NULL) {
        delete sdk;
    }
}

void DeleteClusterArray(ClusterNodeInfo* pointer) {
    if (pointer != NULL) {
        delete[] pointer;
    }
}

ClusterNodeInfo* SDKShowCluster(InsSDK* sdk, int* count) {
    if (sdk == NULL) {
        return NULL;
    }
    std::vector<ClusterNodeInfo> cluster_info;
    sdk->ShowCluster(&cluster_info);
    *count = cluster_info.size();
    ClusterNodeInfo* cluster_arr = new ClusterNodeInfo[*count];
    std::copy(cluster_info.begin(), cluster_info.end(), cluster_arr);
    return cluster_arr;
}

NodeStatInfo* SDKShowStatistics(InsSDK* sdk, int* count) {
    if (sdk == NULL) {
        return NULL;
    }
    std::vector<NodeStatInfo> stat_info;
    sdk->ShowStatistics(&stat_info);
    *count = stat_info.size();
    NodeStatInfo* stat_arr = new NodeStatInfo[*count];
    std::copy(stat_info.begin(), stat_info.end(), stat_arr);
    return stat_arr;
}

void DeleteStatArray(NodeStatInfo* pointer) {
    if (pointer != NULL) {
        delete pointer;
    }
}

bool SDKPut(InsSDK* sdk, const char* key, int key_len, const char* buf, int buf_len,
            SDKError* error) {
    if (sdk == NULL) {
        *error = kPermissionDenied;
        return false;
    }
    return sdk->Put(std::string(key, key_len), std::string(buf, buf_len), error);
}

bool SDKGet(InsSDK* sdk, const char* key, int key_len, char** buf_ptr, int32_t* buf_len,
            SDKError* error) {
    if (sdk == NULL) {
        *error = kPermissionDenied;
        return "";
    }
    std::string value;
    bool ret = sdk->Get(std::string(key, key_len), &value, error);
    if (ret) {
        *buf_len = value.size();
        char* buf = static_cast<char*>(malloc(value.size()));
        memcpy(buf, value.data(), value.size());
        *buf_ptr = buf;
    } else {
        *buf_ptr = NULL;
        *buf_len = 0;
    }
    return ret;
}

bool SDKDelete(InsSDK* sdk, const char* key, int key_len, SDKError* error) {
    if (sdk == NULL) {
        *error = kPermissionDenied;
        return false;
    }
    return sdk->Delete(std::string(key, key_len), error);
}

ScanResult* SDKScan(InsSDK* sdk, const char* start_key, int start_key_len,
                    const char* end_key, int end_key_len) {
    if (sdk == NULL) {
        return NULL;
    }
    return sdk->Scan(std::string(start_key, start_key_len), std::string(end_key, end_key_len));
}

// NOTE: This interface is customized for python sdk.
//       For other purpose, please implement another watch interface
bool SDKWatch(InsSDK* sdk, const char* key, int key_len, CWatchCallback wrapper,
              long callback_id, void* context, SDKError* error) {
    if (sdk == NULL) {
        return false;
    }
    CallbackPack* pack = new CallbackPack;
    pack->callback_wrapper = reinterpret_cast<AbstractFunc>(wrapper);
    pack->callback_id = callback_id;
    pack->ctx = context;
    return sdk->Watch(std::string(key, key_len), WatchCallbackWrapper, pack, error);
}

bool SDKLock(InsSDK* sdk, const char* key, int key_len, SDKError* error) {
    if (sdk == NULL) {
        *error = kPermissionDenied;
        return false;
    }
    return sdk->Lock(std::string(key, key_len), error);
}

bool SDKTryLock(InsSDK* sdk, const char* key, int key_len, SDKError* error) {
    if (sdk == NULL) {
        *error = kPermissionDenied;
        return false;
    }
    return sdk->TryLock(std::string(key, key_len), error);
}

bool SDKUnLock(InsSDK* sdk, const char* key, int key_len, SDKError* error) {
    if (sdk == NULL) {
        *error = kPermissionDenied;
        return false;
    }
    return sdk->UnLock(std::string(key, key_len), error);
}

bool SDKLogin(InsSDK* sdk, const char* username, const char* password, SDKError* error) {
    if (sdk == NULL) {
        *error = kPermissionDenied;
        return false;
    }
    return sdk->Login(username, password, error);
}

bool SDKLogout(InsSDK* sdk, SDKError* error) {
    if (sdk == NULL) {
        *error = kPermissionDenied;
        return false;
    }
    return sdk->Logout(error);
}

bool SDKRegister(InsSDK* sdk, const char* username, const char* password, SDKError* error) {
    if (sdk == NULL) {
        *error = kPermissionDenied;
        return false;
    }
    return sdk->Register(username, password, error);
}

const char* SDKGetSessionID(InsSDK* sdk) {
    if (sdk == NULL) {
        return "";
    }
    return sdk->GetSessionID().c_str();
}

const char* SDKGetCurrentUserID(InsSDK* sdk) {
    if (sdk == NULL) {
        return "";
    }
    return sdk->GetCurrentUserID().c_str();
}

bool SDKIsLoggedIn(InsSDK* sdk) {
    if (sdk == NULL) {
        return false;
    }
    return sdk->IsLoggedIn();
}

// NOTE: This interface is customized for python sdk.
//       For other purpose, please implement another interface
void SDKRegisterSessionTimeout(InsSDK* sdk, SessionTimeoutCallback handle_session_timeout,
                               long callback_id, void* ctx) {
    if (sdk == NULL) {
        return;
    }
    CallbackPack* pack = new CallbackPack();
    pack->callback_wrapper = reinterpret_cast<AbstractFunc>(handle_session_timeout);
    pack->callback_id = callback_id;
    pack->ctx = ctx;
    sdk->RegisterSessionTimeout(handle_session_timeout, ctx);
}

// ----- ScanResult Wrappers -----
void DeleteScanResult(ScanResult* result) {
    if (result != NULL) {
        delete result;
    }
}

bool ScanResultDone(ScanResult* result) {
    if (result == NULL) {
        return false;
    }
    return result->Done();
}

SDKError ScanResultError(ScanResult* result) {
    if (result == NULL) {
        return kPermissionDenied;
    }
    return result->Error();
}

bool ScanResultKey(ScanResult* result, char** buf_ptr, int* buf_len) {
    if (result == NULL) {
        return false;
    }
    const std::string key = result->Key();
    *buf_len = key.size();
    char* buf = static_cast<char*>(malloc(key.size()));
    memcpy(buf, key.data(), key.size());
    *buf_ptr = buf;
    return true;
}

bool ScanResultValue(ScanResult* result, char** buf_ptr, int* buf_len) {
    if (result == NULL) {
        return false;
    }
    const std::string& value = result->Value();
    *buf_len = value.size();
    char* buf = static_cast<char*>(malloc(value.size()));
    memcpy(buf, value.data(), value.size());
    *buf_ptr = buf;
    return true;
}

void ScanResultNext(ScanResult* result) {
    if (result == NULL) {
        return;
    }
    result->Next();
}

void FreeString(char* buf) {
    if (buf) {
        free(buf);
    }
}

}// "extern C"

