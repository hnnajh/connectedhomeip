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

#include "Server.h"

#include <errno.h>
#include <utility>

#include <lib/dnssd/minimal_mdns/core/DnsHeader.h>
#include <platform/CHIPDeviceLayer.h>

namespace mdns {
namespace Minimal {
namespace {

const uint16_t kConnectionBacklogMax = 1;
using chip::Inet::TCPEndPoint;

class ShutdownOnError
{
public:
    ShutdownOnError(ServerBase * s) : mServer(s) {}
    ~ShutdownOnError()
    {
        if (mServer != nullptr)
        {
            mServer->Shutdown();
        }
    }

    CHIP_ERROR ReturnSuccess()
    {
        mServer = nullptr;
        return CHIP_NO_ERROR;
    }

private:
    ServerBase * mServer;
};

#if CHIP_CONFIG_TCP_SUPPORT_ENABLED
/**
 * Extracts the Listening TCP Endpoint from an underlying ServerBase::EndpointInfo
 */
class ListenSocketPickerDelegate : public ServerBase::BroadcastSendDelegate
{
public:
    chip::Inet::TCPEndPoint * Accept(ServerBase::TcpEndpointInfo * info) override { return info->mListenTcp; }
};
#else
/**
 * Extracts the Listening UDP Endpoint from an underlying ServerBase::EndpointInfo
 */
class ListenSocketPickerDelegate : public ServerBase::BroadcastSendDelegate
{
public:
    chip::Inet::UDPEndPoint * Accept(ServerBase::UdpEndpointInfo * info) override { return info->mListenUdp; }
};
#endif

#if CHIP_MINMDNS_USE_EPHEMERAL_UNICAST_PORT

#if CHIP_CONFIG_TCP_SUPPORT_ENABLED
/**
 * Extracts the Querying UDP Endpoint from an underlying ServerBase::EndpointInfo
 */
class QuerySocketPickerDelegate : public ServerBase::BroadcastSendDelegate
{
public:
    chip::Inet::TCPEndPoint * Accept(ServerBase::TcpEndpointInfo * info) override { return info->mUnicastQueryTcp; }
};
#else
/**
 * Extracts the Querying UDP Endpoint from an underlying ServerBase::EndpointInfo
 */
class QuerySocketPickerDelegate : public ServerBase::BroadcastSendDelegate
{
public:
    chip::Inet::UDPEndPoint * Accept(ServerBase::UdpEndpointInfo * info) override { return info->mUnicastQueryUdp; }
};
#endif // CHIP_CONFIG_TCP_SUPPORT_ENABLED

#else

using QuerySocketPickerDelegate = ListenSocketPickerDelegate;

#endif

/**
 * Validates that an endpoint belongs to a specific interface/ip address type before forwarding the
 * endpoint accept logic to another BroadcastSendDelegate.
 *
 * Usage like:
 *
 * SomeDelegate *child = ....;
 * InterfaceTypeFilterDelegate filter(interfaceId, IPAddressType::IPv6, child);
 *
 * UDPEndPoint *udp = filter.Accept(endpointInfo);
 */
class InterfaceTypeFilterDelegate : public ServerBase::BroadcastSendDelegate
{
public:
    InterfaceTypeFilterDelegate(chip::Inet::InterfaceId interface, chip::Inet::IPAddressType type,
                                ServerBase::BroadcastSendDelegate * child) :
        mInterface(interface),
        mAddressType(type), mChild(child)
    {}

#if CHIP_CONFIG_TCP_SUPPORT_ENABLED
    chip::Inet::TCPEndPoint * Accept(ServerBase::TcpEndpointInfo * info) override
    {
        if ((info->mInterfaceId != mInterface) && (info->mInterfaceId != chip::Inet::InterfaceId::Null()))
        {
            return nullptr;
        }

        if ((mAddressType != chip::Inet::IPAddressType::kAny) && (info->mAddressType != mAddressType))
        {
            return nullptr;
        }

        return mChild->Accept(info);
    }
#else
    chip::Inet::TCPEndPoint * Accept(ServerBase::UdpEndpointInfo * info) override
    {
        if ((info->mInterfaceId != mInterface) && (info->mInterfaceId != chip::Inet::InterfaceId::Null()))
        {
            return nullptr;
        }

        if ((mAddressType != chip::Inet::IPAddressType::kAny) && (info->mAddressType != mAddressType))
        {
            return nullptr;
        }

        return mChild->Accept(info);
    }
#endif // CHIP_CONFIG_TCP_SUPPORT_ENABLED
private:
    chip::Inet::InterfaceId mInterface;
    chip::Inet::IPAddressType mAddressType;
    ServerBase::BroadcastSendDelegate * mChild = nullptr;
};

} // namespace

namespace BroadcastIpAddresses {

// Get standard mDNS Broadcast addresses
chip::Inet::IPAddress Get(chip::Inet::IPAddressType addressType)
{
    chip::Inet::IPAddress address;
#if INET_CONFIG_ENABLE_IPV4
    if (addressType == chip::Inet::IPAddressType::kIPv4)
    {
        VerifyOrDie(chip::Inet::IPAddress::FromString("224.0.0.251", address));
    }
    else
#endif
    {
        VerifyOrDie(chip::Inet::IPAddress::FromString("FF02::FB", address));
    }
    return address;
}

} // namespace BroadcastIpAddresses

namespace {

#if CHIP_ERROR_LOGGING
#if !CHIP_CONFIG_TCP_SUPPORT_ENABLED
const char * AddressTypeStr(chip::Inet::IPAddressType addressType)
{
    switch (addressType)
    {
    case chip::Inet::IPAddressType::kIPv6:
        return "IPv6";
#if INET_CONFIG_ENABLE_IPV4
    case chip::Inet::IPAddressType::kIPv4:
        return "IPv4";
#endif // INET_CONFIG_ENABLE_IPV4
    default:
        return "UNKNOWN";
    }
}
#endif // CHIP_CONFIG_TCP_SUPPORT_ENABLED
#endif

} // namespace

ServerBase::~ServerBase()
{
    Shutdown();
}

void ServerBase::Shutdown()
{
    ShutdownEndpoints();
    mIsInitialized = false;
}

void ServerBase::ShutdownEndpoints()
{
    mEndpoints.ReleaseAll();
}

#if CHIP_CONFIG_TCP_SUPPORT_ENABLED
void ServerBase::ShutdownEndpoint(TcpEndpointInfo & aEndpoint)
{
    mEndpoints.ReleaseObject(&aEndpoint);
}
#else
void ServerBase::ShutdownEndpoint(UdpEndpointInfo & aEndpoint)
{
    mEndpoints.ReleaseObject(&aEndpoint);
}
#endif // CHIP_CONFIG_TCP_SUPPORT_ENABLED

bool ServerBase::IsListening() const
{
    bool listening = false;
#if CHIP_CONFIG_TCP_SUPPORT_ENABLED
    mEndpoints.ForEachActiveObject([&](auto * endpoint) {
        if (endpoint->mListenTcp != nullptr)
        {
            listening = true;
            return chip::Loop::Break;
        }
        return chip::Loop::Continue;
    });
    return listening;
#else
    mEndpoints.ForEachActiveObject([&](auto * endpoint) {
        if (endpoint->mListenUdp != nullptr)
        {
            listening = true;
            return chip::Loop::Break;
        }
        return chip::Loop::Continue;
    });
    return listening;
#endif // CHIP_CONFIG_TCP_SUPPORT_ENABLED
}

#if CHIP_CONFIG_TCP_SUPPORT_ENABLED
CHIP_ERROR ServerBase::Listen(chip::Inet::EndPointManager<chip::Inet::TCPEndPoint> * tcpEndPointManager, ListenIterator * it,
                      uint16_t port) {
    ShutdownEndpoints(); // ensure everything starts fresh

    chip::Inet::InterfaceId interfaceId = chip::Inet::InterfaceId::Null();
    chip::Inet::IPAddressType addressType;

    ShutdownOnError autoShutdown(this);

    while (it->Next(&interfaceId, &addressType))
    {
        chip::Inet::TCPEndPoint * listenTcp;
        ChipLogError(Inet, "[TEST] #1 initailize endpoint TCP");
        ReturnErrorOnFailure(tcpEndPointManager->NewEndPoint(&listenTcp));
        std::unique_ptr<chip::Inet::TCPEndPoint, TcpEndpointInfo::EndPointDeletor> endPointHolder(listenTcp, {});

        ReturnErrorOnFailure(listenTcp->Bind(addressType, chip::Inet::IPAddress::Any, port));

        listenTcp->OnDataReceived     = OnTcpPacketReceived;
        ReturnErrorOnFailure(listenTcp->Listen(kConnectionBacklogMax));

        /*
        CHIP_ERROR err = listenTcp->JoinMulticastGroup(interfaceId, BroadcastIpAddresses::Get(addressType));

        if (err != CHIP_NO_ERROR)
        {
            char interfaceName[chip::Inet::InterfaceId::kMaxIfNameLength];
            interfaceId.GetInterfaceName(interfaceName, sizeof(interfaceName));

            // Log only as non-fatal error. Failure to join will mean we reply to unicast queries only.
            ChipLogError(DeviceLayer, "MDNS failed to join multicast group on %s for address type %s: %" CHIP_ERROR_FORMAT,
                         interfaceName, AddressTypeStr(addressType), err.Format());

            endPointHolder.reset();
        }
        //*/
#if CHIP_MINMDNS_USE_EPHEMERAL_UNICAST_PORT
        // Separate TCP endpoint for unicast queries, bound to 0 (i.e. pick random ephemeral port)
        //   - helps in not having conflicts on port 5353, will receive unicast replies directly
        //   - has a *DRAWBACK* of unicast queries being considered LEGACY by mdns since they do
        //     not originate from 5353 and the answers will include a query section.
        chip::Inet::TCPEndPoint * unicastQueryTcp;
        ChipLogError(Inet, "[TEST] #2 initailize endpoint");
        ReturnErrorOnFailure(tcpEndPointManager->NewEndPoint(&unicastQueryTcp));
        std::unique_ptr<chip::Inet::TCPEndPoint, TcpEndpointInfo::EndPointDeletor> endPointHolderUnicast(unicastQueryTcp, {});
        ReturnErrorOnFailure(unicastQueryTcp->Bind(addressType, chip::Inet::IPAddress::Any, 0));
        unicastQueryTcp->OnDataReceived     = OnTcpPacketReceived;
        ReturnErrorOnFailure(unicastQueryTcp->Listen(kConnectionBacklogMax));
#endif

#if CHIP_MINMDNS_USE_EPHEMERAL_UNICAST_PORT
        if (endPointHolder || endPointHolderUnicast)
        {
            // If allocation fails, the rref will not be consumed, so that the endpoint will also be freed correctly
            mEndpoints.CreateObject(interfaceId, addressType, std::move(endPointHolder), std::move(endPointHolderUnicast));
        }
#else
        if (endPointHolder)
        {
            // If allocation fails, the rref will not be consumed, so that the endpoint will also be freed correctly
            mEndpoints.CreateObject(interfaceId, addressType, std::move(endPointHolder));
        }
#endif

        // If at least one IPv6 interface is used by the mDNS server, notify the application that DNS-SD is ready.
        if (!mIsInitialized && addressType == chip::Inet::IPAddressType::kIPv6)
        {
#if !CHIP_DEVICE_LAYER_NONE
            chip::DeviceLayer::ChipDeviceEvent event{};
            event.Type = chip::DeviceLayer::DeviceEventType::kDnssdInitialized;
            chip::DeviceLayer::PlatformMgr().PostEventOrDie(&event);
#endif
            mIsInitialized = true;
        }
    }

    return autoShutdown.ReturnSuccess();
}

#else
CHIP_ERROR ServerBase::Listen(chip::Inet::EndPointManager<chip::Inet::UDPEndPoint> * udpEndPointManager, ListenIterator * it,
                              uint16_t port)
{
    ShutdownEndpoints(); // ensure everything starts fresh

    chip::Inet::InterfaceId interfaceId = chip::Inet::InterfaceId::Null();
    chip::Inet::IPAddressType addressType;

    ShutdownOnError autoShutdown(this);

    while (it->Next(&interfaceId, &addressType))
    {
        chip::Inet::UDPEndPoint * listenUdp;
        ChipLogError(Inet, "[TEST] #1 initailize endpoint UDP");
        ReturnErrorOnFailure(udpEndPointManager->NewEndPoint(&listenUdp));
        std::unique_ptr<chip::Inet::UDPEndPoint, UdpEndpointInfo::EndPointDeletor> endPointHolder(listenUdp, {});

        ReturnErrorOnFailure(listenUdp->Bind(addressType, chip::Inet::IPAddress::Any, port, interfaceId));

        ReturnErrorOnFailure(listenUdp->Listen(OnUdpPacketReceived, nullptr /*OnReceiveError*/, this));

        CHIP_ERROR err = listenUdp->JoinMulticastGroup(interfaceId, BroadcastIpAddresses::Get(addressType));

        if (err != CHIP_NO_ERROR)
        {
            char interfaceName[chip::Inet::InterfaceId::kMaxIfNameLength];
            interfaceId.GetInterfaceName(interfaceName, sizeof(interfaceName));

            // Log only as non-fatal error. Failure to join will mean we reply to unicast queries only.
            ChipLogError(DeviceLayer, "MDNS failed to join multicast group on %s for address type %s: %" CHIP_ERROR_FORMAT,
                         interfaceName, AddressTypeStr(addressType), err.Format());

            endPointHolder.reset();
        }

#if CHIP_MINMDNS_USE_EPHEMERAL_UNICAST_PORT
        // Separate UDP endpoint for unicast queries, bound to 0 (i.e. pick random ephemeral port)
        //   - helps in not having conflicts on port 5353, will receive unicast replies directly
        //   - has a *DRAWBACK* of unicast queries being considered LEGACY by mdns since they do
        //     not originate from 5353 and the answers will include a query section.
        chip::Inet::UDPEndPoint * unicastQueryUdp;
        ChipLogError(Inet, "[TEST] #2 initailize endpoint");
        ReturnErrorOnFailure(udpEndPointManager->NewEndPoint(&unicastQueryUdp));
        std::unique_ptr<chip::Inet::UDPEndPoint, UdpEndpointInfo::EndPointDeletor> endPointHolderUnicast(unicastQueryUdp, {});
        ReturnErrorOnFailure(unicastQueryUdp->Bind(addressType, chip::Inet::IPAddress::Any, 0, interfaceId));
        ReturnErrorOnFailure(unicastQueryUdp->Listen(OnUdpPacketReceived, nullptr /*OnReceiveError*/, this));
#endif

#if CHIP_MINMDNS_USE_EPHEMERAL_UNICAST_PORT
        if (endPointHolder || endPointHolderUnicast)
        {
            // If allocation fails, the rref will not be consumed, so that the endpoint will also be freed correctly
            mEndpoints.CreateObject(interfaceId, addressType, std::move(endPointHolder), std::move(endPointHolderUnicast));
        }
#else
        if (endPointHolder)
        {
            // If allocation fails, the rref will not be consumed, so that the endpoint will also be freed correctly
            mEndpoints.CreateObject(interfaceId, addressType, std::move(endPointHolder));
        }
#endif

        // If at least one IPv6 interface is used by the mDNS server, notify the application that DNS-SD is ready.
        if (!mIsInitialized && addressType == chip::Inet::IPAddressType::kIPv6)
        {
#if !CHIP_DEVICE_LAYER_NONE
            chip::DeviceLayer::ChipDeviceEvent event{};
            event.Type = chip::DeviceLayer::DeviceEventType::kDnssdInitialized;
            chip::DeviceLayer::PlatformMgr().PostEventOrDie(&event);
#endif
            mIsInitialized = true;
        }
    }

    return autoShutdown.ReturnSuccess();
}

#endif // CHIP_CONFIG_TCP_SUPPORT_ENABLED
CHIP_ERROR ServerBase::DirectSend(chip::System::PacketBufferHandle && data, const chip::Inet::IPAddress & addr, uint16_t port,
                                  chip::Inet::InterfaceId interface)
{
    CHIP_ERROR err = CHIP_ERROR_NOT_CONNECTED;
#if CHIP_CONFIG_TCP_SUPPORT_ENABLED
    mEndpoints.ForEachActiveObject([&](auto * info) {
        if (info->mListenTcp == nullptr)
        {
            return chip::Loop::Continue;
        }

        if (info->mAddressType != addr.Type())
        {
            return chip::Loop::Continue;
        }

        err = info->mListenTcp->Send(std::move(data));
        return chip::Loop::Break;
    });
#else
    mEndpoints.ForEachActiveObject([&](auto * info) {
        if (info->mListenUdp == nullptr)
        {
            return chip::Loop::Continue;
        }

        if (info->mAddressType != addr.Type())
        {
            return chip::Loop::Continue;
        }

        chip::Inet::InterfaceId boundIf = info->mListenUdp->GetBoundInterface();

        if ((boundIf.IsPresent()) && (boundIf != interface))
        {
            return chip::Loop::Continue;
        }

        err = info->mListenUdp->SendTo(addr, port, std::move(data));
        return chip::Loop::Break;
    });
#endif // CHIP_CONFIG_TCP_SUPPORT_ENABLED

    return err;
}

CHIP_ERROR ServerBase::BroadcastUnicastQuery(chip::System::PacketBufferHandle && data, uint16_t port)
{
    QuerySocketPickerDelegate socketPicker;
    return BroadcastImpl(std::move(data), port, &socketPicker);
}

CHIP_ERROR ServerBase::BroadcastUnicastQuery(chip::System::PacketBufferHandle && data, uint16_t port,
                                             chip::Inet::InterfaceId interface, chip::Inet::IPAddressType addressType)
{
    QuerySocketPickerDelegate socketPicker;
    InterfaceTypeFilterDelegate filter(interface, addressType, &socketPicker);

    return BroadcastImpl(std::move(data), port, &filter);
}

CHIP_ERROR ServerBase::BroadcastSend(chip::System::PacketBufferHandle && data, uint16_t port, chip::Inet::InterfaceId interface,
                                     chip::Inet::IPAddressType addressType)
{
    ListenSocketPickerDelegate socketPicker;
    InterfaceTypeFilterDelegate filter(interface, addressType, &socketPicker);

    return BroadcastImpl(std::move(data), port, &filter);
}

CHIP_ERROR ServerBase::BroadcastSend(chip::System::PacketBufferHandle && data, uint16_t port)
{
    ListenSocketPickerDelegate socketPicker;
    return BroadcastImpl(std::move(data), port, &socketPicker);
}

CHIP_ERROR ServerBase::BroadcastImpl(chip::System::PacketBufferHandle && data, uint16_t port, BroadcastSendDelegate * delegate)
{
#if CHIP_CONFIG_TCP_SUPPORT_ENABLED
    return CHIP_NO_ERROR;
#else
    // Broadcast requires sending data multiple times, each of which may error
    // out, yet broadcast only has a single error code.
    //
    // The general logic of error handling is:
    //   - if no send done at all, return error
    //   - if at least one broadcast succeeds, assume success overall
    //   + some internal consistency validations for state error.

    unsigned successes   = 0;
    unsigned failures    = 0;
    CHIP_ERROR lastError = CHIP_ERROR_NO_ENDPOINT;

    if (chip::Loop::Break == mEndpoints.ForEachActiveObject([&](auto * info) {
            chip::Inet::UDPEndPoint * udp = delegate->Accept(info);

            if (udp == nullptr)
            {
                return chip::Loop::Continue;
            }

            CHIP_ERROR err = CHIP_NO_ERROR;

            /// The same packet needs to be sent over potentially multiple interfaces.
            /// LWIP does not like having a pbuf sent over serparate interfaces, hence we create a copy
            /// for sending via `CloneData`
            ///
            /// TODO: this wastes one copy of the data and that could be optimized away
            chip::System::PacketBufferHandle tempBuf = data.CloneData();
            if (tempBuf.IsNull())
            {
                // Not enough memory available to clone pbuf
                err = CHIP_ERROR_NO_MEMORY;
            }
            else if (info->mAddressType == chip::Inet::IPAddressType::kIPv6)
            {
                err = udp->SendTo(mIpv6BroadcastAddress, port, std::move(tempBuf), udp->GetBoundInterface());
            }
#if INET_CONFIG_ENABLE_IPV4
            else if (info->mAddressType == chip::Inet::IPAddressType::kIPv4)
            {
                err = udp->SendTo(mIpv4BroadcastAddress, port, std::move(tempBuf), udp->GetBoundInterface());
            }
#endif
            else
            {
                // This is a general error of internal consistency: every address has a known type. Fail completely otherwise.
                lastError = CHIP_ERROR_INCORRECT_STATE;
                return chip::Loop::Break;
            }

            if (err == CHIP_NO_ERROR)
            {
                successes++;
            }
            else
            {
                failures++;
                lastError = err;
#if CHIP_DETAIL_LOGGING
                char ifaceName[chip::Inet::InterfaceId::kMaxIfNameLength];
                err = info->mInterfaceId.GetInterfaceName(ifaceName, sizeof(ifaceName));
                if (err != CHIP_NO_ERROR)
                    strcpy(ifaceName, "???");
                ChipLogDetail(Discovery, "Warning: Attempt to mDNS broadcast failed on %s:  %s", ifaceName, lastError.AsString());
#endif
            }
            return chip::Loop::Continue;
        }))
    {
        return lastError;
    }

    if (failures != 0)
    {
        // if we had failures, log if the final status was success or failure, to make log reading
        // easier. Some mDNS failures may be expected (e.g. for interfaces unavailable)
        if (successes != 0)
        {
            ChipLogDetail(Discovery, "mDNS broadcast had only partial success: %u successes and %u failures.", successes, failures);
        }
        else
        {
            ChipLogProgress(Discovery, "mDNS broadcast full failed in %u separate send attempts.", failures);
        }
    }

    if (!successes)
    {
        return lastError;
    }

    return CHIP_NO_ERROR;
#endif // CHIP_CONFIG_TCP_SUPPORT_ENABLED
}

CHIP_ERROR ServerBase::OnTcpPacketReceived(TCPEndPoint * endPoint, chip::System::PacketBufferHandle && buffer)
{
    ServerBase * srv = static_cast<ServerBase *>(endPoint->mAppState);
    if (!srv->mDelegate)
    {
        return CHIP_NO_ERROR;
    }

    mdns::Minimal::BytesRange data(buffer->Start(), buffer->Start() + buffer->DataLength());
    ChipLogError(Discovery, "[TEST] mDNS data: %d bytes", static_cast<int>(data.Size()));
    return CHIP_NO_ERROR;
    /*
    if (data.Size() < HeaderRef::kSizeBytes)
    {
        ChipLogError(Discovery, "Packet to small for mDNS data: %d bytes", static_cast<int>(data.Size()));
        return;
    }

    if (HeaderRef(const_cast<uint8_t *>(data.Start())).GetFlags().IsQuery())
    {
        // Only consider queries that are received on the same interface we are listening on.
        // Without this, queries show up on all addresses on all interfaces, resulting
        // in more replies than one would expect.
        if (endPoint->GetBoundInterface() == info->Interface)
        {
            srv->mDelegate->OnQuery(data, info);
        }
    }
    else
    {
        srv->mDelegate->OnResponse(data, info);
    }
    //*/
}

void ServerBase::OnUdpPacketReceived(chip::Inet::UDPEndPoint * endPoint, chip::System::PacketBufferHandle && buffer,
                                     const chip::Inet::IPPacketInfo * info)
{
    ServerBase * srv = static_cast<ServerBase *>(endPoint->mAppState);
    if (!srv->mDelegate)
    {
        return;
    }

    mdns::Minimal::BytesRange data(buffer->Start(), buffer->Start() + buffer->DataLength());
    if (data.Size() < HeaderRef::kSizeBytes)
    {
        ChipLogError(Discovery, "Packet to small for mDNS data: %d bytes", static_cast<int>(data.Size()));
        return;
    }

    if (HeaderRef(const_cast<uint8_t *>(data.Start())).GetFlags().IsQuery())
    {
        // Only consider queries that are received on the same interface we are listening on.
        // Without this, queries show up on all addresses on all interfaces, resulting
        // in more replies than one would expect.
        if (endPoint->GetBoundInterface() == info->Interface)
        {
            srv->mDelegate->OnQuery(data, info);
        }
    }
    else
    {
        srv->mDelegate->OnResponse(data, info);
    }
}

} // namespace Minimal
} // namespace mdns
