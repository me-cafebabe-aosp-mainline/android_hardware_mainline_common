/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/mainline/sensors/BnVirtualSensors.h>

#include <shared_mutex>
#include <vector>

namespace aidl::android::hardware::sensors::mainline {

class VirtualBackend;

/*
 * vendor.mainline.sensors.IVirtualSensors/default, for the settings app of
 * the HAL's APEX. One instance per process, registered the first time a
 * backend attaches and never unregistered; registering pins the backend
 * library in memory. Between Detach() and the next Attach() every
 * call fails with EX_ILLEGAL_STATE.
 */
class VirtualSensorsService : public ::aidl::vendor::mainline::sensors::BnVirtualSensors {
  public:
    static void Attach(VirtualBackend* backend);
    // Waits for running calls into `backend` to return.
    static void Detach(VirtualBackend* backend);

    ::ndk::ScopedAStatus getSensors(
            std::vector<::aidl::vendor::mainline::sensors::VirtualSensor>* sensors) override;
    ::ndk::ScopedAStatus setValues(int32_t handle, const std::vector<float>& values) override;
    ::ndk::ScopedAStatus resetValues(int32_t handle) override;

  private:
    std::shared_mutex mutex_;
    VirtualBackend* backend_ = nullptr;
};

}  // namespace aidl::android::hardware::sensors::mainline
