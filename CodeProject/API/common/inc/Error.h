#ifndef __ERROR_H__
#define __ERROR_H__

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

typedef enum
{
    ERR_GROUP_0_GENERAL,
    ERR_GROUP_1_IO_IPC,
    ERR_GROUP_2_USER_CUSTOM,
    ERR_GROUP_CNT
} ENUM_ERR_GROUP;

typedef enum
{
    RET_SUCCESS = 0,
    RET_SKIPPED,    //operation skipped, caller decides how to handle
    RET_EXIT
} ENUM_STATUS;

#define GROUP_ITEMCNT_MAX                                 (31 - ERR_GROUP_CNT)
//--------------------------------------------lists of error type------------------------------------------------------------------------------------------------
/*
    note: Recommend call ERR_* to save error log and return error code, such as "return ERR_ARGV_1" or "ret = ERR_ARGV";
          Call ERRCODE_* only while you need compare between retval and errorcode, such as "if (ret == ERRCODE_FILE_OP_FAILED) printf("file operation failed!\r\n");"
*/
//##ERR_GROUP_0_GENERAL....................................................................................................................................
//
//function para
#define ERR_ARGC                                          ERRLOG(ERR_GROUP_0_GENERAL, 0) //return ERRCODE_ARGC
#define ERRCODE_ARGC                                      ERRCODE(ERR_GROUP_0_GENERAL, 0) //0x90000001
#define ERR_ARGV_1                                        ERRLOG(ERR_GROUP_0_GENERAL, 1) //return ERRCODE_ARGV_1
#define ERRCODE_ARGV_1                                    ERRCODE(ERR_GROUP_0_GENERAL, 1) //0x90000002
#define ERR_ARGV_2                                        ERRLOG(ERR_GROUP_0_GENERAL, 2) //return ERRCODE_ARGV_2
#define ERRCODE_ARGV_2                                    ERRCODE(ERR_GROUP_0_GENERAL, 2) //0x90000004
#define ERR_ARGV_3                                        ERRLOG(ERR_GROUP_0_GENERAL, 3) //return ERRCODE_ARGV_3
#define ERRCODE_ARGV_3                                    ERRCODE(ERR_GROUP_0_GENERAL, 3) //0x90000008
#define ERR_ARGV_4                                        ERRLOG(ERR_GROUP_0_GENERAL, 4) //return ERRCODE_ARGV_4
#define ERRCODE_ARGV_4                                    ERRCODE(ERR_GROUP_0_GENERAL, 4) //0x90000010
#define ERR_ARGV_5                                        ERRLOG(ERR_GROUP_0_GENERAL, 5) //return ERRCODE_ARGV_5
#define ERRCODE_ARGV_5                                    ERRCODE(ERR_GROUP_0_GENERAL, 5) //0x90000020
#define ERR_ARGV                                          ERRLOG(ERR_GROUP_0_GENERAL, 6) //return ERRCODE_ARGV
#define ERRCODE_ARGV                                      ERRCODE(ERR_GROUP_0_GENERAL, 6) //0x90000040
//timeout
#define ERR_TIMEOUT                                       ERRLOG(ERR_GROUP_0_GENERAL, 7) //return ERRCODE_TIMEOUT
#define ERRCODE_TIMEOUT                                   ERRCODE(ERR_GROUP_0_GENERAL, 7) //0x90000080
//file
#define ERR_FILE_NOT_FOUND                                ERRLOG(ERR_GROUP_0_GENERAL, 8) //return ERRCODE_FILE_NOT_FOUND
#define ERRCODE_FILE_NOT_FOUND                            ERRCODE(ERR_GROUP_0_GENERAL, 8) //0x90000100
#define ERR_FILE_OP_FAILED                                ERRLOG(ERR_GROUP_0_GENERAL, 9) //return ERRCODE_FILE_OP_FAILED
#define ERRCODE_FILE_OP_FAILED                            ERRCODE(ERR_GROUP_0_GENERAL, 9) //0x90000200
#define ERR_FILE_FORMAT                                   ERRLOG(ERR_GROUP_0_GENERAL, 10) //return ERRCODE_FILE_FORMAT
#define ERRCODE_FILE_FORMAT                               ERRCODE(ERR_GROUP_0_GENERAL, 10) //0x90000400
//memory
#define ERR_MEMORY_ALLOC                                  ERRLOG(ERR_GROUP_0_GENERAL, 11) //return ERRCODE_MEMORY_ALLOC
#define ERRCODE_MEMORY_ALLOC                              ERRCODE(ERR_GROUP_0_GENERAL, 11) //0x90000800
#define ERR_OVER_BUF                                      ERRLOG(ERR_GROUP_0_GENERAL, 12) //return ERRCODE_OVER_BUF
#define ERRCODE_OVER_BUF                                  ERRCODE(ERR_GROUP_0_GENERAL, 12) //0x90001000
//thread operation
#define ERR_PTHREAD_INIT                                  ERRLOG(ERR_GROUP_0_GENERAL, 13) //return ERRCODE_PTHREAD_INIT
#define ERRCODE_PTHREAD_INIT                              ERRCODE(ERR_GROUP_0_GENERAL, 13) //0x90002000
#define ERR_PTHREAD_CFG                                   ERRLOG(ERR_GROUP_0_GENERAL, 14) //return ERRCODE_PTHREAD_CFG
#define ERRCODE_PTHREAD_CFG                               ERRCODE(ERR_GROUP_0_GENERAL, 14) //0x90004000
//mutex
#define ERR_MUTEX_INIT                                    ERRLOG(ERR_GROUP_0_GENERAL, 15) //return ERRCODE_MUTEX_INIT
#define ERRCODE_MUTEX_INIT                                ERRCODE(ERR_GROUP_0_GENERAL, 15) //0x90008000
//config
#define ERR_CONFIG_PARSE_FAILED                           ERRLOG(ERR_GROUP_0_GENERAL, 16) //return ERRCODE_CONFIG_PARSE_FAILED
#define ERRCODE_CONFIG_PARSE_FAILED                       ERRCODE(ERR_GROUP_0_GENERAL, 16) //0x90010000
#define ERR_CONFIG_VALUE_INVALID                          ERRLOG(ERR_GROUP_0_GENERAL, 17) //return ERRCODE_CONFIG_VALUE_INVALID
#define ERRCODE_CONFIG_VALUE_INVALID                      ERRCODE(ERR_GROUP_0_GENERAL, 17) //0x90020000
#define ERR_UART_CONFIG_FAILED                            ERRLOG(ERR_GROUP_0_GENERAL, 18) //return ERRCODE_UART_CONFIG_FAILED
#define ERRCODE_UART_CONFIG_FAILED                        ERRCODE(ERR_GROUP_0_GENERAL, 18) //0x90040000
//sys
#define ERR_SELECT                                        ERRLOG(ERR_GROUP_0_GENERAL, 19) //return ERRCODE_SELECT
#define ERRCODE_SELECT                                    ERRCODE(ERR_GROUP_0_GENERAL, 19) //0x90080000
#define ERR_EVENT                                         ERRLOG(ERR_GROUP_0_GENERAL, 20) //return ERRCODE_EVENT
#define ERRCODE_EVENT                                     ERRCODE(ERR_GROUP_0_GENERAL, 20) //0x90100000
#define ERR_INIT_FAIL                                     ERRLOG(ERR_GROUP_0_GENERAL, 21) //return ERRCODE_INIT_FAIL
#define ERRCODE_INIT_FAIL                                 ERRCODE(ERR_GROUP_0_GENERAL, 21) //0x90200000
#define ERR_CLOCK                                         ERRLOG(ERR_GROUP_0_GENERAL, 22) //return ERRCODE_CLOCK
#define ERRCODE_CLOCK                                     ERRCODE(ERR_GROUP_0_GENERAL, 22) //0x90400000
#define ERR_EPOLL                                         ERRLOG(ERR_GROUP_0_GENERAL, 23) //return ERRCODE_EPOLL
#define ERRCODE_EPOLL                                     ERRCODE(ERR_GROUP_0_GENERAL, 23) //0x90800000
#define ERR_UDEV_OP_FAILED                                ERRLOG(ERR_GROUP_0_GENERAL, 24) //return ERRCODE_UDEV_OP_FAILED
#define ERRCODE_UDEV_OP_FAILED                            ERRCODE(ERR_GROUP_0_GENERAL, 24) //0x91000000
//data
#define ERR_ID_NOT_FOUND                                  ERRLOG(ERR_GROUP_0_GENERAL, 25) //return ERRCODE_ID_NOT_FOUND
#define ERRCODE_ID_NOT_FOUND                              ERRCODE(ERR_GROUP_0_GENERAL, 25) //0x92000000
//severity
#define ERR_RUNNABLE                                      ERRLOG(ERR_GROUP_0_GENERAL, 26) //return ERRCODE_RUNNABLE
#define ERRCODE_RUNNABLE                                  ERRCODE(ERR_GROUP_0_GENERAL, 26) //0x94000000
#define ERR_SHUT_DOWN                                     ERRLOG(ERR_GROUP_0_GENERAL, 27) //return ERRCODE_SHUT_DOWN
#define ERRCODE_SHUT_DOWN                                 ERRCODE(ERR_GROUP_0_GENERAL, 27) //0x98000000

