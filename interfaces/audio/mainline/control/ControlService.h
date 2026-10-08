/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <string>

#include <aidl/vendor/mainline/audio/BnControl.h>

#include "ModuleMainline.h"

namespace aidl::android::hardware::audio::core::mainline::control {

// vendor.mainline.audio.IControl/default, the interface of the settings app
// in the APEX. It reads the module's state and never changes what the
// framework sees through IModule. Binder threads only.
class ControlService final : public ::aidl::vendor::mainline::audio::BnControl {
  public:
    explicit ControlService(std::shared_ptr<ModuleMainline> module);

    ndk::ScopedAStatus getStatus(::aidl::vendor::mainline::audio::Status* _aidl_return) override;
    ndk::ScopedAStatus getReport(std::string* _aidl_return) override;

  private:
    ::aidl::vendor::mainline::audio::EndpointStatus MakeEndpointStatus(
            const routing::Endpoint& endpoint) const;
    ::aidl::vendor::mainline::audio::HdmiSinkStatus MakeHdmiSinkStatus(
            const routing::Endpoint& head) const;

    const std::shared_ptr<ModuleMainline> module_;
};

}  // namespace aidl::android::hardware::audio::core::mainline::control
