/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.fragment.app.viewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import kotlinx.coroutines.launch
import vendor.mainline.audio.EndpointStatus

/** Sound cards and endpoints as the HAL sees them, and the report for bug reports. */
class DiagnosticsFragment : SettingsFragment() {
    private val viewModel: DiagnosticsViewModel by viewModels()

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceScreen = preferenceManager.createPreferenceScreen(requireContext())
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        viewLifecycleOwner.lifecycleScope.launch {
            viewLifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.refresh()
                viewModel.state.collect(::render)
            }
        }
    }

    private fun render(state: DiagnosticsViewModel.State) {
        val context = requireContext()
        val screen = preferenceManager.createPreferenceScreen(context)
        val text = StatusText(context)

        val report =
            PreferenceCategory(context).apply {
                key = KEY_REPORT
                setTitle(R.string.report_title)
            }
        screen.addPreference(report)
        report.addPreference(
            Preference(context).apply {
                key = "$KEY_REPORT/copy"
                setTitle(R.string.report_copy)
                setSummary(R.string.report_copy_summary)
                setOnPreferenceClickListener {
                    withReport(::copyReport)
                    true
                }
            }
        )
        report.addPreference(
            Preference(context).apply {
                key = "$KEY_REPORT/share"
                setTitle(R.string.report_share)
                setOnPreferenceClickListener {
                    withReport(::shareReport)
                    true
                }
            }
        )

        when (state) {
            DiagnosticsViewModel.State.Loading -> {}
            DiagnosticsViewModel.State.Unavailable ->
                screen.addFooter(KEY_FOOTER, R.string.hal_unavailable)
            is DiagnosticsViewModel.State.Loaded -> {
                val status = state.status
                for (card in status.cards) {
                    val category =
                        PreferenceCategory(context).apply {
                            key = "card/${card.index}"
                            title = text.cardTitle(card)
                        }
                    screen.addPreference(category)
                    category.addPreference(info("card/${card.index}/info", text.cardSummary(card)))
                    for (endpoint in status.endpoints.filter { it.card == card.index }) {
                        category.addPreference(endpoint(endpoint, text))
                    }
                }
                val cardless = status.endpoints.filter { it.nullDevice }
                if (cardless.isNotEmpty()) {
                    val category =
                        PreferenceCategory(context).apply {
                            key = "null"
                            setTitle(R.string.no_card_title)
                        }
                    screen.addPreference(category)
                    cardless.forEach { category.addPreference(endpoint(it, text)) }
                }
            }
        }
        preferenceScreen = screen
    }

    private fun endpoint(endpoint: EndpointStatus, text: StatusText) =
        info("endpoint/${endpoint.name}/${endpoint.input}", text.endpointSummary(endpoint)).apply {
            title =
                getString(
                    if (endpoint.input) R.string.endpoint_input else R.string.endpoint_output,
                    endpoint.name,
                )
        }

    private fun info(key: String, summary: String) =
        Preference(requireContext()).apply {
            this.key = key
            this.summary = summary
            isSelectable = false
            isIconSpaceReserved = false
        }

    private fun withReport(action: (String) -> Unit) {
        viewLifecycleOwner.lifecycleScope.launch {
            val report = viewModel.report()
            if (report == null) {
                Toast.makeText(requireContext(), R.string.hal_unavailable, Toast.LENGTH_SHORT)
                    .show()
            } else {
                action(report)
            }
        }
    }

    private fun copyReport(report: String) {
        val clipboard = requireContext().getSystemService(ClipboardManager::class.java)
        clipboard.setPrimaryClip(ClipData.newPlainText(getString(R.string.report_title), report))
    }

    private fun shareReport(report: String) {
        val send =
            Intent(Intent.ACTION_SEND).apply {
                type = "text/plain"
                putExtra(Intent.EXTRA_SUBJECT, getString(R.string.report_title))
                putExtra(Intent.EXTRA_TEXT, report)
            }
        startActivity(Intent.createChooser(send, getString(R.string.report_share)))
    }

    private companion object {
        const val KEY_REPORT = "report"
        const val KEY_FOOTER = "footer"
    }
}