//##ERR_GROUP_1_IO_IPC.....................................................................................................................................
//
//socket
#define ERR_SOCKET_IO                                     ERRLOG(ERR_GROUP_1_IO_IPC, 0) //return ERRCODE_SOCKET_IO
#define ERRCODE_SOCKET_IO                                 ERRCODE(ERR_GROUP_1_IO_IPC, 0) //0xA0000001
#define ERR_SOCKET_OP_FAILED                              ERRLOG(ERR_GROUP_1_IO_IPC, 1) //return ERRCODE_SOCKET_OP_FAILED
#define ERRCODE_SOCKET_OP_FAILED                          ERRCODE(ERR_GROUP_1_IO_IPC, 1) //0xA0000002
//receive and feedback freshness
#define ERR_RX_FAILED                                     ERRLOG(ERR_GROUP_1_IO_IPC, 2) //return ERRCODE_RX_FAILED
#define ERRCODE_RX_FAILED                                 ERRCODE(ERR_GROUP_1_IO_IPC, 2) //0xA0000004
#define ERR_NO_FRESH_FEEDBACK                             ERRLOG(ERR_GROUP_1_IO_IPC, 3) //return ERRCODE_NO_FRESH_FEEDBACK; Feedback is missing or older than the required freshness limit.
#define ERRCODE_NO_FRESH_FEEDBACK                         ERRCODE(ERR_GROUP_1_IO_IPC, 3) //0xA0000008
//eventfd
#define ERR_EVENTFD_CREATE_FAILED                         ERRLOG(ERR_GROUP_1_IO_IPC, 4) //return ERRCODE_EVENTFD_CREATE_FAILED
#define ERRCODE_EVENTFD_CREATE_FAILED                     ERRCODE(ERR_GROUP_1_IO_IPC, 4) //0xA0000010
#define ERR_EVENTFD_IO_FAILED                             ERRLOG(ERR_GROUP_1_IO_IPC, 5) //return ERRCODE_EVENTFD_IO_FAILED
#define ERRCODE_EVENTFD_IO_FAILED                         ERRCODE(ERR_GROUP_1_IO_IPC, 5) //0xA0000020
//semaphore
#define ERR_SEM_OP_FAILED                                 ERRLOG(ERR_GROUP_1_IO_IPC, 6) //return ERRCODE_SEM_OP_FAILED
#define ERRCODE_SEM_OP_FAILED                             ERRCODE(ERR_GROUP_1_IO_IPC, 6) //0xA0000040
//common ring
#define ERR_COMM_RING_EMPTY                               ERRLOG(ERR_GROUP_1_IO_IPC, 7) //return ERRCODE_COMM_RING_EMPTY
#define ERRCODE_COMM_RING_EMPTY                           ERRCODE(ERR_GROUP_1_IO_IPC, 7) //0xA0000080
#define ERR_COMM_RING_FULL                                ERRLOG(ERR_GROUP_1_IO_IPC, 8) //return ERRCODE_COMM_RING_FULL
#define ERRCODE_COMM_RING_FULL                            ERRCODE(ERR_GROUP_1_IO_IPC, 8) //0xA0000100
#define ERR_COMM_RING_ELEM_WR_BUSY                        ERRLOG(ERR_GROUP_1_IO_IPC, 9) //return ERRCODE_COMM_RING_ELEM_WR_BUSY
#define ERRCODE_COMM_RING_ELEM_WR_BUSY                    ERRCODE(ERR_GROUP_1_IO_IPC, 9) //0xA0000200
#define ERR_COMM_RING_ELEM_MISSED                         ERRLOG(ERR_GROUP_1_IO_IPC, 10) //return ERRCODE_COMM_RING_ELEM_MISSED
#define ERRCODE_COMM_RING_ELEM_MISSED                     ERRCODE(ERR_GROUP_1_IO_IPC, 10) //0xA0000400
#define ERR_COMM_RING_OP_FAIL                             ERRLOG(ERR_GROUP_1_IO_IPC, 11) //return ERRCODE_COMM_RING_OP_FAIL
#define ERRCODE_COMM_RING_OP_FAIL                         ERRCODE(ERR_GROUP_1_IO_IPC, 11) //0xA0000800
//network setup
#define ERR_NETWORK_INTERFACE_INVALID                     ERRLOG(ERR_GROUP_1_IO_IPC, 12) //return ERRCODE_NETWORK_INTERFACE_INVALID; Configured network interface is missing or unsuitable for SDK communication.
#define ERRCODE_NETWORK_INTERFACE_INVALID                 ERRCODE(ERR_GROUP_1_IO_IPC, 12) //0xA0001000
#define ERR_MULTICAST_JOIN_FAILED                         ERRLOG(ERR_GROUP_1_IO_IPC, 13) //return ERRCODE_MULTICAST_JOIN_FAILED; Failed to join the required multicast group.
#define ERRCODE_MULTICAST_JOIN_FAILED                     ERRCODE(ERR_GROUP_1_IO_IPC, 13) //0xA0002000
//DDS
#define ERR_DDS_INIT_FAILED                               ERRLOG(ERR_GROUP_1_IO_IPC, 14) //return ERRCODE_DDS_INIT_FAILED; Failed to initialize DDS resources or create the domain participant.
#define ERRCODE_DDS_INIT_FAILED                           ERRCODE(ERR_GROUP_1_IO_IPC, 14) //0xA0004000
#define ERR_DDS_TOPIC_CREATE_FAILED                       ERRLOG(ERR_GROUP_1_IO_IPC, 15) //return ERRCODE_DDS_TOPIC_CREATE_FAILED; Failed to create or register a DDS topic.
#define ERRCODE_DDS_TOPIC_CREATE_FAILED                   ERRCODE(ERR_GROUP_1_IO_IPC, 15) //0xA0008000
#define ERR_DDS_ENDPOINT_CREATE_FAILED                    ERRLOG(ERR_GROUP_1_IO_IPC, 16) //return ERRCODE_DDS_ENDPOINT_CREATE_FAILED; Failed to create a DDS publisher/subscriber or DataWriter/DataReader.
#define ERRCODE_DDS_ENDPOINT_CREATE_FAILED                ERRCODE(ERR_GROUP_1_IO_IPC, 16) //0xA0010000
#define ERR_DDS_DISCOVERY_TIMEOUT                         ERRLOG(ERR_GROUP_1_IO_IPC, 17) //return ERRCODE_DDS_DISCOVERY_TIMEOUT; Required robot DDS reader/writer discovery did not complete before the deadline.
#define ERRCODE_DDS_DISCOVERY_TIMEOUT                     ERRCODE(ERR_GROUP_1_IO_IPC, 17) //0xA0020000
#define ERR_DDS_PUBLISH_FAILED                            ERRLOG(ERR_GROUP_1_IO_IPC, 18) //return ERRCODE_DDS_PUBLISH_FAILED; Failed to write a DDS sample to the transport.
#define ERRCODE_DDS_PUBLISH_FAILED                        ERRCODE(ERR_GROUP_1_IO_IPC, 18) //0xA0040000
#define ERR_DDS_TAKE_FAILED                               ERRLOG(ERR_GROUP_1_IO_IPC, 19) //return ERRCODE_DDS_TAKE_FAILED; Failed to take DDS samples from a DataReader.
#define ERRCODE_DDS_TAKE_FAILED                           ERRCODE(ERR_GROUP_1_IO_IPC, 19) //0xA0080000
#define ERR_DDS_QOS_INCOMPATIBLE                          ERRLOG(ERR_GROUP_1_IO_IPC, 20) //return ERRCODE_DDS_QOS_INCOMPATIBLE; DDS endpoints have incompatible QoS settings.
#define ERRCODE_DDS_QOS_INCOMPATIBLE                      ERRCODE(ERR_GROUP_1_IO_IPC, 20) //0xA0100000
//network/media packet
#define ERR_DATA_PACKET_FORMAT                            ERRLOG(ERR_GROUP_1_IO_IPC, 21) //return ERRCODE_DATA_PACKET_FORMAT; Network/media packet framing or payload format is invalid.
#define ERRCODE_DATA_PACKET_FORMAT                        ERRCODE(ERR_GROUP_1_IO_IPC, 21) //0xA0200000
#define ERR_DDS_ENTITY_DELETE_FAILED                      ERRLOG(ERR_GROUP_1_IO_IPC, 22) //return ERRCODE_DDS_ENTITY_DELETE_FAILED; Failed to delete a DDS entity.
#define ERRCODE_DDS_ENTITY_DELETE_FAILED                  ERRCODE(ERR_GROUP_1_IO_IPC, 22) //0xA0400000
#define ERR_MUTEX_OP_FAILED                               ERRLOG(ERR_GROUP_1_IO_IPC, 23) //return ERRCODE_MUTEX_OP_FAILED; Failed to operate a mutex.
#define ERRCODE_MUTEX_OP_FAILED                           ERRCODE(ERR_GROUP_1_IO_IPC, 23) //0xA0800000

