# Member 1 authentication and integration note

`digestPin()` stores `cas-pbkdf2-sha256-v1$150000$<32 lowercase hex salt>$<64 lowercase hex derived key>`. The salt is 16 random bytes from Windows CNG. PBKDF2 uses HMAC-SHA-256, 150,000 iterations, and a 32-byte derived key. `login()` verifies this exact version and uses one generic error message for an unknown account ID, wrong PIN, invalid PIN, or unreadable digest. Unknown IDs and unreadable digests still run the PIN derivation so they do not take an obvious faster path. PINs remain strings, so leading zeroes survive. This version must be agreed before any accounts are saved; changing it later requires a migration or a version-aware verifier.

The current implementation uses Windows CNG through `bcrypt` and compiles with `-lbcrypt` on the project's MSYS2 UCRT64 G++ toolchain. PIN input is currently visible in the terminal.

`TeamInterfaces.h` declares the teammate functions from the shared contract. `MenuController.cpp` calls those functions but does not implement their business rules, validation, storage, or reports. Their `.cpp` files are not on this branch, so the complete executable cannot link yet. The current source passes a C++17 syntax check. Temporary local stand-ins verified Member 1's registration save failure/success, client deposit scope and save behavior, logout, admin status routing, and audit call; they were removed after the check. Real persistence and banking behavior remain untested.

`main.cpp` now handles first-run admin setup when `data/bank_state.txt` is absent. It prompts for the admin name and PIN, creates a candidate with ID `ADM-000001`, saves it, and only then displays that ID. There is no default PIN. When the snapshot exists, a load error stops startup instead of starting over. This path still needs Member 3's real saver and loader for integration testing.

Compile Member 1's source without linking missing teammate definitions:

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -fsyntax-only main.cpp MenuController.cpp Authentication.cpp
```

After the teammate `.cpp` files are available and their signatures match this branch, the complete build also needs `Authentication.cpp` and `-lbcrypt` in addition to the files listed in the project overview.

Before merging branches, agree on one shared C++ model header. The currently pushed ReportGenerator branch defines its own `Account`, `Transaction`, `BankData`, and `ReportResult` in its `.cpp` file. Those definitions need to match the integrated model exactly; this branch does not edit that teammate-owned source.
