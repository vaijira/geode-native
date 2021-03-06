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
#include <geode/CacheableDate.hpp>
#include <geode/internal/CacheableKeys.hpp>
#include <geode/DataOutput.hpp>
#include <geode/DataInput.hpp>
#include <geode/ExceptionTypes.hpp>

#include <cwchar>
#include <ace/OS.h>

#include <chrono>
#include <ctime>

namespace apache {
namespace geode {
namespace client {

static CacheableDate::time_point posixEpoch =
    CacheableDate::clock::from_time_t(0);

void CacheableDate::toData(DataOutput& output) const {
  output.writeInt(m_timevalue);
}

void CacheableDate::fromData(DataInput& input) {
  m_timevalue = input.readInt64();
}

std::shared_ptr<Serializable> CacheableDate::createDeserializable() {
  return std::make_shared<CacheableDate>();
}

DSCode CacheableDate::getDsCode() const { return DSCode::CacheableDate; }

bool CacheableDate::operator==(const CacheableKey& other) const {
  if (auto otherDate = dynamic_cast<const CacheableDate*>(&other)) {
    return m_timevalue == otherDate->m_timevalue;
  }

  return false;
}

int64_t CacheableDate::milliseconds() const { return m_timevalue; }

int32_t CacheableDate::hashcode() const {
  return static_cast<int>(m_timevalue) ^ static_cast<int>(m_timevalue >> 32);
}

CacheableDate::CacheableDate(const time_t value) {
  m_timevalue = (static_cast<int64_t>(value)) * 1000;
}

CacheableDate::CacheableDate(const CacheableDate::time_point& value) {
  // Set based on time since local system clock epoch plus time since POSIX
  // epoch since local system clock epoch to get milliseconds since POSIX epoch.
  m_timevalue =
      std::chrono::duration_cast<CacheableDate::duration>(value - posixEpoch)
          .count();
}

CacheableDate::CacheableDate(const CacheableDate::duration& value) {
  m_timevalue = value.count();
}

std::string CacheableDate::toString() const {
  char buffer[25];
  struct tm date = {};
  time_t sec = m_timevalue / 1000;
  ACE_OS::localtime_r(&sec, &date);
  ACE_OS::snprintf(buffer, 24, "%d/%d/%d %d:%d:%d", date.tm_mon + 1,
                   date.tm_mday, date.tm_year + 1900, date.tm_hour, date.tm_min,
                   date.tm_sec);
  return std::string(buffer);
}

}  // namespace client
}  // namespace geode
}  // namespace apache
