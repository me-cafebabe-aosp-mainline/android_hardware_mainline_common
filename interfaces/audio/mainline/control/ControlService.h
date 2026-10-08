/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <string>

#include <aidl/vendor/mainline/audio/BnControl.h>

#include "ModuleMainline.h"
#include "control/MixerSettings.h"
#include "control/SettingsStore.h"

namespace aidl::android::hardware::audio::core::mainline::control {

// vendor.mainline.audio.IControl/default, the interface of the settings app
// in the APEX. It reads the module's state and adjusts the mixer, and never
// changes what the framework sees through IModule. Binder threads only.
class ControlService final : public ::aidl::vendor::mainline::audio::BnControl {
  public:
    ControlService(std::shared_ptr<ModuleMainline> module, std::shared_ptr<MixerSettings> mixer,
                   std::shared_ptr<SettingsStore> settings);

    ndk::ScopedAStatus getStatus(::aidl::vendor::mainline::audio::Status* _aidl_return) override;
    ndk::ScopedAStatus getReport(std::string* _aidl_return) override;
    ndk::ScopedAStatus getMixerControls(
            int32_t card,
            std::vector<::aidl::vendor::mainline::audio::MixerControl>* _aidl_return) override;
    ndk::ScopedAStatus setMixerControl(int32_t card, int32_t numid,
                                       const std::vector<int64_t>& values) override;
    ndk::ScopedAStatus resetMixerControls(int32_t card) override;
    ndk::ScopedAStatus getSettings(
            std::vector<::aidl::vendor::mainline::audio::Setting>* _aidl_return) override;
    ndk::ScopedAStatus applySettings(
            const std::vector<::aidl::vendor::mainline::audio::SettingChange>& changes) override;
    ndk::ScopedAStatus getTrialStatus(
            ::aidl::vendor::mainline::audio::TrialStatus* _aidl_return) override;
    ndk::ScopedAStatus keepSettings() override;
    ndk::ScopedAStatus revertSettings() override;
    ndk::ScopedAStatus resetSettings() override;

  private:
    ::aidl::vendor::mainline::audio::EndpointStatus MakeEndpointStatus(
            const routing::Endpoint& endpoint) const;
    ::aidl::vendor::mainline::audio::HdmiSinkStatus MakeHdmiSinkStatus(
            const routing::Endpoint& head) const;

    // Whether the HAL uses `card`: other cards are none of the app's business.
    bool UsesCard(int32_t card) const;

    const std::shared_ptr<ModuleMainline> module_;
    const std::shared_ptr<MixerSettings> mixer_;
    const std::shared_ptr<SettingsStore> settings_;
};

}  // namespace aidl::android::hardware::audio::core::mainline::control
