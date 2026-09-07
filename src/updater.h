#pragma once

namespace Updater {
    // Checks GitHub Releases in the background. Downloads and relaunches if a newer build exists.
    void CheckForUpdatesAsync();
}
