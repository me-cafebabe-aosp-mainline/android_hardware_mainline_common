/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.routing

import android.content.ContentProvider
import android.content.ContentValues
import android.content.Context
import android.database.Cursor
import android.net.Uri
import android.os.Bundle

/** Supplies the default output and input as the summary of the Settings tile. */
class RoutingSummaryProvider : ContentProvider() {
    override fun onCreate() = true

    override fun call(method: String, arg: String?, extras: Bundle?): Bundle? {
        if (method != METHOD_GET_DYNAMIC_SUMMARY) return null
        val context = context ?: return null
        val labels = DeviceLabels(context)
        val routing = AudioRouting(context)
        val output = describe(context, routing.outputChoice(), labels)
        val summary =
            if (routing.inputs().isEmpty()) {
                output
            } else {
                context.getString(
                    R.string.summary_format,
                    output,
                    describe(context, routing.inputChoice(), labels),
                )
            }
        return Bundle().apply { putString(META_DATA_PREFERENCE_SUMMARY, summary) }
    }

    override fun query(
        uri: Uri,
        projection: Array<String>?,
        selection: String?,
        selectionArgs: Array<String>?,
        sortOrder: String?,
    ): Cursor? = null

    override fun getType(uri: Uri): String? = null

    override fun insert(uri: Uri, values: ContentValues?): Uri? = null

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<String>?) = 0

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        selectionArgs: Array<String>?,
    ) = 0

    private fun describe(context: Context, choice: AudioRouting.Choice, labels: DeviceLabels) =
        when (choice) {
            is AudioRouting.Choice.Device -> labels.label(choice.id)
            else -> context.getString(R.string.automatic_title)
        }

    companion object {
        // Keys of the Settings tile protocol, see SettingsLib TileUtils.
        private const val METHOD_GET_DYNAMIC_SUMMARY = "getDynamicSummary"
        private const val META_DATA_PREFERENCE_SUMMARY = "com.android.settings.summary"

        /** Must match the `com.android.settings.summary_uri` meta-data in the manifest. */
        private val SUMMARY_URI =
            Uri.parse("content://vendor.mainline.audio.routing.summary/$METHOD_GET_DYNAMIC_SUMMARY")

        /** Asks a visible Settings › Sound page to fetch the summary again. */
        fun notifyChanged(context: Context) {
            context.contentResolver.notifyChange(SUMMARY_URI, /* observer= */ null)
        }
    }
}
