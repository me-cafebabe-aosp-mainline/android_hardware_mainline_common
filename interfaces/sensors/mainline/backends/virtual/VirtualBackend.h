/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libsensors_common/PeriodicWorker.h>
#include <libsensors_mainline/SensorBackend.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace aidl::android::hardware::sensors::mainline {

/*
 * Backend whose sensors report what the user sets through
 * vendor.mainline.sensors.IVirtualSensors (VirtualSensorsService), e.g. from
 * the settings app of the HAL's APEX.
 *
 * The set of sensors comes from the "virtual.sensors" setting and is fixed for
 * the life of the process like every other sensor list. Values set by the user
 * are stored in kStateFile and survive restarts of the HAL. Unlike the mock
 * backend it is not "fallback only": its sensors are exposed next to real ones
 * of the same type.
 */
class VirtualBackend : public ISensorBackend {
  public:
    // What VirtualSensorsService reports about a sensor.
    struct SensorState {
        SensorInfo info;
        float min_value;
        std::vector<float> values;
        bool stored;
        bool active;
        int64_t period_ns;
    };

    static constexpr const char* kStateFile = "/data/vendor/mainline_sensors/virtual";

    VirtualBackend();
    ~VirtualBackend() override;

    std::string GetName() const override;
    int32_t Initialize(const PostEventsCallback& callback) override;
    void Deinitialize() override;
    std::vector<SensorInfo> GetSensorsList() override;
    int32_t Activate(int32_t sensor_handle, bool enabled) override;
    int32_t Batch(int32_t sensor_handle, int64_t sampling_period_ns,
                  int64_t max_report_latency_ns) override;
    int32_t Flush(int32_t sensor_handle) override;
    int32_t SetOperationMode(OperationMode mode) override;

    // Control interface, called from Binder threads.
    std::vector<SensorState> GetSensorStates();
    // Clamps the values to the sensor's range and stores them. Returns 0, or
    // -EINVAL for an unknown handle, a wrong number of values or NaN.
    int32_t SetValues(int32_t sensor_handle, const std::vector<float>& values);
    // Returns to the default values and forgets the stored ones. Returns 0 or
    // -EINVAL for an unknown handle.
    int32_t ResetValues(int32_t sensor_handle);

  private:
    struct VirtualSensor {
        SensorInfo info;
        // Key of the sensor in kStateFile: the type's configuration name.
        std::string key;
        float min_value;
        std::vector<float> default_values;
        std::vector<float> values;
        bool stored = false;
        int64_t period_ns;
        bool active = false;
        std::optional<Event> last_event;
        std::unique_ptr<PeriodicWorker> worker;
    };

    void CreateSensor(SensorType type);
    void ReportSample(VirtualSensor* sensor);
    Event BuildEvent(const VirtualSensor& sensor, int64_t timestamp_ns) const;
    // Both called with mutex_ held.
    void LoadStoredValues();
    void SaveStoredValues() const;

    std::mutex mutex_;
    std::map<int32_t, std::unique_ptr<VirtualSensor>> sensors_;
    int32_t next_handle_ = 1;
    PostEventsCallback post_events_;
    std::atomic<OperationMode> operation_mode_{OperationMode::NORMAL};
};

}  // namespace aidl::android::hardware::sensors::mainline
