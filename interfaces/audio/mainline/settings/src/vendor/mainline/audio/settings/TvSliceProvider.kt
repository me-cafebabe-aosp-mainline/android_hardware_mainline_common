/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.app.PendingIntent
import android.app.slice.Slice
import android.app.slice.SliceProvider
import android.app.slice.SliceSpec
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri

/**
 * The page TvSettings shows in Display & sound for its "default audio output settings" slot, which
 * the device's overlay points at [SLICE_URI]: one row per page of this app and the routing app,
 * each opening that page.
 *
 * Built with the framework's slice API alone, in the format TvSettings' TwoPanelSettingsLib reads
 * (`SlicePreferenceUtil.extract()`, the row layout of `PreferenceSliceBuilderImpl`). That library
 * is not available to an app in an APEX. Only callers holding [AUTO_GRANT] (TvSettings, signed with
 * the platform key) are given the slice; others get the framework's permission request.
 */
class TvSliceProvider : SliceProvider(AUTO_GRANT) {
    override fun onCreate() = true

    // Runs under a StrictMode policy that kills the process on disk reads: resources, the package
    // manager and PendingIntents only, never the HAL.
    override fun onBindSlice(sliceUri: Uri, supportedSpecs: Set<SliceSpec>): Slice? {
        if (sliceUri.authority != AUTHORITY || sliceUri.path != PATH) return null
        val context = context ?: return null
        val slice = Slice.Builder(sliceUri, LIST_SPEC)
        slice.addSubSlice(
            Slice.Builder(slice)
                .addText(context.getString(R.string.tv_slice_title), null, listOf(Slice.HINT_TITLE))
                .addHints(listOf(Slice.HINT_LIST_ITEM))
                .build(),
            TYPE_PREFERENCE_SCREEN_TITLE,
        )
        routingIntent(context)?.let { intent ->
            slice.addSubSlice(
                row(
                    context,
                    slice,
                    KEY_ROUTING,
                    context.getString(R.string.tv_routing_title),
                    context.getString(R.string.tv_routing_summary),
                    intent,
                ),
                TYPE_PREFERENCE,
            )
        }
        for (page in HardwarePage.entries) {
            slice.addSubSlice(
                row(
                    context,
                    slice,
                    page.key,
                    context.getString(page.titleRes),
                    context.getString(page.summaryRes),
                    page.intent(context),
                ),
                TYPE_PREFERENCE,
            )
        }
        return slice.build()
    }

    /** The routing app's page, if it is installed. */
    private fun routingIntent(context: Context): Intent? {
        val intent =
            Intent()
                .setClassName(ROUTING_PACKAGE, ROUTING_ACTIVITY)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        val found =
            context.packageManager.resolveActivity(intent, PackageManager.ResolveInfoFlags.of(0))
        return intent.takeIf { found != null }
    }

    /** A preference that opens [intent]: title, summary, key, and the action TvSettings fires. */
    private fun row(
        context: Context,
        parent: Slice.Builder,
        key: String,
        title: String,
        summary: String,
        intent: Intent,
    ): Slice {
        val row = Slice.Builder(parent)
        val pending =
            PendingIntent.getActivity(
                context,
                key.hashCode(),
                intent,
                PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
            )
        val action = Slice.Builder(row).addHints(listOf(Slice.HINT_TITLE, Slice.HINT_SHORTCUT))
        val label = Slice.Builder(action).addText(title, null, listOf(Slice.HINT_TITLE)).build()
        action.addAction(pending, label, null)
        // The title comes first: TvSettings takes the first item with HINT_TITLE as the row's
        // start item if it is a slice, which the action is.
        return row.addText(title, null, listOf(Slice.HINT_TITLE))
            .addText(summary, null, emptyList())
            .addText(key, TAG_KEY, listOf(Slice.HINT_KEYWORDS))
            .addSubSlice(action.build(), SUBTYPE_INTENT)
            .addHints(listOf(Slice.HINT_LIST_ITEM))
            .build()
    }

    companion object {
        const val AUTHORITY = "vendor.mainline.audio.settings.slices"
        private const val PATH = "/hardware"

        /** What the TvSettings overlay names (`default_audio_output_settings_slice_uri`). */
        val SLICE_URI: Uri = Uri.parse("content://$AUTHORITY$PATH")

        private const val AUTO_GRANT = "android.permission.MANAGE_SLICE_PERMISSIONS"
        private val LIST_SPEC = SliceSpec("androidx.slice.LIST", 1)

        // TwoPanelSettingsLib's SlicesConstants.
        private const val TYPE_PREFERENCE = "TYPE_PREFERENCE"
        private const val TYPE_PREFERENCE_SCREEN_TITLE = "TYPE_PREFERENCE_SCREEN_TITLE"
        private const val SUBTYPE_INTENT = "SUBTYPE_INTENT"
        private const val TAG_KEY = "TAG_KEY"

        private const val KEY_ROUTING = "routing"
        private const val ROUTING_PACKAGE = "vendor.mainline.audio.routing"
        private const val ROUTING_ACTIVITY = "$ROUTING_PACKAGE.RoutingActivity"
    }
}
