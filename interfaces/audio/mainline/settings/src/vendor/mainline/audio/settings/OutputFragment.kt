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
import androidx.preference.PreferenceGroup
import androidx.preference.SeekBarPreference
import androidx.preference.SwitchPreferenceCompat
import java.text.DecimalFormat
import kotlinx.coroutines.launch
import vendor.mainline.audio.OutputSettings
import vendor.mainline.audio.settings.common.SettingsFragment

/**
 * The speaker setup of one output: layout, fill, bass management, swap and mono, and per speaker
 * level, delay and test tone. Changes apply right away.
 */
class OutputFragment : SettingsFragment() {
    private val viewModel: OutputViewModel by viewModels()
    private val milliseconds = DecimalFormat("0.0")

    /** The layout the screen was built for: another one needs other speakers. */
    private var renderedLayout: Int? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        viewModel.load(requireArguments().getString(ARG_KEY).orEmpty())
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceScreen = preferenceManager.createPreferenceScreen(requireContext())
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        renderedLayout = null
        viewLifecycleOwner.lifecycleScope.launch {
            viewLifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) {
                launch { viewModel.failures.collect(::showFailure) }
                viewModel.state.collect(::render)
            }
        }
    }

    /** Tells the user about a change the HAL did not take. */
    private fun showFailure(message: Int) {
        Toast.makeText(requireContext(), message, Toast.LENGTH_LONG).show()
    }

    private fun render(state: OutputViewModel.State) {
        if (state !is OutputViewModel.State.Loaded) {
            val screen = preferenceManager.createPreferenceScreen(requireContext())
            if (state is OutputViewModel.State.Unavailable) {
                screen.addFooter(KEY_FOOTER, R.string.hal_unavailable)
            }
            preferenceScreen = screen
            renderedLayout = null
            return
        }
        if (renderedLayout != state.settings.layout) {
            build(state)
            renderedLayout = state.settings.layout
        }
        bind(state.settings)
    }

    /** Creates the preferences for the state's layout; [bind] fills in the values. */
    private fun build(state: OutputViewModel.State.Loaded) {
        val context = requireContext()
        val screen = preferenceManager.createPreferenceScreen(context)
        val speakers = Speakers.of(state.settings.layout)

        val setup = screen.addCategory(KEY_SETUP, getString(R.string.output_setup))
        val layouts =
            LAYOUTS.filter { (layout, _) ->
                layout == OutputSettings.LAYOUT_AUTO ||
                    Speakers.of(layout).size <= state.endpoint.maxChannels
            }
        setup.addPreference(
            ListPreference(context).apply {
                key = KEY_LAYOUT
                setTitle(R.string.output_layout)
                entries = layouts.map { getString(it.second) }.toTypedArray()
                entryValues = layouts.map { it.first.toString() }.toTypedArray()
                dialogTitle = getString(R.string.output_layout)
                common()
                setOnPreferenceChangeListener { _, value ->
                    val layout = (value as String).toInt()
                    viewModel.update { this.layout = layout }
                    false
                }
            }
        )
        setup.addPreference(
            switch(KEY_FILL, R.string.output_fill, R.string.output_fill_summary) { fill = it }
        )
        setup.addPreference(switch(KEY_SWAP, R.string.output_swap, null) { swapLeftRight = it })
        setup.addPreference(
            switch(KEY_MONO, R.string.output_mono, R.string.output_mono_summary) { mono = it }
        )

        if (Speakers.hasSubwoofer(state.settings.layout)) {
            val bass = screen.addCategory(KEY_BASS, getString(R.string.output_bass))
            bass.addPreference(
                switch(
                    KEY_SUBWOOFER,
                    R.string.output_subwoofer,
                    R.string.output_subwoofer_summary,
                ) {
                    subwoofer = it
                }
            )
            bass.addPreference(
                switch(KEY_SMALL, R.string.output_small, R.string.output_small_summary) {
                    smallSpeakers = it
                }
            )
            bass.addPreference(
                slider(KEY_CROSSOVER, R.string.output_crossover, CROSSOVER_MIN, CROSSOVER_MAX) {
                        crossoverHz = it
                    }
                    .apply { seekBarIncrement = CROSSOVER_STEP }
            )
        }

        speakers.forEachIndexed { channel, speaker ->
            val category = screen.addCategory("speaker/$channel", getString(speaker.titleRes))
            category.addPreference(
                slider("speaker/$channel/level", R.string.output_level, LEVEL_MIN, LEVEL_MAX) {
                        levelsDb[channel] = it.toFloat()
                    }
                    .apply { seekBarIncrement = LEVEL_STEP }
            )
            category.addPreference(
                slider("speaker/$channel/delay", R.string.output_delay, 0, DELAY_MAX_TENTHS) {
                        delaysMs[channel] = it / 10f
                    }
                    .apply {
                        showSeekBarValue = false
                        seekBarIncrement = DELAY_STEP_TENTHS
                    }
            )
            category.addPreference(
                Preference(context).apply {
                    key = "speaker/$channel/test"
                    setTitle(R.string.output_test)
                    setSummary(R.string.output_test_summary)
                    isIconSpaceReserved = false
                    setOnPreferenceClickListener {
                        viewModel.playTestTone(channel)
                        true
                    }
                }
            )
        }

        val actions = screen.addCategory(KEY_ACTIONS, getString(R.string.settings_actions))
        actions.addPreference(
            Preference(context).apply {
                key = KEY_RESET
                setTitle(R.string.output_reset)
                setSummary(R.string.output_reset_summary)
                isIconSpaceReserved = false
                setOnPreferenceClickListener {
                    viewModel.reset()
                    true
                }
            }
        )
        screen.addFooter(
            KEY_FOOTER,
            if (speakers.isEmpty()) R.string.output_auto_footer else R.string.output_footer,
        )
        preferenceScreen = screen
    }

    /** Shows the values of [settings] on the preferences [build] made. */
    private fun bind(settings: OutputSettings) {
        val auto = settings.layout == OutputSettings.LAYOUT_AUTO
        findPreference<ListPreference>(KEY_LAYOUT)?.apply {
            value = settings.layout.toString()
            summary = entry
        }
        findPreference<SwitchPreferenceCompat>(KEY_FILL)?.apply {
            isChecked = settings.fill
            isEnabled = !auto
        }
        findPreference<SwitchPreferenceCompat>(KEY_SWAP)?.isChecked = settings.swapLeftRight
        findPreference<SwitchPreferenceCompat>(KEY_MONO)?.isChecked = settings.mono
        findPreference<SwitchPreferenceCompat>(KEY_SUBWOOFER)?.isChecked = settings.subwoofer
        findPreference<SwitchPreferenceCompat>(KEY_SMALL)?.apply {
            isChecked = settings.smallSpeakers
            isEnabled = settings.subwoofer
        }
        findPreference<SeekBarPreference>(KEY_CROSSOVER)?.apply {
            value = settings.crossoverHz
            summary = getString(R.string.output_crossover_value, settings.crossoverHz)
            isEnabled = settings.subwoofer && (settings.smallSpeakers || settings.fill)
        }
        Speakers.of(settings.layout).indices.forEach { channel ->
            val level = settings.levelsDb.getOrElse(channel) { 0f }
            val delay = settings.delaysMs.getOrElse(channel) { 0f }
            findPreference<SeekBarPreference>("speaker/$channel/level")?.apply {
                value = Math.round(level)
                summary = getString(R.string.output_level_value, Math.round(level))
            }
            findPreference<SeekBarPreference>("speaker/$channel/delay")?.apply {
                value = Math.round(delay * 10)
                summary = getString(R.string.output_delay_value, milliseconds.format(delay))
            }
            findPreference<Preference>("speaker/$channel")?.isVisible =
                Speakers.of(settings.layout)[channel] != Speakers.SUBWOOFER || settings.subwoofer
        }
    }

    private fun switch(
        key: String,
        titleRes: Int,
        summaryRes: Int?,
        change: OutputSettings.(Boolean) -> Unit,
    ) =
        SwitchPreferenceCompat(requireContext()).apply {
            this.key = key
            setTitle(titleRes)
            summaryRes?.let { setSummary(it) }
            common()
            setOnPreferenceChangeListener { _, value ->
                viewModel.update { change(value as Boolean) }
                false
            }
        }

    private fun slider(
        key: String,
        titleRes: Int,
        min: Int,
        max: Int,
        change: OutputSettings.(Int) -> Unit,
    ) =
        SeekBarPreference(requireContext()).apply {
            this.key = key
            setTitle(titleRes)
            // setMin() and setMax() clamp to each other: min, max, then min again.
            this.min = min
            this.max = max
            this.min = min
            updatesContinuously = true
            common()
            setOnPreferenceChangeListener { _, value ->
                viewModel.update { change(value as Int) }
                true
            }
        }

    private fun Preference.common() {
        isPersistent = false
        isIconSpaceReserved = false
    }

    private fun PreferenceGroup.addCategory(key: String, title: String): PreferenceCategory {
        val category =
            PreferenceCategory(context).apply {
                this.key = key
                this.title = title
            }
        addPreference(category)
        return category
    }

    companion object {
        /** EndpointStatus.key of the output. */
        const val ARG_KEY = "key"

        private const val KEY_SETUP = "setup"
        private const val KEY_LAYOUT = "layout"
        private const val KEY_FILL = "fill"
        private const val KEY_SWAP = "swap"
        private const val KEY_MONO = "mono"
        private const val KEY_BASS = "bass"
        private const val KEY_SUBWOOFER = "subwoofer"
        private const val KEY_SMALL = "small"
        private const val KEY_CROSSOVER = "crossover"
        private const val KEY_ACTIONS = "actions"
        private const val KEY_RESET = "reset"
        private const val KEY_FOOTER = "footer"

        private const val CROSSOVER_MIN = 40
        private const val CROSSOVER_MAX = 250
        private const val CROSSOVER_STEP = 10
        private const val LEVEL_MIN = -30
        private const val LEVEL_MAX = 10
        private const val DELAY_MAX_TENTHS = 200

        // What one press of a remote's left / right key changes (by default a twentieth of the
        // range: 2 dB, 1 ms).
        private const val LEVEL_STEP = 1
        private const val DELAY_STEP_TENTHS = 5

        private val LAYOUTS =
            listOf(
                OutputSettings.LAYOUT_AUTO to R.string.layout_auto,
                OutputSettings.LAYOUT_STEREO to R.string.layout_stereo,
                OutputSettings.LAYOUT_QUAD to R.string.layout_quad,
                OutputSettings.LAYOUT_5_1 to R.string.layout_5_1,
                OutputSettings.LAYOUT_7_1 to R.string.layout_7_1,
            )
    }
}
