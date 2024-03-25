//
// File created on: 2024/03/02
// Author: Zizhou

#include "util.h"

#include <sys/time.h> // Include the header file for gettimeofday
#include <execinfo.h> // Include the header file for backtrace

#include <sstream> // Include the header file for std::stringstream

#include <libgo/libgo.h> // Include the header file that defines co::Processor
#include "spdlog/spdlog.h" // Include the header file for spdlog
#include "spdlog/sinks/stdout_color_sinks.h" // Include the header file for stdout_color_mt


namespace RR {
uint64_t GetCuurentTimeMs() {
    struct timeval tm;
    gettimeofday(&tm, 0);
    return tm.tv_sec * 1000ul + tm.tv_usec / 1000;
}

uint64_t GetCuurentTimeUs() {
    struct timeval tu;
    gettimeofday(&tu, 0);
    return tu.tv_sec * 1000000ul + tu.tv_usec;
}


static std::shared_ptr<spdlog::logger> GetLoggerInstanceUnique() {
    // 创建一个日志记录器
    auto instance = spdlog::stdout_color_mt("RR_Logger");
    // 对日志记录器进行配置
#if RR_ENABLE_DEBUGGER // 这里是不是应该写#if SPDLOG_ACTIVE_LEVEL
    instance->set_level(spdlog::level::debug);
    SPDLOG_LOGGER_ERROR(instance,"ENABLE DEBUGGER");
#endif
    return instance;
}


std::shared_ptr<spdlog::logger> GetLoggerInstance() {
    // static保证了日志记录器的唯一性
    static auto instance = GetLoggerInstanceUnique();
    return instance;
}


void BackTrace(int size, int skip, std::vector<std::string>& bt) {
    auto funcCallStackArr = static_cast<void **>(new void* [size]);
    auto funcCallStackLength = backtrace(funcCallStackArr,size);
    char** stackString = backtrace_symbols(funcCallStackArr,funcCallStackLength);

    if (stackString == nullptr) {
        SPDLOG_LOGGER_ERROR(GetLoggerInstance(),"backtrace_symbols error");
        delete[] funcCallStackArr;
        funcCallStackArr = nullptr;
        delete[] stackString;
        stackString = nullptr;
        return;
    }

    for (int i = 0;i<funcCallStackLength;++i) {
        bt.emplace_back(stackString[i]);
    }
    delete[] funcCallStackArr;
    funcCallStackArr = nullptr;
    delete[] stackString;
    stackString = nullptr;
    return;
}

/***/
std::string BackTraceToString(int size, int skip, const std::string& prefix) {
    std::vector<std::string> bt_vector;
    BackTrace(size,skip,bt_vector);

    std::stringstream ss;
    for (size_t i = 0;i<bt_vector.size();++i) {
        ss << prefix << bt_vector[i] << '\n';
    }
    return ss.str();
}

CycleTimerTocken::operator bool() {
    
}

bool CycleTimerTocken::isCancel() const {
    if (cancel) {
        return *cancel;
    }
    return true;
}



void CycleTimerTocken::stop() const {
    if (!cancel) {
        return;
    }
    *cancel = true;
}

CycleTimerTocken CycleTimer(unsigned interval_ms, std::function<void()> callback, co::Scheduler* scheduler, int times) {
    // 如果没有传入调度器，则使用当前协程所在的调度器；
    // 如果当前进程调度器获取失败，则使用全局调度器
    if (!scheduler) {
        auto sc = co::Processer::GetCurrentScheduler();
        if (sc) {
            scheduler = sc;
        }else {
            scheduler = &co_sched;
        }
    }
    
    // 创建循环令牌
    CycleTimerTocken tocken(std::make_shared<bool>(false));

    // 使用go创建一个协程，并指定协程所属的协程调度器
    go co_scheduler(scheduler) [interval_ms, times,callback,tocken] mutable {
        // 根据给定的循环次数进行循环
        while(times--) {
            // 等待给定的间隔时间
            co_sleep(interval_ms);
            // 如果令牌被取消，则直接退出协程
            if (tocken.isCancel()) {
                return;
            }
            callback();

        }
    };
    return tocken;
    
}

}


