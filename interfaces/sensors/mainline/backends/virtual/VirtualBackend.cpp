/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineSensorsVirtual"

#include "VirtualBackend.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parsedouble.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>
#include <libsensors_common/SensorEvents.h>
#include <libsensors_common/SensorTypes.h>
#include <libsensors_common/Settings.h>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <limits>

#include "VirtualSensorsService.h"

namespace aidl::android::hardware::sensors::mainline {

namespace {

constexpr int64_t kDefaultPeriodNs = 200LL * 1000 * 1000;
constexpr float kGravity = 9.80665f;
// A default above the range: clamped to maxRange, i.e. "far" for proximity.
constexpr float kMax = std::numeric_limits<float>::infinity();

// Types the backend can provide: their payload is a Vec3 or a scalar.
struct TypeSpec {
    SensorType type;
    size_t value_count;
    // Lower bound of every value; nullopt for -maxRange.
    std::optional<float> min_value;
    float default_values[3];
};

const TypeSpec kTypes[] = {
        {SensorType::ACCELEROMETER, 3, std::nullopt, {0.0f, 0.0f, kGravity}},
        {SensorType::GYROSCOPE, 3, std::nullopt, {0.0f, 0.0f, 0.0f}},
        // About the earth's field at mid latitudes, device lying flat.
        {SensorType::MAGNETIC_FIELD, 3, std::nullopt, {0.0f, 22.0f, -42.0f}},
        {SensorType::GRAVITY, 3, std::nullopt, {0.0f, 0.0f, kGravity}},
        {SensorType::LINEAR_ACCELERATION, 3, std::nullopt, {0.0f, 0.0f, 0.0f}},
        {SensorType::LIGHT, 1, 0.0f, {200.0f}},
        {SensorType::PRESSURE, 1, 0.0f, {1013.25f}},
        {SensorType::PROXIMITY, 1, 0.0f, {kMax}},
        {SensorType::AMBIENT_TEMPERATURE, 1, -40.0f, {22.0f}},
        {SensorType::RELATIVE_HUMIDITY, 1, 0.0f, {40.0f}},
        {SensorType::HINGE_ANGLE, 1, 0.0f, {180.0f}},
};

const SensorType kDefaultSensorTypes[] = {
        SensorType::ACCELEROMETER,       SensorType::GYROSCOPE,
        SensorType::MAGNETIC_FIELD,      SensorType::LIGHT,
        SensorType::PROXIMITY,           SensorType::PRESSURE,
        SensorType::AMBIENT_TEMPERATURE, SensorType::RELATIVE_HUMIDITY,
};

const TypeSpec* FindTypeSpec(SensorType type) {
    for (const TypeSpec& spec : kTypes) {
        if (spec.type == type) {
            return &spec;
        }
    }
    return nullptr;
}

std::vector<float> Clamp(const std::vector<float>& values, float min_value, float max_value) {
    std::vector<float> clamped;
    for (float value : values) {
        clamped.push_back(std::clamp(value, min_value, max_value));
    }
    return clamped;
}

std::string FormatValues(const std::vector<float>& values) {
    std::vector<std::string> parts;
    for (float value : values) {
        parts.push_back(::android::base::StringPrintf("%.9g", value));
    }
    return ::android::base::Join(parts, ",");
}

std::optional<std::vector<float>> ParseValues(const std::string& text) {
    std::vector<float> values;
    for (const auto& part : ::android::base::Split(text, ",")) {
        float value;
        if (!::android::base::ParseFloat(::android::base::Trim(part), &value) ||
            std::isnan(value)) {
            return std::nullopt;
        }
        values.push_back(value);
    }
    return values;
}

}  // namespace

DEFINE_SENSOR_BACKEND(VirtualBackend, 0)

VirtualBackend::VirtualBackend() = default;

VirtualBackend::~VirtualBackend() {
    Deinitialize();
}

std::string VirtualBackend::GetName() const {
    return "virtual";
}

int32_t VirtualBackend::Initialize(const PostEventsCallback& callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        post_events_ = callback;

        std::string configured = Settings::Get().GetString("virtual.sensors", "");
        if (configured.empty()) {
            for (SensorType type : kDefaultSensorTypes) {
                CreateSensor(type);
            }
        } else if (!::android::base::EqualsIgnoreCase(configured, "none")) {
            for (const auto& token : ::android::base::Split(configured, ",")) {
                std::string name = ::android::base::Trim(token);
                if (name.empty()) {
                    continue;
                }
                auto type = ParseSensorType(name);
                if (!type.has_value()) {
                    LOG(WARNING) << "virtual.sensors: unknown sensor type '" << name << "'";
                    continue;
                }
                CreateSensor(*type);
            }
        }
        LoadStoredValues();
        LOG(INFO) << "Virtual backend initialized with " << sensors_.size() << " sensor(s)";
    }
    // Also without sensors, so that the settings app can tell why there are
    // none.
    VirtualSensorsService::Attach(this);
    return 0;
}

