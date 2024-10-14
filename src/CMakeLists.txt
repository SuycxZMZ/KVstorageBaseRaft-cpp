# find_package(Boost REQUIRED COMPONENTS system)
# find_package(Protobuf REQUIRED)
# find_package(gRPC CONFIG REQUIRED)

# 公共链接库
set(COMMON_LIBS Boost::system pthread gRPC::grpc++ boost_serialization)

file(GLOB KVRAFT_SRC_LIST
    # 列出所有需要编译的源文件
    rpc/*.cc  
    raftCore/*.cc 
    raftRpcPro/*.cc 
    common/*.cc 
)

add_library(skip_list_on_raft SHARED ${KVRAFT_SRC_LIST})
set(ALL_LIBS
  skip_list_on_raft
  ${COMMON_LIBS}
)
target_link_libraries(skip_list_on_raft ${COMMON_LIBS})

