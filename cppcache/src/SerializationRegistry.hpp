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

#pragma once

#ifndef GEODE_SERIALIZATIONREGISTRY_H_
#define GEODE_SERIALIZATIONREGISTRY_H_

#include <string>
#include <functional>
#include <typeinfo>
#include <iostream>
#include <memory>
#include <typeindex>
#include <unordered_map>

#include <ace/Hash_Map_Manager.h>
#include <ace/Thread_Mutex.h>
#include <ace/Null_Mutex.h>

#include <geode/internal/geode_globals.hpp>
#include <geode/internal/DataSerializableInternal.hpp>
#include <geode/Serializable.hpp>
#include <geode/PdxSerializer.hpp>
#include <geode/DataOutput.hpp>
#include <geode/ExceptionTypes.hpp>
#include <geode/Delta.hpp>
#include <geode/PdxSerializable.hpp>
#include <geode/DataSerializable.hpp>

#include "util/concurrent/spinlock_mutex.hpp"
#include "NonCopyable.hpp"
#include "MemberListForVersionStamp.hpp"
#include "config.h"

#if defined(_MACOSX)
namespace ACE_VERSIONED_NAMESPACE_NAME {
// TODO CMake check type int64_t
template <>
class ACE_Export ACE_Hash<int64_t> {
 public:
  inline unsigned long operator()(int64_t t) const {
    return static_cast<long>(t);
  }
};

}  // namespace ACE_VERSIONED_NAMESPACE_NAME
#endif

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

using apache::geode::client::DSCode;
template <>
class ACE_Hash<DSCode> {
 public:
  inline u_long operator()(const DSCode key) {
    return static_cast<u_long>(key);
  }
};

ACE_END_VERSIONED_NAMESPACE_DECL

namespace apache {
namespace geode {
namespace client {

using internal::DataSerializableInternal;

typedef ACE_Hash_Map_Manager<DSCode, TypeFactoryMethod, ACE_Null_Mutex>
    DSCodeToFactoryMap;

typedef ACE_Hash_Map_Manager<int32_t, TypeFactoryMethod, ACE_Null_Mutex>
    IdToFactoryMap;

typedef ACE_Hash_Map_Manager<std::string, TypeFactoryMethodPdx, ACE_Null_Mutex>
    StrToPdxFactoryMap;

class TheTypeMap : private NonCopyable {
 private:
  DSCodeToFactoryMap* m_dataSerializablePrimitiveMap;
  IdToFactoryMap* m_dataSerializableMap;
  IdToFactoryMap* m_dataSerializableFixedIdMap;
  StrToPdxFactoryMap* m_pdxSerializableMap;
  mutable util::concurrent::spinlock_mutex m_dataSerializablePrimitiveMapLock;
  mutable util::concurrent::spinlock_mutex m_dataSerializableMapLock;
  mutable util::concurrent::spinlock_mutex m_dataSerializableFixedIdMapLock;
  mutable util::concurrent::spinlock_mutex m_pdxSerializableMapLock;

 public:
  std::unordered_map<std::type_index, int32_t> typeToClassId;

 public:
  TheTypeMap() {
    // map to hold DataSerializablePrimitive
    m_dataSerializablePrimitiveMap = new DSCodeToFactoryMap();

    // map to hold Data Serializable IDs
    m_dataSerializableMap = new IdToFactoryMap();

    // map to hold internal Data Serializable Fixed IDs
    m_dataSerializableFixedIdMap = new IdToFactoryMap();

    // map to hold PDX types <string, funptr>.
    m_pdxSerializableMap = new StrToPdxFactoryMap();

    setup();
  }

  virtual ~TheTypeMap() {
    if (m_dataSerializableMap != nullptr) {
      delete m_dataSerializableMap;
    }

    if (m_dataSerializableFixedIdMap != nullptr) {
      delete m_dataSerializableFixedIdMap;
    }

    if (m_pdxSerializableMap != nullptr) {
      delete m_pdxSerializableMap;
    }
  }

  void setup();

  void clear();

  void findDataSerializable(int32_t id, TypeFactoryMethod& func) const;

  void bindDataSerializable(TypeFactoryMethod func, int32_t id);

  void rebindDataSerializable(int32_t id, TypeFactoryMethod func);

  void unbindDataSerializable(int32_t id);

  void findDataSerializableFixedId(int32_t id, TypeFactoryMethod& func) const;

  void bindDataSerializableFixedId(TypeFactoryMethod func);

  void rebindDataSerializableFixedId(int32_t idd, TypeFactoryMethod func);

  void unbindDataSerializableFixedId(int32_t id);

  void bindPdxSerializable(TypeFactoryMethodPdx func);

  void findPdxSerializable(const std::string& objFullName,
                           TypeFactoryMethodPdx& func) const;

  void unbindPdxSerializable(const std::string& objFullName);

  void rebindPdxSerializable(std::string objFullName,
                             TypeFactoryMethodPdx func);

