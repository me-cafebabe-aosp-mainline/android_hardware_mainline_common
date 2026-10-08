/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.os.Bundle
import android.text.InputType
import android.view.View
import androidx.fragment.app.viewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.preference.EditTextPreference
import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceGroup
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import vendor.mainline.audio.Setting

/**
 * The HAL's vendor.audio.mainline.* settings. Edits collect until Apply, which restarts the HAL
 * with them on trial; [ConfirmSettingsDialogFragment] then asks to keep them.
 */
class HalSettingsFragment : SettingsFragment() {
    private val viewModel: HalSettingsViewModel by viewModels()

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceScreen = preferenceManager.createPreferenceScreen(requireContext())
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        viewLifecycleOwner.lifecycleScope.launch {
            viewLifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) {
                launch { viewModel.ui.collect(::render) }
                launch {
                    viewModel.ui
                        .map {
                            it is HalSettingsViewModel.Ui.Loaded &&
                                it.state.trial.active &&
                                !it.state.restarting
                        }
                        .distinctUntilChanged()
                        .collect(::updateConfirmDialog)
                }
            }
        }
    }

    private fun render(ui: HalSettingsViewModel.Ui) {
        val context = requireContext()
        val screen = preferenceManager.createPreferenceScreen(context)
        when (ui) {
            HalSettingsViewModel.Ui.Loading -> {}
            HalSettingsViewModel.Ui.Unavailable ->
                screen.addFooter(KEY_FOOTER, R.string.hal_unavailable)
            is HalSettingsViewModel.Ui.Loaded -> renderLoaded(screen, ui.state)
        }
        preferenceScreen = screen
    }

    private fun renderLoaded(screen: PreferenceGroup, state: HalSettingsViewModel.State) {
        val context = requireContext()
        if (state.restarting) {
            screen.addFooter(KEY_HEADER, R.string.settings_restarting)
            return
        }
        screen.addFooter(KEY_HEADER, R.string.settings_header)
        if (state.trial.revertedAfterFailure)
            screen.addFooter(KEY_REVERTED, R.string.settings_reverted)

        val byKey = state.settings.associateBy { it.key }
        for (group in GROUPS) {
            val settings = group.keys.mapNotNull { byKey[it] }
            if (settings.isEmpty()) continue
            val category = screen.addCategory("group/${group.title}", getString(group.title))
            settings.forEach { category.addPreference(create(it, state)) }
        }
        for (card in state.cards) {
            val settings = CARD_SUFFIXES.mapNotNull { byKey["card.${card.id}.$it"] }
            if (settings.isEmpty()) continue
            val category =
                screen.addCategory("card/${card.id}", StatusText(context).cardTitle(card))
            settings.forEach { category.addPreference(create(it, state)) }
        }

        val actions = screen.addCategory(KEY_ACTIONS, getString(R.string.settings_actions))
        actions.addPreference(
            Preference(context).apply {
                key = "$KEY_ACTIONS/apply"
                setTitle(R.string.settings_apply)
                setSummary(R.string.settings_apply_summary)
                isEnabled = state.edits.isNotEmpty() && !state.trial.active
                setOnPreferenceClickListener {
                    viewModel.apply()
                    true
                }
            }
        )
        actions.addPreference(
            Preference(context).apply {
                key = "$KEY_ACTIONS/discard"
                setTitle(R.string.settings_discard)
                isEnabled = state.edits.isNotEmpty()
                setOnPreferenceClickListener {
                    viewModel.discardEdits()
                    true
                }
            }
        )
        actions.addPreference(
            Preference(context).apply {
                key = "$KEY_ACTIONS/reset"
                setTitle(R.string.settings_reset)
                setSummary(R.string.settings_reset_summary)
                isEnabled = state.hasStored && !state.trial.active
                setOnPreferenceClickListener {
                    viewModel.resetAll()
                    true
                }
            }
        )
    }

    private fun create(setting: Setting, state: HalSettingsViewModel.State): Preference {
        val context = requireContext()
        val edited = state.edits.containsKey(setting.key)
        // What the HAL would run with after Apply: the edit, else the stored value, else the
        // device's property.
        val target =
            if (edited) state.edits[setting.key] ?: setting.defaultValue
            else setting.stored ?: setting.defaultValue
        val preference =
            if (setting.type == Setting.TYPE_BOOLEAN) {
                ListPreference(context).apply {
                    entries =
                        arrayOf(
                            getString(
                                R.string.setting_default,
                                describe(setting, setting.defaultValue),
                            ),
                            getString(R.string.setting_on),
                            getString(R.string.setting_off),
                        )
                    entryValues = arrayOf(VALUE_DEFAULT, "true", "false")
                    value =
                        when {
                            edited -> state.edits[setting.key] ?: VALUE_DEFAULT
                            else -> setting.stored ?: VALUE_DEFAULT
                        }
                }
            } else {
                EditTextPreference(context).apply {
                    text =
                        if (edited) state.edits[setting.key].orEmpty() else setting.stored.orEmpty()
                    dialogMessage =
                        getString(
                            R.string.setting_edit_hint,
                            describe(setting, setting.defaultValue),
                        )
                    setOnBindEditTextListener { edit ->
                        edit.inputType =
                            if (setting.type == Setting.TYPE_INTEGER) {
                                InputType.TYPE_CLASS_NUMBER
                            } else {
                                InputType.TYPE_CLASS_TEXT
                            }
                        edit.setSelection(edit.text.length)
                    }
                }
            }
        return preference.apply {
            key = setting.key
            title = titleOf(setting.key)
            isPersistent = false
            isIconSpaceReserved = false
            isEnabled = !state.trial.active
            summary =
                buildList {
                        add(describe(setting, target))
                        when {
                            edited -> add(getString(R.string.setting_edited))
                            setting.stored != null -> add(getString(R.string.setting_stored))
                        }
                        if (setting.value != target && !edited) {
                            add(
                                getString(
                                    R.string.setting_running,
                                    describe(setting, setting.value),
                                )
                            )
                        }
                    }
                    .joinToString(" · ")
            setOnPreferenceChangeListener { _, newValue ->
                val text = (newValue as? String)?.trim().orEmpty()
                viewModel.edit(setting.key, text.takeUnless { it.isEmpty() || it == VALUE_DEFAULT })
                false
            }
        }
    }

    /** A value as the user reads it: "On", "Off", "Not set", or the text. */
    private fun describe(setting: Setting, value: String): String =
        when {
            setting.type == Setting.TYPE_BOOLEAN && value == "true" ->
                getString(R.string.setting_on)
            setting.type == Setting.TYPE_BOOLEAN && value == "false" ->
                getString(R.string.setting_off)
            value.isEmpty() -> getString(R.string.setting_unset)
            else -> value
        }

    private fun titleOf(key: String): String {
        val suffix = key.substringAfterLast('.')
        val res = if (key.startsWith("card.")) CARD_TITLES[suffix] else TITLES[key]
        return res?.let { getString(it) } ?: key
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

    private fun updateConfirmDialog(show: Boolean) {
        val dialog = childFragmentManager.findFragmentByTag(ConfirmSettingsDialogFragment.TAG)
        if (show && dialog == null) {
            ConfirmSettingsDialogFragment()
                .show(childFragmentManager, ConfirmSettingsDialogFragment.TAG)
        } else if (!show && dialog is ConfirmSettingsDialogFragment) {
            dialog.dismissAllowingStateLoss()
        }
    }

    private data class Group(val title: Int, val keys: List<String>)

    private companion object {
        const val KEY_HEADER = "header"
        const val KEY_REVERTED = "reverted"
        const val KEY_ACTIONS = "actions"
        const val KEY_FOOTER = "footer"
        const val VALUE_DEFAULT = "default"

        val GROUPS =
            listOf(
                Group(
                    R.string.settings_group_cards,
                    listOf(
                        "cards",
                        "primary_card",
                        "include_usb_cards",
                        "wait_for_cards_ms",
                        "null_mic",
                    ),
                ),
                Group(R.string.settings_group_ucm, listOf("ucm.enabled", "ucm.verb")),
                Group(
                    R.string.settings_group_streams,
                    listOf("latency_ms", "fast_latency_ms", "multichannel"),
                ),
                Group(
                    R.string.settings_group_hdmi,
                    listOf("hdmi.passthrough", "hdmi.passthrough_formats"),
                ),
                Group(
                    R.string.settings_group_mixer,
                    listOf("mixer.init", "mixer.playback_percent", "mixer.capture_percent"),
                ),
                Group(R.string.settings_group_debug, listOf("log.verbose")),
            )

        val CARD_SUFFIXES = listOf("rates", "bits", "hbr")

        val TITLES =
            mapOf(
                "cards" to R.string.setting_cards,
                "primary_card" to R.string.setting_primary_card,
                "include_usb_cards" to R.string.setting_include_usb_cards,
                "wait_for_cards_ms" to R.string.setting_wait_for_cards_ms,
                "null_mic" to R.string.setting_null_mic,
                "ucm.enabled" to R.string.setting_ucm_enabled,
                "ucm.verb" to R.string.setting_ucm_verb,
                "latency_ms" to R.string.setting_latency_ms,
                "fast_latency_ms" to R.string.setting_fast_latency_ms,
                "multichannel" to R.string.setting_multichannel,
                "hdmi.passthrough" to R.string.setting_hdmi_passthrough,
                "hdmi.passthrough_formats" to R.string.setting_hdmi_passthrough_formats,
                "mixer.init" to R.string.setting_mixer_init,
                "mixer.playback_percent" to R.string.setting_mixer_playback_percent,
                "mixer.capture_percent" to R.string.setting_mixer_capture_percent,
                "log.verbose" to R.string.setting_log_verbose,
            )

        val CARD_TITLES =
            mapOf(
                "rates" to R.string.setting_card_rates,
                "bits" to R.string.setting_card_bits,
                "hbr" to R.string.setting_card_hbr,
            )
    }
}
