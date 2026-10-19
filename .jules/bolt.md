## 2023-10-19 - Optimization in verify_image terminal size and stdout
**Learning:** Calling `shutil.get_terminal_size()` and printing to stdout on every single iteration of a tight loop parsing small disk chunks is a major performance bottleneck for local file operations. In `macrecovery.py`, the image verification loop checks hashes for thousands of chunks.
**Action:** Move `get_terminal_size()` outside the loop and throttle terminal output (e.g., `if cnkcount % 100 == 1:`) to drastically reduce I/O overhead without removing the user-facing progress indication.
