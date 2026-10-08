/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.sensors.settings

import android.os.Bundle
import android.text.InputType
import android.view.View
import android.widget.Toast
import androidx.fragment.app.viewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.preference.EditTextPreference
import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceScreen
import androidx.preference.SeekBarPreference
import androidx.preference.SwitchPreferenceCompat
import java.text.DecimalFormat
import kotlin.math.ceil
import kotlin.math.log10
import kotlin.math.pow
import kotlin.math.roundToInt
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import vendor.mainline.sensors.VirtualSensor

/**
 * Every sensor of the virtual backend, with its values. Values the user sets are kept by the HAL
 * across restarts.
 */
class VirtualSensorsFragment : SettingsFragment() {
    private val viewModel: VirtualSensorsViewModel by viewModels()
    private val number = DecimalFormat("0.##")

    /** Keys of the rendered sensors: the same keys again are updated in place. */
    private var renderedKeys: List<String>? = null

    /** The latest state of every sensor by handle, for the listeners of in place updates. */
    private var current: Map<Int, VirtualSensor> = emptyMap()

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceScreen = preferenceManager.createPreferenceScreen(requireContext())
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        renderedKeys = null
        viewLifecycleOwner.lifecycleScope.launch {
            viewLifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) {
                // Whether the framework uses a sensor changes behind the page's back.
                launch {
                    while (true) {
                        viewModel.refresh()
                        delay(REFRESH_INTERVAL_MS)
                    }
                }
                viewModel.state.collect(::render)
            }
        }
    }

    private fun render(state: VirtualSensorsViewModel.State) {
        if (state !is VirtualSensorsViewModel.State.Loaded) {
            val screen = preferenceManager.createPreferenceScreen(requireContext())
            if (state is VirtualSensorsViewModel.State.Unavailable) {
                screen.addFooter(KEY_FOOTER, R.string.hal_unavailable)
            }
            preferenceScreen = screen
            renderedKeys = null
            return
        }
        current = state.sensors.associateBy { it.handle }
        val keys = state.sensors.map(::sensorKey)
        if (keys == renderedKeys) {
            // Same sensors: update them, which keeps the scroll position and a moving slider.
            bindPosition(state.sensors)
            state.sensors.forEach(::bind)
            return
        }
        val screen = preferenceManager.createPreferenceScreen(requireContext())
        if (state.sensors.isEmpty()) {
            screen.addFooter(KEY_FOOTER, R.string.no_sensors)
        } else {
            if (state.sensors.any(SensorKinds::isMotion)) screen.addPosition()
            state.sensors.forEach { screen.addSensor(it) }
            screen.addFooter(KEY_FOOTER, R.string.sensors_footer)
        }
        preferenceScreen = screen
        renderedKeys = keys
        bindPosition(state.sensors)
        state.sensors.forEach(::bind)
    }

    /** Sets the accelerometer and gravity sensors at once. */
    private fun PreferenceScreen.addPosition() {
        addPreference(
            ListPreference(context).apply {
                key = KEY_POSITION
                setTitle(R.string.position_title)
                setDialogTitle(R.string.position_title)
                entries =
                    DevicePosition.entries
                        .map<DevicePosition, CharSequence> { context.getString(it.titleRes) }
                        .toTypedArray()
                entryValues =
                    DevicePosition.entries
                        .map<DevicePosition, CharSequence> { it.name }
                        .toTypedArray()
                isPersistent = false
                isIconSpaceReserved = false
                setOnPreferenceChangeListener { _, newValue ->
                    viewModel.setPosition(DevicePosition.valueOf(newValue as String))
                    true
                }
            }
        )
    }

    private fun PreferenceScreen.addSensor(sensor: VirtualSensor) {
        val key = sensorKey(sensor)
        val category =
            PreferenceCategory(context).apply {
                this.key = key
                title = sensor.name
            }
        // A preference can only be added to a category that is attached to the screen.
        addPreference(category)
        if (SensorKinds.isProximity(sensor)) {
            category.addPreference(
                SwitchPreferenceCompat(context).apply {
                    this.key = "$key/near"
                    setTitle(R.string.proximity_near)
                    isPersistent = false
                    isIconSpaceReserved = false
                    setOnPreferenceChangeListener { _, newValue ->
                        val value = if (newValue as Boolean) sensor.minValue else sensor.maxValue
                        viewModel.set(sensor.handle, floatArrayOf(value))
                        true
                    }
                }
            )
        } else {
            val step = sliderStep(sensor)
            SensorKinds.valueTitles(context, sensor).forEachIndexed { index, valueTitle ->
                category.addPreference(
                    SeekBarPreference(context).apply {
                        this.key = "$key/$index"
                        title = valueTitle
                        // setMin() and setMax() clamp to each other: min, max, then min again
                        // works whatever the defaults.
                        min = (sensor.minValue / step).roundToInt()
                        max = (sensor.maxValue / step).roundToInt()
                        min = (sensor.minValue / step).roundToInt()
                        showSeekBarValue = false
                        updatesContinuously = false
                        isPersistent = false
                        isIconSpaceReserved = false
                        setOnPreferenceChangeListener { _, newValue ->
                            // The other values as they are now, not as they were rendered.
                            val values = (current[sensor.handle] ?: sensor).values.copyOf()
                            values[index] =
                                ((newValue as Int) * step).coerceIn(
                                    sensor.minValue,
                                    sensor.maxValue,
                                )
                            viewModel.set(sensor.handle, values)
                            true
                        }
                    }
                )
            }
        }
        category.addPreference(
            EditTextPreference(context).apply {
                this.key = "$key/exact"
                setTitle(R.string.exact_title)
                dialogTitle = sensor.name
                dialogMessage =
                    context.getString(
                        R.string.exact_message,
                        number.format(sensor.minValue),
                        number.format(sensor.maxValue),
                        SensorKinds.unit(context, sensor),
                    )
                isPersistent = false
                isIconSpaceReserved = false
                setOnBindEditTextListener {
                    it.inputType =
                        InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
                    it.setSelection(it.text.length)
                }
                setOnPreferenceChangeListener { _, newValue ->
                    val values = parseValues(newValue as String, sensor.values.size)
                    if (values == null) {
                        Toast.makeText(
                                context,
                                context.resources.getQuantityString(
                                    R.plurals.exact_invalid,
                                    sensor.values.size,
                                    sensor.values.size,
                                ),
                                Toast.LENGTH_LONG,
                            )
                            .show()
                        return@setOnPreferenceChangeListener false
                    }
                    viewModel.set(sensor.handle, values)
                    true
                }
            }
        )
        category.addPreference(
            Preference(context).apply {
                this.key = "$key/reset"
                setTitle(R.string.reset_title)
                isIconSpaceReserved = false
                setOnPreferenceClickListener {
                    viewModel.reset(sensor.handle)
                    true
                }
            }
        )
    }

    private fun bindPosition(sensors: List<VirtualSensor>) {
        val preference = findPreference<ListPreference>(KEY_POSITION) ?: return
        val positions = sensors.filter(SensorKinds::isMotion).map(DevicePosition::of)
        // Only a position all motion sensors agree on.
        val position = positions.distinct().singleOrNull()
        preference.value = position?.name
        preference.summary = getString(position?.titleRes ?: R.string.position_other)
    }

    /** Shows the sensor's current state on the preferences [addSensor] made for it. */
    private fun bind(sensor: VirtualSensor) {
        val key = sensorKey(sensor)
        findPreference<PreferenceCategory>(key)?.summary = status(sensor)
        val unit = SensorKinds.unit(requireContext(), sensor)
        findPreference<SwitchPreferenceCompat>("$key/near")?.let {
            it.isChecked = sensor.values[0] < sensor.maxValue
            it.summary = formatValue(sensor.values[0], unit)
        }
        val step = sliderStep(sensor)
        sensor.values.forEachIndexed { index, value ->
            findPreference<SeekBarPreference>("$key/$index")?.let {
                it.value = (value / step).roundToInt()
                it.summary = formatValue(value, unit)
            }
        }
        findPreference<EditTextPreference>("$key/exact")?.let {
            val text = sensor.values.joinToString(", ") { value -> number.format(value) }
            if (it.text != text) it.text = text
            it.summary = text
        }
        findPreference<Preference>("$key/reset")?.let {
            it.isEnabled = sensor.stored
            it.setSummary(if (sensor.stored) R.string.reset_summary else R.string.reset_default)
        }
    }

    private fun status(sensor: VirtualSensor): String {
        val parts = mutableListOf<String>()
        parts +=
            if (sensor.active && sensor.samplingPeriodNs > 0) {
                getString(R.string.status_active, number.format(1e9 / sensor.samplingPeriodNs))
            } else if (sensor.active) {
                getString(R.string.status_active_no_rate)
            } else {
                getString(R.string.status_inactive)
            }
        if (sensor.stored) parts += getString(R.string.status_stored)
        return parts.joinToString(" · ")
    }

    private fun formatValue(value: Float, unit: String) =
        if (unit.isEmpty()) number.format(value)
        else getString(R.string.value_with_unit, number.format(value), unit)

    /** Comma or space separated numbers, exactly [count] of them. */
    private fun parseValues(text: String, count: Int): FloatArray? {
        val values =
            text
                .split(',', ' ', ';')
                .filter { it.isNotBlank() }
                .map {
                    it.trim().toFloatOrNull()?.takeIf { value -> value.isFinite() } ?: return null
                }
        return values.takeIf { it.size == count }?.toFloatArray()
    }

    /** A power of ten that splits the sensor's range into at most 1000 slider steps. */
    private fun sliderStep(sensor: VirtualSensor): Float {
        val range = sensor.maxValue - sensor.minValue
        if (range <= 0f) return 1f
        return 10f.pow(ceil(log10(range / MAX_SLIDER_STEPS)))
    }

    private fun sensorKey(sensor: VirtualSensor) = "sensor/${sensor.handle}/${sensor.type}"

    private companion object {
        const val KEY_POSITION = "position"
        const val KEY_FOOTER = "footer"
        const val REFRESH_INTERVAL_MS = 2_000L
        const val MAX_SLIDER_STEPS = 1000f
    }
}
