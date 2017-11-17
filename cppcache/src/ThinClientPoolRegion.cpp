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
/*
 * ThinClientPoolRegion.cpp
 *
 *  Created on: Nov 20, 2008
 *      Author: abhaware
 */

#include "ThinClientPoolRegion.hpp"
#include "CacheImpl.hpp"
#include <geode/SystemProperties.hpp>
#include "ThinClientPoolDM.hpp"
#include <geode/PoolManager.hpp>

using namespace apache::geode::client;

ThinClientPoolRegion::ThinClientPoolRegion(
    const std::string& name, CacheImpl* cache,
    const std::shared_ptr<RegionInternal>& rPtr,
    const std::shared_ptr<RegionAttributes>& attributes,
    const std::shared_ptr<CacheStatistics>& stats, bool shared)
    : ThinClientRegion(name, cache, rPtr, attributes, stats, shared) {}

ThinClientPoolRegion::~ThinClientPoolRegion() { m_tcrdm = nullptr; }

void ThinClientPoolRegion::initTCR() {
  try {
    ThinClientPoolDM* poolDM = dynamic_cast<ThinClientPoolDM*>(
        getCache()
            ->getPoolManager()
            .find(m_regionAttributes->getPoolName())
            .get());
    m_tcrdm = dynamic_cast<ThinClientBaseDM*>(poolDM);
    if (!m_tcrdm) {
      //  TODO: create a PoolNotFound exception.
      throw IllegalStateException("pool not found");
    }
    poolDM->incRegionCount();
  } catch (const Exception& ex) {
    LOGERROR("Failed to initialize region due to %s: %s", ex.getName(),
             ex.what());
    throw;
  }
}

void ThinClientPoolRegion::destroyDM(bool keepEndpoints) {
  dynamic_cast<ThinClientPoolDM*>(m_tcrdm)->decRegionCount();
}
