/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineAudio_Control"

#include "control/StateStore.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>

namespace aidl::android::hardware::audio::core::mainline::control {

namespace {

constexpr char kSeparator = '\t';

}  // namespace

StateStore::StateStore(std::string name) : path_(std::string(kDirectory) + "/" + std::move(name)) {}

bool StateStore::IsStorable(const std::string& text) {
    return text.find_first_of("\t\n\r") == std::string::npos;
}

StateStore::Entries StateStore::Load() const {
    Entries entries;
    std::string contents;
    if (!::android::base::ReadFileToString(path_, &contents)) {
        if (errno != ENOENT) PLOG(WARNING) << "reading " << path_;
        return entries;
    }
    for (const std::string& line : ::android::base::Split(contents, "\n")) {
        const size_t separator = line.find(kSeparator);
        if (separator == std::string::npos) continue;
        entries[line.substr(0, separator)] = line.substr(separator + 1);
    }
    return entries;
}

bool StateStore::Save(const Entries& entries) const {
    std::string contents;
    for (const auto& [key, value] : entries) {
        if (!IsStorable(key) || !IsStorable(value)) {
            LOG(ERROR) << "not storing \"" << key << "\" in " << path_ << ": invalid characters";
            continue;
        }
        contents += key + kSeparator + value + "\n";
    }
    // Write, sync and rename, so that a crash or power loss leaves either the
    // old or the new file.
    const std::string temp = path_ + ".tmp";
    ::android::base::unique_fd fd(
            TEMP_FAILURE_RETRY(open(temp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0660)));
    if (fd < 0) {
        PLOG(ERROR) << "creating " << temp;
        return false;
    }
    if (!::android::base::WriteStringToFd(contents, fd) || fsync(fd) != 0) {
        PLOG(ERROR) << "writing " << temp;
        fd.reset();
        std::remove(temp.c_str());
        return false;
    }
    fd.reset();
    if (std::rename(temp.c_str(), path_.c_str()) != 0) {
        PLOG(ERROR) << "renaming " << temp << " to " << path_;
        std::remove(temp.c_str());
        return false;
    }
    return true;
}

}  // namespace aidl::android::hardware::audio::core::mainline::control