//##ERR_GROUP_2_USER_CUSTOM................................................................................................................................
//
//SDK lifecycle and robot connection
#define ERR_SDK_NOT_READY                                 ERRLOG(ERR_GROUP_2_USER_CUSTOM, 0) //return ERRCODE_SDK_NOT_READY; SDK initialization/start prerequisites have not been met.
#define ERRCODE_SDK_NOT_READY                             ERRCODE(ERR_GROUP_2_USER_CUSTOM, 0) //0xC0000001
#define ERR_STATE_TRANSITION_INVALID                      ERRLOG(ERR_GROUP_2_USER_CUSTOM, 1) //return ERRCODE_STATE_TRANSITION_INVALID; Requested SDK/task state transition is invalid.
#define ERRCODE_STATE_TRANSITION_INVALID                  ERRCODE(ERR_GROUP_2_USER_CUSTOM, 1) //0xC0000002
#define ERR_FEATURE_UNAVAILABLE                           ERRLOG(ERR_GROUP_2_USER_CUSTOM, 2) //return ERRCODE_FEATURE_UNAVAILABLE; Required SDK, robot, or bridge capability is unavailable.
#define ERRCODE_FEATURE_UNAVAILABLE                       ERRCODE(ERR_GROUP_2_USER_CUSTOM, 2) //0xC0000004
#define ERR_ROBOT_OFFLINE                                 ERRLOG(ERR_GROUP_2_USER_CUSTOM, 3) //return ERRCODE_ROBOT_OFFLINE; Robot link is offline according to connection/liveness monitoring.
#define ERRCODE_ROBOT_OFFLINE                             ERRCODE(ERR_GROUP_2_USER_CUSTOM, 3) //0xC0000008
//resource and request
#define ERR_RESOURCE_BUSY                                 ERRLOG(ERR_GROUP_2_USER_CUSTOM, 4) //return ERRCODE_RESOURCE_BUSY; Requested resource is already in use.
#define ERRCODE_RESOURCE_BUSY                             ERRCODE(ERR_GROUP_2_USER_CUSTOM, 4) //0xC0000010
#define ERR_REQUEST_TIMEOUT                               ERRLOG(ERR_GROUP_2_USER_CUSTOM, 5) //return ERRCODE_REQUEST_TIMEOUT; Pending request did not complete before its deadline.
#define ERRCODE_REQUEST_TIMEOUT                           ERRCODE(ERR_GROUP_2_USER_CUSTOM, 5) //0xC0000020
#define ERR_REQUEST_REJECTED                              ERRLOG(ERR_GROUP_2_USER_CUSTOM, 6) //return ERRCODE_REQUEST_REJECTED; Bridge, arbitration, or robot explicitly rejected the request.
#define ERRCODE_REQUEST_REJECTED                          ERRCODE(ERR_GROUP_2_USER_CUSTOM, 6) //0xC0000040
#define ERR_REQUEST_CANCELLED                             ERRLOG(ERR_GROUP_2_USER_CUSTOM, 7) //return ERRCODE_REQUEST_CANCELLED; Pending request was cancelled.
#define ERRCODE_REQUEST_CANCELLED                         ERRCODE(ERR_GROUP_2_USER_CUSTOM, 7) //0xC0000080
//topic and subscription
#define ERR_TOPIC_NOT_FOUND                               ERRLOG(ERR_GROUP_2_USER_CUSTOM, 8) //return ERRCODE_TOPIC_NOT_FOUND; Requested topic is absent from the SDK topic registry.
#define ERRCODE_TOPIC_NOT_FOUND                           ERRCODE(ERR_GROUP_2_USER_CUSTOM, 8) //0xC0000100
#define ERR_TOPIC_DIRECTION_INVALID                       ERRLOG(ERR_GROUP_2_USER_CUSTOM, 9) //return ERRCODE_TOPIC_DIRECTION_INVALID; Requested publish/subscribe operation conflicts with the topic direction.
#define ERRCODE_TOPIC_DIRECTION_INVALID                   ERRCODE(ERR_GROUP_2_USER_CUSTOM, 9) //0xC0000200
#define ERR_SUBSCRIPTION_NOT_FOUND                        ERRLOG(ERR_GROUP_2_USER_CUSTOM, 10) //return ERRCODE_SUBSCRIPTION_NOT_FOUND; Requested SDK subscription does not exist.
#define ERRCODE_SUBSCRIPTION_NOT_FOUND                    ERRCODE(ERR_GROUP_2_USER_CUSTOM, 10) //0xC0000400
//head/motion command
#define ERR_TARGET_OUT_OF_RANGE                           ERRLOG(ERR_GROUP_2_USER_CUSTOM, 11) //return ERRCODE_TARGET_OUT_OF_RANGE; Requested head/motion target exceeds its allowed range.
#define ERRCODE_TARGET_OUT_OF_RANGE                       ERRCODE(ERR_GROUP_2_USER_CUSTOM, 11) //0xC0000800
#define ERR_TARGET_REJECTED_BY_STATE                      ERRLOG(ERR_GROUP_2_USER_CUSTOM, 12) //return ERRCODE_TARGET_REJECTED_BY_STATE; Head/motion command is disallowed by the current robot/task state.
#define ERRCODE_TARGET_REJECTED_BY_STATE                  ERRCODE(ERR_GROUP_2_USER_CUSTOM, 12) //0xC0001000
#define ERR_MOTION_DATA_INVALID                           ERRLOG(ERR_GROUP_2_USER_CUSTOM, 13) //return ERRCODE_MOTION_DATA_INVALID; Motion/servo data has invalid fields or an inconsistent layout.
#define ERRCODE_MOTION_DATA_INVALID                       ERRCODE(ERR_GROUP_2_USER_CUSTOM, 13) //0xC0002000
//media resource and timeline
#define ERR_MEDIA_FORMAT_UNSUPPORTED                      ERRLOG(ERR_GROUP_2_USER_CUSTOM, 14) //return ERRCODE_MEDIA_FORMAT_UNSUPPORTED; Requested audio/video format or codec is unsupported.
#define ERRCODE_MEDIA_FORMAT_UNSUPPORTED                  ERRCODE(ERR_GROUP_2_USER_CUSTOM, 14) //0xC0004000
#define ERR_RESOURCE_NOT_FOUND                            ERRLOG(ERR_GROUP_2_USER_CUSTOM, 15) //return ERRCODE_RESOURCE_NOT_FOUND; Requested action, expression, or media asset was not found.
#define ERRCODE_RESOURCE_NOT_FOUND                        ERRCODE(ERR_GROUP_2_USER_CUSTOM, 15) //0xC0008000
#define ERR_TIMELINE_INVALID                              ERRLOG(ERR_GROUP_2_USER_CUSTOM, 16) //return ERRCODE_TIMELINE_INVALID; Playback/task timeline has invalid timestamps or ordering.
#define ERRCODE_TIMELINE_INVALID                          ERRCODE(ERR_GROUP_2_USER_CUSTOM, 16) //0xC0010000
//task, data integrity, and synchronization
#define ERR_TASK_NOT_FOUND                                ERRLOG(ERR_GROUP_2_USER_CUSTOM, 17) //return ERRCODE_TASK_NOT_FOUND; Requested SDK task does not exist.
#define ERRCODE_TASK_NOT_FOUND                            ERRCODE(ERR_GROUP_2_USER_CUSTOM, 17) //0xC0020000
#define ERR_DATA_INCOMPLETE                               ERRLOG(ERR_GROUP_2_USER_CUSTOM, 18) //return ERRCODE_DATA_INCOMPLETE; Known frame loss or truncation made the requested data incomplete.
#define ERRCODE_DATA_INCOMPLETE                           ERRCODE(ERR_GROUP_2_USER_CUSTOM, 18) //0xC0040000
#define ERR_TIMEBASE_UNAVAILABLE                          ERRLOG(ERR_GROUP_2_USER_CUSTOM, 19) //return ERRCODE_TIMEBASE_UNAVAILABLE; Shared device timebase required for synchronized tasks is unavailable.
#define ERRCODE_TIMEBASE_UNAVAILABLE                      ERRCODE(ERR_GROUP_2_USER_CUSTOM, 19) //0xC0080000
//audio/video channel
#define ERR_AUDIO_DEVICE_NOT_FOUND                        ERRLOG(ERR_GROUP_2_USER_CUSTOM, 20) //return ERRCODE_AUDIO_DEVICE_NOT_FOUND; Requested local audio capture/playback device was not found.
#define ERRCODE_AUDIO_DEVICE_NOT_FOUND                    ERRCODE(ERR_GROUP_2_USER_CUSTOM, 20) //0xC0100000
#define ERR_MEDIA_CHANNEL_NOT_READY                       ERRLOG(ERR_GROUP_2_USER_CUSTOM, 21) //return ERRCODE_MEDIA_CHANNEL_NOT_READY; Media stream/session is not ready for data transfer.
#define ERRCODE_MEDIA_CHANNEL_NOT_READY                   ERRCODE(ERR_GROUP_2_USER_CUSTOM, 21) //0xC0200000

