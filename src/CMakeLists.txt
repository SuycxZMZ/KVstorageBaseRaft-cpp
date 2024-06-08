file(GLOB KVRAFT_SRC_LIST
    # 列出所有需要编译的源文件
    rpc/*.cc 
    fiber/*.cc 
    raftCore/*.cc 
    raftRpcPro/*.cc 
    common/*.cc 
)

add_library(skip_list_on_raft STATIC  ${KVRAFT_SRC_LIST})
target_link_libraries(skip_list_on_raft tinymuduo pthread dl)

