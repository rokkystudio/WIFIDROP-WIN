#pragma once

#include <chrono>

namespace Protocol {

inline constexpr unsigned short kDiscoveryPort = 49230;
inline constexpr unsigned short kTcpPort = 49231;
inline constexpr int kProtocolVersion = 1;
inline constexpr std::chrono::milliseconds kControlHeartbeatInterval{2000};
inline constexpr std::chrono::milliseconds kControlDisconnectTimeout{6000};

}  // namespace Protocol
