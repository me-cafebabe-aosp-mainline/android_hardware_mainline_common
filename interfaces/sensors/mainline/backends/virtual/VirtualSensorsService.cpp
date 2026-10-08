/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineSensorsVirtual"

#include "VirtualSensorsService.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>

#include <dlfcn.h>

#include <memory>
#include <mutex>
#include <string>

#include "VirtualBackend.h"

namespace aidl::android::hardware::sensors::mainline {

namespace {

using ::aidl::vendor::mainline::sensors::VirtualSensor;

std::shared_ptr<VirtualSensorsService> GetInstance() {
    static std::once_flag once;
    static std::shared_ptr<VirtualSensorsService> instance;
    std::call_once(once, [] {
        // servicemanager keeps the service once it is registered, so this
        // library must never be unloaded afterwards.
        Dl_info info;
        if (dladdr(reinterpret_cast<void*>(&GetInstance), &info) == 0 ||
            info.dli_fname == nullptr) {
            LOG(WARNING) << "Could not find the virtual backend library to pin it";
        } else if (dlopen(info.dli_fname, RTLD_NOW | RTLD_NOLOAD | RTLD_NODELETE) == nullptr) {
            LOG(WARNING) << "Could not pin " << info.dli_fname << ": " << dlerror();
        }
        instance = ::ndk::SharedRefBase::make<VirtualSensorsService>();
        // Served by the Binder threads of the HAL process. The backend works
        // without it, so a failure is only logged.
        const std::string name = std::string(VirtualSensorsService::descriptor) + "/default";
        if (const binder_status_t status =
                    AServiceManager_addService(instance->asBinder().get(), name.c_str());
            status != STATUS_OK) {
            LOG(ERROR) << "Failed to register " << name << ": " << status;
        } else {
            LOG(INFO) << "Registered " << name;
        }
    });
    return instance;
}

::ndk::ScopedAStatus ToStatus(int32_t ret) {
    if (ret == 0) {
        return ::ndk::ScopedAStatus::ok();
    }
    if (ret == -EINVAL) {
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    return ::ndk::ScopedAStatus::fromServiceSpecificError(-ret);
}

::ndk::ScopedAStatus NotAttached() {
    return ::ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_STATE,
                                                              "virtual backend not initialized");
}

}  // namespace

void VirtualSensorsService::Attach(VirtualBackend* backend) {
    std::shared_ptr<VirtualSensorsService> instance = GetInstance();
    std::unique_lock lock(instance->mutex_);
    instance->backend_ = backend;
}

void VirtualSensorsService::Detach(VirtualBackend* backend) {
    std::shared_ptr<VirtualSensorsService> instance = GetInstance();
    std::unique_lock lock(instance->mutex_);
    if (instance->backend_ == backend) {
        instance->backend_ = nullptr;
    }
}

::ndk::ScopedAStatus VirtualSensorsService::getSensors(std::vector<VirtualSensor>* sensors) {
    std::shared_lock lock(mutex_);
    if (backend_ == nullptr) {
        return NotAttached();
    }
    sensors->clear();
    for (const VirtualBackend::SensorState& state : backend_->GetSensorStates()) {
        VirtualSensor sensor;
        sensor.handle = state.info.sensorHandle;
        sensor.type = static_cast<int32_t>(state.info.type);
        sensor.name = state.info.name;
        sensor.minValue = state.min_value;
        sensor.maxValue = state.info.maxRange;
        sensor.resolution = state.info.resolution;
        sensor.values = state.values;
        sensor.stored = state.stored;
        sensor.active = state.active;
        sensor.samplingPeriodNs = state.active ? state.period_ns : 0;
        sensors->push_back(std::move(sensor));
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus VirtualSensorsService::setValues(int32_t handle,
                                                      const std::vector<float>& values) {
    std::shared_lock lock(mutex_);
    if (backend_ == nullptr) {
        return NotAttached();
    }
    return ToStatus(backend_->SetValues(handle, values));
}

::ndk::ScopedAStatus VirtualSensorsService::resetValues(int32_t handle) {
    std::shared_lock lock(mutex_);
    if (backend_ == nullptr) {
        return NotAttached();
    }
    return ToStatus(backend_->ResetValues(handle));
}

}  // namespace aidl::android::hardware::sensors::mainline
