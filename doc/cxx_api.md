# Nexus C++ API Manual

中文API手册请参考[这里](cxx_api_cn.md)  

**nexus** has a few quite easy-to-go interfaces for beginners to use. The system is based on a Raft protocol key-value storage. So although some functions may behave operating on a tree-like data structure, their are NOT.  
Now all the interfaces will be listed below. All the functions are in a class named `InsSDK` in the namespace `galaxy::ins`.

## Functions
|           Functions           | Interfaces                                                         |
|-------------------------------|--------------------------------------------------------------------|
|        Cluster Status         | `bool ShowCluster(members[OUT])`                                   |
|         Put k/v Data          | `bool Put(key[IN], value[IN], error[OUT])`                         |
|         Get k/v Data          | `bool Get(key[IN], value[OUT], error[OUT])`                        |
|        Delete k/v Data        | `bool Delete(key[IN], error[OUT])`                                 |
|       Get Range of Data       | `ScanResult Scan(start_key[IN], end_key[IN])`                      |
|     Value Event Listener      | `bool Watch(key[IN], callback[IN], context[IN], error[OUT])`       |
|Get Exclusive Priority on A Key| `bool Lock(key[IN], error[OUT])`                                   |
|        Try to Get Lock        | `bool TryLock(key[IN], error[OUT])`                                |
|       Release the Lock        | `bool UnLock(key[IN], error[OUT])`                                 |
|    Login to New Namespace     | `bool Login(username[IN], password[IN], error[OUT])`               |
|  Logout to Default Namespace  | `bool Logout(error[OUT])`                                          |
|   Register a User Namespace   | `bool Register(username[IN], password[IN], error[OUT])`            |
|         Get Sessin ID         | `string GetSessionID()`                                            |
|          Get User ID          | `string GetCurrentUserID()`                                        |
|      Check If Logged In       | `bool IsLoggedIn()`                                                |
| Get Event When Session Fails  | `void RegisterSessionTimeout(handle_session_timeout[IN], ctx[IN])` |
| Get RPC Statistics of Cluster | `bool ShowStatistics(statistics[OUT])`                             |
|   String of Cluster Status    | `string StatusToString(status[IN])`                                |
|     String of Error Code      | `string ErrorToString(error[IN])`                                  |

## Conceptions
1. **Session**  
	A session means a heartbeat kept communication between client and cluster. When `Lock`, `Watch`, and user-related functions are used, a session will be automatically created. A client will send heartbeat package to cluster every 2 seconds to declare the existence of itself. And the leader of cluster will abondone the session after certain seconds without receiving any heartbeat packages. Meanwhile, the client will detect if the heartbeat succeeded, and will create another session ID and mark the session timeout if after certain seconds without sending any heartbeat packages.  
	The session is essential to `Lock`, `Watch`, and user-related functions. These operations will be considered invalid after session timeout.  
2. **User**  
	User is designed to separate data of different user in case of collision. Use `Register` to create a user. And after authentication by login, all the operations are in a unique namespace and will not affect by others. Operations will be preformed in a public default namespace without logging in.  
	Logged user will get a unique user ID and this ID is tied with survival of session. If session timeout, the operations will keep returning an `kUnknownUser` error code. At that time a maunal `Logout` operation is needed to clean the login status.  
3. **Unique User IDentifier**  
	UUID is used to avoid cheating. All logged users get a universal unique user ID representing its current session, which will be considered invalid after logout or timeout.  
4. **`WatchCallback`**  
	This is a function pointer accepts `(const WatchParam&, SDKError)` parameters and returns void. When the value of watched key changes, whether is modified or deleted, the user defined `WatchCallback` type callback function will be called. `WatchParam` stores the detail of the change, and SDKError suggests the status of operation.  
	**NOTICE:** the callback here is synchronized and should not contains time consuming operations like disk operations.  
