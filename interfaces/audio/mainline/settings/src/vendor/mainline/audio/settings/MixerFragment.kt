/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.fragment.app.viewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import androidx.preference.SeekBarPreference
import androidx.preference.SwitchPreferenceCompat
import java.text.DecimalFormat
import kotlinx.coroutines.launch
import vendor.mainline.audio.MixerControl
import vendor.mainline.audio.settings.common.SettingsFragment

/**
 * Every adjustable ALSA mixer control of the HAL's cards. Values the user sets are kept by the HAL
 * and applied again at every start and after UCM switched devices.
 */
class MixerFragment : SettingsFragment() {
    private val viewModel: MixerViewModel by viewModels()
    private val decibels = DecimalFormat("0.#")

    /** Keys of the rendered preferences: the same keys again are updated in place. */
    private var renderedKeys: List<String> = emptyList()

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceScreen = preferenceManager.createPreferenceScreen(requireContext())
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        renderedKeys = emptyList()
        viewLifecycleOwner.lifecycleScope.launch {
            viewLifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.refresh()
                launch { viewModel.failures.collect(::showFailure) }
                viewModel.state.collect(::render)
            }
        }
    }

    /** Tells the user about a change the HAL did not take. */
    private fun showFailure(message: Int) {
        Toast.makeText(requireContext(), message, Toast.LENGTH_LONG).show()
    }

    private fun render(state: MixerViewModel.State) {
        if (state !is MixerViewModel.State.Loaded) {
            val screen = preferenceManager.createPreferenceScreen(requireContext())
            if (state is MixerViewModel.State.Unavailable) {
                screen.addFooter(KEY_FOOTER, R.string.hal_unavailable)
            }
            preferenceScreen = screen
            renderedKeys = emptyList()
            return
        }
        val keys = state.cards.flatMap { card -> card.controls.map { key(card, it) } }
        if (keys == renderedKeys) {
            // Same controls: update them, which keeps the scroll position and a moving slider.
            for (card in state.cards) {
                card.controls.forEach { control ->
                    findPreference<Preference>(key(card, control))?.let { bind(it, card, control) }
                }
                findPreference<Preference>(forgetKey(card))?.isEnabled =
                    card.controls.any { it.stored }
            }
            return
        }
        val context = requireContext()
        val screen = preferenceManager.createPreferenceScreen(context)
        screen.addFooter(KEY_HEADER, R.string.mixer_header)
        for (card in state.cards) {
            val category =
                PreferenceCategory(context).apply {
                    key = "card/${card.status.index}"
                    title = StatusText(context).cardTitle(card.status)
                }
            screen.addPreference(category)
            if (card.controls.isEmpty()) {
                category.addPreference(
                    Preference(context).apply {
                        key = "card/${card.status.index}/empty"
                        setSummary(R.string.mixer_no_controls)
                        isSelectable = false
                    }
                )
                continue
            }
            for (control in card.controls) {
                category.addPreference(create(card, control).also { bind(it, card, control) })
            }
            category.addPreference(
                Preference(context).apply {
                    key = forgetKey(card)
                    setTitle(R.string.mixer_forget)
                    setSummary(R.string.mixer_forget_summary)
                    isEnabled = card.controls.any { it.stored }
                    setOnPreferenceClickListener {
                        viewModel.forget(card.status.index)
                        true
                    }
                }
            )
        }
        preferenceScreen = screen
        renderedKeys = keys
    }

    private fun create(card: MixerViewModel.Card, control: MixerControl): Preference {
        val context = requireContext()
        val preference =
            when (control.type) {
                MixerControl.TYPE_BOOLEAN -> SwitchPreferenceCompat(context)
                MixerControl.TYPE_ENUMERATED ->
                    ListPreference(context).apply {
                        entries = control.items
                        entryValues = control.items.indices.map { it.toString() }.toTypedArray()
                        dialogTitle = control.name
                    }
                else ->
                    SeekBarPreference(context).apply {
                        // setMin() and setMax() clamp to each other: min, max, then min again
                        // works whatever the defaults.
                        min = control.min.toIntRange()
                        max = control.max.toIntRange()
                        min = control.min.toIntRange()
                        if (control.step > 0) seekBarIncrement = control.step.toIntRange()
                        showSeekBarValue = true
                        updatesContinuously = false
                    }
            }
        return preference.apply {
            key = key(card, control)
            title = control.name
            isPersistent = false
            isIconSpaceReserved = false
            setOnPreferenceChangeListener { _, newValue ->
                val value =
                    when (newValue) {
                        is Boolean -> if (newValue) 1L else 0L
                        is Int -> newValue.toLong()
                        is String ->
                            newValue.toLongOrNull() ?: return@setOnPreferenceChangeListener false
                        else -> return@setOnPreferenceChangeListener false
                    }
                viewModel.set(card.status.index, control, value)
                true
            }
        }
    }

    /** Shows the control's current value on [preference], which [create] made for it. */
    private fun bind(preference: Preference, card: MixerViewModel.Card, control: MixerControl) {
        val value = control.values.maxOrNull() ?: 0L
        when (preference) {
            is SwitchPreferenceCompat -> preference.isChecked = value != 0L
            is ListPreference -> preference.value = value.toString()
            is SeekBarPreference -> preference.value = value.toIntRange()
        }
        preference.summary = summary(control)
    }

    private fun summary(control: MixerControl): String? {
        val parts = mutableListOf<String>()
        if (control.type == MixerControl.TYPE_ENUMERATED) {
            control.items.getOrNull(control.values.firstOrNull()?.toInt() ?: -1)?.let {
                parts += it
            }
        }
        if (control.hasDbRange) {
            parts +=
                getString(
                    R.string.mixer_db_range,
                    decibels.format(control.minDb / 100.0),
                    decibels.format(control.maxDb / 100.0),
                )
        }
        if (control.values.distinct().size > 1) {
            parts += getString(R.string.mixer_channels, control.values.joinToString())
        }
        if (control.stored) parts += getString(R.string.mixer_stored)
        return parts.takeIf { it.isNotEmpty() }?.joinToString(" · ")
    }

    private fun Long.toIntRange() = coerceIn(Int.MIN_VALUE.toLong(), Int.MAX_VALUE.toLong()).toInt()

    private fun key(card: MixerViewModel.Card, control: MixerControl) =
        "card/${card.status.index}/${control.numid}"

    private fun forgetKey(card: MixerViewModel.Card) = "card/${card.status.index}/forget"

    private companion object {
        const val KEY_HEADER = "header"
        const val KEY_FOOTER = "footer"
    }
}
