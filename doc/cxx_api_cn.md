# Nexus C++ API手册

For English version of this manual, please refer to [this](cxx_api.md)

**nexus**使用kv结构来存储用户数据，其功能本不基于树形结构实现，因此请以kv结构的思路来理解下面的接口。  
以下接口均在`InsSDK`类中，放在`galaxy::ins`名空间里。  

## 功能
|         功能         |                               接口格式                             |
|----------------------|--------------------------------------------------------------------|
|     展示节点信息     | `bool ShowCluster(members[OUT])`                                   |
|         写入         | `bool Put(key[IN], value[IN], error[OUT])`                         |
|         读取         | `bool Get(key[IN], value[OUT], error[OUT])`                        |
|         删除         | `bool Delete(key[IN], error[OUT])`                                 |
|         扫描         | `ScanResult Scan(start_key[IN], end_key[IN])`                      |
|       变更监视       | `bool Watch(key[IN], callback[IN], context[IN], error[OUT])`       |
|         锁定         | `bool Lock(key[IN], error[OUT])`                                   |
|       尝试加锁       | `bool TryLock(key[IN], error[OUT])`                                |
|         解锁         | `bool UnLock(key[IN], error[OUT])`                                 |
|         登录         | `bool Login(username[IN], password[IN], error[OUT])`               |
|         登出         | `bool Logout(error[OUT])`                                          |
|         注册         | `bool Register(username[IN], password[IN], error[OUT])`            |
|      获取会话ID      | `string GetSessionID()`                                            |
|    获取当前用户ID    | `string GetCurrentUserID()`                                        |
|       登录状态       | `bool IsLoggedIn()`                                                |
| 注册会话超时回调函数 | `void RegisterSessionTimeout(handle_session_timeout[IN], ctx[IN])` |
|     RPC压力数据      | `bool ShowStatistics(statistics[OUT])`                             |
|     解析节点状态     | `string StatusToString(status[IN])`                                |
|      解析错误码      | `string ErrorToString(error[IN])`                                  |

## 名词及类型解释
1. 会话(Session)  
	当创建一个sdk对象时，对象会按照配置和iNexus集群进行连接，并生成一个全局唯一的会话ID。在整个连接过程中，sdk对象会向集群发送心跳包来保持连接。若集群三次未收到心跳包则会认为此次会话超时，并将该会话注册的监视、锁和登录的用户移除。这是为了应对客户端crash造成的集群垃圾信息堆积。同时，客户端在一段时间（默认为30s）内没有正常收到集群的心跳回复便会重新生成会话ID开启新的会话。  
	会话是锁、监视事件的单位，也是保持一个用户登录单位。所有这些操作均由某一特定会话发出并维护。当会话过期后这些操作均视为失效。  

2. 用户  
	用户是对数据资源的名空间划分。每个用户拥有独立的名空间，所有针对特定用户的操作不会影响其他用户。每个用户需要注册才会存在，但root用户除外（root用户相关功能暂时不可用）。  
	对用户名空间的操作以登录为前提，通过用户名密码的方式进行校验。登录用户会得到一个全局唯一的用户ID，用于标识这一用户的操作。用户登录后通过心跳和集群保持联系，若心跳超时，在移除会话信息的同时会销毁这一用户ID。此后使用该过期用户ID的操作均被视为无效，并返回`kUnknownUser`的错误。之后sdk需调用`Logout`清除当前的登录状态。  
	为方便以及与老版本的兼容，未登录状态的所有操作均会在一个公共空间生效。  

3. 用户ID(Unique User IDentifier)  
	用户ID是一个全局唯一的用于标识用户信息的编号，为了防止作弊，sdk使用用户ID而非用户名作为操作名空间的标识。用户ID在超时后或登出后过期，使用过期用户ID的操作均会被视为无效。  

4. `WatchCallback`  
	`WatchCallback`是一个返回值为`void`，参数表为`(const WatchParam&, SDKError)`的函数指针。当监视的键值对值发生改变（修改或删除）时，便会回调一个`WatchCallback`类型的函数。`WatchParam`中存储着这次监视结果的信息，`SDKError`则存储监视操作的情况。  
	**注意：** 监视回调是个同步操作，建议不要在回调函数中进行太多耗时的操作或者磁盘读写。  

5. `WatchParam`  
	`WatchParam`是一个结构体，用于存储本次监视的结果。其成员有：  
	* `key(string)` - 监视作用的键
	* `value(string)` - 监视的键改变后的值
	* `deleted(bool)` - 标识该键值对是否被删除
	* `context(void*)` - 注册变更通知时传入的参数，用于在`WatchCallback`函数中使用

