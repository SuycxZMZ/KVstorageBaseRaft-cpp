#ifndef CONFIG_H
#define CONFIG_H

// const bool Debug = true;

const int debugMul = 1;  // 时间单位：time.Millisecond，不同网络环境rpc速度不同，因此需要乘以一个系数
const int HeartBeatTimeout = 30 * debugMul;  // 心跳时间一般要比选举超时小一个数量级

const int ApplyInterval = 10 * debugMul;     //

const int minRandomizedElectionTime = 300 * debugMul;  // ms
const int maxRandomizedElectionTime = 500 * debugMul;  // ms

const int CONSENSUS_TIMEOUT = 500 * debugMul;  // ms

const int CLERK_REQUEST_TIMEOUT = 500;  // 客户端请求超时时间

// 协程相关设置

const int FIBER_THREAD_NUM = 5;              // raft节点中的协程调度器中线程池的大小
const bool FIBER_USE_CALLER_THREAD = false;  // 是否使用caller_thread执行调度任务

// 推荐节点数量
const int MAX_NODE_NUM = 5;

#endif  // CONFIG_H