void VirtualBackend::CreateSensor(SensorType type) {
    const TypeSpec* spec = FindTypeSpec(type);
    auto traits = GetSensorTypeTraits(type);
    if (spec == nullptr || !traits.has_value()) {
        LOG(WARNING) << "virtual.sensors: type " << toString(type) << " is not supported";
        return;
    }
    for (const auto& [handle, existing] : sensors_) {
        if (existing->info.type == type) {
            LOG(WARNING) << "virtual.sensors: " << toString(type) << " listed twice";
            return;
        }
    }

    auto sensor = std::make_unique<VirtualSensor>();
    sensor->info.sensorHandle = next_handle_++;
    sensor->info.name = std::string("Virtual ") + traits->label;
    sensor->info.vendor = "Mainline Sensors HAL";
    sensor->info.version = 1;
    sensor->info.type = type;
    ApplySensorTypeDefaults(&sensor->info);
    sensor->key = traits->config_name;
    sensor->min_value = spec->min_value.value_or(-sensor->info.maxRange);
    sensor->default_values = Clamp(
            std::vector<float>(spec->default_values, spec->default_values + spec->value_count),
            sensor->min_value, sensor->info.maxRange);
    sensor->values = sensor->default_values;
    sensor->period_ns = kDefaultPeriodNs;

    LOG(INFO) << "Virtual sensor created: " << SensorInfoToString(sensor->info);
    int32_t handle = sensor->info.sensorHandle;
    sensors_[handle] = std::move(sensor);
}

void VirtualBackend::Deinitialize() {
    // Waits for running control calls; none can start afterwards.
    VirtualSensorsService::Detach(this);

    std::vector<std::unique_ptr<PeriodicWorker>> workers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [handle, sensor] : sensors_) {
            sensor->active = false;
            if (sensor->worker) {
                workers.push_back(std::move(sensor->worker));
            }
        }
        post_events_ = nullptr;
    }
    // Workers may be blocked on mutex_ inside ReportSample(); stop them
    // without holding it.
    for (auto& worker : workers) {
        worker->Stop();
    }
    {
        // Leave a clean slate: Initialize() may be called again to retry
        // discovery while the frontend waits for late sensors.
        std::lock_guard<std::mutex> lock(mutex_);
        sensors_.clear();
        next_handle_ = 1;
    }
    LOG(INFO) << "Virtual backend deinitialized";
}

std::vector<SensorInfo> VirtualBackend::GetSensorsList() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SensorInfo> list;
    for (const auto& [handle, sensor] : sensors_) {
        list.push_back(sensor->info);
    }
    return list;
}

Event VirtualBackend::BuildEvent(const VirtualSensor& sensor, int64_t timestamp_ns) const {
    const int32_t handle = sensor.info.sensorHandle;
    if (sensor.values.size() == 3) {
        return MakeVec3Event(handle, sensor.info.type, timestamp_ns, sensor.values[0],
                             sensor.values[1], sensor.values[2]);
    }
    return MakeScalarEvent(handle, sensor.info.type, timestamp_ns, sensor.values[0]);
}