  void findDataSerializablePrimitive(DSCode dsCode,
                                     TypeFactoryMethod& func) const;

  void bindDataSerializablePrimitive(TypeFactoryMethod func, DSCode id);

  void rebindDataSerializablePrimitive(DSCode dsCode, TypeFactoryMethod func);

 private:
};

class Pool;

/**
 * Used to register handlers for the PDX DsCode. .NET client extends this to
 * intercept PDX (de)serialization.
 */
class PdxTypeHandler {
 public:
  virtual ~PdxTypeHandler() noexcept = default;
  virtual void serialize(
      const std::shared_ptr<PdxSerializable>& pdxSerializable,
      DataOutput& dataOutput) const;
  virtual std::shared_ptr<PdxSerializable> deserialize(
      DataInput& dataInput) const;
};

/**
 * Used to register handlers for the DataSerializable. .NET client extends this
 * to intercept for (de)serialization.
 */
class DataSerializableHandler {
 public:
  virtual ~DataSerializableHandler() noexcept = default;
  virtual void serialize(
      const std::shared_ptr<DataSerializable>& dataSerializable,
      DataOutput& dataOutput, bool isDelta) const;
  virtual std::shared_ptr<DataSerializable> deserialize(DataInput& input,
                                                        DSCode typeId) const;
};

class APACHE_GEODE_EXPORT SerializationRegistry {
 public:
  SerializationRegistry() : theTypeMap() {}

  /** write the length of the serialization, write the typeId of the object,
   * then write whatever the object's toData requires. The length at the
   * front is backfilled after the serialization.
   */
  inline void serialize(const std::shared_ptr<Serializable>& obj,
                        DataOutput& output, bool isDelta = false) const {
    if (obj == nullptr) {
      output.write(static_cast<int8_t>(DSCode::NullObj));
    } else if (auto&& pdxSerializable =
                   std::dynamic_pointer_cast<PdxSerializable>(obj)) {
      serialize(pdxSerializable, output);
    } else if (const auto&& dataSerializableFixedId =
                   std::dynamic_pointer_cast<DataSerializableFixedId>(obj)) {
      serialize(dataSerializableFixedId, output);
    } else if (const auto&& dataSerializablePrimitive =
                   std::dynamic_pointer_cast<DataSerializablePrimitive>(obj)) {
      serialize(dataSerializablePrimitive, output);
    } else if (const auto&& dataSerializable =
                   std::dynamic_pointer_cast<DataSerializable>(obj)) {
      dataSerializeableHandler->serialize(dataSerializable, output, isDelta);
    } else if (const auto&& dataSerializableInternal =
                   std::dynamic_pointer_cast<DataSerializableInternal>(obj)) {
      serialize(dataSerializableInternal, output);
    } else {
      throw UnsupportedOperationException(
          "SerializationRegistry::serialize: Serialization type not "
          "implemented.");
    }
  }

  inline void serializeWithoutHeader(const std::shared_ptr<Serializable>& obj,
                                     DataOutput& output) const {
    if (auto&& pdxSerializable =
            std::dynamic_pointer_cast<PdxSerializable>(obj)) {
      serializeWithoutHeader(pdxSerializable, output);
    } else if (const auto&& dataSerializableFixedId =
                   std::dynamic_pointer_cast<DataSerializableFixedId>(obj)) {
      serializeWithoutHeader(dataSerializableFixedId, output);
    } else if (const auto&& dataSerializablePrimitive =
                   std::dynamic_pointer_cast<DataSerializablePrimitive>(obj)) {
      serializeWithoutHeader(dataSerializablePrimitive, output);
    } else if (const auto&& dataSerializable =
                   std::dynamic_pointer_cast<DataSerializable>(obj)) {
      serializeWithoutHeader(dataSerializable, output);
    } else if (const auto&& pdxSerializable =
                   std::dynamic_pointer_cast<PdxSerializable>(obj)) {
      serializeWithoutHeader(pdxSerializable, output);
    } else if (const auto&& dataSerializableInternal =
                   std::dynamic_pointer_cast<DataSerializableInternal>(obj)) {
      serializeWithoutHeader(dataSerializableInternal, output);
    } else {
      throw UnsupportedOperationException(
          "SerializationRegistry::serializeWithoutHeader: Serialization type "
          "not implemented.");
    }
  }

  /**
   * Read the length, typeid, and run the objs fromData. Returns the New
   * object.
   */
  std::shared_ptr<Serializable> deserialize(DataInput& input,
                                            int8_t typeId = -1) const;

  void addDataSerializableType(TypeFactoryMethod func, int32_t id);

  void addPdxSerializableType(TypeFactoryMethodPdx func);

  void setPdxSerializer(std::shared_ptr<PdxSerializer> pdxSerializer);

  std::shared_ptr<PdxSerializer> getPdxSerializer();

  void removeDataSerializableType(int32_t id);

  void addDataSerializableFixedIdType(TypeFactoryMethod func);

