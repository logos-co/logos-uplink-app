#pragma once

namespace referral {

// How long a send may take: lez_core proves a private transaction in minutes without
// RISC0_DEV_MODE. A call's timeout of 0 or less is the protocol's 20 s default, not
// "wait forever", so this must stay a real bound.
constexpr int kSendTimeoutMs = 30 * 60 * 1000;

} // namespace referral
