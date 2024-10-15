# raftKvDB -----------------------------------------------------------------------------------
add_executable(raftCoreRun raftCoreExample/raftKvDB.cpp)
target_link_libraries(raftCoreRun skip_list_on_raft Boost::system pthread gRPC::grpc++ boost_serialization)

# raft caller -----------------------------------------------------------------------------------
aux_source_directory(${PROJECT_SOURCE_DIR}/src/raftClerk/ SRC_RAFT_CLERK)
add_executable(callerMain raftCoreExample/caller.cpp ${SRC_RAFT_CLERK})
target_link_libraries(callerMain skip_list_on_raft Boost::system pthread gRPC::grpc++ boost_serialization)

add_executable(spdlogExample spdlogExample/spdlogExample.cc)
target_link_libraries(spdlogExample pthread)