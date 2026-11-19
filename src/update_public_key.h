#pragma once
// Public keys allowed to sign releases (see update_signature.h). Written by
// scripts/new-release-key.ps1; the matching private key stays on the maintainer's PC.
// More than one key lets a new key be added before an old one is retired.
// Empty means no key yet: official builds then refuse to compile, and updates cannot be installed.
// Zek's fork (ahizechi/Battery): the original project's key was removed on 19 Nov 2026 so this app
// never trusts a release signed by anyone else. Run scripts/new-release-key.ps1 to add Zek's key.
namespace updates {
inline constexpr const char* ReleaseKeys[] = {
    "",
};
}
