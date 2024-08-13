set(SRC_LIST1 raftKvDB.cpp)
add_executable(raftCoreRun ${SRC_LIST1})
target_link_libraries(raftCoreRun skip_list_on_raft protobuf pthread boost_serialization yaml-cpp zookeeper_mt sylar)
#-----------------------------------------------------------------------------------
set(SRC_LIST2 caller.cpp)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/raftClerk/ SRC_RAFT_CLERK)
add_executable(callerMain ${SRC_RAFT_CLERK} ${SRC_LIST2})
target_link_libraries(callerMain skip_list_on_raft protobuf pthread boost_serialization yaml-cpp zookeeper_mt sylar)