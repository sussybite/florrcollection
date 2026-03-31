#pragma once

#include <cstdint>
#include <string>

#ifdef CLIENTSIDE
extern std::string const WS_URL;
#endif
extern uint64_t const VERSION_HASH;
extern uint32_t const SERVER_PORT;
extern uint32_t const MAX_NAME_LENGTH;