# add_subdirectory(raftCoreExample)

set(SRC_LIST1 raftCoreExample/raftKvDB.cpp)
add_executable(raftCoreRun ${SRC_LIST1})
target_link_libraries(raftCoreRun skip_list_on_raft sylar dl protobuf zookeeper_mt pthread yaml-cpp boost_serialization)
#-----------------------------------------------------------------------------------
set(SRC_LIST2 raftCoreExample/caller.cpp)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/raftClerk/ SRC_RAFT_CLERK)
add_executable(callerMain ${SRC_RAFT_CLERK} ${SRC_LIST2})
target_link_libraries(callerMain skip_list_on_raft sylar dl protobuf zookeeper_mt pthread yaml-cpp boost_serialization)
