#include <Client/Socket.hh>
#include <Client/Game.hh>

#include <Shared/Binary.hh>
#include <Shared/Config.hh>

#include <cstring>
#include <iostream>

#include <emscripten.h>

uint8_t INCOMING_PACKET[64 * 1024] = {0};
uint8_t OUTGOING_PACKET[1 * 1024] = {0};

extern "C" {
    void on_message(uint8_t type, uint32_t len, char *reason) {
        if (type == 0) {
            std::printf("Connected\n");
            Writer w(OUTGOING_PACKET);
            w.write<uint8_t>(Serverbound::kVerify);
            w.write<uint64_t>(VERSION_HASH);
            Game::reset();
            Game::socket.ready = 1; // Set ready and keep it ready
            Game::socket.send(w.packet, w.at - w.packet);
            // Don't set ready back to 0 - connection is established
        } 
        else if (type == 2) {
            Game::on_game_screen = 0;
            Game::socket.ready = 0;
            std::printf("Disconnected [%d](%s)\n", len, reason);
            if (std::strlen(reason))
                Game::disconnect_message = std::format("Disconnected with code {} (\"{}\")", len, reason);
            else
                Game::disconnect_message = std::format("Disconnected with code {}", len);
            free(reason);
        }
        else if (type == 1) {
            Game::socket.ready = 1;
            Game::on_message(INCOMING_PACKET, len);
        }
    }
}

Socket::Socket() {}

void Socket::connect(std::string const url) {
#ifdef SINGLEPLAYER
    std::cout << "Connecting to web worker server (singleplayer mode)\n";
    EM_ASM({
        var worker = Module.worker = new Worker('server-worker.js');
        Module.workerReady = false;
        
        worker.onmessage = function(e) {
            var msgType = e.data.type;
            var msgLen = e.data.len;
            var msgData = e.data.data;
            if (msgType === 0) {
                // Connected - notify (ready will be set in on_message)
                Module.workerReady = true;
                _on_message(0, 0, 0);
            } else if (msgType === 1) {
                // Message received
                if (msgData && msgData.length > 0) {
                    var buffer = new Uint8Array(msgData);
                    HEAPU8.set(buffer, $0);
                    _on_message(1, msgLen, 0);
                }
            } else if (msgType === 2) {
                // Disconnected
                Module.workerReady = false;
                var reason = e.data.reason || "";
                _on_message(2, msgLen, reason ? stringToNewUTF8(reason) : 0);
            }
        };
        
        worker.onerror = function(error) {
            console.error('Worker error:', error);
            console.error('Error message:', error.message);
            console.error('Error filename:', error.filename);
            console.error('Error lineno:', error.lineno);
            Module.workerReady = false;
            var errorMsg = "Worker error: " + (error.message || "Unknown error");
            _on_message(2, 1006, stringToNewUTF8(errorMsg));
        };
        
        worker.postMessage({ type: 'connect' });
    }, INCOMING_PACKET);
#else
    std::cout << "Connecting to " << url << '\n';
    EM_ASM({
        let string = UTF8ToString($1);
        function connect() {
            let socket = Module.socket = new WebSocket(string);
            socket.binaryType = "arraybuffer";
            socket.onopen = function() {
                _on_message(0, 0, 0);
            };
            socket.onclose = function(a) {
                _on_message(2, a.code, stringToNewUTF8(a.reason));
                setTimeout(connect, 1000);
            };
            socket.onmessage = function(event) {
                HEAPU8.set(new Uint8Array(event.data), $0);
                _on_message(1, event.data.byteLength, 0);
            };
        }
        setTimeout(connect, 1000);
    }, INCOMING_PACKET, url.c_str());
#endif
}

void Socket::send(uint8_t *ptr, uint32_t len) {
    if (ready == 0) return;
#ifdef SINGLEPLAYER
    EM_ASM({
        if (Module.worker && Module.workerReady) {
            var data = HEAPU8.subarray($0, $0 + $1);
            // console.log('Client: Sending message to worker, length:', data.length, 'first byte:', data[0]);
            Module.worker.postMessage({
                type: 'message',
                data: Array.from(data)
            });
        } else {
            console.warn('Client: Cannot send - worker not ready. worker:', !!Module.worker, 'ready:', Module.workerReady);
        }
    }, ptr, len);
#else
    EM_ASM({
        if (Module.socket?.readyState == 1) {
            Module.socket.send(HEAPU8.subarray($0, $0+$1));
        }
    }, ptr, len);
#endif
}