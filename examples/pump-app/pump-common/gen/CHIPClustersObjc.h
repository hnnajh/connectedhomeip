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

#ifndef CHIP_CLUSTERS_H
#define CHIP_CLUSTERS_H

#import <Foundation/Foundation.h>

@class CHIPDevice;

typedef void (^ResponseHandler)(NSError * _Nullable error, NSDictionary * _Nullable values);

NS_ASSUME_NONNULL_BEGIN

/**
 * CHIPCluster
 *    This is the base class for clusters.
 */
@interface CHIPCluster : NSObject

- (nullable instancetype)initWithDevice:(CHIPDevice *)device
                               endpoint:(uint8_t)endpoint
                                  queue:(dispatch_queue_t)queue NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

/**
 * Cluster Identify
 *
 */
@interface CHIPIdentify : CHIPCluster

- (void)identify:(uint16_t)identifyTime responseHandler:(ResponseHandler)responseHandler;
- (void)identifyQuery:(ResponseHandler)responseHandler;

- (void)readAttributeIdentifyTimeWithResponseHandler:(ResponseHandler)responseHandler;
- (void)writeAttributeIdentifyTimeWithValue:(uint16_t)value responseHandler:(ResponseHandler)responseHandler;
- (void)readAttributeClusterRevisionWithResponseHandler:(ResponseHandler)responseHandler;

@end

NS_ASSUME_NONNULL_END

#endif /* CHIP_CLUSTERS_H */