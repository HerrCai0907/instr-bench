#pragma once

#include "machine_code.hpp"
#include "multiple_thread_queue.hpp"
#include "statistic.hpp"
#include "uuid.hpp"

namespace ib::rt {

class Executor {
  MultipleThreadQueue<MachineCode> &machine_code_queue_;
  MultipleThreadQueue<UUID> &cancel_queue_;
  MultipleThreadQueue<Sample> &statistic_queue_;

public:
  explicit Executor(MultipleThreadQueue<MachineCode> &queue,
                    MultipleThreadQueue<UUID> &cancel_queue,
                    MultipleThreadQueue<Sample> &statQueue)
      : machine_code_queue_(queue), cancel_queue_(cancel_queue),
        statistic_queue_(statQueue) {}

  void start();
};

} // namespace ib::rt