//Prohibit below function directly, user can only use ERR_ or ERRCODE_ Marco.........................................................
//
#define ERRCODE(group, item)      ( (int)((1U << (GROUP_ITEMCNT_MAX + ERR_GROUP_CNT)) | ((1 << ((group) % ERR_GROUP_CNT)) << GROUP_ITEMCNT_MAX) | (1 << ((item) % GROUP_ITEMCNT_MAX))) )
//"Prohibit" user to call ERRLOG funciton directly, user can only use ERR_ or ERRCODE_ Marco.
#define ERRLOG(group, item)       SaveErrLog(__FUNCTION__, __LINE__, group, item)
//"Prohibit" user to call SaveErrLog funciton directly, user can only use ERR_ or ERRCODE_ Marco.
int SaveErrLog(const char *funcName, int errLine, ENUM_ERR_GROUP group, unsigned int item);

/* This overload-style macro was discarded because it only works when int FuncRetDbgPrint(int funcret, const char *funcName, ...) exists, but callers may directly call FuncRetDbgPrint(ERR_ARGV_1, __FUNCTION__).
#define __ARGC(...)	__ARGNO10(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define __ARGNO10(_1, _2, _3, _4, _5, _6, _7, _8, _9, _N, ...) ( _N==1 ? (#_1)[0]!=0 : _N )
#define FUNC_RET_PRINT(funcret, funcName, ...) ( __ARGC(__VA_ARGS__) ? FuncRetDbgPrint(funcret, funcName, ##__VA_ARGS__) : FunctRetPrint(funcret, funcName) )
*/

