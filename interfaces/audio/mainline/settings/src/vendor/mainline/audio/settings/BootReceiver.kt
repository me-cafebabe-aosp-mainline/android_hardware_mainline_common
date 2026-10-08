/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.UserManager

/** Restores the default output and input, which AudioService does not keep across reboots. */
class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != Intent.ACTION_LOCKED_BOOT_COMPLETED) return
        // The preferences are global to the device: the system user's choice is the one that
        // counts, the same way Settings has only one Sound page for routing.
        if (!context.getSystemService(UserManager::class.java).isSystemUser) return
        val pending = goAsync()
        Thread {
                try {
                    AudioRouting(context).reapply()
                } finally {
                    pending.finish()
                }
            }
            .start()
    }
}