6. `ScanResult`  
	`ScanResult`是一个拥有`Next`方法的类似迭代器的类，由`Scan`操作创建并返回，记录了`Scan`操作的结果和状态。其接口如下：  
	1. `bool Done()`  
		返回本次扫描是否完成。
	2. `SDKError Error()`  
		返回本次扫描操作的状态信息。
	3. `const std::string Key()`  
		返回当前`ScanResult`指向的键值对的键
	4. `const std::string Value()`  
		返回当前`ScanResult`指向的键值对的值
	5. `void Next()`  
		指向下一个结果，用于迭代整个扫描结果。类似迭代器中的`++`操作。

## 说明
1. `bool ShowCluster(std::vector<ClusterNodeInfo>* cluster)`  
	获取当前各个节点的状态信息，并存放在给定的数组中。获取正确返回`true`，失败返回`false`。  
	* 参数：
		* `cluster` - 用于记录已获取的节点状态信息的数组
	* 返回值：`bool`值 - 表示状态获取是否成功

2. `bool Put(const std::string& key, const std::string& value, SDKError* error)`  
	写入一条Key-Value数据，正确写入返回`true`，失败返回`false`。操作的具体信息写入`error`，正确时写入`kOK`。  
	* 参数：
		* `key` - 写入数据的键
		* `value` - 写入数据的值
		* `error` - 记录操作状态信息。可能的值有：`kOK, kUnknownUser, kClusterDown`
	* 返回值：`bool`值 - 表示是否写入成功

3. `bool Get(const std::string& key, const std::string* value, SDKError* error)`  
	根据Key的值获取对应的值，正确时返回`true`，并将结果写入value，若无此键也返回`true`。失败返回`false`。操作的具体信息写入`error`，正确时写入`kOK`。  
	* 参数：
		* `key` - 获取数据的键
		* `value` - 得到数据的值
		* `error` - 记录操作状态信息。可能的值有：`kOk, kUnknownUser, kNoSuchKey, kClusterDown`
	* 返回值：`bool`值 - 表示读入是否成功

4. `bool Delete(const std::string& key, SDKError* error)`  
	删除`key`所对应的值，正确时返回`true`，失败返回`false`。操作的具体信息写入`error`，正确时写入`kOK`。  
	* 参数：
		* `key` - 删除数据的键
		* `error` - 记录操作状态信息。可能的值有：`kOK, kUnknownUser, kClusterDown`
	* 返回值：`bool`值 - 表示删除是否成功

5. `ScanResult* Scan(const std::string& start_key, const std::string& end_key)`  
	根据给定的数据范围扫描数据库，得到[`start_key`, `end_key`)的结果。返回一个`ScanResult`类型的类似迭代器的对象，通过该对象的接口来迭代所有的搜索结果。  
	**注意：** 由调用者负责释放获得的`ScanResult`对象，否则会发生内存泄漏。
	* 参数：
		* `start_key` - 扫描范围的开始的键
		* `end_key` - 扫描范围的结束的键
	* 返回值：`ScanResult`对象 - 记录扫描信息的类似迭代器结构的对象

6. `bool Watch(const std::string& key, WatchCallback callback, void* context, SDKError* error)`  
	对给定键添加变更通知，当数据的值变动（修改或删除）时调用给定的callback函数。考虑到iNexus可能被用来做分布式文件系统中的锁，因此该监视功能中包含对于子目录的监视，即如果为某一个键添加监视，则以该键为子目录的键变更也会收到通知。添加正确时返回`true`，失败时返回`false`。操作的具体信息写入`error`，正确时写入`kOK`。  
	* 参数：
		* `key` - 监视数据的键
		* `callback` - 回调函数指针
		* `context` - 回调函数使用的环境和参数
		* `error` - 记录操作状态信息，可能的值有：`kOK, kUnknownUser, kClusterDown`。回调函数中传入的`SDKError`可能为`kOK, kUnknownUser`
	* 返回值：`bool`值 - 表示监视是否成功

7. `bool Lock(const std::string& key, SDKError* error)`  
	锁定给定的键及对应的数据，使得其他用户锁定同一键时；会阻塞在这一过程上，直到获取到该锁为止。锁定正确时返回`true`，失败返回`false`。操作的具体信息写入`error`，正确时写入`kOK`。  
	**注意：** 锁定的值不能写入，也不能锁定一个已经存在键值对数据的键。这一行为可能会导致数据丢失和锁失效。由于锁定时会将锁定会话的ID写入值，可以通过`Get`获取。  
	* 参数：
		* `key` - 待锁定的键
		* `error` - 记录操作状态信息，可能的值有：`kOK, kUnknownUser, kLockFail`
	* 返回值：`bool`值 - 添加锁是否成功

8. `bool TryLock(const std::string& key, SDKError* error)`  
	尝试对给定的键进行锁定，与`Lock`的区别在于当锁定失败时，会立即返回而非阻塞至获取锁。加锁正确时返回`true`，失败时返回`false`。操作的具体信息写入`error`，正确时写入`kOK`。  
	* 参数：
		* `key` - 待锁定的键
		* `error` - 记录操作状态信息，可能的值有：`kOK, kUnknownUser, kLockFail`
	* 返回值：`bool`值 - 添加锁是否成功

