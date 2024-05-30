# raft 层与 kvDB 层之间的交互

在 raft 执行 init初始化时，起的t3线程执行 Raft::applierTicker 负责 raft 层与 kvDB 层之间的交互

```C++
std::thread t3(&Raft::applierTicker, this); 
```

```C++
/**
 * @brief 定期向状态机写入日志，将已提交但未应用的日志应用，加入到 applyChan
 */
void Raft::applierTicker() {
    while (true) {
        m_mtx.lock();
        if (m_status == Leader) {
        }
        auto applyMsgs = getApplyLogs();
        m_mtx.unlock();
        if (!applyMsgs.empty()) {
        }
        for (auto& message : applyMsgs) {
            applyChan->Push(message);
        }
        sleepNMilliseconds(ApplyInterval);
    }
}

/**
 * @brief 返回待提交的日志信息
*/
std::vector<ApplyMsg> Raft::getApplyLogs() {
    std::vector<ApplyMsg> applyMsgs;
    myAssert(m_commitIndex <= getLastLogIndex(),format(""));
    while (m_lastApplied < m_commitIndex) {
        m_lastApplied++;
        myAssert(m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].logindex() == m_lastApplied,format("",));
        ApplyMsg applyMsg;
        applyMsg.CommandValid = true;
        applyMsg.SnapshotValid = false;
        applyMsg.Command = m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].command();
        applyMsg.CommandIndex = m_lastApplied;
        applyMsgs.emplace_back(applyMsg);
    }
    return applyMsgs;
}
```
