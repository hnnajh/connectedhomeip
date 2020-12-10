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

/**
 *    @file
 *      This file describes a Additional Data Payload class to hold
 *      data enumerated from a byte stream
 */

#ifndef ADDITIONAL_DATA_PAYLOAD_H
#define ADDITIONAL_DATA_PAYLOAD_H

#include <vector>

namespace chip {

const uint8_t   kRotatingDeviceIdTag                    = 0x01;
const uint32_t  kTag_AdditionalDataExensionDescriptor   = 0x00;

class AdditionalDataPayload
{
public:
    std::vector<uint8_t> rotatingDeviceId;
};

} // namespace chip

#endif  // ADDITIONAL_DATA_PAYLOAD_H
