/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineAudio_Control"

#include "control/SettingsStore.h"

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <thread>

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>

#include "Properties.h"

namespace aidl::android::hardware::audio::core::mainline::control {

namespace {

constexpr char kMetaTrial[] = "trial";
constexpr char kMetaStarts[] = "starts";
constexpr char kMetaReverted[] = "reverted";

}  // namespace

void RestartSoon() {
    std::thread([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        LOG(INFO) << "restarting to apply the audio settings";
        // init starts the service again; destructors of a running HAL have
        // nothing to tear down cleanly anyway.
        _exit(EXIT_SUCCESS);
    }).detach();
}

SettingsStore::SettingsStore()
    : committed_store_("settings"),
      trial_store_("settings.trial"),
      meta_store_("settings.meta"),
      started_(std::chrono::steady_clock::now()) {
    if (::android::base::GetBoolProperty(std::string(Properties::kPrefix) + kIgnoreProperty,
                                         false)) {
        // The way out when even confirmed settings break the HAL: nothing is
        // dropped, the stored settings just stay unused while it is set.
        LOG(WARNING) << Properties::kPrefix << kIgnoreProperty << " is set, ignoring the stored "
                     << "audio settings";
        return;
    }
    StateStore::Entries meta = meta_store_.Load();
    std::lock_guard guard(lock_);
    reverted_ = meta[kMetaReverted] == "1";
    if (meta[kMetaTrial] != "1") {
        active_ = committed_store_.Load();
        return;
    }
    int starts = 0;
    ::android::base::ParseInt(meta[kMetaStarts], &starts);
    ++starts;
    if (starts > kMaxTrialStarts) {
        LOG(ERROR) << "the HAL did not come up with the new audio settings " << kMaxTrialStarts
                   << " times, dropping them";
        meta_store_.Save({{kMetaReverted, "1"}});
        trial_store_.Save({});
        reverted_ = true;
        active_ = committed_store_.Load();
        return;
    }
    meta[kMetaStarts] = std::to_string(starts);
    meta_store_.Save(meta);
    active_ = trial_store_.Load();
    on_trial_ = true;
    LOG(INFO) << "trying new audio settings (start " << starts << " of " << kMaxTrialStarts
              << "), reverting in " << kTrialTimeout.count() << " s unless confirmed";
    std::thread(&SettingsStore::WatchTrial, this).detach();
}

SettingsStore::Settings SettingsStore::committed() const {
    std::lock_guard guard(lock_);
    return committed_store_.Load();
}

bool SettingsStore::on_trial() const {
    std::lock_guard guard(lock_);
    return on_trial_;
}

bool SettingsStore::reverted_after_failure() const {
    std::lock_guard guard(lock_);
    return reverted_;
}

std::chrono::seconds SettingsStore::TrialTimeLeft() const {
    std::lock_guard guard(lock_);
    if (!on_trial_) return std::chrono::seconds(0);
    const auto left = started_ + kTrialTimeout - std::chrono::steady_clock::now();
    return std::max(std::chrono::seconds(0), std::chrono::ceil<std::chrono::seconds>(left));
}

bool SettingsStore::StartTrial(Settings settings) {
    std::lock_guard guard(lock_);
    if (!trial_store_.Save(settings) ||
        !meta_store_.Save({{kMetaTrial, "1"}, {kMetaStarts, "0"}})) {
        return false;
    }
    LOG(INFO) << "new audio settings stored for a trial";
    RestartSoon();
    return true;
}

bool SettingsStore::Keep() {
    std::lock_guard guard(lock_);
    if (!on_trial_) return false;
    if (!committed_store_.Save(active_)) return false;
    meta_store_.Save({});
    trial_store_.Save({});
    on_trial_ = false;
    reverted_ = false;
    LOG(INFO) << "new audio settings confirmed";
    return true;
}

bool SettingsStore::Revert() {
    std::lock_guard guard(lock_);
    if (!on_trial_) return false;
    RevertLocked(false);
    return true;
}

void SettingsStore::RevertLocked(bool failure) {
    meta_store_.Save(failure ? StateStore::Entries{{kMetaReverted, "1"}} : StateStore::Entries{});
    trial_store_.Save({});
    on_trial_ = false;
    RestartSoon();
}

bool SettingsStore::Reset() {
    std::lock_guard guard(lock_);
    if (!committed_store_.Save({})) return false;
    trial_store_.Save({});
    meta_store_.Save({});
    on_trial_ = false;
    LOG(INFO) << "audio settings reset to the device's properties";
    RestartSoon();
    return true;
}

void SettingsStore::WatchTrial() {
    std::this_thread::sleep_until(started_ + kTrialTimeout);
    std::lock_guard guard(lock_);
    if (!on_trial_) return;
    LOG(WARNING) << "new audio settings were not confirmed within " << kTrialTimeout.count()
                 << " s, reverting";
    RevertLocked(true);
}

}  // namespace aidl::android::hardware::audio::core::mainline::control
