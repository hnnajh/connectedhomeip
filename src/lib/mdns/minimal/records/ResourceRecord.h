/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <cstddef>

#include <mdns/minimal/core/Constants.h>
#include <mdns/minimal/core/QName.h>

#include <support/BufferWriter.h>

namespace mdns {
namespace Minimal {

/// A generic Reply record that supports data serialization
class ResourceRecord
{
public:
    static constexpr uint64_t kDefaultTtl = 30;

    virtual ~ResourceRecord() {}

    const FullQName & GetName() const { return mQName; }
    QClass GetClass() const { return QClass::IN; }
    QType GetType() const { return mType; }

    uint64_t GetTtl() const { return mTtl; }
    ResourceRecord & SetTtl(uint64_t ttl)
    {
        mTtl = ttl;
        return *this;
    }

    /// Append the given record to the underlying output.
    /// Updates header item count on success, does NOT update header on failure.
    bool Append(HeaderRef & hdr, ResourceType asType, chip::Encoding::BigEndian::BufferWriter & out) const;

protected:
    /// Output the data portion of the resource record.
    virtual bool WriteData(chip::Encoding::BigEndian::BufferWriter & out) const = 0;

    ResourceRecord(QType type, FullQName name) : mType(type), mQName(name) {}

private:
    const QType mType;
    uint64_t mTtl = kDefaultTtl;
    const FullQName mQName;
};

} // namespace Minimal
} // namespace mdns