void VirtualBackend::ReportSample(VirtualSensor* sensor) {
    PostEventsCallback callback;
    std::optional<Event> event;
    bool wakeup = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!sensor->active || operation_mode_.load() != OperationMode::NORMAL) {
            return;
        }
        Event candidate = BuildEvent(*sensor, GetBootTimeNs());
        // On-change sensors report the first sample after activation and then
        // only what the user changes.
        if (IsOnChangeSensor(sensor->info.flags) && sensor->last_event.has_value() &&
            HaveSamePayload(*sensor->last_event, candidate)) {
            return;
        }
        sensor->last_event = candidate;
        event = candidate;
        wakeup = IsWakeUpSensor(sensor->info.flags);
        callback = post_events_;
    }
    if (callback && event.has_value()) {
        LOG(VERBOSE) << "Virtual event: " << EventToString(*event);
        callback({*event}, wakeup);
    }
}

int32_t VirtualBackend::Activate(int32_t sensor_handle, bool enabled) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto it = sensors_.find(sensor_handle);
    if (it == sensors_.end()) {
        return -EINVAL;
    }
    VirtualSensor* sensor = it->second.get();
    if (sensor->active == enabled) {
        return 0;
    }
    sensor->active = enabled;
    sensor->last_event.reset();
    if (enabled) {
        if (!sensor->worker) {
            sensor->worker =
                    std::make_unique<PeriodicWorker>("virtual-" + std::to_string(sensor_handle),
                                                     [this, sensor]() { ReportSample(sensor); });
        }
        sensor->worker->Start(sensor->period_ns);
    } else if (sensor->worker) {
        // Stop without holding the lock: the worker may be inside
        // ReportSample() waiting for it.
        std::unique_ptr<PeriodicWorker> worker = std::move(sensor->worker);
        lock.unlock();
        worker->Stop();
        lock.lock();
    }
    LOG(INFO) << "Virtual sensor " << sensor_handle << (enabled ? " activated" : " deactivated");
    return 0;
}

int32_t VirtualBackend::Batch(int32_t sensor_handle, int64_t sampling_period_ns,
                              int64_t /* max_report_latency_ns */) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sensors_.find(sensor_handle);
    if (it == sensors_.end()) {
        return -EINVAL;
    }
    VirtualSensor* sensor = it->second.get();
    if (sampling_period_ns <= 0) {
        sampling_period_ns = kDefaultPeriodNs;
    }
    sensor->period_ns = sampling_period_ns;
    if (sensor->worker) {
        sensor->worker->SetPeriod(sampling_period_ns);
    }
    LOG(DEBUG) << "Virtual sensor " << sensor_handle << " period " << sampling_period_ns / 1000
               << " us";
    return 0;
}

int32_t VirtualBackend::Flush(int32_t sensor_handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sensors_.find(sensor_handle) == sensors_.end()) {
        return -EINVAL;
    }
    return kFlushHandledByFrontend;
}

int32_t VirtualBackend::SetOperationMode(OperationMode mode) {
    operation_mode_.store(mode);
    LOG(INFO) << "Virtual backend operation mode " << toString(mode);
    return 0;
}

std::vector<VirtualBackend::SensorState> VirtualBackend::GetSensorStates() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SensorState> states;
    for (const auto& [handle, sensor] : sensors_) {
        states.push_back({sensor->info, sensor->min_value, sensor->values, sensor->stored,
                          sensor->active, sensor->period_ns});
    }
    return states;
}

int32_t VirtualBackend::SetValues(int32_t sensor_handle, const std::vector<float>& values) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sensors_.find(sensor_handle);
    if (it == sensors_.end()) {
        return -EINVAL;
    }
    VirtualSensor* sensor = it->second.get();
    if (values.size() != sensor->values.size() ||
        std::any_of(values.begin(), values.end(), [](float value) { return std::isnan(value); })) {
        return -EINVAL;
    }
    sensor->values = Clamp(values, sensor->min_value, sensor->info.maxRange);
    sensor->stored = true;
    SaveStoredValues();
    LOG(INFO) << "Virtual sensor " << sensor_handle << " (" << sensor->key << ") set to "
              << FormatValues(sensor->values);
    // Report the new values right away rather than at the next period.
    if (sensor->worker) {
        sensor->worker->Poke();
    }
    return 0;
}

