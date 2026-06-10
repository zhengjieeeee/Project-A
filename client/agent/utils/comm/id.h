#pragma once

#include <atomic>

inline std::atomic<int> agent_id{1};

inline int getNextId()
{
    return agent_id.fetch_add(1);
}


inline void setStartId(int start_id)
{
    agent_id.store(start_id);
}