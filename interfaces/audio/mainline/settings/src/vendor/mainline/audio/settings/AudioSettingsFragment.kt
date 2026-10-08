/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.os.Bundle
import android.view.View
import androidx.fragment.app.viewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceScreen
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.launch

/** The entry page: default output and input. */
class AudioSettingsFragment : SettingsFragment() {
    private val viewModel: RoutingViewModel by viewModels()

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceScreen = preferenceManager.createPreferenceScreen(requireContext())
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        viewLifecycleOwner.lifecycleScope.launch {
            viewLifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.state.filterNotNull().collect(::render)
            }
        }
    }

    private fun render(state: RoutingViewModel.State) {
        // A new screen rather than an emptied one, so that the list keeps no stale preferences.
        val screen = preferenceManager.createPreferenceScreen(requireContext())
        screen.addSelection(
            KEY_OUTPUT,
            R.string.output_title,
            R.string.automatic_output_summary,
            state.output,
            viewModel::selectOutput,
        )
        if (state.input.options.isNotEmpty()) {
            screen.addSelection(
                KEY_INPUT,
                R.string.input_title,
                R.string.automatic_input_summary,
                state.input,
                viewModel::selectInput,
            )
        } else {
            screen.addPreference(
                Preference(requireContext()).apply {
                    key = KEY_INPUT
                    setTitle(R.string.input_title)
                    setSummary(R.string.no_inputs)
                    isSelectable = false
                }
            )
        }
        if (state.halAvailable) screen.addHardware()
        screen.addFooter(KEY_FOOTER, R.string.routing_footer)
        preferenceScreen = screen
    }

    /** Pages that need the mainline audio HAL. */
    private fun PreferenceScreen.addHardware() {
        val category =
            PreferenceCategory(context).apply {
                key = KEY_HARDWARE
                setTitle(R.string.hardware_title)
            }
        addPreference(category)
        category.addPreference(
            Preference(context).apply {
                key = "$KEY_HARDWARE/diagnostics"
                setTitle(R.string.diagnostics_title)
                setSummary(R.string.diagnostics_summary)
                fragment = DiagnosticsFragment::class.java.name
            }
        )
    }

    private fun PreferenceScreen.addSelection(
        key: String,
        titleRes: Int,
        automaticSummaryRes: Int,
        selection: RoutingViewModel.Selection,
        select: (DeviceId?) -> Unit,
    ) {
        val category =
            PreferenceCategory(context).apply {
                this.key = key
                setTitle(titleRes)
                if (selection.choice == AudioRouting.Choice.Mixed)
                    setSummary(R.string.mixed_summary)
            }
        // A preference can only be added to a category that is attached to the screen.
        addPreference(category)
        category.addPreference(
            RadioPreference(context).apply {
                this.key = "$key/automatic"
                setTitle(R.string.automatic_title)
                setSummary(automaticSummaryRes)
                isChecked = selection.choice == AudioRouting.Choice.Automatic
                setOnPreferenceClickListener {
                    select(null)
                    true
                }
            }
        )
        val chosen = (selection.choice as? AudioRouting.Choice.Device)?.id
        for (option in selection.options) {
            category.addPreference(
                RadioPreference(context).apply {
                    this.key = "$key/${option.id.serialize()}"
                    title = option.label
                    if (!option.connected) setSummary(R.string.not_connected)
                    isChecked = option.id == chosen
                    setOnPreferenceClickListener {
                        select(option.id)
                        true
                    }
                }
            )
        }
    }

    private companion object {
        const val KEY_OUTPUT = "output"
        const val KEY_INPUT = "input"
        const val KEY_HARDWARE = "hardware"
        const val KEY_FOOTER = "footer"
    }
}
