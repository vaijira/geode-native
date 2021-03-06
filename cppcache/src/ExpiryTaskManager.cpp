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

#include "config.h"
#include "ExpiryTaskManager.hpp"
#include "util/Log.hpp"
#include "DistributedSystem.hpp"
#include "Assert.hpp"
#include "DistributedSystemImpl.hpp"

#if defined(_WIN32)
#include <ace/WFMO_Reactor.h>
#endif
#if defined(WITH_ACE_Select_Reactor)
#include <ace/Select_Reactor.h>
#else
#include <ace/Dev_Poll_Reactor.h>
#endif

namespace apache {
namespace geode {
namespace client {

const char* ExpiryTaskManager::NC_ETM_Thread = "NC ETM Thread";

ExpiryTaskManager::ExpiryTaskManager() : m_reactorEventLoopRunning(false) {
#if defined(_WIN32)
  m_reactor = new ACE_Reactor(
      new ACE_WFMO_Reactor(nullptr, new GF_Timer_Heap_ImmediateReset()), 1);
#elif defined(WITH_ACE_Select_Reactor)
  m_reactor = new ACE_Reactor(
      new ACE_Select_Reactor(nullptr, new GF_Timer_Heap_ImmediateReset()), 1);
#else
  m_reactor = new ACE_Reactor(
      new ACE_Dev_Poll_Reactor(nullptr, new GF_Timer_Heap_ImmediateReset()), 1);
#endif
}

long ExpiryTaskManager::scheduleExpiryTask(ACE_Event_Handler* handler,
                                           uint32_t expTime, uint32_t interval,
                                           bool cancelExistingTask) {
  LOGFINER("ExpiryTaskManager: expTime %d, interval %d, cancelExistingTask %d",
           expTime, interval, cancelExistingTask);
  if (cancelExistingTask) {
    m_reactor->cancel_timer(handler, 1);
  }

  ACE_Time_Value expTimeValue(expTime);
  ACE_Time_Value intervalValue(interval);
  return m_reactor->schedule_timer(handler, nullptr, expTimeValue,
                                   intervalValue);
}

long ExpiryTaskManager::scheduleExpiryTask(ACE_Event_Handler* handler,
                                           ACE_Time_Value expTimeValue,
                                           ACE_Time_Value intervalVal,
                                           bool cancelExistingTask) {
  if (cancelExistingTask) {
    m_reactor->cancel_timer(handler, 1);
  }

  return m_reactor->schedule_timer(handler, nullptr, expTimeValue, intervalVal);
}

int ExpiryTaskManager::resetTask(ExpiryTaskManager::id_type id, uint32_t sec) {
  ACE_Time_Value interval(sec);
  return m_reactor->reset_timer_interval(id, interval);
}

int ExpiryTaskManager::cancelTask(ExpiryTaskManager::id_type id) {
  return m_reactor->cancel_timer(id, nullptr, 0);
}

int ExpiryTaskManager::svc() {
  DistributedSystemImpl::setThreadName(NC_ETM_Thread);
  LOGFINE("ExpiryTaskManager thread is running.");
  m_reactorEventLoopRunning = true;
  m_reactor->owner(ACE_OS::thr_self());
  m_reactor->run_reactor_event_loop();
  LOGFINE("ExpiryTaskManager thread has stopped.");
  return 0;
}

void ExpiryTaskManager::stopExpiryTaskManager() {
  if (m_reactorEventLoopRunning) {
    m_reactor->end_reactor_event_loop();
    this->wait();
    GF_D_ASSERT(m_reactor->reactor_event_loop_done() > 0);
    m_reactorEventLoopRunning = false;
  }
}

void ExpiryTaskManager::begin() {
  this->activate();
  ACE_Time_Value t;
  t.msec(50);
  while (!m_reactorEventLoopRunning) {
    ACE_OS::sleep(t);
  }
}

ExpiryTaskManager::~ExpiryTaskManager() {
  stopExpiryTaskManager();
  delete m_reactor;
  m_reactor = nullptr;
}

}  // namespace client
}  // namespace geode
}  // namespace apache
