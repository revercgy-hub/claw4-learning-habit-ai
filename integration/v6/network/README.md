# V6 saved-network fallback

Source: `78/esp-wifi-connect` 3.3.1, commit
`cc103899ea8199b0fbf18c9b6e65152d9a92091e`, MIT. The original LICENSE is
retained in the generated component. No upstream repository is modified.

`overlay.json` records every original package file hash and exact source
replacements. `network_overlay.py` generates a project-local
`components/78__esp-wifi-connect` from the matching managed package. It rejects
unknown source or local changes. Registry checksums are omitted from the local
copy because they describe the unmodified package. Other files are preserved.
`saved_network_policy.h` is the project-owned bounded candidate selector.

Behavior: keep saved-channel then full-band discovery and visible-AP priority.
After a full-band scan yields no matching saved network, try at most three
distinct, valid saved SSIDs in stored order. Use channel 0, all-channel driver
discovery, no invented BSSID, and zero driver extra retries for this fallback.
On disconnect, skip the normal five repeats of the same fallback entry and
advance through the queue, then reuse scan backoff. A later successful IP event
restores the upstream reconnect path. Fixed-BSSID opt-in disables fallback:
stored credentials do not contain an authoritative BSSID, so silently removing
the restriction would change the user's connection preference.

Empty/oversize/NUL-containing credentials are skipped rather than truncated.
The package's NVS format is unchanged. No credentials are introduced or uploaded.
New diagnostic lines contain only counts/error codes; upstream private logs may
still include SSIDs and must stay outside Git. The existing board connection
deadline remains 60 seconds; large/multiple unavailable saved networks may not
all finish within it. This is a bounded fallback, not a replacement WiFi state
machine or a promise that every hidden AP configuration works.

Build: `build_device.py` prepares the override, reconfigures and builds.
For a fresh tree, its first upstream build resolves the dependencies, followed
by the override build. Only the latter can pass `freeze_candidate.py`: freezing
requires the generated inventory and CMake's actual selected component path.
To reproduce, use the frozen source/dependency pins; never hand-edit managed
components or use broad patch matching. Existing local edits are not overwritten.

Host evidence: policy boundary tests, seven actual generated method-body driver
shim scenarios, six overlay integrity tests. These do not simulate the ESP event
loop, RF/association timing, disconnect races, C5 or the physical access point.
Device verification remains required for visible/hidden boot, wrong password,
AP absence, reconnect and provisioning deadline.
