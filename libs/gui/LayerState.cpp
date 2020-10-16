/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "LayerState"

#include <inttypes.h>

#include <binder/Parcel.h>
#include <gui/IGraphicBufferProducer.h>
#include <gui/ISurfaceComposerClient.h>
#include <gui/LayerState.h>
#include <utils/Errors.h>

#include <cmath>

namespace android {

layer_state_t::layer_state_t()
      : what(0),
        x(0),
        y(0),
        z(0),
        w(0),
        h(0),
        layerStack(0),
        alpha(0),
        flags(0),
        mask(0),
        reserved(0),
        crop_legacy(Rect::INVALID_RECT),
        cornerRadius(0.0f),
        backgroundBlurRadius(0),
        barrierFrameNumber(0),
        transform(0),
        transformToDisplayInverse(false),
        crop(Rect::INVALID_RECT),
        orientedDisplaySpaceRect(Rect::INVALID_RECT),
        dataspace(ui::Dataspace::UNKNOWN),
        surfaceDamageRegion(),
        api(-1),
        colorTransform(mat4()),
        bgColorAlpha(0),
        bgColorDataspace(ui::Dataspace::UNKNOWN),
        colorSpaceAgnostic(false),
        shadowRadius(0.0f),
        frameRateSelectionPriority(-1),
        frameRate(0.0f),
        frameRateCompatibility(ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_DEFAULT),
        fixedTransformHint(ui::Transform::ROT_INVALID),
        frameNumber(0) {
    matrix.dsdx = matrix.dtdy = 1.0f;
    matrix.dsdy = matrix.dtdx = 0.0f;
    hdrMetadata.validTypes = 0;
}

status_t layer_state_t::write(Parcel& output) const
{
    SAFE_PARCEL(output.writeStrongBinder, surface);
    SAFE_PARCEL(output.writeInt32, layerId);
    SAFE_PARCEL(output.writeUint64, what);
    SAFE_PARCEL(output.writeFloat, x);
    SAFE_PARCEL(output.writeFloat, y);
    SAFE_PARCEL(output.writeInt32, z);
    SAFE_PARCEL(output.writeUint32, w);
    SAFE_PARCEL(output.writeUint32, h);
    SAFE_PARCEL(output.writeUint32, layerStack);
    SAFE_PARCEL(output.writeFloat, alpha);
    SAFE_PARCEL(output.writeUint32, flags);
    SAFE_PARCEL(output.writeUint32, mask);
    SAFE_PARCEL(matrix.write, output);
    SAFE_PARCEL(output.write, crop_legacy);
    SAFE_PARCEL(SurfaceControl::writeNullableToParcel, output, barrierSurfaceControl_legacy);
    SAFE_PARCEL(SurfaceControl::writeNullableToParcel, output, reparentSurfaceControl);
    SAFE_PARCEL(output.writeUint64, barrierFrameNumber);
    SAFE_PARCEL(SurfaceControl::writeNullableToParcel, output, relativeLayerSurfaceControl);
    SAFE_PARCEL(SurfaceControl::writeNullableToParcel, output, parentSurfaceControlForChild);
    SAFE_PARCEL(output.writeFloat, color.r);
    SAFE_PARCEL(output.writeFloat, color.g);
    SAFE_PARCEL(output.writeFloat, color.b);
#ifndef NO_INPUT
    SAFE_PARCEL(inputHandle->writeToParcel, &output);
#endif
    SAFE_PARCEL(output.write, transparentRegion);
    SAFE_PARCEL(output.writeUint32, transform);
    SAFE_PARCEL(output.writeBool, transformToDisplayInverse);
    SAFE_PARCEL(output.write, crop);
    SAFE_PARCEL(output.write, orientedDisplaySpaceRect);

    if (buffer) {
        SAFE_PARCEL(output.writeBool, true);
        SAFE_PARCEL(output.write, *buffer);
    } else {
        SAFE_PARCEL(output.writeBool, false);
    }

    if (acquireFence) {
        SAFE_PARCEL(output.writeBool, true);
        SAFE_PARCEL(output.write, *acquireFence);
    } else {
        SAFE_PARCEL(output.writeBool, false);
    }

    SAFE_PARCEL(output.writeUint32, static_cast<uint32_t>(dataspace));
    SAFE_PARCEL(output.write, hdrMetadata);
    SAFE_PARCEL(output.write, surfaceDamageRegion);
    SAFE_PARCEL(output.writeInt32, api);

    if (sidebandStream) {
        SAFE_PARCEL(output.writeBool, true);
        SAFE_PARCEL(output.writeNativeHandle, sidebandStream->handle());
    } else {
        SAFE_PARCEL(output.writeBool, false);
    }

    SAFE_PARCEL(output.write, colorTransform.asArray(), 16 * sizeof(float));
    SAFE_PARCEL(output.writeFloat, cornerRadius);
    SAFE_PARCEL(output.writeUint32, backgroundBlurRadius);
    SAFE_PARCEL(output.writeStrongBinder, cachedBuffer.token.promote());
    SAFE_PARCEL(output.writeUint64, cachedBuffer.id);
    SAFE_PARCEL(output.writeParcelable, metadata);
    SAFE_PARCEL(output.writeFloat, bgColorAlpha);
    SAFE_PARCEL(output.writeUint32, static_cast<uint32_t>(bgColorDataspace));
    SAFE_PARCEL(output.writeBool, colorSpaceAgnostic);
    SAFE_PARCEL(output.writeVectorSize, listeners);

    for (auto listener : listeners) {
        SAFE_PARCEL(output.writeStrongBinder, listener.transactionCompletedListener);
        SAFE_PARCEL(output.writeInt64Vector, listener.callbackIds);
    }
    SAFE_PARCEL(output.writeFloat, shadowRadius);
    SAFE_PARCEL(output.writeInt32, frameRateSelectionPriority);
    SAFE_PARCEL(output.writeFloat, frameRate);
    SAFE_PARCEL(output.writeByte, frameRateCompatibility);
    SAFE_PARCEL(output.writeUint32, fixedTransformHint);
    SAFE_PARCEL(output.writeUint64, frameNumber);
    return NO_ERROR;
}

status_t layer_state_t::read(const Parcel& input)
{
    SAFE_PARCEL(input.readNullableStrongBinder, &surface);
    SAFE_PARCEL(input.readInt32, &layerId);
    SAFE_PARCEL(input.readUint64, &what);
    SAFE_PARCEL(input.readFloat, &x);
    SAFE_PARCEL(input.readFloat, &y);
    SAFE_PARCEL(input.readInt32, &z);
    SAFE_PARCEL(input.readUint32, &w);
    SAFE_PARCEL(input.readUint32, &h);
    SAFE_PARCEL(input.readUint32, &layerStack);
    SAFE_PARCEL(input.readFloat, &alpha);

    uint32_t tmpUint32 = 0;
    SAFE_PARCEL(input.readUint32, &tmpUint32);
    flags = static_cast<uint8_t>(tmpUint32);

    SAFE_PARCEL(input.readUint32, &tmpUint32);
    mask = static_cast<uint8_t>(tmpUint32);

    SAFE_PARCEL(matrix.read, input);
    SAFE_PARCEL(input.read, crop_legacy);
    SAFE_PARCEL(SurfaceControl::readNullableFromParcel, input, &barrierSurfaceControl_legacy);
    SAFE_PARCEL(SurfaceControl::readNullableFromParcel, input, &reparentSurfaceControl);
    SAFE_PARCEL(input.readUint64, &barrierFrameNumber);

    SAFE_PARCEL(SurfaceControl::readNullableFromParcel, input, &relativeLayerSurfaceControl);
    SAFE_PARCEL(SurfaceControl::readNullableFromParcel, input, &parentSurfaceControlForChild);

    float tmpFloat = 0;
    SAFE_PARCEL(input.readFloat, &tmpFloat);
    color.r = tmpFloat;
    SAFE_PARCEL(input.readFloat, &tmpFloat);
    color.g = tmpFloat;
    SAFE_PARCEL(input.readFloat, &tmpFloat);
    color.b = tmpFloat;
#ifndef NO_INPUT
    SAFE_PARCEL(inputHandle->readFromParcel, &input);
#endif

    SAFE_PARCEL(input.read, transparentRegion);
    SAFE_PARCEL(input.readUint32, &transform);
    SAFE_PARCEL(input.readBool, &transformToDisplayInverse);
    SAFE_PARCEL(input.read, crop);
    SAFE_PARCEL(input.read, orientedDisplaySpaceRect);

    bool tmpBool = false;
    SAFE_PARCEL(input.readBool, &tmpBool);
    if (tmpBool) {
        buffer = new GraphicBuffer();
        SAFE_PARCEL(input.read, *buffer);
    }

    SAFE_PARCEL(input.readBool, &tmpBool);
    if (tmpBool) {
        acquireFence = new Fence();
        SAFE_PARCEL(input.read, *acquireFence);
    }

    SAFE_PARCEL(input.readUint32, &tmpUint32);
    dataspace = static_cast<ui::Dataspace>(tmpUint32);

    SAFE_PARCEL(input.read, hdrMetadata);
    SAFE_PARCEL(input.read, surfaceDamageRegion);
    SAFE_PARCEL(input.readInt32, &api);
    SAFE_PARCEL(input.readBool, &tmpBool);
    if (tmpBool) {
        sidebandStream = NativeHandle::create(input.readNativeHandle(), true);
    }

    SAFE_PARCEL(input.read, &colorTransform, 16 * sizeof(float));
    SAFE_PARCEL(input.readFloat, &cornerRadius);
    SAFE_PARCEL(input.readUint32, &backgroundBlurRadius);
    sp<IBinder> tmpBinder;
    SAFE_PARCEL(input.readNullableStrongBinder, &tmpBinder);
    cachedBuffer.token = tmpBinder;
    SAFE_PARCEL(input.readUint64, &cachedBuffer.id);
    SAFE_PARCEL(input.readParcelable, &metadata);

    SAFE_PARCEL(input.readFloat, &bgColorAlpha);
    SAFE_PARCEL(input.readUint32, &tmpUint32);
    bgColorDataspace = static_cast<ui::Dataspace>(tmpUint32);
    SAFE_PARCEL(input.readBool, &colorSpaceAgnostic);

    int32_t numListeners = 0;
    SAFE_PARCEL_READ_SIZE(input.readInt32, &numListeners, input.dataSize());
    listeners.clear();
    for (int i = 0; i < numListeners; i++) {
        sp<IBinder> listener;
        std::vector<CallbackId> callbackIds;
        SAFE_PARCEL(input.readNullableStrongBinder, &listener);
        SAFE_PARCEL(input.readInt64Vector, &callbackIds);
        listeners.emplace_back(listener, callbackIds);
    }
    SAFE_PARCEL(input.readFloat, &shadowRadius);
    SAFE_PARCEL(input.readInt32, &frameRateSelectionPriority);
    SAFE_PARCEL(input.readFloat, &frameRate);
    SAFE_PARCEL(input.readByte, &frameRateCompatibility);
    SAFE_PARCEL(input.readUint32, &tmpUint32);
    fixedTransformHint = static_cast<ui::Transform::RotationFlags>(tmpUint32);
    SAFE_PARCEL(input.readUint64, &frameNumber);
    return NO_ERROR;
}

status_t ComposerState::write(Parcel& output) const {
    return state.write(output);
}

status_t ComposerState::read(const Parcel& input) {
    return state.read(input);
}

DisplayState::DisplayState()
      : what(0),
        layerStack(0),
        layerStackSpaceRect(Rect::EMPTY_RECT),
        orientedDisplaySpaceRect(Rect::EMPTY_RECT),
        width(0),
        height(0) {}

status_t DisplayState::write(Parcel& output) const {
    SAFE_PARCEL(output.writeStrongBinder, token);
    SAFE_PARCEL(output.writeStrongBinder, IInterface::asBinder(surface));
    SAFE_PARCEL(output.writeUint32, what);
    SAFE_PARCEL(output.writeUint32, layerStack);
    SAFE_PARCEL(output.writeUint32, toRotationInt(orientation));
    SAFE_PARCEL(output.write, layerStackSpaceRect);
    SAFE_PARCEL(output.write, orientedDisplaySpaceRect);
    SAFE_PARCEL(output.writeUint32, width);
    SAFE_PARCEL(output.writeUint32, height);
    return NO_ERROR;
}

status_t DisplayState::read(const Parcel& input) {
    SAFE_PARCEL(input.readStrongBinder, &token);
    sp<IBinder> tmpBinder;
    SAFE_PARCEL(input.readNullableStrongBinder, &tmpBinder);
    surface = interface_cast<IGraphicBufferProducer>(tmpBinder);

    SAFE_PARCEL(input.readUint32, &what);
    SAFE_PARCEL(input.readUint32, &layerStack);
    uint32_t tmpUint = 0;
    SAFE_PARCEL(input.readUint32, &tmpUint);
    orientation = ui::toRotation(tmpUint);

    SAFE_PARCEL(input.read, layerStackSpaceRect);
    SAFE_PARCEL(input.read, orientedDisplaySpaceRect);
    SAFE_PARCEL(input.readUint32, &width);
    SAFE_PARCEL(input.readUint32, &height);
    return NO_ERROR;
}

void DisplayState::merge(const DisplayState& other) {
    if (other.what & eSurfaceChanged) {
        what |= eSurfaceChanged;
        surface = other.surface;
    }
    if (other.what & eLayerStackChanged) {
        what |= eLayerStackChanged;
        layerStack = other.layerStack;
    }
    if (other.what & eDisplayProjectionChanged) {
        what |= eDisplayProjectionChanged;
        orientation = other.orientation;
        layerStackSpaceRect = other.layerStackSpaceRect;
        orientedDisplaySpaceRect = other.orientedDisplaySpaceRect;
    }
    if (other.what & eDisplaySizeChanged) {
        what |= eDisplaySizeChanged;
        width = other.width;
        height = other.height;
    }
}

void layer_state_t::merge(const layer_state_t& other) {
    if (other.what & ePositionChanged) {
        what |= ePositionChanged;
        x = other.x;
        y = other.y;
    }
    if (other.what & eLayerChanged) {
        what |= eLayerChanged;
        what &= ~eRelativeLayerChanged;
        z = other.z;
    }
    if (other.what & eSizeChanged) {
        what |= eSizeChanged;
        w = other.w;
        h = other.h;
    }
    if (other.what & eAlphaChanged) {
        what |= eAlphaChanged;
        alpha = other.alpha;
    }
    if (other.what & eMatrixChanged) {
        what |= eMatrixChanged;
        matrix = other.matrix;
    }
    if (other.what & eTransparentRegionChanged) {
        what |= eTransparentRegionChanged;
        transparentRegion = other.transparentRegion;
    }
    if (other.what & eFlagsChanged) {
        what |= eFlagsChanged;
        flags &= ~other.mask;
        flags |= (other.flags & other.mask);
        mask |= other.mask;
    }
    if (other.what & eLayerStackChanged) {
        what |= eLayerStackChanged;
        layerStack = other.layerStack;
    }
    if (other.what & eCropChanged_legacy) {
        what |= eCropChanged_legacy;
        crop_legacy = other.crop_legacy;
    }
    if (other.what & eCornerRadiusChanged) {
        what |= eCornerRadiusChanged;
        cornerRadius = other.cornerRadius;
    }
    if (other.what & eBackgroundBlurRadiusChanged) {
        what |= eBackgroundBlurRadiusChanged;
        backgroundBlurRadius = other.backgroundBlurRadius;
    }
    if (other.what & eDeferTransaction_legacy) {
        what |= eDeferTransaction_legacy;
        barrierSurfaceControl_legacy = other.barrierSurfaceControl_legacy;
        barrierFrameNumber = other.barrierFrameNumber;
    }
    if (other.what & eReparentChildren) {
        what |= eReparentChildren;
        reparentSurfaceControl = other.reparentSurfaceControl;
    }
    if (other.what & eDetachChildren) {
        what |= eDetachChildren;
    }
    if (other.what & eRelativeLayerChanged) {
        what |= eRelativeLayerChanged;
        what &= ~eLayerChanged;
        z = other.z;
        relativeLayerSurfaceControl = other.relativeLayerSurfaceControl;
    }
    if (other.what & eReparent) {
        what |= eReparent;
        parentSurfaceControlForChild = other.parentSurfaceControlForChild;
    }
    if (other.what & eDestroySurface) {
        what |= eDestroySurface;
    }
    if (other.what & eTransformChanged) {
        what |= eTransformChanged;
        transform = other.transform;
    }
    if (other.what & eTransformToDisplayInverseChanged) {
        what |= eTransformToDisplayInverseChanged;
        transformToDisplayInverse = other.transformToDisplayInverse;
    }
    if (other.what & eCropChanged) {
        what |= eCropChanged;
        crop = other.crop;
    }
    if (other.what & eFrameChanged) {
        what |= eFrameChanged;
        orientedDisplaySpaceRect = other.orientedDisplaySpaceRect;
    }
    if (other.what & eBufferChanged) {
        what |= eBufferChanged;
        buffer = other.buffer;
    }
    if (other.what & eAcquireFenceChanged) {
        what |= eAcquireFenceChanged;
        acquireFence = other.acquireFence;
    }
    if (other.what & eDataspaceChanged) {
        what |= eDataspaceChanged;
        dataspace = other.dataspace;
    }
    if (other.what & eHdrMetadataChanged) {
        what |= eHdrMetadataChanged;
        hdrMetadata = other.hdrMetadata;
    }
    if (other.what & eSurfaceDamageRegionChanged) {
        what |= eSurfaceDamageRegionChanged;
        surfaceDamageRegion = other.surfaceDamageRegion;
    }
    if (other.what & eApiChanged) {
        what |= eApiChanged;
        api = other.api;
    }
    if (other.what & eSidebandStreamChanged) {
        what |= eSidebandStreamChanged;
        sidebandStream = other.sidebandStream;
    }
    if (other.what & eColorTransformChanged) {
        what |= eColorTransformChanged;
        colorTransform = other.colorTransform;
    }
    if (other.what & eHasListenerCallbacksChanged) {
        what |= eHasListenerCallbacksChanged;
    }

#ifndef NO_INPUT
    if (other.what & eInputInfoChanged) {
        what |= eInputInfoChanged;
        inputHandle = new InputWindowHandle(*other.inputHandle);
    }
#endif

    if (other.what & eCachedBufferChanged) {
        what |= eCachedBufferChanged;
        cachedBuffer = other.cachedBuffer;
    }
    if (other.what & eBackgroundColorChanged) {
        what |= eBackgroundColorChanged;
        color = other.color;
        bgColorAlpha = other.bgColorAlpha;
        bgColorDataspace = other.bgColorDataspace;
    }
    if (other.what & eMetadataChanged) {
        what |= eMetadataChanged;
        metadata.merge(other.metadata);
    }
    if (other.what & eShadowRadiusChanged) {
        what |= eShadowRadiusChanged;
        shadowRadius = other.shadowRadius;
    }
    if (other.what & eFrameRateSelectionPriority) {
        what |= eFrameRateSelectionPriority;
        frameRateSelectionPriority = other.frameRateSelectionPriority;
    }
    if (other.what & eFrameRateChanged) {
        what |= eFrameRateChanged;
        frameRate = other.frameRate;
        frameRateCompatibility = other.frameRateCompatibility;
    }
    if (other.what & eFixedTransformHintChanged) {
        what |= eFixedTransformHintChanged;
        fixedTransformHint = other.fixedTransformHint;
    }
    if (other.what & eFrameNumberChanged) {
        what |= eFrameNumberChanged;
        frameNumber = other.frameNumber;
    }
    if ((other.what & what) != other.what) {
        ALOGE("Unmerged SurfaceComposer Transaction properties. LayerState::merge needs updating? "
              "other.what=0x%" PRIu64 " what=0x%" PRIu64,
              other.what, what);
    }
}

status_t layer_state_t::matrix22_t::write(Parcel& output) const {
    SAFE_PARCEL(output.writeFloat, dsdx);
    SAFE_PARCEL(output.writeFloat, dtdx);
    SAFE_PARCEL(output.writeFloat, dtdy);
    SAFE_PARCEL(output.writeFloat, dsdy);
    return NO_ERROR;
}

status_t layer_state_t::matrix22_t::read(const Parcel& input) {
    SAFE_PARCEL(input.readFloat, &dsdx);
    SAFE_PARCEL(input.readFloat, &dtdx);
    SAFE_PARCEL(input.readFloat, &dtdy);
    SAFE_PARCEL(input.readFloat, &dsdy);
    return NO_ERROR;
}

// ------------------------------- InputWindowCommands ----------------------------------------

bool InputWindowCommands::merge(const InputWindowCommands& other) {
    bool changes = false;
#ifndef NO_INPUT
    changes |= !other.focusRequests.empty();
    focusRequests.insert(focusRequests.end(), std::make_move_iterator(other.focusRequests.begin()),
                         std::make_move_iterator(other.focusRequests.end()));
#endif
    changes |= other.syncInputWindows && !syncInputWindows;
    syncInputWindows |= other.syncInputWindows;
    return changes;
}

bool InputWindowCommands::empty() const {
    bool empty = true;
#ifndef NO_INPUT
    empty = focusRequests.empty() && !syncInputWindows;
#endif
    return empty;
}

void InputWindowCommands::clear() {
#ifndef NO_INPUT
    focusRequests.clear();
#endif
    syncInputWindows = false;
}

status_t InputWindowCommands::write(Parcel& output) const {
#ifndef NO_INPUT
    SAFE_PARCEL(output.writeParcelableVector, focusRequests);
#endif
    SAFE_PARCEL(output.writeBool, syncInputWindows);
    return NO_ERROR;
}

status_t InputWindowCommands::read(const Parcel& input) {
#ifndef NO_INPUT
    SAFE_PARCEL(input.readParcelableVector, &focusRequests);
#endif
    SAFE_PARCEL(input.readBool, &syncInputWindows);
    return NO_ERROR;
}

bool ValidateFrameRate(float frameRate, int8_t compatibility, const char* inFunctionName) {
    const char* functionName = inFunctionName != nullptr ? inFunctionName : "call";
    int floatClassification = std::fpclassify(frameRate);
    if (frameRate < 0 || floatClassification == FP_INFINITE || floatClassification == FP_NAN) {
        ALOGE("%s failed - invalid frame rate %f", functionName, frameRate);
        return false;
    }

    if (compatibility != ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_DEFAULT &&
        compatibility != ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_FIXED_SOURCE) {
        ALOGE("%s failed - invalid compatibility value %d", functionName, compatibility);
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------------

status_t CaptureArgs::write(Parcel& output) const {
    SAFE_PARCEL(output.writeInt32, static_cast<int32_t>(pixelFormat));
    SAFE_PARCEL(output.write, sourceCrop);
    SAFE_PARCEL(output.writeFloat, frameScale);
    SAFE_PARCEL(output.writeBool, captureSecureLayers);
    SAFE_PARCEL(output.writeInt32, uid);
    SAFE_PARCEL(output.writeInt32, static_cast<int32_t>(dataspace));
    SAFE_PARCEL(output.writeBool, allowProtected);
    return NO_ERROR;
}

status_t CaptureArgs::read(const Parcel& input) {
    int32_t value = 0;
    SAFE_PARCEL(input.readInt32, &value);
    pixelFormat = static_cast<ui::PixelFormat>(value);
    SAFE_PARCEL(input.read, sourceCrop);
    SAFE_PARCEL(input.readFloat, &frameScale);
    SAFE_PARCEL(input.readBool, &captureSecureLayers);
    SAFE_PARCEL(input.readInt32, &uid);
    SAFE_PARCEL(input.readInt32, &value);
    dataspace = static_cast<ui::Dataspace>(value);
    SAFE_PARCEL(input.readBool, &allowProtected);
    return NO_ERROR;
}

status_t DisplayCaptureArgs::write(Parcel& output) const {
    SAFE_PARCEL(CaptureArgs::write, output);

    SAFE_PARCEL(output.writeStrongBinder, displayToken);
    SAFE_PARCEL(output.writeUint32, width);
    SAFE_PARCEL(output.writeUint32, height);
    SAFE_PARCEL(output.writeBool, useIdentityTransform);
    return NO_ERROR;
}

status_t DisplayCaptureArgs::read(const Parcel& input) {
    SAFE_PARCEL(CaptureArgs::read, input);

    SAFE_PARCEL(input.readStrongBinder, &displayToken);
    SAFE_PARCEL(input.readUint32, &width);
    SAFE_PARCEL(input.readUint32, &height);
    SAFE_PARCEL(input.readBool, &useIdentityTransform);
    return NO_ERROR;
}

status_t LayerCaptureArgs::write(Parcel& output) const {
    SAFE_PARCEL(CaptureArgs::write, output);

    SAFE_PARCEL(output.writeStrongBinder, layerHandle);
    SAFE_PARCEL(output.writeInt32, excludeHandles.size());
    for (auto el : excludeHandles) {
        SAFE_PARCEL(output.writeStrongBinder, el);
    }
    SAFE_PARCEL(output.writeBool, childrenOnly);
    return NO_ERROR;
}

status_t LayerCaptureArgs::read(const Parcel& input) {
    SAFE_PARCEL(CaptureArgs::read, input);

    SAFE_PARCEL(input.readStrongBinder, &layerHandle);

    int32_t numExcludeHandles = 0;
    SAFE_PARCEL_READ_SIZE(input.readInt32, &numExcludeHandles, input.dataSize());
    excludeHandles.reserve(numExcludeHandles);
    for (int i = 0; i < numExcludeHandles; i++) {
        sp<IBinder> binder;
        SAFE_PARCEL(input.readStrongBinder, &binder);
        excludeHandles.emplace(binder);
    }

    SAFE_PARCEL(input.readBool, &childrenOnly);
    return NO_ERROR;
}

status_t ScreenCaptureResults::write(Parcel& output) const {
    if (buffer != nullptr) {
        SAFE_PARCEL(output.writeBool, true);
        SAFE_PARCEL(output.write, *buffer);
    } else {
        SAFE_PARCEL(output.writeBool, false);
    }
    SAFE_PARCEL(output.writeBool, capturedSecureLayers);
    SAFE_PARCEL(output.writeUint32, static_cast<uint32_t>(capturedDataspace));
    SAFE_PARCEL(output.writeInt32, result);
    return NO_ERROR;
}

status_t ScreenCaptureResults::read(const Parcel& input) {
    bool hasGraphicBuffer;
    SAFE_PARCEL(input.readBool, &hasGraphicBuffer);
    if (hasGraphicBuffer) {
        buffer = new GraphicBuffer();
        SAFE_PARCEL(input.read, *buffer);
    }

    SAFE_PARCEL(input.readBool, &capturedSecureLayers);
    uint32_t dataspace = 0;
    SAFE_PARCEL(input.readUint32, &dataspace);
    capturedDataspace = static_cast<ui::Dataspace>(dataspace);
    SAFE_PARCEL(input.readInt32, &result);
    return NO_ERROR;
}

}; // namespace android