9. `bool UnLock(const std::string& key, SDKError* error)`  
	解除对给定键的锁定。正确时返回`true`，失败返回`false`。操作的具体信息写入`error`，正确时写入`kOK`。  
	**注意：** 解锁操作只能由加锁的同一个会话来进行。试图解除其他会话加的锁会失败。可以强制删除其他会话加锁的键使得加锁失效。  
	* 参数：
		* `key` - 带解除锁定的键
		* `error` - 记录操作状态信息。可能的值有：`kOK, kUnknownUser, kClusterDown`
	* 返回值：`bool`值 - 解除锁定是否成功

10. `bool Login(const std::string& username, const std::string& password, SDKError* error)`  
	登录用户并以该用户身份进行操作。每个用户有独立的数据库，每个用户的数据之间互不影响，每个用户独立监视、锁定、读写自己的数据。登录后的以上操作均对该用户进行，直到该会话超时或登出。未登录用户则使用公共空间，每次登录会分配一个全局唯一的用户ID作为标识。若登录成功则会返回`true`，失败返回`false`。操作的具体信息写入`error`，正确时写入`kOK`。  
	* 参数：
		* `username` - 带登录用户名
		* `password` - 用户的密码
		* `error` - 记录操作状态信息，可能的值有：`kOK, kUnknownUser, kUserExists, kPasswordError, kClusterDown`
	* 返回值：`bool`值 - 登录是否成功

11. `bool Logout(SDKError* error)`  
	登出已登录的用户。直到该操作之后各类读写操作会在公共空间生效。若用户因超时而离线，之后进行的操作会全部失效，需调用`Logout`来登出并重新登录或在公共空间操作。登出成功返回`true`，失败返回`false`。操作的具体信息写入`error`，正确时写入`kOK`。  
	* 参数：
		* `error` - 记录操作状态信息。可能的值有：`kOK, kUnknownUser, kClusterDown`
	* 返回值：`bool`值 - 登出是否成功

12. `bool Register(const std::string& username, const std::string& password, SDKError* error)`  
	注册一个新用户。注册的新用户生效后便可以在多个会话中登录，并享有独立的名空间。注册成功返回`true`，失败返回`false`。操作的具体情况写入`error`，正确时写入`kOK`。  
	* 参数：
		* `username` - 新用户的用户名，不能为空
		* `password` - 新用户的密码，不能为空
		* `error` - 记录操作状态信息，可能的值有：`kOK, kUserExists, kPasswordError, kClusterDown`
	* 返回值：`bool`值 - 注册是否成功

13. `std::string GetSessionID()`  
	获取当前会话的ID。每个sdk对象会与iNexus集群产生一个会话，并由一个全局唯一的会话ID来标识。会话超时则会重新生成会话ID并尝试重连。返回当前会话的ID。  
	* 参数：没有参数
	* 返回值：`string`类型 - 会话的ID

14. `std::string GetCurrentUserID()`  
	获取当前用户的ID。因已登录用户会由iNexus集群分配一个全局唯一的用户ID来标识。返回值则为该用户ID。  
	* 参数：没有参数
	* 返回值：`string`类型 - 当前用户的ID。若用户未登录则返回一个空字符串

15. `bool IsLoggedIn()`  
	返回当前是否有用户登录。  
	* 参数：没有参数
	* 返回值：`bool`值 - 是否有用户登录

16. `void RegisterSessionTimeout(void (*handle_session_timeout)(void*), void* ctx)`  
	注册当会话超时时的回调函数。当会话超时时调用该函数通知客户端。无返回值。  
	* 参数：
		* `handle_session_timeout` - 会话超时时的回调函数指针
		* `ctx` - 回调函数的参数
	* 返回值：无返回值

17. `bool ShowStatistics(std::vector<NodeStatInfo* statistics)`
	获取各个节点的RPC调用数据并保存在指定的数组中。获取成功返回`true`，失败返回`false`。  
	* 参数：
		* `cluster` - 用于记录已获取的节点RPC调用数据的数组
	* 返回值：`bool`值 - 表示数据获取是否成功

18. `static std::string StatusToString(int32_t status)`  
	静态函数。将节点的当前状态：Follower、Candidate、Leader或Offline，转换成字符串。  
	* 参数：
		* `status` - 整型表示的节点信息
	* 返回值：`string`类型 - 节点状态字符串

19. `static std::string ErrorToString(SDKError error)`  
	静态函数。将返回的`SDKError`类型的错误码转换成字符串
	* 参数：
		* `error` - `SDKError`类型的字符串
	* 返回值：`string`类型 - 错误码字符串

