#!/usr/bin/python

from ctypes import CDLL, byref, CFUNCTYPE, POINTER, Structure
from ctypes import c_void_p, c_char_p, c_long, c_int, c_bool, py_object
from collections import Iterable
import threading
import time

SDKError = ('OK', 'ClusterDown', 'NoSuchKey', 'Timeout', 'LockFail',
            'CleanBinlogFail', 'UserExists', 'PermissionDenied', 'PasswordError',
            'UnknownUser')
NodeStatus = ('Leader', 'Candidate', 'Follower', 'Offline')
ClusterInfo = ('server_id', 'status', 'term', 'last_log_index', 'last_log_term',
               'commit_index', 'last_applied')

class InsSDK:
    class _ClusterNodeInfo(Structure):
        _fields_ = [('server_id', c_char_p),
                    ('status', c_int),
                    ('term', c_long),
                    ('last_log_index', c_long),
                    ('last_log_term', c_long),
                    ('commit_index', c_long),
                    ('last_applied', c_long)]
    class _WatchParam(Structure):
        _fields_ = [('key', c_char_p),
                    ('value', c_char_p),
                    ('deleted', c_bool),
                    ('context', c_void_p)]

    def __init__(self, members):
        if isinstance(members, basestring):
            self.sdk = _ins.GetSDK(members)
        else:
            self.sdk = _ins.GetSDKFromArray(members)
        self.local = threading.local()
        self.local.errno = c_int()

    def __del__(self):
        if _ins != None:
            _ins.DeleteSDK(self.sdk)

    def error(self):
        return SDKError[self.local.errno]

    def show(self):
        count = c_int()
        cluster_ptr = _ins.SDKShowCluster(self.sdk, byref(count))
        count = count.value
        ClusterArray = self._ClusterNodeInfo * count
        clusters = ClusterArray.from_address(cluster_ptr)
        cluster_list = []
        for i in xrange(count):
            cluster_list.append({
                'server_id' : str(clusters[i].server_id),
                'status' : NodeStatus[clusters[i].status],
                'term' : clusters[i].term,
                'last_log_index' : clusters[i].last_log_index,
                'last_log_term' : clusters[i].last_log_term,
                'commit_index' : clusters[i].commit_index,
                'last_applied' : clusters[i].last_applied
            })
        _ins.DeleteClusterArray(cluster_ptr)
        return cluster_list

    def get(self, key):
        return _ins.SDKGet(self.sdk, key, byref(self.local.errno))

    def put(self, key, value):
        return _ins.SDKPut(self.sdk, key, value, byref(self.local.errno))

    def delete(self, key):
        return _ins.SDKDelete(self.sdk, key, byref(self.local.errno))

    def scan(self, start_key, end_key):
        self.local.errno = c_int(0)
        return ScanResult(_ins.SDKScan(self.sdk, start_key, end_key))

    # TODO unable to transfer the function pointer
    def watch(self, key, callback, context):
        WatchCallback = CFUNCTYPE(self._WatchParam, c_int)
        ctx = py_object(context)
        return _ins.SDKWatch(self.sdk, key, WatchCallback(callback), ctx, byref(self.local.errno))

    def lock(self, key):
        return _ins.SDKLock(self.sdk, key, byref(self.local.errno))

    def trylock(self, key):
        return _ins.SDKTryLock(self.sdk, key, byref(self.local.errno))

    def unlock(self, key):
        return _ins.SDKUnLock(self.sdk, key, byref(self.local.errno))

    def login(self, username, password):
        return _ins.SDKLogin(self.sdk, username, password, byref(self.local.errno))

    def logout(self):
        return _ins.SDKLogout(self.sdk, byref(self.local.errno))

    def register(self, username, password):
        return _ins.SDKRegister(self.sdk, username, password, byref(self.local.errno))

    def get_session_id(self):
        return _ins.SDKGetSessionID(self.sdk)

    def get_current_user_id(self):
        return _ins.SDKGetCurrentUserID(self.sdk)

    def is_logged_in(self):
        return _ins.SDKIsLoggedIn(self.sdk)

    def register_session_timeout(self, callback, context):
        SessionTimeoutCallback = CFUNCTYPE(c_void_p)
        ctx = py_object(context)
        _ins.SDKRegisterSessionTimeout(self.sdk, SessionTimeoutCallback(callback), byref(ctx))

# <-- InsSDK class definition ends here

