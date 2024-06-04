# set(SRC_LIST raftKvDB.cpp)

# add_executable(raftCoreRun ${SRC_LIST})
# target_link_libraries(raftCoreRun skip_list_on_raft rpc_lib protobuf pthread)

#-----------------------------------------------------------------------------------

set(SRC_LIST2 caller.cpp)
# set(SRC_RAFT_CLERK ${PROJECT_SOURCE_DIR}/src/raftClerk/*.cpp)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/raftClerk/ SRC_RAFT_CLERK)
# add_executable(callerMain ${src_raftClerk} ${SRC_LIST2}  ${src_common})
add_executable(callerMain ${SRC_RAFT_CLERK} ${SRC_LIST2})

target_link_libraries(callerMain skip_list_on_raft protobuf boost_serialization)