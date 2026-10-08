/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.sensors.settings

import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.net.Uri
import android.os.Bundle
import kotlinx.coroutines.runBlocking

/** Supplies the number of virtual sensors as the summary of the Settings › System tile. */
class SensorsSummaryProvider : ContentProvider() {
    override fun onCreate() = true

    override fun call(method: String, arg: String?, extras: Bundle?): Bundle? {
        if (method != METHOD_GET_DYNAMIC_SUMMARY) return null
        val context = context ?: return null
        // A Binder thread of this process: blocking is fine, the HAL lookup has a timeout.
        val sensors = runBlocking { HalControl.sensors() }
        val summary =
            when {
                sensors == null -> context.getString(R.string.summary_unavailable)
                sensors.isEmpty() -> context.getString(R.string.summary_none)
                else -> {
                    val inUse = sensors.count { it.active }
                    context.resources.getQuantityString(
                        R.plurals.summary_sensors,
                        sensors.size,
                        sensors.size,
                        inUse,
                    )
                }
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

    private companion object {
        // Keys of the Settings tile protocol, see SettingsLib TileUtils. The method must match
        // the `com.android.settings.summary_uri` meta-data in the manifest.
        const val METHOD_GET_DYNAMIC_SUMMARY = "getDynamicSummary"
        const val META_DATA_PREFERENCE_SUMMARY = "com.android.settings.summary"
    }
}
