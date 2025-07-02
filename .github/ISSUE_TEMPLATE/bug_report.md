---
name: Bug report
about: Create a report to help us improve
labels: bug

---

**Read the README.md and search for similar issues before posting a bug report!**

<!-- Any bug that can be solved by just reading the [prerequisites](https://github.com/aristocratos/btop#prerequisites) section of the README will likely be ignored. -->

**Describe the bug**

<!-- A clear and concise description of what the bug is. -->

**To Reproduce**

<!-- Steps to reproduce the behavior. ->

**Expected behavior**

<!-- A clear and concise description of what you expected to happen. -->

**Screenshots**

<!-- If applicable, add screenshots to help explain your problem. -->

**Info (please complete the following information):**
 - btop++ version: `btop --version`
   - If using snap: `snap info btop`
 - Binary: [self compiled or static binary from release]
 - Architecture: [x86_64, aarch64, etc.] `uname -m`
 - Platform: [Linux, FreeBSD, OsX]
 - (Linux) Kernel: `uname -r`
 - (OSX/FreeBSD) Os release version:
 - Terminal used:
 - Font used:

**Additional context**

<!-- Contents of `~/.local/state/btop.log` -->

<!-- Note: The snap uses: `~/snap/btop/current/.local/state/btop.log` -->

<!-- (try running btop with `--debug` flag if btop.log is empty) -->

**GDB Backtrace**

<!-- If btop++ is crashing at start the following steps could be helpful: -->

<!-- (Extra helpful if compiled with `make OPTFLAGS="-O0 -g"`) -->

<!-- 1. run (linux): `gdb btop` (macos): `lldb btop` -->

<!-- 2. `r` to run, wait for crash and press enter if prompted, CTRL+L to clear screen if needed. -->

<!-- 3. (gdb): `thread apply all bt` (lldb): `bt all` to get backtrace for all threads -->

<!-- 4. Copy and paste the backtrace here: -->
