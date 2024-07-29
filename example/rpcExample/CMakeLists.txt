set (RpcProvider_LIST friendService.cpp friend.pb.cc)
set (RpcCaller_LIST callFriendService.cpp friend.pb.cc)

add_executable(PrcProvider ${RpcProvider_LIST})
add_executable(RpcConsumer ${RpcCaller_LIST})

target_link_libraries(PrcProvider skip_list_on_raft protobuf pthread boost_serialization)
target_link_libraries(RpcConsumer skip_list_on_raft protobuf pthread boost_serialization)