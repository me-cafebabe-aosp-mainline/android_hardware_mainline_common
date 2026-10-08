/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include <android-base/thread_annotations.h>

#include "control/StateStore.h"

namespace aidl::android::hardware::audio::core::mainline::control {

// The HAL settings (Properties overrides) the user set through IControl.
//
// Settings change ports and profiles, so they only apply when the HAL starts.
// New settings first run as a trial: the HAL restarts with them and keeps them
// only once the user confirms. Without a confirmation within kTrialTimeout,
// the HAL drops them and restarts with the previous ones, and so it does when
// it failed to come up with them kMaxTrialStarts times: a setting that leaves
// the device silent or crashing undoes itself.
//
// Create one instance, first thing at start-up: the constructor decides which
// settings this process runs with. Thread safe.
class SettingsStore {
  public:
    using Settings = std::map<std::string, std::string>;

    static constexpr std::chrono::seconds kTrialTimeout{30};
    static constexpr int kMaxTrialStarts = 2;
    // Property (after Properties::kPrefix) that makes the HAL run without the
    // stored settings, for a device that fails even with confirmed ones.
    static constexpr const char* kIgnoreProperty = "settings.ignore";

    SettingsStore();

    // The settings in effect for this process.
    const Settings& active() const { return active_; }
    // The confirmed settings.
    Settings committed() const;
    // This process runs with settings that are not confirmed yet.
    bool on_trial() const;
    // Time left to confirm the trial, zero without one.
    std::chrono::seconds TrialTimeLeft() const;
    // The last trial was dropped because it was not confirmed or the HAL
    // failed with it. Cleared by the next trial.
    bool reverted_after_failure() const;

    // Starts a trial of `settings` (the complete set) and restarts the HAL.
    // False, without a restart, when the settings can not be stored.
    bool StartTrial(Settings settings);
    // Confirms the running trial. False without one.
    bool Keep();
    // Drops the running trial and restarts the HAL with the confirmed
    // settings. False without one.
    bool Revert();
    // Drops every setting and restarts the HAL with the device's properties.
    // False, without a restart, when that can not be stored.
    bool Reset();

  private:
    // Ends the trial at its deadline unless it was confirmed.
    void WatchTrial();
    void RevertLocked(bool failure) REQUIRES(lock_);

    const StateStore committed_store_;
    const StateStore trial_store_;
    // "starts": trial starts so far, "reverted": the last trial was dropped.
    const StateStore meta_store_;

    Settings active_;
    const std::chrono::steady_clock::time_point started_;

    mutable std::mutex lock_;
    bool on_trial_ GUARDED_BY(lock_) = false;
    bool reverted_ GUARDED_BY(lock_) = false;
};

// Ends the process shortly, so that the Binder reply that asked for it still
// goes out; init starts the HAL again (and audioserver with it).
void RestartSoon();

}  // namespace aidl::android::hardware::audio::core::mainline::control
