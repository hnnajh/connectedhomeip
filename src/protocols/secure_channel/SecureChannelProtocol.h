/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
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
 *      The defines constants for the CHIP Secure Channel Protocol, present in
 *      every CHIP device.
 *
 */

#pragma once

/**
 *   @namespace chip::Protocols::SecureChannel
 *
 *   @brief
 *     This namespace includes all interfaces within CHIP for the
 *     CHIP SecureChannel protocol.
 *
 *     The interfaces define message types and status codes.
 */

namespace chip {
namespace Protocols {
namespace SecureChannel {

/**
 * SecureChannel Protocol Message Types
 */
enum
{
    // Message Counter Synchronization Protocol Message Types
    kMsgType_MsgCounterSyncReq = 0x00,
    kMsgType_MsgCounterSyncRsp = 0x01,

    // Reliable Messaging Protocol Message Types
    kMsgType_StandaloneAck = 0x10,

    // Password-based session establishment Message Types
    kMsgType_PASE_Spake2pA = 0x20,
    kMsgType_PASE_Spake2pB = 0x21,
    kMsgType_PASE_Spake2cA = 0x22,

    // Certificate-based session establishment Message Types
    kMsgType_CASE_SigmaR1  = 0x30,
    kMsgType_CASE_SigmaR2  = 0x31,
    kMsgType_CASE_SigmaR3  = 0x32,
    kMsgType_CASE_SigmaErr = 0x3F,
};

/**
 * SecureChannel Protocol Status Codes
 */
enum
{
    kStatusCode_AlreadyMemberOfFabric = 1, /**< The recipient is already a member of a fabric. */
    kStatusCode_NotMemberOfFabric     = 2, /**< The recipient is not a member of a fabric. */
    kStatusCode_InvalidFabricConfig   = 3  /**< The specified fabric configuration was invalid. */
};

} // namespace SecureChannel
} // namespace Protocols
} // namespace chip