class ScanResult:
    def __init__(self, res_ptr):
        self.scanner = res_ptr

    def __del__(self):
        if _ins != None:
            _ins.DeleteScanResult(self.scanner)

    def __iter__(self):
        return self

    def done(self):
        return _ins.ScanResultDone(self.scanner)

    def error(self):
        return SDKError[_ins.ScanResultError(self.scanner)]

    def key(self):
        return _ins.ScanResultKey(self.scanner)

    def value(self):
        return _ins.ScanResultValue(self.scanner)

    def pair(self):
        return self.key(), self.value()

    def next(self):
        if self.done():
            raise StopIteration()
        else:
            key, value = self.pair()
            _ins.ScanResultNext(self.scanner)
            return key, value

# <-- ScanResult class definition ends here

_ins = CDLL('./libins_py.so')
def _set_function_sign():
    _ins.SDKShowCluster.argtypes = [c_void_p, POINTER(c_int)]
    _ins.SDKShowCluster.restype = c_void_p
    _ins.SDKGet.argtypes = [c_void_p, c_char_p, POINTER(c_int)]
    _ins.SDKGet.restype = c_char_p
    _ins.SDKPut.argtypes = [c_void_p, c_char_p, c_char_p, POINTER(c_int)]
    _ins.SDKPut.restype = c_bool
    _ins.SDKDelete.argtypes = [c_void_p, c_char_p, POINTER(c_int)]
    _ins.SDKDelete.restype = c_bool
    _ins.SDKScan.argtypes = [c_void_p, c_char_p, c_char_p]
    _ins.SDKScan.restype = c_void_p
    _ins.SDKWatch.argtypes = [c_void_p, c_char_p, c_char_p, c_void_p]
    _ins.SDKWatch.restype = c_bool
    _ins.SDKLock.argtypes = [c_void_p, c_char_p, POINTER(c_int)]
    _ins.SDKLock.restype = c_bool
    _ins.SDKTryLock.argtypes = [c_void_p, c_char_p, POINTER(c_int)]
    _ins.SDKTryLock.restype = c_bool
    _ins.SDKUnLock.argtypes = [c_void_p, c_char_p, POINTER(c_int)]
    _ins.SDKUnLock.restype = c_bool
    _ins.SDKLogin.argtypes = [c_void_p, c_char_p, c_char_p, POINTER(c_int)]
    _ins.SDKLogin.restype = c_bool
    _ins.SDKLogout.argtypes = [c_void_p, POINTER(c_int)]
    _ins.SDKLogout.restype = c_bool
    _ins.SDKRegister.argtypes = [c_void_p, c_char_p, c_char_p, POINTER(c_int)]
    _ins.SDKRegister.restype = c_bool
    _ins.SDKGetSessionID.argtypes = [c_void_p]
    _ins.SDKGetSessionID.restype = c_char_p
    _ins.SDKGetCurrentUserID.argtypes = [c_void_p]
    _ins.SDKGetCurrentUserID.restype = c_char_p
    _ins.SDKIsLoggedIn.argtypes = [c_void_p]
    _ins.SDKIsLoggedIn.restype = c_bool
    _ins.SDKRegisterSessionTimeout.argtypes = [c_void_p, CFUNCTYPE(c_void_p), c_void_p]
    _ins.ScanResultDone.argtypes = [c_void_p]
    _ins.ScanResultDone.restype = c_bool
    _ins.ScanResultError.argtypes = [c_void_p]
    _ins.ScanResultError.restype = c_int
    _ins.ScanResultKey.argtypes = [c_void_p]
    _ins.ScanResultKey.restype = c_char_p
    _ins.ScanResultValue.argtypes = [c_void_p]
    _ins.ScanResultValue.restype = c_char_p
_set_function_sign()

g_done = False
def default_callback(error):
    print 'status: %s' % SDKError[error]
    #print 'key: %s' % param.key
    #print 'value: %s' % param.value
    #print 'deleted: %s' % param.deleted
    #print 'context: %s' % param.context
    g_done = True

if __name__ == '__main__':
    FLAG_members = 'john-ubuntu:8868,john-ubuntu:8869,john-ubuntu:8870,john-ubuntu:8871,john-ubuntu:8872'
    result = sdk.scan('','')
    while not result.done():
        print result.key(), result.value()
        result.next()
    sdk.watch('test', default_callback, 'context')
    while not g_done:
        time.sleep(1)

