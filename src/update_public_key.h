#pragma once
// Public keys allowed to sign releases (see update_signature.h). Written by
// scripts/new-release-key.ps1; the matching private key stays on the maintainer's PC.
// More than one key lets a new key be added before an old one is retired.
// Empty means no key yet: official builds then refuse to compile, and updates cannot be installed.
// Zek's fork (ahizechi/Battery): only Zek's release key (created 19 Nov 2026) is trusted; the original
// project's key was removed, so this app never installs a release signed by anyone else.
namespace updates {
inline constexpr const char* ReleaseKeys[] = {
    "RUNTMSAAAADb4pXPCdLVNPJya8oCYShPAYDGQuaQE6fFpI1n+DTO0j8f9wrhNYOTd/KFefTtc/0naYsoRlpV86DUV/W6+H+T",
};
}
