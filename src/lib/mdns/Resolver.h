/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

#include <cstdint>

#include <core/CHIPError.h>
#include <core/PeerId.h>
#include <inet/IPAddress.h>
#include <inet/InetInterface.h>
#include <inet/InetLayer.h>

namespace chip {
namespace Mdns {

struct ResolvedNodeData
{
    PeerId mPeerId;
    Inet::InterfaceId mInterfaceId;
    Inet::IPAddress mAddress;
    uint16_t mPort;
};

/// Groups callbacks for CHIP service resolution requests
class ResolverDelegate
{
public:
    virtual ~ResolverDelegate() = default;

    /// Called when a requested CHIP node ID has been successfully resolved
    virtual void OnNodeIdResolved(const ResolvedNodeData & nodeData) = 0;

    /// Called when a CHIP node ID resolution has failed
    virtual void OnNodeIdResolutionFailed(const PeerId & peerId, CHIP_ERROR error) = 0;
};

/// Interface for resolving CHIP services
class Resolver
{
public:
    virtual ~Resolver() {}

    /// Ensures that the resolver is started.
    /// Must be called before any ResolveNodeId calls.
    ///
    /// Unsual name to allow base MDNS classes to implement both Advertiser and Resolver interfaces.
    virtual CHIP_ERROR StartResolver(chip::Inet::InetLayer * inetLayer, uint16_t port) = 0;

    /// Registers a resolver delegate if none has been registered before
    virtual CHIP_ERROR SetResolverDelegate(ResolverDelegate * delegate) = 0;

    /// Requests resolution of a node ID to its address
    virtual CHIP_ERROR ResolveNodeId(const PeerId & peerId, Inet::IPAddressType type) = 0;

    /// Provides the system-wide implementation of the service resolver
    static Resolver & Instance();
};

} // namespace Mdns
} // namespace chip
