# raftKvDB -----------------------------------------------------------------------------------
set(SRC_LIST1 raftCoreExample/raftKvDB.cpp)
add_executable(raftCoreRun ${SRC_LIST1})
target_link_libraries(raftCoreRun skip_list_on_raft sylar dl protobuf zookeeper_mt pthread yaml-cpp boost_serialization)
# raft caller -----------------------------------------------------------------------------------
set(SRC_LIST2 raftCoreExample/caller.cpp)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/raftClerk/ SRC_RAFT_CLERK)
add_executable(callerMain ${SRC_RAFT_CLERK} ${SRC_LIST2})
target_link_libraries(callerMain skip_list_on_raft sylar dl protobuf zookeeper_mt pthread yaml-cpp boost_serialization)
# threadpool -----------------------------------------------------------------------------------
set(SRC_LIST3 threadPool/test_threadPool.cpp)
add_executable(test_threadPoll ${SRC_LIST3})
target_link_libraries(test_threadPoll pthread)