5. **`WatchParam`**  
	This is a struct representing the result of a watch event. It has following members:  
	* `key(string)` - watched key
	* `value(string)` - changed value
	* `deleted(bool)` - suggests if the k/v is deleted
	* `context(void*)` - user defined parameter, passed when `Watch` was called
6. **`ScanResult`**  
	This is a iterator styled class returned by `Scan` function. Use the object as an iterator:  
	* `bool Done()` - suggests if the result is exhausted
	* `SDKError Error()` - status of current operation
	* `const std::string Key()` - key of current data
	* `const std::string Value()` - value of current data
	* `void Next()` - moves to next data, similiar to `++` operation in iterator

## Interfaces
1. `bool ShowCluster(std::vector<ClusterNodeInfo>* cluster)`  
	Get status of every node in the cluster and stores in defined vector. Returns `true` when success, or `false` otherwise  
	* Parameter:
		* `cluster` - pointer to a vector which will be filled with node status information
	* Return value: `bool` - suggests if the operation is succeeded

2. `bool Put(const std::string& key, const std::string& value, SDKError* error)`  
	Put a key-value data to the cluster. Returns `true` when success, or `false` otherwise and `error` will be set  
	* Parameter:
		* `key` - key of the data
		* `value` - value of the data
		* `error` - status of the operation, would be `kOk, kUnknownUser, kClusterDown`
	* Return value: `bool` - suggests if the operation is succeeded

3. `bool Get(const std::string& key, const std::string* value, SDKError* error)`  
	Get the value of the key. Returns `true` when success or no such key, or `false` otherwise and `error` will be set  
	* Parameter:
		* `key` - key of the data
		* `value` - pointer to the value of the data
		* `error` - status of the operation, would be `kOk, kNoSuchKey, kUnknownUser, kClusterDown`
	* Return value: `bool` - suggests if the operation is succeeded

4. `bool Delete(const std::string& key, SDKError* error)`  
	Delete the k/v data. Returns `true` when success, or `false` otherwise and `error` will be set  
	* Parameter:
		* `key` - key of the data
		* `error` - status of the operation, would be `kOk, kUnknownUser, kClusterDown`
	* Return value: `bool` - suggests if the operation is succeeded

5. `ScanResult* Scan(const std::string& start_key, const std::string& end_key)`  
	Get a sorted range of [`start_key`, `end_key`) data. Returns pointer to an object of `ScanResult`  
	**NOTICE:** Caller should be responsible for releasing the result pointer  
	* Parameter:
		* `start_key` - start key of the range
		* `end_key` - end key of the range
	* Return value: pointer to an object of `ScanResult` - stores the result of this scan operation

6. `bool Watch(const std::string& key, WatchCallback callback, void* context, SDKError* error)`  
	Register callback function and get notification when key is touched.  Returns `true` when success, or `false` otherwise and `error` will be set  
	**NOTICE:** Consider that **nexus** can be used to manage large scale services, the caller will get notification when the children path is touched. e.g.: `/product/instance` is touched and watcher to `/product` will alse get notification  
	* Parameter:
		* `key` - key which needs to be watched
		* `callback` - `WatchCallback` type pointer
		* `context` - user defined parameter and will be passed to callback function
		* `error` - status of the operation, would be `kOk, kUnknownUser, kClusterDown`, and the callback function would get `kOk, kUnknownUser`
	* Return value: `bool` - suggests if the operation is succeeded

7. `bool Lock(const std::string& key, SDKError* error)`  
	Declare ownership of a key and same operation of others will be blocked until the first one release the key. Returns `true` when success, or `false` otherwise and `error`will be set  
	**NOTICE:** Do not write/delete a locked key, or lock an unempty key, which may lead to lock or data loss, since the cluster will actually occupy the value to record the session ID of caller. `Get` can be used to check this result  
	* Parameter:
		* `key` - key which needs to be locked
		* `error` - status of the operation, would be `kOk, kUnknownUser, kLockFail`
	* Return value: `bool` - suggests if the operation is succeeded

