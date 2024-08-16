#ifndef APPLYMSG_H
#define APPLYMSG_H
#include <string>

/**
 * @brief 应用到状态机的信息
 * 
 */
class ApplyMsg {
public:
    bool CommandValid;
    std::string Command;
    int CommandIndex;
    bool SnapshotValid;
    std::string Snapshot;
    int SnapshotTerm;
    int SnapshotIndex;

public:
    // 两个valid最开始要赋予false
    ApplyMsg()
        : CommandValid(false),
          Command(),
          CommandIndex(-1),
          SnapshotValid(false),
          SnapshotTerm(-1),
          SnapshotIndex(-1)
    {
    };
};

#endif  // APPLYMSG_H