  void addDataSerializableFixedIdType(int32_t id, TypeFactoryMethod func);

  void removeDataSerializableFixeIdType(int32_t id);

  void setDataSerializablePrimitiveType(TypeFactoryMethod func, DSCode dsCode);

  int32_t GetPDXIdForType(Pool* pool,
                          std::shared_ptr<Serializable> pdxType) const;

  std::shared_ptr<Serializable> GetPDXTypeById(Pool* pool,
                                               int32_t typeId) const;

  int32_t GetEnumValue(std::shared_ptr<Pool> pool,
                       std::shared_ptr<Serializable> enumInfo) const;

  std::shared_ptr<Serializable> GetEnum(std::shared_ptr<Pool> pool,
                                        int32_t val) const;

  std::shared_ptr<PdxSerializable> getPdxSerializableType(
      const std::string& className) const;

  void setPdxTypeHandler(PdxTypeHandler* handler) {
    this->pdxTypeHandler = std::unique_ptr<PdxTypeHandler>(handler);
  }
  void setDataSerializableHandler(DataSerializableHandler* handler) {
    this->dataSerializeableHandler =
        std::unique_ptr<DataSerializableHandler>(handler);
  }

  TypeFactoryMethod getDataSerializableCreationMethod(int32_t objectId) {
    TypeFactoryMethod createType;
    theTypeMap.findDataSerializable(objectId, createType);
    return createType;
  }

  int32_t getIdForDataSerializableType(std::type_index objectType) const {
    auto&& typeIterator = theTypeMap.typeToClassId.find(objectType);
    auto&& id = typeIterator->second;
    return id;
  }

 private:
  std::unique_ptr<PdxTypeHandler> pdxTypeHandler;
  std::shared_ptr<PdxSerializer> pdxSerializer;
  std::unique_ptr<DataSerializableHandler> dataSerializeableHandler;
  TheTypeMap theTypeMap;

  std::shared_ptr<Serializable> deserializeDataSerializableFixedId(
      DataInput& input, DSCode dsCode) const;

  inline void serialize(const std::shared_ptr<DataSerializableFixedId>& obj,
                        DataOutput& output) const {
    auto id = static_cast<int32_t>(obj->getDSFID());
    if (id <= std::numeric_limits<int8_t>::max() &&
        id >= std::numeric_limits<int8_t>::min()) {
      output.write(static_cast<int8_t>(DSCode::FixedIDByte));
      output.write(static_cast<int8_t>(id));
    } else if (id <= std::numeric_limits<int16_t>::max() &&
               id >= std::numeric_limits<int16_t>::min()) {
      output.write(static_cast<int8_t>(DSCode::FixedIDShort));
      output.writeInt(static_cast<int16_t>(id));
    } else {
      output.write(static_cast<int8_t>(DSCode::FixedIDInt));
      output.writeInt(static_cast<int32_t>(id));
    }

    serializeWithoutHeader(obj, output);
  }

  inline void serializeWithoutHeader(
      const std::shared_ptr<DataSerializableFixedId>& obj,
      DataOutput& output) const {
    obj->toData(output);
  }

  inline void serialize(const std::shared_ptr<DataSerializablePrimitive>& obj,
                        DataOutput& output) const {
    auto id = obj->getDsCode();
    output.write(static_cast<int8_t>(id));

    serializeWithoutHeader(obj, output);
  }

  inline void serializeWithoutHeader(
      const std::shared_ptr<DataSerializablePrimitive>& obj,
      DataOutput& output) const {
    obj->toData(output);
  }

  inline void serialize(const std::shared_ptr<PdxSerializable>& obj,
                        DataOutput& output) const {
    output.write(static_cast<int8_t>(DSCode::PDX));
    serializeWithoutHeader(obj, output);
  }

  void serializeWithoutHeader(const std::shared_ptr<PdxSerializable>& obj,
                              DataOutput& output) const;

  inline void serialize(const std::shared_ptr<DataSerializableInternal>& obj,
                        DataOutput& output) const {
    serializeWithoutHeader(obj, output);
  }

  inline void serializeWithoutHeader(
      const std::shared_ptr<DataSerializableInternal>& obj,
      DataOutput& output) const {
    obj->toData(output);
  }

 public:
  static inline DSCode getSerializableDataDsCode(int32_t classId) {
    if (classId <= std::numeric_limits<int8_t>::max() &&
        classId >= std::numeric_limits<int8_t>::min()) {
      return DSCode::CacheableUserData;
    } else if (classId <= std::numeric_limits<int16_t>::max() &&
               classId >= std::numeric_limits<int16_t>::min()) {
      return DSCode::CacheableUserData2;
    } else {
      return DSCode::CacheableUserData4;
    }
  }

 private:
  void deserialize(DataInput& input,
                   const std::shared_ptr<Serializable>& obj) const;
};

}  // namespace client
}  // namespace geode
}  // namespace apache

#endif  // GEODE_SERIALIZATIONREGISTRY_H_
