//
// File created on: 2024/03/25
// Author: Zizhou

#include "raft_log.h"
#include <spdlog/spdlog.h>

namespace RR::raft {
static auto Logger = GetLoggerInstance();

RaftLog::RaftLog(Persister::ptr persister, int64_t maxNextEntriesSize) {
    if (!persister) {
        SPDLOG_LOGGER_CRITICAL(Logger, "persister must not be nullptr");
        return;
    }

    // 尝试从Persister对象中加载条目
    auto opt = persister->loadEntries();

    // 检查是否成功加载条目
    if (opt.has_value()) {
        m_entries = std::move(*opt);
        m_committed = persister->loadHardState()->commit;
        // 将m_applied成员变量设置为第一个索引减1
        // 表示还没有任何日志条目被应用到状态机，因为此时不是从snapshot中恢复的，而是从持久化对象中加载的
        m_applied = firstIndex() - 1;
    } else {
        // 如果没有从持久化对象中加载条目，则创建一个空的日志条目
        m_entries.emplace_back();
        m_committed = 0;
        m_applied = 0;
    }

    // 将m_maxNextEntriesSize成员变量设置为传入的最大条目大小参数
    m_maxNextEntriesSize = maxNextEntriesSize;
}

int64_t RaftLog::maybeAppend(int64_t prevLogIndex, int64_t prevLogTerm, int64_t committed, std::vector<Entry>& entries) {
    // 检查给定的prevLogIndex和prevLogTerm是否与日志中的相应条目匹配
    if (matchLog(prevLogIndex, prevLogTerm)) {
        // 如果匹配，计算新条目的最后一个索引
        int64_t lastIndexOfNewEntries = prevLogIndex + entries.size();
        // 找出新条目中与现有日志冲突的第一个条目的索引
        int64_t conflictIndex = findConflict(entries);
        // 如果存在冲突
        if (conflictIndex) {
            // 检查冲突的条目是否已经被提交
            if (conflictIndex <= m_committed) {
                // 如果已经被提交，那么这是一个严重的错误，程序将记录一条错误日志并退出
                SPDLOG_LOGGER_CRITICAL(Logger, "entry {} conflict with committed entry [committed = {}]", conflictIndex, m_committed);
                exit(EXIT_FAILURE);
            } else{
                // 检查冲突的条目是否在新条目的范围内
                int64_t off = prevLogIndex +1;
                if (conflictIndex - off > static_cast<int>(entries.size())) {
                    // 如果不在范围内，那么这是一个严重的错误，程序将记录一条错误日志并退出
                    SPDLOG_LOGGER_CRITICAL(Logger, "index {}, is out of range {}", conflictIndex - off, entries.size() );
                    exit(EXIT_FAILURE);
                }
                // 如果在范围内，将冲突的条目及其后面的所有条目追加到日志中
                append(std::vector<Entry>{entries.begin() + conflictIndex - off, entries.end()});
            }
        }
        // 更新已提交的最高条目的索引
        commitTo(std::min(committed, lastIndexOfNewEntries));
        // 返回新条目的最后一个索引
        return lastIndexOfNewEntries
    }

    // 如果不匹配，返回-1
    return -1;
}

int64_t RaftLog::append(const std::vector<Entry>& entries) {
    if (entries.empty()) {
        return lastIndex();
    }

    // 获取entries的第一个条目的索引
    int64_t after = entries.front().index;
    // 如果after - 1小于已提交的最大索引，那么打印一条错误消息并退出
    // 这代表传入的entries中的前几个（最少是第一个）条目已经被提交，这是一个严重的错误
    if (after - 1 < m_committed) {
        SPDLOG_LOGGER_CRITICAL(Logger, "after({}) is out of range [committed({})]", after, m_committed);
        exit(EXIT_FAILURE);
        return lastIndex();
    }

    // 如果after等于当前日志的最后一个索引加1，那么直接将entries追加到日志的末尾
    if (after == lastIndex() +1) {
        m_entries.insert(m_entries.end(), entries.begin(), entries.end());
    } else if (after <=  m_entries.front().index) { // 传入的entries的第一个条目的索引小于等于当前日志的第一个条目的索引，那么直接替换整个日志
        SPDLOG_LOGGER_INFO(Logger, "replace the entries from index {}", after);
        m_entries = entries;
    } else { // 有重叠的日志，那就用最新的日志覆盖重叠的老日志
        SPDLOG_LOGGER_INFO(Logger, "truncate the entries before index {}", after);
        auto offset = after - m_entries.front().index;
        m_entries.insert(m_entries.begin() + offset, entries.begin(), entries.end());
        
    }
    return lastIndex();
}

void RaftLog::append(const Entry& entry) {
    m_entries.push_back(entry);
}

int64_t RaftLog::findConflict(const std::vector<Entry>& entries) {
    // 遍历entries中的每一个条目。
    for (const Entry& entry : entries) {
        if (!matchLog(entry.index, entry.term)) {
            // 如果条目的索引和任期与现有的日志不匹配
            if (entry.index <= lastIndex()) {
                // 如果条目的索引小于等于现有日志的最后一个索引，证明有同一个index的日志，但是term不同
                SPDLOG_LOGGER_CRITICAL(Logger, "found conflict at index {} [existing term: {}, conflicting term: {}]", entry.index, term(entry.index), entry.term);
            }
            return entry.index;
        }
    }
    return 0;
}

int64_t RaftLog::findConflict(int64_t prevLogIndex, int64_t prevLogTerm) {
    int64_t last = lastIndex();
    if (prevLogIndex > last) {
        // 如果prevLogIndex大于最后一个索引，那么返回最后一个索引加1
        SPDLOG_LOGGER_TRACE(Logger, "index({}) is out of range [ 0, lastIndex({})] in findConflict", prevLogIndex, last);
        return last + 1;
    }
    // 如果prevLogIndex小于等于最后一个索引，证明有同一个index的日志，但是term不同

    int64_t first = firstIndex();
    // 获取prevLogIndex对应的任期，存储在conflictTerm中
    auto conflictTerm = term(prevLogIndex);
    int64_t index = prevLogIndex - 1; // 用于保存冲突的index
    // 从prevLogIndex开始向前遍历，直到找到第一个任期不等于conflictTerm的日志
    while (index >= first && (term((index) == conflictTerm))) {
        --index;
    }
    return index;
}

std::vector<Entry> RaftLog::nextEntries() {
    int64_t offset = std::max(m_applied + 1, firstIndex());
    if (m_committed + 1 > offset) {
        return slice(offset, m_committed +1, m_maxNextEntriesSize);
    }
    return {};
}

bool RaftLog::hasNextEntries() {
    int64_t offset = std::max(m_applied +1, firstIndex());
    return m_committed +1 >offset;
}

void RaftLog::clearEntries(int64_t lastSnapshotIndex =0, int64_t lastSnapshotTerm = 0);

int64_t RaftLog::firstIndex() {
    // +1是因为，日志条目的数组的第一个位置存储的是最近一次快照中最后一条日志条目的索引
    return m_entries.front().index +1;
}

int64_t RaftLog::lastIndex() {
    return m_entries.front().index + static_cast<int64_t>(m_entries.size()) - 1;
}

int64_t RaftLog::lastTerm() {
    int64_t t = term(lastIndex());
    if (t < 0) {
        SPDLOG_LOGGER_CRITICAL(Logger, "unexpected error when getting the last term");
    }
    return t;
}

int64_t RaftLog::lastSnapshotIndex() {
    return m_entries.front().index;
}

int64_t RaftLog::lastSnapshotTerm() {
    return m_entries.front().term;
}

void RaftLog::commitTo(int64_t commit) {
    if (m_committed < commit) { // 如果当前已提交的索引m_committed小于要提交的索引commit
        if (lastIndex() < commit) { // 如果要提交的index大于日志条目数组中最大的index，则出错
            SPDLOG_LOGGER_CRITICAL(Logger, "commit({}) is out of range [lastIndex({})]. Was the raft log corrupted, truncated, or lost?", commit, lastIndex());
            return;
        }
        m_committed = commit;
    }
}

void RaftLog::appliedTo(int64_t index) {
    if (!index) {
        return;
    }

    // 如果已提交的小于要应用的index，或要应用的小于已应用的，都是错误的
    if (m_committed < index || index < m_applied) {
        SPDLOG_LOGGER_CRITICAL(Logger, "applied({}) is out of range [prevApplied({}), committed({})]", index, m_applied, m_committed);
        return ;
    }
    m_applied = index;
}

int64_t RaftLog::term(int64_t index) {
    int64_t offset = m_entries.front().index;
    if (index < offset) {
        // 如果指定的索引小于第一个索引，那么返回-1，因为此时没有对应的日志条目
        return -1;
    }
    if (index - offset >= static_cast<int64_t>(m_entries.size())) {
        // 如果指定的索引超出了日志的范围，那么返回-1，因为此时没有对应的日志条目
        return -1;
    }
    return m_entries[index - offset].term;
}

std::vector<Entry> RaftLog::entries(int64_t index) {
    if (index > lastIndex()) {
        return {};
    }
    return slice(index, lastIndex() + 1, m_maxNextEntriesSize);
}

std::vector<Entry> RaftLog::allEntries() {
    return m_entries;
}

bool RaftLog::isUpToDate(int64_t index, int64_t term) {
    return term > lastTerm() || (term == lastTerm() && index >=lastIndex());
}

bool RaftLog::matchLog(int64_t index, int64_t term) {
    int64_t t = this->term(index); // 根据index获取到对应term时，就已经判断了index是否合法
    if (t < 0) {
        return false;
    }
    return t == term;
}

bool RaftLog::maybeCommit(int64_t maxIndex, int64_t term) {
    // 只有领导人当前任期里的日志条目通过计算副本数目可以被提交
    if (maxIndex > m_committed && this->term(maxIndex) == term) {
        commitTo(maxIndex);
        return true;
    }
    return false;
}

std::vector<Entry> RaftLog::slice(int64_t low, int64_t high, int64_t maxSize) {
    mustCheckOutOfBounds(low,high - 1);
    if (maxSize != NO_LIMIT) {
        high = std::min(high, low+maxSize);
    }

    // 计算实际下标
    low -= lastSnapshotIndex();
    high -= lastSnapshotIndex();

    return std::vector<Entry>{m_entries.begin() + low, m_entries.begin() + high};
}

/**
 * @brief 检查 index 是否正确，
 * @note firstIndex <= low <= high <= lastIndex
 */
void RaftLog::mustCheckOutOfBounds(int64_t low, int64_t high) {
    if (low > high) {
        SPDLOG_LOGGER_CRITICAL(Logger, "invalid slice {} > {}", low, high);
        exit(EXIT_FAILURE);
    }
    if (low < firstIndex() || high > lastIndex()) {
        SPDLOG_LOGGER_ERROR(Logger, "slice [{}, {}] out of bound [{}, {}]", low, high, firstIndex(), lastIndex());
        exit(EXIT_FAILURE);
    }
}

Snapshot::ptr RaftLog::createSnapshot(int64_t index, const std::string& data) {
    if (index < firstIndex()) { // 结束压缩的index不能小于第一个日志index(其实是第二个日志，因为第一个日志是快照的最后一条日志)
        return nullptr;
    }
    if (index > m_committed) { // 结束压缩的index不能大于已提交的index
        SPDLOG_LOGGER_CRITICAL(Logger, "index > m_committed");
        return nullptr;
    }

    int64_t offset = m_entries.front().index;

    if (index > lastIndex()) { // 结束压缩的index不能大于最后一个日志index
        SPDLOG_LOGGER_CRITICAL(Logger, "snapshot {} is out of bound last index({})", index, lastIndex());
        return nullptr;
    }

    // 创建用于返回的快照智能指针，并赋值其成员变量
    Snapshot::ptr snapshot = std::make_shared<Snapshot>();
    snapshot->metadata.index = index;
    snapshot->metadata.term = term(index);
    snapshot->data = data;
    SPDLOG_LOGGER_DEBUG(Logger, "log [{}] starts to restore snapshot [index: {}, term: {}]",toString(), snap->metadata.index, snap->metadata.term);
    return snapshot;
}

bool RaftLog::compact(int64_t compactIndex) {
    auto offset = firstIndex();
    if (compactIndex < firstIndex()) {
        return false;
    }
    if (compactIndex>lastIndex()) {
        SPDLOG_LOGGER_CRITICAL(Logger, "snapshot {} is out of bound last index {}", compactIndex, lastIndex());
        return false;
    }

    auto index = compactIndex - offset + 1;
    m_entries.erase(m_entries.begin(),m_entries.begin() + index);
    m_entries[0].data = "";
    return true;
}

}