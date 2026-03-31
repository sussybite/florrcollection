#include <Shared/Config.hh>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

extern const uint64_t VERSION_HASH = 19235684321324ull;

extern const uint32_t SERVER_PORT = 4178;
extern const uint32_t MAX_NAME_LENGTH = 16;

#ifdef CLIENTSIDE
char* get_host_name() {
    char buffer[256];
    int length = EM_ASM_INT({
        let host = window.location.hostname;
        let encoder = new TextEncoder();
        let encoded = encoder.encode(host);
        let length = encoded.length;
        if (length >= 256) length = 255;
        HEAPU8.set(encoded.subarray(0, length), $0);
        HEAPU8[$0 + length] = 0;
        return length;
    }, buffer);
    return strdup(buffer);
}

//your ws host url may not follow this format, change it to fit your needs
extern std::string const WS_URL = std::string("ws://") + get_host_name() + ":" + std::to_string(SERVER_PORT);
#endif