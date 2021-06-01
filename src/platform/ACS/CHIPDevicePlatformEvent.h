/**
 *    @file
 *          Defines platform-specific event types and data for the chip
 *          Device Layer on ACS platforms.
 */

#pragma once

#include <platform/CHIPDeviceEvent.h>

namespace chip {
namespace DeviceLayer {

namespace DeviceEventType {

/**
 * Enumerates ACS platform-specific event types that are visible to the application.
 */
enum PublicPlatformSpecificEventTypes
{
    /* None currently defined */
};

/**
 * Enumerates ACS platform-specific event types that are internal to the chip Device Layer.
 */
enum InternalPlatformSpecificEventTypes
{
    kPlatformLinuxEvent = kRange_InternalPlatformSpecific,
    kPlatformLinuxBLECentralConnected,
    kPlatformLinuxBLECentralConnectFailed,
    kPlatformLinuxBLEWriteComplete,
    kPlatformLinuxBLESubscribeOpComplete,
    kPlatformLinuxBLEIndicationReceived,
    kPlatformLinuxBLEC1WriteEvent,
    kPlatformLinuxBLEOutOfBuffersEvent,
    kPlatformLinuxBLEPeripheralRegisterAppComplete,
    kPlatformLinuxBLEPeripheralAdvConfiguredComplete,
    kPlatformLinuxBLEPeripheralAdvStartComplete,
    kPlatformLinuxBLEPeripheralAdvStopComplete
};

} // namespace DeviceEventType

/**
 * Represents platform-specific event information for ACS platforms.
 */
struct ChipDevicePlatformEvent
{
    union
    {
        struct
        {
            BLE_CONNECTION_OBJECT mConnection;
        } BLECentralConnected;
        struct
        {
            CHIP_ERROR mError;
        } BLECentralConnectFailed;
        struct
        {
            BLE_CONNECTION_OBJECT mConnection;
        } BLEWriteComplete;
        struct
        {
            BLE_CONNECTION_OBJECT mConnection;
            bool mIsSubscribed;
        } BLESubscribeOpComplete;
        struct
        {
            BLE_CONNECTION_OBJECT mConnection;
            chip::System::PacketBuffer * mData;
        } BLEIndicationReceived;
        struct
        {
            bool mIsSuccess;
            void * mpAppstate;
        } BLEPeripheralRegisterAppComplete;
        struct
        {
            bool mIsSuccess;
            void * mpAppstate;
        } BLEPeripheralAdvConfiguredComplete;
        struct
        {
            bool mIsSuccess;
            void * mpAppstate;
        } BLEPeripheralAdvStartComplete;
        struct
        {
            bool mIsSuccess;
            void * mpAppstate;
        } BLEPeripheralAdvStopComplete;
    };
};

} // namespace DeviceLayer
} // namespace chip
