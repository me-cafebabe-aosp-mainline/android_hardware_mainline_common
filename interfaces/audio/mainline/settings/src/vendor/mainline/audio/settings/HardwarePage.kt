/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.content.Context
import android.content.Intent
import androidx.fragment.app.Fragment

/** The hardware pages, each of which [AudioSettingsActivity] can also open on its own. */
enum class HardwarePage(
    val key: String,
    val titleRes: Int,
    val summaryRes: Int,
    val fragment: Class<out Fragment>,
) {
    DIAGNOSTICS(
        "diagnostics",
        R.string.diagnostics_title,
        R.string.diagnostics_summary,
        DiagnosticsFragment::class.java,
    ),
    SPEAKERS(
        "speakers",
        R.string.speakers_title,
        R.string.speakers_summary,
        SpeakersFragment::class.java,
    ),
    MIXER("mixer", R.string.mixer_title, R.string.mixer_summary, MixerFragment::class.java),
    SETTINGS(
        "settings",
        R.string.settings_page_title,
        R.string.settings_page_summary,
        HalSettingsFragment::class.java,
    );

    /** Opens this page alone; Back leaves the app. */
    fun intent(context: Context): Intent =
        Intent(context, AudioSettingsActivity::class.java)
            .putExtra(EXTRA_PAGE, key)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)

    companion object {
        /** [key] of the page [AudioSettingsActivity] opens instead of the entry page. */
        const val EXTRA_PAGE = "vendor.mainline.audio.settings.extra.PAGE"

        fun fromIntent(intent: Intent?): HardwarePage? {
            val key = intent?.getStringExtra(EXTRA_PAGE) ?: return null
            return entries.firstOrNull { it.key == key }
        }
    }
}