#if defined(__GNUC__)
#define PRINTF_FORMAT_CHECK(fmtIdx, argIdx) __attribute__((format(printf, fmtIdx, argIdx)))
#else
#define PRINTF_FORMAT_CHECK(fmtIdx, argIdx)
#endif

#define PRINT_FUNCTION_ERRINFO_PICK(_ret, _func, _fmt, _4, _5, _6, _7, _8, _9, _10, _11, _12, PRINT_FUNC, ...) PRINT_FUNC
/*
    Use these helpers in functions returning pointers, void, business enums or counts instead of ERR_* / ERRCODE_* values.
    Functions returning error codes should return new ERR_* values directly or forward existing error codes without logging again.
    ERR_* arguments record errors; ERRCODE_* and runtime error-code arguments do not add records.
    Pick PrintFunctionErrInfo when only (funcret, funcName) are passed;
    pick PrintFunctionErrDbgInfo when fmt/debug arguments are present.
*/
#define PRINT_FUNCTION_ERRINFO(funcret, funcName, ...)   PRINT_FUNCTION_ERRINFO_PICK(funcret, funcName, ##__VA_ARGS__, \
		                                                                                PrintFunctionErrDbgInfo, PrintFunctionErrDbgInfo, PrintFunctionErrDbgInfo, PrintFunctionErrDbgInfo, \
		                                                                                PrintFunctionErrDbgInfo, PrintFunctionErrDbgInfo, PrintFunctionErrDbgInfo, PrintFunctionErrDbgInfo, \
		                                                                                PrintFunctionErrDbgInfo, PrintFunctionErrDbgInfo, \
		                                                                                PrintFunctionErrInfo)(funcret, funcName, ##__VA_ARGS__)

int PrintFunctionErrInfo(int funcret, const char *funcName);
int PrintFunctionErrDbgInfo(int funcret, const char *funcName, const char *fmt, ...) PRINTF_FORMAT_CHECK(3, 4);



#define ERRSTACK_ITEMCNT_MAX      119
#define BITCNT_ITEM               GROUP_ITEMCNT_MAX
#define BITCNT_GROUP              ERR_GROUP_CNT

typedef struct
{
    const char *funcName;
    int errLine;
    union
    {
        int errcode;
        struct
        {
            unsigned int itemField  : BITCNT_ITEM;
            unsigned int groupField : BITCNT_GROUP;
            unsigned int sign : 1;
        };
    };
} STRUCT_ERRLIST;

typedef struct
{
    unsigned int top;
    STRUCT_ERRLIST szstErrLog[ERRSTACK_ITEMCNT_MAX];
} STACK_ERRLOG;

typedef struct
{
    unsigned int isWriting;
    unsigned int droppedErrLogCntForBusy;
    unsigned int droppedErrLogCntForFull;
    STACK_ERRLOG stack;
    pthread_mutex_t errPrintMutex;
} ERR_STACK_CTX;

int StoreAllErrLogToFile(const char *storeFilePath); //store all error logs since system power-on if ClearErrStack() has not been called
int PrintAllErrLog(void); //print all error logs since system power-on if ClearErrStack() has not been called
int Print1stErrLog(void);
int PrintLatest1ErrLog(void);
int Copy1stErrLogOfErrList(STRUCT_ERRLIST *pstErrLogCopy);
int CopyLatest1ErrLogOfErrList(STRUCT_ERRLIST *pstErrLogCopy);
unsigned int GetDroppedErrLogCntForBusy(void);
unsigned int GetDroppedErrLogCntForFull(void);
void ClearDroppedErrLogCnt(void);
int ClearErrStack(void);

#endif