8. `bool TryLock(const std::string& key, SDKError* error)`  
	Try to declare ownership of a key. Will return instantly if the key is already locked. Returns `true` when success, or `false` otherwise and `error`will be set  
	* Parameter:
		* `key` - key which needs to be locked
		* `error` - status of the operation, would be `kOk, kUnknownUser, kLockFail`
	* Return value: `bool` - suggests if the operation is succeeded

9. `bool UnLock(const std::string& key, SDKError* error)`  
	Release the ownership of a key. Returns `true` when success, or `false` otherwise and `error`will be set  
	**NOTICE:** This operation should be used by the same session with locking, and others will get failure. To deprive a lock, try `Delete` it  
	* Parameter:
		* `key` - key which needs to be unlocked
		* `error` - status of the operation, would be `kOk, kUnknownUser, kClusterDown`
	* Return value: `bool` - suggests if the operation is succeeded

10. `bool Login(const std::string& username, const std::string& password, SDKError* error)`  
	Login and act as new user. Returns `true` when success, or `false` otherwise and `error` will be set  
	* Parameter:
		* `username` - username of the new user
		* `password` - password of the new user
		* `error` - status of the operation, would be `kOk, kUnknownUser, kUserExists, kPasswordError, kClusterDown`
	* Return value: `bool` - suggests if the operation is succeeded

11. `bool Logout(SDKError* error)`  
	Logout and return to default user. Should be called after user session timeout. Returns `true` when success, or `false` otherwise and `error` will be set  
	* Parameter:
		* `error` - status of the operation, would be `kOk, kUnknownUser, kClusterDown`
	* Return value: `bool` - suggests if the operation is succeeded

12. `bool Register(const std::string& username, const std::string& password, SDKError* error)`  
	Register a new user and can be use in mulitple session. Returns `true` when success, or `false` otherwise and `error` will be set  
	* Parameter:
		* `username` - username of the new user, should not be empty
		* `password` - password of the new user, should not be empty
		* `error` - status of the operation, would be `kOk, kUserExists, kPasswordError, kClusterDown`
	* Return value: `bool` - suggests if the operation is succeeded

13. `std::string GetSessionID()`  
	Get current session ID generated when session started. ID will changed if the session times out. Returns the ID as string  
	* Parameter: None
	* Return value: `string` - ID of current session

14. `std::string GetCurrentUserID()`  
	Get current user ID generated by the cluster at login time. Returns the ID as string  
	* Parameter: None
	* Return value: `string` - current user ID, would be empty if user is not logged in

15. `bool IsLoggedIn()`  
	Returns if the user is logged in  
	* Parameter: None
	* Return value: `bool` - suggests if the user is logged in

16. `void RegisterSessionTimeout(void (*handle_session_timeout)(void*), void* ctx)`  
	Register a callback function and get notification when session timeout. Returns void  
	* Parameter:
		* `handle_session_timeout` - pointer to a function which receives `void*` as parameter and returns void
		* `ctx` - user defined parameter and will be passed to callback function
	* Return value: None

17. `bool ShowStatistics(std::vector<NodeStatInfo* statistics)`
	Get RPC statistics of every node in the cluster and stores in defined vector. Returns `true` when success, or `false` otherwise  
	* Parameter:
		* `cluster` - pointer to a vector which will be filled with RPC statistics
	* Return value: `bool` - suggests if the operation is succeeded

18. `static std::string StatusToString(int32_t status)`  
	Static function to convert `int` type status to human readable string: Follower, Candidate, Leader, and Offline. Returns status as string  
	* Parameter:
		* `status` - int type status
	* Return value: `string` - status string

19. `static std::string ErrorToString(SDKError error)`  
	Static function to convert `SDKError` type error to human readable string. Returns error as string  
	* Parameter:
		* `error` - error returned by functions above
	* Return value: `string` - error string

