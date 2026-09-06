/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <AVFoundation/AVFoundation.h>
#ifdef WEBRTC_IOS
#import <UIKit/UIKit.h>
#endif

#import "modules/video_capture/objc/device_info_objc.h"
#import "modules/video_capture/objc/rtc_video_capture_objc.h"

#include "rtc_base/logging.h"
#include "third_party/libyuv/include/libyuv.h"
#include "api/video/i420_buffer.h"

using namespace webrtc;
using namespace webrtc::videocapturemodule;

@interface RTCVideoCaptureIosObjC (hidden)
- (int)changeCaptureInputWithName:(NSString*)captureDeviceName;
@end

@implementation RTCVideoCaptureIosObjC {
  webrtc::videocapturemodule::VideoCaptureIos* _owner;
  webrtc::VideoCaptureCapability _capability;
  AVCaptureSession* _captureSession;
  BOOL _orientationHasChanged;
  AVCaptureConnection* _connection;
  BOOL _captureChanging;  // Guarded by _captureChangingCondition.
  NSCondition* _captureChangingCondition;
}

@synthesize frameRotation = _framRotation;

- (id)initWithOwner:(VideoCaptureIos*)owner {
  if (self == [super init]) {
    _owner = owner;
    _captureSession = [[AVCaptureSession alloc] init];
#if defined(WEBRTC_IOS)
    _captureSession.usesApplicationAudioSession = NO;
#endif
    _captureChanging = NO;
    _captureChangingCondition = [[NSCondition alloc] init];

    if (!_captureSession || !_captureChangingCondition) {
      return nil;
    }

    // create and configure a new output (using callbacks)
    AVCaptureVideoDataOutput* captureOutput = [[AVCaptureVideoDataOutput alloc] init];
    NSString* key = (NSString*)kCVPixelBufferPixelFormatTypeKey;

    NSNumber* val = [NSNumber numberWithUnsignedInt:kCVPixelFormatType_420YpCbCr8BiPlanarFullRange];
    NSDictionary* videoSettings = [NSDictionary dictionaryWithObject:val forKey:key];
    captureOutput.videoSettings = videoSettings;

    // add new output
    if ([_captureSession canAddOutput:captureOutput]) {
      [_captureSession addOutput:captureOutput];
    } else {
      RTC_LOG(LS_ERROR) << __FUNCTION__ << ": Could not add output to AVCaptureSession";
    }

#ifdef WEBRTC_IOS
    [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];

    NSNotificationCenter* notify = [NSNotificationCenter defaultCenter];
    [notify addObserver:self
               selector:@selector(onVideoError:)
                   name:AVCaptureSessionRuntimeErrorNotification
                 object:_captureSession];
    [notify addObserver:self
               selector:@selector(deviceOrientationDidChange:)
                   name:UIDeviceOrientationDidChangeNotification
                 object:nil];
#endif
  }

  return self;
}

- (void)directOutputToSelf {
  [[self currentOutput]
      setSampleBufferDelegate:self
                        queue:dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];
}

- (void)directOutputToNil {
  [[self currentOutput] setSampleBufferDelegate:nil queue:NULL];
}

- (void)deviceOrientationDidChange:(NSNotification*)notification {
  _orientationHasChanged = YES;
  [self setRelativeVideoOrientation];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (BOOL)setCaptureDeviceByUniqueId:(NSString*)uniqueId {
  [self waitForCaptureChangeToFinish];
  // check to see if the camera is already set
  if (_captureSession) {
    NSArray* currentInputs = [NSArray arrayWithArray:[_captureSession inputs]];
    if ([currentInputs count] > 0) {
      AVCaptureDeviceInput* currentInput = [currentInputs objectAtIndex:0];
      if ([uniqueId isEqualToString:[currentInput.device localizedName]]) {
        return YES;
      }
    }
  }

  return [self changeCaptureInputByUniqueId:uniqueId];
}

- (BOOL)startCaptureWithCapability:(const VideoCaptureCapability&)capability {
  [self waitForCaptureChangeToFinish];
  if (!_captureSession) {
    return NO;
  }

  // check limits of the resolution
  if (capability.maxFPS < 0 || capability.maxFPS > 60) {
    return NO;
  }

  if ([_captureSession canSetSessionPreset:AVCaptureSessionPreset1280x720]) {
    if (capability.width > 1280 || capability.height > 720) {
      return NO;
    }
  } else if ([_captureSession canSetSessionPreset:AVCaptureSessionPreset640x480]) {
    if (capability.width > 640 || capability.height > 480) {
      return NO;
    }
  } else if ([_captureSession canSetSessionPreset:AVCaptureSessionPreset352x288]) {
    if (capability.width > 352 || capability.height > 288) {
      return NO;
    }
  } else if (capability.width < 0 || capability.height < 0) {
    return NO;
  }

  _capability = capability;

  AVCaptureVideoDataOutput* currentOutput = [self currentOutput];
  if (!currentOutput) return NO;

  [self directOutputToSelf];

  _orientationHasChanged = NO;
  _captureChanging = YES;
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    [self startCaptureInBackgroundWithOutput:currentOutput];
  });
  return YES;
}

