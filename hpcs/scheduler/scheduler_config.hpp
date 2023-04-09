/***************************
@Author: Xhosa-LEE
@Contact: lixiaoxmm@163.com
@Time: 2023/04/10
***************************/
#pragma once
#include <errno.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/singleton.hpp"
#include "croutine.hpp"
#include "nlohmann/json.hpp"
#include "scheduler/scheduler.hpp"
namespace XEngine {
#define DEFAULT_GROUP_NAME "default_group_name"
enum class SchedulingPolicy {
  PRIORITY,
};

enum class ThreadPolicy {
  THREAD_SCHED_OTHER = SCHED_OTHER,
  THREAD_SCHED_FIFO = SCHED_FIFO,
  THREAD_SCHED_RR = SCHED_RR,
};
struct ThreadAttribute {
  // std::string name{};
  std::vector<int32_t> cpuset{};
  ThreadPolicy policy;
  int32_t priority{};
};

struct TaskInfo {
  // std::string name{};
  std::string GroupName{};
  int32_t priority{};
};

struct CoroutineGroup {
  /* 可配置协程组，用来配置协程调度 */
  // std::string name{};
  ThreadAttribute attr{};
  int32_t processor_num{};
};

class SchedulerConfig {
 public:
  ~SchedulerConfig();
  SchedulerConfig() = default;

  /* 自定义配置文件,初始化Scheduler之前使用 */
  bool SetConfigPath(const std::string& path) {
    if (std::filesystem::exists(path)) [[likely]] {
        std::fstream file(path, std::ios::in | std::ios::binary);
        if (file.is_open()) [[likely]] {
            config_ = nlohmann::json::parse(
                std::string(std::istreambuf_iterator<char>(file),
                            std::istreambuf_iterator<char>()));

            return true;
          }
      }
    return false;
  }
  /* 获取scheduler当前的调度策略 */
  SchedulingPolicy GetPolicy() { return SchedulingPolicy::PRIORITY; }

  /* 获取线程的配置属性 */
  std::map<std::string, ThreadAttribute> GetThreadAttr() {
    // thread attr
    return {};
  }

  /* 获取主进程的CPU亲和性设置参数 */
  std::vector<int32_t> GetProcessAffinityCpuset() {
    // Process CPU affinity
    std::vector<int32_t> cpuset{};
    for (int i = 0; i < std::thread::hardware_concurrency(); ++i) {
      cpuset.emplace_back(i);
    }
    return cpuset;
  }

  /* 获取协程组的配置属性 */
  std::map<std::string, CoroutineGroup> GetCoroutineGroup() {
    // thread attr
    CoroutineGroup info{};
    for (int i = 0; i < std::thread::hardware_concurrency(); ++i) {
      info.attr.cpuset.emplace_back(i);
    }
    info.attr.policy = ThreadPolicy::THREAD_SCHED_FIFO;
    info.attr.priority = 1;
    info.processor_num = std::thread::hardware_concurrency();
    return {{ProcessGroupName(), info}};
  }

  /* 获取协程任务的配置属性 */
  std::map<std::string, TaskInfo> GetTask() {
    // thread attr
    TaskInfo task{};
    task.GroupName = ProcessGroupName();
    task.priority = 10;
    return {{"test_task", task}};
  }

  /* 线程池分配的线程数 */
  int32_t TaskPoolSize() { return std::thread::hardware_concurrency(); }

  /* 批量设置亲和性接口 */
  static bool AffinityCpuset(const pthread_t& thread,
                             const std::vector<int32_t>& cpus) {
    if (cpus.empty()) [[unlikely]] {
        return true;
      }
    cpu_set_t available_cpus;
    if (sched_getaffinity(getpid(), sizeof(available_cpus), &available_cpus) ==
        -1)
      [[unlikely]] {
        ERROR << "Failed to get affinity:\n";
        return false;
      }
    int num_cpus = sysconf(_SC_NPROCESSORS_CONF);
    for (int32_t cpu : cpus) {
      if (cpu >= num_cpus || !CPU_ISSET(cpu, &available_cpus)) [[unlikely]] {
          continue;
        }
      cpu_set_t set;
      CPU_ZERO(&set);
      CPU_SET(cpu, &set);
      int err_code = pthread_setaffinity_np(thread, sizeof(set), &set);
      if (err_code != 0) [[unlikely]] {
          ERROR << "Failed to set thread. errno:" << strerror(err_code) << "\n";
          return false;
        }
    }
    return true;
  }
  /* 批量设置系统线程调度策略接口 */
  static bool SetSchedPolicy(const pthread_t& thread,
                             const ThreadPolicy& policy, int32_t priority,
                             pid_t tid) {
    if (policy == ThreadPolicy::THREAD_SCHED_OTHER) {
      setpriority(PRIO_PROCESS, tid, priority);
    } else {
      int policy_in;
      struct sched_param param {};
      int err_code = pthread_getschedparam(thread, &policy_in, &param);
      if (err_code != 0) [[unlikely]] {
          ERROR << "Failed to get schedparam. errno: " << strerror(err_code)
                << "\n";
          return false;
        }
      policy_in = static_cast<int>(policy);
      param.sched_priority = priority;
      std::cout << priority << std::endl;
      err_code = pthread_setschedparam(thread, policy_in, &param);
      if (err_code != 0) [[unlikely]] {
          // 注意需要权限运行程序才能成功,wsl不支持
          ERROR << "Failed to set thread. errno: " << strerror(err_code)
                << "\n";
          return false;
        }
    }
    return true;
  }
  /* 批量设置线程属性接口 */
  static bool SetThreadAttr(const std::string& name, const pthread_t& thread,
                            const ThreadPolicy& policy, int32_t priority,
                            pid_t tid, const std::vector<int32_t>& cpus) {
    pthread_setname_np(tid, name.c_str());
    /* set affinity */
    if (!AffinityCpuset(thread, cpus)) [[unlikely]] {
        ERROR << "Failed to AffinityCpuset. \n";
        return false;
      }
    /* set policy*/
    if (!SetSchedPolicy(thread, policy, priority, tid)) [[unlikely]] {
        ERROR << "Failed to set Policy.\n";
        return false;
      }
    return true;
  }

  void SetProcessGroupName(const std::string& process_group) {
    process_group_ = process_group;
  }
  [[nodiscard]] const std::string& ProcessGroupName() const {
    return process_group_;
  }
  std::string process_group_{DEFAULT_GROUP_NAME};

  nlohmann::json config_{};
  DECLARE_SINGLETON(SchedulerConfig)
};

}  // namespace XEngine
