#include "src/log.h"
#include "src/thread.h"
#include <memory>
#include <unistd.h>
#include <vector>
auto g_logger = GET_ROOT_LOGGER();

void fn_1() {
    LOG_FMT_DEBUG(
        g_logger,
        "当前线程 id = %d/%d, 当前线程名 = %s",
        zjl::getThreadID(),
        zjl::Thread::GetThis()->getId(),
        zjl::Thread::GetThisThreadName().c_str());
}
void fn_2() {
}
// 测试线程创建
void TEST_createThread() {
    LOG_DEBUG(g_logger, "Call TEST_createThread() 测试线程创建");
    std::vector<zjl::Thread::ptr> thread_list;
    for (size_t i = 0; i < 5; ++i) {
        thread_list.push_back(
            std::make_shared<zjl::Thread>(&fn_1, "thread_" + std::to_string(i)));
    }
    LOG_DEBUG(g_logger, "调用 join() 启动子线程，将子线程并入主线程");
    for (auto thread : thread_list) {
        thread->join();
    }
    LOG_DEBUG(g_logger, "创建子线程，使用析构函数调用 detach() 分离子线程");
    for (size_t i = 0; i < 5; ++i) {
        std::make_unique<zjl::Thread>(&fn_1, "detach_thread_" + std::to_string(i));
    }
}

int main() {
    TEST_createThread();

    sleep(3);
    return 0;
}