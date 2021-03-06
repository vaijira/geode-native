#pragma once

#ifndef GEODE_FWKLIB_CLIENTTASK_H_
#define GEODE_FWKLIB_CLIENTTASK_H_

/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <atomic>
#include "fwklib/PerfFwk.hpp"
#include "fwklib/FwkObjects.hpp"
#include "config.h"

#include <string>

namespace apache {
namespace geode {
namespace client {
namespace testframework {

class ClientTask {
 private:
  std::atomic<int32_t> m_TotalIters;
  std::atomic<int32_t> m_passCount;
  std::atomic<int32_t> m_failCount;

 protected:
  bool m_Exit;
  bool m_Run;
  int32_t m_Iterations;
  int32_t m_Loop;

 public:
  explicit ClientTask(int32_t iterations = 0)
      : m_Exit(false), m_Run(true), m_Iterations(iterations), m_Loop(-1) {}

  virtual ~ClientTask() {}

  void initTask() {
    m_Exit = false;
    m_Run = true;
    m_Iterations = 0;
    m_Loop = -1;
    m_TotalIters = 0;
  }

  void passed() { m_passCount++; }
  void failed() { m_failCount++; }
  int32_t getPassCount() { return m_passCount; }
  int32_t getFailCount() { return m_failCount; }

  // Defined by subclasses to implement the task functionality.
  // The id parameter is intended to be the thread id,
  // for informational purposes such as logging within doTask().
  // The return value should be the actual number of iterations performed,
  // allowing performance measurments of ops/sec.
  virtual uint32_t doTask(ACE_thread_t id) = 0;

  // Defined by subclasses to implement functionality that should be
  // performed prior to running the task. Used to remove actions that
  // should not be a part of a performance measurement,
  // but need to be performed before the task.
  // The id parameter is intended to be the thread id,
  // for informational purposes such as logging within doTask().
  //
  // This should return true, if it is OK to execute doTask() and doCleanup().
  virtual bool doSetup(ACE_thread_t id) = 0;

  // Defined by subclasses to implement functionality that should be
  // performed prior to running the task. Used to remove actions that
  // should not be a part of a performance measurement,
  // but need to be performed after the task.
  // The id parameter is intended to be the thread id,
  // for informational purposes such as logging within doTask().
  virtual void doCleanup(ACE_thread_t id) = 0;

  // Add iterations to the total.
  void addIters(int32_t iters) { m_TotalIters += iters; }

  // Get total iterations.
  int32_t getIters() { return m_TotalIters; }

  // Set the number of iterations the test should do.
  void setIterations(int32_t iterations) {
    m_Iterations = iterations;
    m_TotalIters = 0;
  }

  // Used to terminate tasks being run by threads
  // m_Run should be used to control execution loop in doTask(),
  // possibly in conjuction with m_Iterations
  void endRun() { m_Run = false; }

  // used to ask threads to exit
  bool mustExit() { return m_Exit; }
};

class ExitTask : public ClientTask {
 public:
  ExitTask() { m_Exit = true; }
  bool doSetup(ACE_thread_t id) {
    id = ACE_Thread_NULL;
    return true;
  }
  uint32_t doTask(ACE_thread_t id) {
    id = ACE_Thread_NULL;
    return 0;
  }
  void doCleanup(ACE_thread_t id) {
    id = ACE_Thread_NULL;
  }
};

class ThreadedTask : public ClientTask {
  FwkAction m_func;
  std::string m_args;

 public:
  ThreadedTask(FwkAction func, std::string args) : m_func(func), m_args(args) {}

  uint32_t doTask(ACE_thread_t id) {
    id = ACE_Thread_NULL;
    int32_t result = m_func(m_args.c_str());
    if (result != FWK_SUCCESS) {
      failed();
    } else {
      passed();
    }
    return 0;
  }

  bool doSetup(ACE_thread_t id) {
    id = ACE_Thread_NULL;
    return true;
  }
  void doCleanup(ACE_thread_t id) {
    id = ACE_Thread_NULL;
  }
};

}  // namespace testframework
}  // namespace client
}  // namespace geode
}  // namespace apache

#endif  // GEODE_FWKLIB_CLIENTTASK_H_
