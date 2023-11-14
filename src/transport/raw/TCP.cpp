/*
 *
 *    Copyright (c) 2020-2021 Project CHIP Authors
 *    Copyright (c) 2013-2017 Nest Labs, Inc.
 *    All rights reserved.
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

/**
 *    @file
 *      This file implements the CHIP Transport object that maintains TCP connections
 *      to peers. Handles both establishing new connections and accepting peer connection
 *      requests.
 */
#include <transport/raw/TCP.h>

#include <lib/core/CHIPEncoding.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <transport/raw/MessageHeader.h>

#include <inttypes.h>
#include <limits>

namespace chip {
namespace Transport {
namespace {

using namespace chip::Encoding;

// Packets start with a 16-bit size
constexpr size_t kPacketSizeBytes = 2;

// TODO: Actual limit may be lower (spec issue #2119)
constexpr uint16_t kMaxMessageSize = static_cast<uint16_t>(System::PacketBuffer::kMaxSizeWithoutReserve - kPacketSizeBytes);

constexpr int kListenBacklogSize = 2;

} // namespace

TCPBase::~TCPBase()
{
    if (mListenSocket != nullptr)
    {
        // endpoint is only non null if it is initialized and listening
        mListenSocket->Free();
        mListenSocket = nullptr;
    }

    CloseActiveConnections();
}

void TCPBase::CloseActiveConnections()
{
    for (size_t i = 0; i < mActiveConnectionsSize; i++)
    {
        if (mActiveConnections[i].InUse())
        {
            CloseConnectionInternal(&mActiveConnections[i], CHIP_NO_ERROR, SuppressCallback::Yes);
        }
    }
}

CHIP_ERROR TCPBase::Init(TcpListenParameters & params)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    VerifyOrExit(mState == State::kNotReady, err = CHIP_ERROR_INCORRECT_STATE);

#if INET_CONFIG_ENABLE_TCP_ENDPOINT
    err = params.GetEndPointManager()->NewEndPoint(&mListenSocket);
#else
    err = CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
#endif
    SuccessOrExit(err);

    err = mListenSocket->Bind(params.GetAddressType(), Inet::IPAddress::Any, params.GetListenPort(),
                              params.GetInterfaceId().IsPresent());
    SuccessOrExit(err);

    mListenSocket->mAppState            = reinterpret_cast<void *>(this);
    mListenSocket->OnConnectionReceived = HandleIncomingConnection;
    mListenSocket->OnAcceptError        = HandleAcceptError;

    mEndpointType = params.GetAddressType();

    err = mListenSocket->Listen(kListenBacklogSize);
    SuccessOrExit(err);

    mState = State::kInitialized;

exit:
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Inet, "Failed to initialize TCP transport: %s", ErrorStr(err));
        if (mListenSocket)
        {
            mListenSocket->Free();
            mListenSocket = nullptr;
        }
    }

    return err;
}

void TCPBase::Close()
{
    if (mListenSocket)
    {
        mListenSocket->Free();
        mListenSocket = nullptr;
    }
    mState = State::kNotReady;
}

TCPBase::ActiveConnectionState * TCPBase::AllocateConnection()
{
    for (size_t i = 0; i < mActiveConnectionsSize; i++)
    {
        if (!mActiveConnections[i].InUse())
        {
            return &mActiveConnections[i];
        }
    }

    return nullptr;
}

TCPBase::ActiveConnectionState * TCPBase::FindActiveConnection(const PeerAddress & address)
{
    if (address.GetTransportType() != Type::kTcp)
    {
        return nullptr;
    }

    for (size_t i = 0; i < mActiveConnectionsSize; i++)
    {
        if (!mActiveConnections[i].InUse())
        {
            continue;
        }
        Inet::IPAddress addr;
        uint16_t port;
        mActiveConnections[i].mEndPoint->GetPeerInfo(&addr, &port);

        if ((addr == address.GetIPAddress()) && (port == address.GetPort()))
        {
            return &mActiveConnections[i];
        }
    }

    return nullptr;
}

TCPBase::ActiveConnectionState * TCPBase::FindActiveConnection(const Inet::TCPEndPoint * endPoint)
{
    for (size_t i = 0; i < mActiveConnectionsSize; i++)
    {
        if (mActiveConnections[i].mEndPoint == endPoint)
        {
            return &mActiveConnections[i];
        }
    }
    return nullptr;
}