- (AVCaptureVideoDataOutput*)currentOutput {
  return [[_captureSession outputs] firstObject];
}

- (void)startCaptureInBackgroundWithOutput:(AVCaptureVideoDataOutput*)currentOutput {
  NSString* captureQuality = [NSString stringWithString:AVCaptureSessionPresetLow];
  if (_capability.width >= 1280 || _capability.height >= 720) {
    captureQuality = [NSString stringWithString:AVCaptureSessionPreset1280x720];
  } else if (_capability.width >= 640 || _capability.height >= 480) {
    captureQuality = [NSString stringWithString:AVCaptureSessionPreset640x480];
  } else if (_capability.width >= 352 || _capability.height >= 288) {
    captureQuality = [NSString stringWithString:AVCaptureSessionPreset352x288];
  }

  // begin configuration for the AVCaptureSession
  [_captureSession beginConfiguration];

  // picture resolution
  [_captureSession setSessionPreset:captureQuality];

  _connection = [currentOutput connectionWithMediaType:AVMediaTypeVideo];
  [self setRelativeVideoOrientation];

  // finished configuring, commit settings to AVCaptureSession.
  [_captureSession commitConfiguration];

  [_captureSession startRunning];
  [self signalCaptureChangeEnd];
}

- (void)setRelativeVideoOrientation {
  if (!_connection.supportsVideoOrientation) {
    return;
  }
#ifndef WEBRTC_IOS
  _connection.videoOrientation = AVCaptureVideoOrientationLandscapeRight;
  return;
#else
  switch ([UIDevice currentDevice].orientation) {
    case UIDeviceOrientationPortrait:
      _connection.videoOrientation = AVCaptureVideoOrientationPortrait;
      break;
    case UIDeviceOrientationPortraitUpsideDown:
      _connection.videoOrientation = AVCaptureVideoOrientationPortraitUpsideDown;
      break;
    case UIDeviceOrientationLandscapeLeft:
      _connection.videoOrientation = AVCaptureVideoOrientationLandscapeRight;
      break;
    case UIDeviceOrientationLandscapeRight:
      _connection.videoOrientation = AVCaptureVideoOrientationLandscapeLeft;
      break;
    case UIDeviceOrientationFaceUp:
    case UIDeviceOrientationFaceDown:
    case UIDeviceOrientationUnknown:
      if (!_orientationHasChanged) {
        _connection.videoOrientation = AVCaptureVideoOrientationPortrait;
      }
      break;
  }
#endif
}

- (void)onVideoError:(NSNotification*)notification {
  NSLog(@"onVideoError: %@", notification);
  // TODO(sjlee): make the specific error handling with this notification.
  RTC_LOG(LS_ERROR) << __FUNCTION__ << ": [AVCaptureSession startRunning] error.";
}

- (BOOL)stopCapture {
#ifdef WEBRTC_IOS
  [[UIDevice currentDevice] endGeneratingDeviceOrientationNotifications];
#endif
  _orientationHasChanged = NO;
  [self waitForCaptureChangeToFinish];
  [self directOutputToNil];

  if (!_captureSession) {
    return NO;
  }

  _captureChanging = YES;
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(void) {
    [self stopCaptureInBackground];
  });
  return YES;
}

- (void)stopCaptureInBackground {
  [_captureSession stopRunning];
  [self signalCaptureChangeEnd];
}

- (BOOL)changeCaptureInputByUniqueId:(NSString*)uniqueId {
  [self waitForCaptureChangeToFinish];
  NSArray* currentInputs = [_captureSession inputs];
  // remove current input
  if ([currentInputs count] > 0) {
    AVCaptureInput* currentInput = (AVCaptureInput*)[currentInputs objectAtIndex:0];

    [_captureSession removeInput:currentInput];
  }

  // Look for input device with the name requested (as our input param)
  // get list of available capture devices
  int captureDeviceCount = [DeviceInfoIosObjC captureDeviceCount];
  if (captureDeviceCount <= 0) {
    return NO;
  }

  AVCaptureDevice* captureDevice = [DeviceInfoIosObjC captureDeviceForUniqueId:uniqueId];

  if (!captureDevice) {
    return NO;
  }

  // now create capture session input out of AVCaptureDevice
  NSError* deviceError = nil;
  AVCaptureDeviceInput* newCaptureInput =
      [AVCaptureDeviceInput deviceInputWithDevice:captureDevice error:&deviceError];

  if (!newCaptureInput) {
    const char* errorMessage = [[deviceError localizedDescription] UTF8String];

    RTC_LOG(LS_ERROR) << __FUNCTION__ << ": deviceInputWithDevice error:" << errorMessage;

    return NO;
  }

  // try to add our new capture device to the capture session
  [_captureSession beginConfiguration];

  BOOL addedCaptureInput = NO;
  if ([_captureSession canAddInput:newCaptureInput]) {
    [_captureSession addInput:newCaptureInput];
    addedCaptureInput = YES;
  } else {
    addedCaptureInput = NO;
  }

  [_captureSession commitConfiguration];

  return addedCaptureInput;
}

