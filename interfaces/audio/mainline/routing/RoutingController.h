/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "routing/DeviceInventory.h"
#include "routing/Endpoint.h"

namespace aidl::android::hardware::audio::core::mainline::routing {

// Keeps the hardware routing (UCM device enable / disable) in sync with the
// streams that use an endpoint. Multiple streams may share an endpoint, so
// the UCM device is enabled on the first Acquire() and disabled on the last
// Release(). Endpoints without a UCM device are no-ops here: their PCM device
// is simply opened by the stream.
class RoutingController {
  public:
    explicit RoutingController(std::shared_ptr<DeviceInventory> inventory);

    // Called with the card index after a UCM device of the card was enabled,
    // for state that UCM sequences overwrite. Set it before the first stream
    // opens; it runs on the caller's thread with the routing lock held.
    void SetDeviceEnabledListener(std::function<void(int card)> listener);

    void Acquire(const Endpoint& endpoint);
    void Release(const Endpoint& endpoint);
    // Streams holding the endpoint's UCM device, 0 for endpoints without one.
    int UseCount(const Endpoint& endpoint) const;

    std::string Dump() const;

  private:
    static std::string Key(const Endpoint& endpoint);

    const std::shared_ptr<DeviceInventory> inventory_;
    mutable std::mutex lock_;
    std::map<std::string, int> refcounts_;
    std::function<void(int card)> device_enabled_listener_;
};

}  // namespace aidl::android::hardware::audio::core::mainline::routing