CHIP_ERROR TCPBase::SendMessage(const Transport::PeerAddress & address, System::PacketBufferHandle && msgBuf)
{
    // Sent buffer data format is:
    //    - packet size as a uint16_t
    //    - actual data

    VerifyOrReturnError(address.GetTransportType() == Type::kTcp, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(mState == State::kInitialized, CHIP_ERROR_INCORRECT_STATE);
    VerifyOrReturnError(kPacketSizeBytes + msgBuf->DataLength() <= std::numeric_limits<uint16_t>::max(),
                        CHIP_ERROR_INVALID_ARGUMENT);

    // The check above about kPacketSizeBytes + msgBuf->DataLength() means it definitely fits in uint16_t.
    VerifyOrReturnError(msgBuf->EnsureReservedSize(static_cast<uint16_t>(kPacketSizeBytes)), CHIP_ERROR_NO_MEMORY);

    msgBuf->SetStart(msgBuf->Start() - kPacketSizeBytes);

    uint8_t * output = msgBuf->Start();
    LittleEndian::Write16(output, static_cast<uint16_t>(msgBuf->DataLength() - kPacketSizeBytes));

    // Reuse existing connection if one exists, otherwise a new one
    // will be established
    ActiveConnectionState * connection = FindActiveConnection(address);

    if (connection != nullptr && connection->mConnectionState == State::kConnected)
    {
        return connection->mEndPoint->Send(std::move(msgBuf));
    }

    return SendAfterConnect(address, std::move(msgBuf));
}

CHIP_ERROR TCPBase::StartConnect(const PeerAddress & addr)
{
#if INET_CONFIG_ENABLE_TCP_ENDPOINT
    ActiveConnectionState * activeConnection = nullptr;
    Inet::TCPEndPoint * endPoint             = nullptr;
    ReturnErrorOnFailure(mListenSocket->GetEndPointManager().NewEndPoint(&endPoint));

    auto EndPointDeletor = [](Inet::TCPEndPoint * e) { e->Free(); };
    std::unique_ptr<Inet::TCPEndPoint, decltype(EndPointDeletor)> endPointHolder(endPoint, EndPointDeletor);

    endPoint->mAppState         = reinterpret_cast<void *>(this);
    endPoint->OnConnectComplete = HandleTCPEndPointConnectComplete;
    endPoint->SetConnectTimeout(mConnectTimeout);

    activeConnection = AllocateConnection();
    VerifyOrReturnError(activeConnection != nullptr, CHIP_ERROR_NO_MEMORY);
    activeConnection->mEndPoint        = endPoint;
    activeConnection->mConnectionState = State::kConnecting;

    ReturnErrorOnFailure(endPoint->Connect(addr.GetIPAddress(), addr.GetPort(), addr.GetInterface()));

    mUsedEndPointCount++;

    endPointHolder.release();

    return CHIP_NO_ERROR;
#else
    return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
#endif
}

CHIP_ERROR TCPBase::SendAfterConnect(const PeerAddress & addr, System::PacketBufferHandle && msg)
{
#if INET_CONFIG_ENABLE_TCP_ENDPOINT
    // This will initiate a connection to the specified peer
    bool alreadyConnecting = false;

    // Iterate through the ENTIRE array. If a pending packet for
    // the address already exists, this means a connection is pending and
    // does NOT need to be re-established.
    mPendingPackets.ForEachActiveObject([&](PendingPacket * pending) {
        if (pending->mPeerAddress == addr)
        {
            // same destination exists.
            alreadyConnecting = true;
            pending->mPacketBuffer->AddToEnd(std::move(msg));
            return Loop::Break;
        }
        return Loop::Continue;
    });

    // If already connecting, buffer was just enqueued for more sending
    if (alreadyConnecting)
    {
        return CHIP_NO_ERROR;
    }

    // Ensures sufficient active connections size exist
    VerifyOrReturnError(mUsedEndPointCount < mActiveConnectionsSize, CHIP_ERROR_NO_MEMORY);

    ReturnErrorOnFailure(StartConnect(addr));

    // enqueue the packet once the connection succeeds
    VerifyOrReturnError(mPendingPackets.CreateObject(addr, std::move(msg)) != nullptr, CHIP_ERROR_NO_MEMORY);
    mUsedEndPointCount++;

    return CHIP_NO_ERROR;
#else
    return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
#endif
}

CHIP_ERROR TCPBase::ProcessReceivedBuffer(Inet::TCPEndPoint * endPoint, const PeerAddress & peerAddress,
                                          System::PacketBufferHandle && buffer)
{
    ActiveConnectionState * state = FindActiveConnection(endPoint);
    VerifyOrReturnError(state != nullptr, CHIP_ERROR_INTERNAL);
    state->mReceived.AddToEnd(std::move(buffer));

    while (!state->mReceived.IsNull())
    {
        uint8_t messageSizeBuf[kPacketSizeBytes];
        CHIP_ERROR err = state->mReceived->Read(messageSizeBuf);
        if (err == CHIP_ERROR_BUFFER_TOO_SMALL)
        {
            // We don't have enough data to read the message size. Wait until there's more.
            return CHIP_NO_ERROR;
        }
        if (err != CHIP_NO_ERROR)
        {
            return err;
        }
        uint16_t messageSize = LittleEndian::Get16(messageSizeBuf);
        if (messageSize >= kMaxMessageSize)
        {
            // This message is too long for upper layers.
            return CHIP_ERROR_MESSAGE_TOO_LONG;
        }
        // The subtraction will not underflow because we successfully read kPacketSizeBytes.
        if (messageSize > (state->mReceived->TotalLength() - kPacketSizeBytes))
        {
            // We have not yet received the complete message.
            return CHIP_NO_ERROR;
        }
        state->mReceived.Consume(kPacketSizeBytes);
        ReturnErrorOnFailure(ProcessSingleMessage(peerAddress, state, messageSize));
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR TCPBase::ProcessSingleMessage(const PeerAddress & peerAddress, ActiveConnectionState * state, uint16_t messageSize)
{
    // We enter with `state->mReceived` containing at least one full message, perhaps in a chain.
    // `state->mReceived->Start()` currently points to the message data.
    // On exit, `state->mReceived` will have had `messageSize` bytes consumed, no matter what.
    System::PacketBufferHandle message;
    if (state->mReceived->DataLength() == messageSize)
    {
        // In this case, the head packet buffer contains exactly the message.
        // This is common because typical messages fit in a network packet, and are delivered as such.
        // Peel off the head to pass upstream, which effectively consumes it from `state->mReceived`.
        message = state->mReceived.PopHead();
    }
    else
    {
        // The message is either longer or shorter than the head buffer.
        // In either case, copy the message to a fresh linear buffer to pass upstream. We always copy, rather than provide
        // a shared reference to the current buffer, in case upper layers manipulate the buffer in ways that would affect
        // our use, e.g. chaining it elsewhere or reusing space beyond the current message.
        message = System::PacketBufferHandle::New(messageSize, 0);
        if (message.IsNull())
        {
            return CHIP_ERROR_NO_MEMORY;
        }
        CHIP_ERROR err = state->mReceived->Read(message->Start(), messageSize);
        state->mReceived.Consume(messageSize);
        ReturnErrorOnFailure(err);
        message->SetDataLength(messageSize);
    }

    HandleMessageReceived(peerAddress, std::move(message));
    return CHIP_NO_ERROR;
}

void TCPBase::CloseConnectionInternal(ActiveConnectionState * connection, CHIP_ERROR err, SuppressCallback suppressCallback)
{
    State prevState;

    if (connection == nullptr)
    {
        return;
    }

    if (connection->mConnectionState != State::kClosed && connection->mEndPoint)
    {
        if (err == CHIP_NO_ERROR)
        {
            connection->mEndPoint->Close();
        }
        else
        {
            connection->mEndPoint->Abort();
        }

        prevState                    = connection->mConnectionState;
        connection->mConnectionState = State::kClosed;

        if (suppressCallback == SuppressCallback::No)
        {
            if (prevState == State::kConnecting)
            {
                // Call upper layer connection complete handler
                HandleConnectionComplete(connection->mEndPoint, err);
            }
            else
            {
                // Call upper layer connection closed handler
                HandleConnectionClosed(connection->mEndPoint, err);
            }
        }

        connection->Free();
        mUsedEndPointCount--;
    }
}

CHIP_ERROR TCPBase::HandleTCPEndPointDataReceived(Inet::TCPEndPoint * endPoint, System::PacketBufferHandle && buffer)
{
    Inet::IPAddress ipAddress;
    uint16_t port;
    Inet::InterfaceId interfaceId;

    endPoint->GetPeerInfo(&ipAddress, &port);
    endPoint->GetInterfaceId(&interfaceId);
    PeerAddress peerAddress = PeerAddress::TCP(ipAddress, port, interfaceId);

    TCPBase * tcp  = reinterpret_cast<TCPBase *>(endPoint->mAppState);
    CHIP_ERROR err = tcp->ProcessReceivedBuffer(endPoint, peerAddress, std::move(buffer));

    if (err != CHIP_NO_ERROR)
    {
        // Connection could need to be closed at this point
        ChipLogError(Inet, "Failed to accept received TCP message: %s", ErrorStr(err));
        return CHIP_ERROR_UNEXPECTED_EVENT;
    }
    return CHIP_NO_ERROR;
}

void TCPBase::HandleTCPEndPointConnectComplete(Inet::TCPEndPoint * endPoint, CHIP_ERROR conErr)
{
    CHIP_ERROR err          = CHIP_NO_ERROR;
    bool foundPendingPacket = false;
    TCPBase * tcp           = reinterpret_cast<TCPBase *>(endPoint->mAppState);
    Inet::IPAddress ipAddress;
    uint16_t port;
    Inet::InterfaceId interfaceId;
    ActiveConnectionState * activeConnection = nullptr;

    endPoint->GetPeerInfo(&ipAddress, &port);
    endPoint->GetInterfaceId(&interfaceId);
    PeerAddress addr = PeerAddress::TCP(ipAddress, port, interfaceId);

    if (conErr == CHIP_NO_ERROR)
    {
        // Set the Data received handler when connection completes
        endPoint->OnDataReceived     = HandleTCPEndPointDataReceived;
        endPoint->OnDataSent         = nullptr;
        endPoint->OnConnectionClosed = HandleTCPEndPointConnectionClosed;

        activeConnection = tcp->FindActiveConnection(endPoint);
        VerifyOrDie(activeConnection != nullptr);

        activeConnection->Init(endPoint, addr);
        activeConnection->mConnectionState = State::kConnected;

        // Disable TCP Nagle buffering by setting TCP_NODELAY socket option to true
        err = endPoint->EnableNoDelay();
        if (err != CHIP_NO_ERROR)
        {
            tcp->CloseConnectionInternal(activeConnection, err, SuppressCallback::No);
            return;
        }

        // Send any pending packets that are queued for this connection
        tcp->mPendingPackets.ForEachActiveObject([&](PendingPacket * pending) {
            if (pending->mPeerAddress == addr)
            {
                foundPendingPacket                = true;
                System::PacketBufferHandle buffer = std::move(pending->mPacketBuffer);
                tcp->mPendingPackets.ReleaseObject(pending);

                if ((conErr == CHIP_NO_ERROR) && (err == CHIP_NO_ERROR))
                {
                    err = endPoint->Send(std::move(buffer));
                }
            }
            return Loop::Continue;
        });

        // Let higher layer/delegate know that connection is successfully
        // established
        tcp->HandleConnectionComplete(endPoint, CHIP_NO_ERROR);
    }
    else
    {
        ChipLogError(Inet, "Connection complete encountered an error: %s", ErrorStr(err));
        endPoint->Free();
        tcp->mUsedEndPointCount--;
    }
}

void TCPBase::HandleTCPEndPointConnectionClosed(Inet::TCPEndPoint * endPoint, CHIP_ERROR err)
{
    TCPBase * tcp                            = reinterpret_cast<TCPBase *>(endPoint->mAppState);
    ActiveConnectionState * activeConnection = tcp->FindActiveConnection(endPoint);
    if (activeConnection == nullptr)
    {
        endPoint->Free();
        return;
    }

    if (err == CHIP_NO_ERROR && activeConnection->mConnectionState == State::kConnected)
    {
        err = CHIP_ERROR_CONNECTION_CLOSED_UNEXPECTEDLY;
    }

    tcp->CloseConnectionInternal(activeConnection, err, SuppressCallback::No);

    ChipLogProgress(Inet, "Connection closed.");
}

// Handler for incoming connection requests from peer nodes
void TCPBase::HandleIncomingConnection(Inet::TCPEndPoint * listenEndPoint, Inet::TCPEndPoint * endPoint,
                                       const Inet::IPAddress & peerAddress, uint16_t peerPort)
{
    TCPBase * tcp                            = reinterpret_cast<TCPBase *>(listenEndPoint->mAppState);
    ActiveConnectionState * activeConnection = nullptr;
    Inet::InterfaceId interfaceId;
    Inet::IPAddress ipAddress;
    uint16_t port;

    endPoint->GetPeerInfo(&ipAddress, &port);
    endPoint->GetInterfaceId(&interfaceId);
    PeerAddress addr = PeerAddress::TCP(ipAddress, port, interfaceId);

    if (tcp->mUsedEndPointCount < tcp->mActiveConnectionsSize)
    {
        activeConnection = tcp->AllocateConnection();

        endPoint->mAppState          = listenEndPoint->mAppState;
        endPoint->OnDataReceived     = HandleTCPEndPointDataReceived;
        endPoint->OnDataSent         = nullptr;
        endPoint->OnConnectionClosed = HandleTCPEndPointConnectionClosed;

        // By default, disable TCP Nagle buffering by setting TCP_NODELAY socket option to true
        endPoint->EnableNoDelay();

        // Update state for the active connection
        activeConnection->Init(endPoint, addr);
        tcp->mUsedEndPointCount++;
        activeConnection->mConnectionState = State::kConnected;
    }
    else
    {
        ChipLogError(Inet, "Insufficient connection space to accept new connections");
        endPoint->Free();
        listenEndPoint->OnAcceptError(endPoint, CHIP_ERROR_TOO_MANY_CONNECTIONS);
    }
}

void TCPBase::HandleAcceptError(Inet::TCPEndPoint * endPoint, CHIP_ERROR err)
{
    endPoint->Free();
    ChipLogError(Inet, "Accept error: %s", ErrorStr(err));
}

CHIP_ERROR TCPBase::ConnectToPeer(const PeerAddress & address)
{
    VerifyOrReturnError(mState == State::kInitialized, CHIP_ERROR_INCORRECT_STATE);

    // Verify that PeerAddress AddressType is TCP
    VerifyOrReturnError(address.GetTransportType() == Transport::Type::kTcp, CHIP_ERROR_INVALID_ARGUMENT);

    VerifyOrReturnError(mUsedEndPointCount < mActiveConnectionsSize, CHIP_ERROR_NO_MEMORY);

    ChipLogProgress(Inet, "Con start to peer");

    ReturnErrorOnFailure(StartConnect(address));

    return CHIP_NO_ERROR;
}

void TCPBase::Disconnect(const PeerAddress & address)
{
    // Closes an existing connection
    for (size_t i = 0; i < mActiveConnectionsSize; i++)
    {
        if (mActiveConnections[i].InUse())
        {
            Inet::IPAddress ipAddress;
            uint16_t port;
            Inet::InterfaceId interfaceId;

            mActiveConnections[i].mEndPoint->GetPeerInfo(&ipAddress, &port);
            mActiveConnections[i].mEndPoint->GetInterfaceId(&interfaceId);
            if (address == PeerAddress::TCP(ipAddress, port, interfaceId))
            {
                // NOTE: this leaves the socket in TIME_WAIT.
                // Calling Abort() would clean it since SO_LINGER would be set to 0,
                // however this seems not to be useful.
                CloseConnectionInternal(&mActiveConnections[i], CHIP_NO_ERROR, SuppressCallback::Yes);
            }
        }
    }
}

void TCPBase::Disconnect(Inet::TCPEndPoint * endPoint, bool shouldAbort)
{

    if (endPoint == nullptr)
    {
        return;
    }

    // Closes an existing connection
    for (size_t i = 0; i < mActiveConnectionsSize; i++)
    {
        if (mActiveConnections[i].InUse())
        {
            if (endPoint == mActiveConnections[i].mEndPoint)
            {
                if (shouldAbort)
                {
                    CloseConnectionInternal(&mActiveConnections[i], CHIP_ERROR_CONNECTION_ABORTED, SuppressCallback::Yes);
                }
                else
                {
                    CloseConnectionInternal(&mActiveConnections[i], CHIP_NO_ERROR, SuppressCallback::Yes);
                }
            }
        }
    }
}

bool TCPBase::HasActiveConnections() const
{
    for (size_t i = 0; i < mActiveConnectionsSize; i++)
    {
        if (mActiveConnections[i].InUse())
        {
            return true;
        }
    }

    return false;
}

} // namespace Transport
} // namespace chip