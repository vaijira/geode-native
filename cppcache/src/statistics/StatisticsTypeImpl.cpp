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

#include <string>

#include <ace/OS.h>

#include "StatisticsTypeImpl.hpp"
#include "StatisticDescriptorImpl.hpp"
#include "../util/Log.hpp"

namespace apache {
namespace geode {
namespace statistics {

using client::IllegalArgumentException;
using client::NullPointerException;

StatisticsTypeImpl::StatisticsTypeImpl(std::string nameArg,
                                       std::string descriptionArg,
                                       StatisticDescriptor** statsArg,
                                       int32_t statsLengthArg) {
  if (nameArg.empty()) {
    const char* s = "Cannot have an empty statistics type name";
    throw NullPointerException(s);
  }
  if (statsArg == nullptr) {
    const char* s = "Cannot have a null statistic descriptors";
    throw NullPointerException(s);
  }
  if (statsLengthArg > MAX_DESCRIPTORS_PER_TYPE) {
    char buffer[100];
    ACE_OS::snprintf(buffer, 100, "%d", statsLengthArg);
    std::string temp(buffer);
    std::string s = "The requested descriptor count " + temp +
                    " exceeds the maximum which is ";
    ACE_OS::snprintf(buffer, 100, "%d", MAX_DESCRIPTORS_PER_TYPE);
    std::string buf(buffer);
    s += buf + ".";
    throw IllegalArgumentException(s.c_str());
  }
  this->name = nameArg;
  this->description = descriptionArg;
  this->stats = statsArg;
  this->statsLength = statsLengthArg;
  int32_t intCount = 0;
  int32_t longCount = 0;
  int32_t doubleCount = 0;
  for (int32_t i = 0; i < this->statsLength; i++) {
    // Concrete class required to set the ids only.
    StatisticDescriptorImpl* sd =
        dynamic_cast<StatisticDescriptorImpl*>(stats[i]);
    /* adongre
     * CID 28694: Dereference after null check (FORWARD_NULL)Comparing "sd" to
     * null implies that "sd" might be null.
     *
     * FIX : Do not check null from the dynamic_cast
     *       catch the exception thrown by the dynamic_cast
     */

    if (sd != nullptr) {
      if (sd->getTypeCode() == INT_TYPE) {
        sd->setId(intCount);
        intCount++;
      } else if (sd->getTypeCode() == LONG_TYPE) {
        sd->setId(longCount);
        longCount++;
      } else if (sd->getTypeCode() == DOUBLE_TYPE) {
        sd->setId(doubleCount);
        doubleCount++;
      }
      std::string str = stats[i]->getName();
      StatisticsDescMap::iterator iterFind = statsDescMap.find(str);
      if (iterFind != statsDescMap.end()) {
        std::string s = "Duplicate StatisticDescriptor named ";
        s += sd->getName();
        throw IllegalArgumentException(s.c_str());
      } else {
        // statsDescMap.insert(make_pair(stats[i]->getName(), stats[i]));
        statsDescMap.insert(
            StatisticsDescMap::value_type(stats[i]->getName(), stats[i]));
      }
    }
  }  // for
  this->intStatCount = intCount;
  this->longStatCount = longCount;
  this->doubleStatCount = doubleCount;
}

StatisticsTypeImpl::~StatisticsTypeImpl() {
  try {
    // Delete the descriptor pointers from the array
    for (int32_t i = 0; i < statsLength; i++) {
      delete stats[i];
      stats[i] = nullptr;
    }
    // same pointers are also stored in this map.
    // So, Set the pointers to null.
    StatisticsDescMap::iterator iterFind = statsDescMap.begin();
    while (iterFind != statsDescMap.end()) {
      (*iterFind).second = nullptr;
      iterFind++;
    }
  } catch (...) {
  }
}

const std::string& StatisticsTypeImpl::getName() const { return name; }

const std::string& StatisticsTypeImpl::getDescription() const {
  return description;
}

StatisticDescriptor** StatisticsTypeImpl::getStatistics() const {
  return stats;
}

int32_t StatisticsTypeImpl::nameToId(const std::string& nameArg) const {
  return nameToDescriptor(nameArg)->getId();
}

StatisticDescriptor* StatisticsTypeImpl::nameToDescriptor(
    const std::string& nameArg) const {
  const auto iterFind = statsDescMap.find(nameArg);
  if (iterFind == statsDescMap.end()) {
    std::string s = "There is no statistic named " + nameArg +
                    " in this statistics instance ";
    LOGWARN("StatisticsTypeImpl::nameToDescriptor %s", s.c_str());
    throw IllegalArgumentException(s.c_str());
  } else {
    return iterFind->second;
  }
}

int32_t StatisticsTypeImpl::getIntStatCount() const { return intStatCount; }

int32_t StatisticsTypeImpl::getLongStatCount() const { return longStatCount; }

int32_t StatisticsTypeImpl::getDoubleStatCount() const {
  return doubleStatCount;
}

int32_t StatisticsTypeImpl::getDescriptorsCount() const { return statsLength; }

}  // namespace statistics
}  // namespace geode
}  // namespace apache