// 定义 iOS 物理朝向枚举
typedef enum {
    kIOSOrientPortrait = 0,          // 竖屏 (Home 键朝下)
    kIOSOrientPortraitUpsideDown,    // 竖屏倒置 (Home 键朝上)
    kIOSOrientLandscapeLeft,         // 左横屏 (Home 键在右侧)
    kIOSOrientLandscapeRight         // 右横屏 (Home 键在左侧)
} IOSPhysicalOrientation;


- (void)captureOutput:(AVCaptureOutput*)captureOutput
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection*)connection {

    CVImageBufferRef videoFrame = CMSampleBufferGetImageBuffer(sampleBuffer);
    const int kFlags = 0;
    if (CVPixelBufferLockBaseAddress(videoFrame, kFlags) != kCVReturnSuccess) {
        return;
    }

    // 1. 直接获取摄像头吐出的原始宽高（不需任何额外处理）
    int32_t width = (int32_t)CVPixelBufferGetWidth(videoFrame);
    int32_t height = (int32_t)CVPixelBufferGetHeight(videoFrame);

    // ========= 核心逻辑 1：根据物理朝向，更新全局角度变量 =========
    // 获取当前设备的朝向并赋值给全局变量
    IOSPhysicalOrientation g_ios_physical_orientation = kIOSOrientPortrait;
    
    UIDeviceOrientation ori = [UIDevice currentDevice].orientation;
    if (ori == UIDeviceOrientationPortrait) {
        g_ios_physical_orientation = kIOSOrientPortrait;
    } else if (ori == UIDeviceOrientationPortraitUpsideDown) {
        g_ios_physical_orientation = kIOSOrientPortraitUpsideDown;
    } else if (ori == UIDeviceOrientationLandscapeLeft) {
        g_ios_physical_orientation = kIOSOrientLandscapeLeft;
    } else if (ori == UIDeviceOrientationLandscapeRight) {
        g_ios_physical_orientation = kIOSOrientLandscapeRight;
    } else {
        // FaceUp, FaceDown, Unknown 等情况，默认当做竖屏处理
        g_ios_physical_orientation = kIOSOrientPortrait;
    }
    // ==============================================================

    const int kYPlaneIndex = 0;
    const int kUVPlaneIndex = 1;

    uint8_t* src_y = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(videoFrame, kYPlaneIndex);
    int32_t src_stride_y = (int32_t)CVPixelBufferGetBytesPerRowOfPlane(videoFrame, kYPlaneIndex);

    uint8_t* src_uv = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(videoFrame, kUVPlaneIndex);
    int32_t src_stride_uv = (int32_t)CVPixelBufferGetBytesPerRowOfPlane(videoFrame, kUVPlaneIndex);

    // ========= 核心逻辑 2：NV12 转 I420 解决行对齐花屏 =========
    // 注意：这里务必保持原始宽高(width, height)，不要让尺寸发生变化！
    rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer =
        webrtc::I420Buffer::Create(width, height, width, width/2, width/2);

    libyuv::NV12ToI420(
        src_y, src_stride_y,
        src_uv, src_stride_uv,
        i420_buffer->MutableDataY(), i420_buffer->StrideY(),
        i420_buffer->MutableDataU(), i420_buffer->StrideU(),
        i420_buffer->MutableDataV(), i420_buffer->StrideV(),
        width, height);

    CVPixelBufferUnlockBaseAddress(videoFrame, kFlags);

    VideoCaptureCapability tempCaptureCapability;
    tempCaptureCapability.width = width;
    tempCaptureCapability.height = height;
    tempCaptureCapability.maxFPS = _capability.maxFPS;
    tempCaptureCapability.videoType = VideoType::kI420;

    size_t i420_data_size = (size_t)(width * height * 3 / 2);
    
    // 将数据原封不动地传给 OnFrame
    _owner->IncomingFrame((uint8_t*)i420_buffer->DataY(), i420_data_size, tempCaptureCapability, g_ios_physical_orientation);
}

- (void)signalCaptureChangeEnd {
  [_captureChangingCondition lock];
  _captureChanging = NO;
  [_captureChangingCondition signal];
  [_captureChangingCondition unlock];
}

- (void)waitForCaptureChangeToFinish {
  [_captureChangingCondition lock];
  while (_captureChanging) {
    [_captureChangingCondition wait];
  }
  [_captureChangingCondition unlock];
}
@end