int32_t VirtualBackend::ResetValues(int32_t sensor_handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sensors_.find(sensor_handle);
    if (it == sensors_.end()) {
        return -EINVAL;
    }
    VirtualSensor* sensor = it->second.get();
    sensor->values = sensor->default_values;
    if (sensor->stored) {
        sensor->stored = false;
        SaveStoredValues();
    }
    LOG(INFO) << "Virtual sensor " << sensor_handle << " (" << sensor->key << ") reset";
    if (sensor->worker) {
        sensor->worker->Poke();
    }
    return 0;
}

/*
 * kStateFile holds one line per sensor the user set: the type's configuration
 * name, a tab and the comma separated values, e.g. "accel\t0,9.80665,0".
 * Lines of types that are not configured right now are kept, so that removing
 * a type from virtual.sensors for a while does not lose its values.
 */
void VirtualBackend::LoadStoredValues() {
    std::string contents;
    if (!::android::base::ReadFileToString(kStateFile, &contents)) {
        if (errno != ENOENT) {
            PLOG(WARNING) << "Reading " << kStateFile;
        }
        return;
    }
    for (const std::string& line : ::android::base::Split(contents, "\n")) {
        const size_t separator = line.find('\t');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, separator);
        auto values = ParseValues(line.substr(separator + 1));
        for (auto& [handle, sensor] : sensors_) {
            if (sensor->key != key) {
                continue;
            }
            if (!values.has_value() || values->size() != sensor->values.size()) {
                LOG(WARNING) << "Ignoring invalid stored values of " << key;
                break;
            }
            sensor->values = Clamp(*values, sensor->min_value, sensor->info.maxRange);
            sensor->stored = true;
            LOG(INFO) << "Virtual sensor " << handle << " (" << key << ") restored to "
                      << FormatValues(sensor->values);
            break;
        }
    }
}

void VirtualBackend::SaveStoredValues() const {
    // Keep the lines of sensors that are not configured right now.
    std::map<std::string, std::string> lines;
    std::string contents;
    if (::android::base::ReadFileToString(kStateFile, &contents)) {
        for (const std::string& line : ::android::base::Split(contents, "\n")) {
            const size_t separator = line.find('\t');
            if (separator != std::string::npos) {
                lines[line.substr(0, separator)] = line.substr(separator + 1);
            }
        }
    }
    for (const auto& [handle, sensor] : sensors_) {
        if (sensor->stored) {
            lines[sensor->key] = FormatValues(sensor->values);
        } else {
            lines.erase(sensor->key);
        }
    }
    contents.clear();
    for (const auto& [key, values] : lines) {
        contents += key + "\t" + values + "\n";
    }

    // Write, sync and rename, so that a crash or power loss leaves either the
    // old or the new file.
    const std::string temp = std::string(kStateFile) + ".tmp";
    ::android::base::unique_fd fd(
            TEMP_FAILURE_RETRY(open(temp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0660)));
    if (fd < 0) {
        PLOG(ERROR) << "Creating " << temp << "; values are not kept across restarts";
        return;
    }
    if (!::android::base::WriteStringToFd(contents, fd) || fsync(fd) != 0) {
        PLOG(ERROR) << "Writing " << temp;
        fd.reset();
        std::remove(temp.c_str());
        return;
    }
    fd.reset();
    if (std::rename(temp.c_str(), kStateFile) != 0) {
        PLOG(ERROR) << "Renaming " << temp << " to " << kStateFile;
        std::remove(temp.c_str());
    }
}

}  // namespace aidl::android::hardware::sensors::mainline
