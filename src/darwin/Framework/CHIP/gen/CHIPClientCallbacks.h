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

// THIS FILE IS GENERATED BY ZAP

#pragma once

#include <inttypes.h>

// Global Response Callbacks
typedef void (*DefaultSuccessCallback)(void * context);
typedef void (*DefaultFailureCallback)(void * context, uint8_t status);
typedef void (*BooleanAttributeCallback)(void * context, bool value);
typedef void (*Int8uAttributeCallback)(void * context, uint8_t value);
typedef void (*Int8sAttributeCallback)(void * context, int8_t value);
typedef void (*Int16uAttributeCallback)(void * context, uint16_t value);
typedef void (*Int16sAttributeCallback)(void * context, int16_t value);
typedef void (*Int32uAttributeCallback)(void * context, uint32_t value);
typedef void (*Int32sAttributeCallback)(void * context, int32_t value);
typedef void (*Int64uAttributeCallback)(void * context, uint64_t value);
typedef void (*Int64sAttributeCallback)(void * context, int64_t value);
typedef void (*ReadReportingConfigurationReportedCallback)(void* context, uint16_t minInterval, uint16_t maxInterval);
typedef void (*ReadReportingConfigurationReceivedCallback)(void* context, uint16_t timeout);

// Cluster Specific Response Callbacks
typedef void (*DoorLockClusterClearAllPinsResponseCallback)(void * context);
typedef void (*DoorLockClusterClearAllRfidsResponseCallback)(void * context);
typedef void (*DoorLockClusterClearHolidayScheduleResponseCallback)(void * context);
typedef void (*DoorLockClusterClearPinResponseCallback)(void * context);
typedef void (*DoorLockClusterClearRfidResponseCallback)(void * context);
typedef void (*DoorLockClusterClearWeekdayScheduleResponseCallback)(void * context);
typedef void (*DoorLockClusterClearYeardayScheduleResponseCallback)(void * context);
typedef void (*DoorLockClusterGetHolidayScheduleResponseCallback)(void * context, uint8_t scheduleId, uint32_t localStartTime, uint32_t localEndTime, uint8_t operatingModeDuringHoliday);
typedef void (*DoorLockClusterGetLogRecordResponseCallback)(void * context, uint16_t logEntryId, uint32_t timestamp, uint8_t eventType, uint8_t source, uint8_t eventIdOrAlarmCode, uint16_t userId, uint8_t * pin);
typedef void (*DoorLockClusterGetPinResponseCallback)(void * context, uint16_t userId, uint8_t userStatus, uint8_t userType, uint8_t * pin);
typedef void (*DoorLockClusterGetRfidResponseCallback)(void * context, uint16_t userId, uint8_t userStatus, uint8_t userType, uint8_t * rfid);
typedef void (*DoorLockClusterGetUserTypeResponseCallback)(void * context, uint16_t userId, uint8_t userType);
typedef void (*DoorLockClusterGetWeekdayScheduleResponseCallback)(void * context, uint8_t scheduleId, uint16_t userId, uint8_t daysMask, uint8_t startHour, uint8_t startMinute, uint8_t endHour, uint8_t endMinute);
typedef void (*DoorLockClusterGetYeardayScheduleResponseCallback)(void * context, uint8_t scheduleId, uint16_t userId, uint32_t localStartTime, uint32_t localEndTime);
typedef void (*DoorLockClusterLockDoorResponseCallback)(void * context);
typedef void (*DoorLockClusterSetHolidayScheduleResponseCallback)(void * context);
typedef void (*DoorLockClusterSetPinResponseCallback)(void * context);
typedef void (*DoorLockClusterSetRfidResponseCallback)(void * context);
typedef void (*DoorLockClusterSetUserTypeResponseCallback)(void * context);
typedef void (*DoorLockClusterSetWeekdayScheduleResponseCallback)(void * context);
typedef void (*DoorLockClusterSetYeardayScheduleResponseCallback)(void * context);
typedef void (*DoorLockClusterUnlockDoorResponseCallback)(void * context);
typedef void (*DoorLockClusterUnlockWithTimeoutResponseCallback)(void * context);
typedef void (*GroupsClusterAddGroupResponseCallback)(void * context, uint16_t groupId);
typedef void (*GroupsClusterGetGroupMembershipResponseCallback)(void * context, uint8_t capacity, uint8_t groupCount, /* TYPE WARNING: array array defaults to */ uint8_t *  groupList);
typedef void (*GroupsClusterRemoveGroupResponseCallback)(void * context, uint16_t groupId);
typedef void (*GroupsClusterViewGroupResponseCallback)(void * context, uint16_t groupId, uint8_t * groupName);
typedef void (*IdentifyClusterIdentifyQueryResponseCallback)(void * context, uint16_t timeout);
typedef void (*ScenesClusterAddSceneResponseCallback)(void * context, uint16_t groupId, uint8_t sceneId);
typedef void (*ScenesClusterGetSceneMembershipResponseCallback)(void * context, uint8_t capacity, uint16_t groupId, uint8_t sceneCount, /* TYPE WARNING: array array defaults to */ uint8_t *  sceneList);
typedef void (*ScenesClusterRemoveAllScenesResponseCallback)(void * context, uint16_t groupId);
typedef void (*ScenesClusterRemoveSceneResponseCallback)(void * context, uint16_t groupId, uint8_t sceneId);
typedef void (*ScenesClusterStoreSceneResponseCallback)(void * context, uint16_t groupId, uint8_t sceneId);
typedef void (*ScenesClusterViewSceneResponseCallback)(void * context, uint16_t groupId, uint8_t sceneId, uint16_t transitionTime, uint8_t * sceneName, /* TYPE WARNING: array array defaults to */ uint8_t *  extensionFieldSets);
