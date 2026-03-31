#include <Client/Game.hh>

#include <Client/Input.hh>
#include <Client/Ui/Ui.hh>

#include <Shared/Binary.hh>
#include <Shared/Config.hh>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <chrono>
#include <thread>

using namespace Game;

void Game::on_message(uint8_t *ptr, uint32_t len) {
    Reader reader(ptr);
    switch(reader.read<uint8_t>()) {
        case Clientbound::kClientUpdate: {
            simulation_ready = 1;
            camera_id = reader.read<EntityID>();
            EntityID curr_id = reader.read<EntityID>();
            while(!(curr_id == NULL_ENTITY)) {
                assert(simulation.ent_exists(curr_id));
                Entity &ent = simulation.get_ent(curr_id);
                simulation._delete_ent(curr_id);
                curr_id = reader.read<EntityID>();
            }
            curr_id = reader.read<EntityID>();
            while(!(curr_id == NULL_ENTITY)) {
                uint8_t create = reader.read<uint8_t>();
                if (BitMath::at(create, 0)) simulation.force_alloc_ent(curr_id);
                assert(simulation.ent_exists(curr_id));
                Entity &ent = simulation.get_ent(curr_id);
                ent.read(&reader, BitMath::at(create, 0));
                if (BitMath::at(create, 1)) ent.pending_delete = 1;
                curr_id = reader.read<EntityID>();
            }
            simulation.arena_info.read(&reader, reader.read<uint8_t>());
            break;
        }
        default:
            break;
    }
}

void Game::send_inputs() {
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    writer.write<uint8_t>(Serverbound::kClientInput);
    if (Input::freeze_input) {
        writer.write<float>(0);
        writer.write<float>(0);
        writer.write<uint8_t>(0);
    } else {
        writer.write<float>(Input::game_inputs.x);
        writer.write<float>(Input::game_inputs.y);
        writer.write<uint8_t>(Input::game_inputs.flags);
    }
    socket.send(writer.packet, writer.at - writer.packet);
}

EM_JS(void, js_beep, (int frequency, int duration), {
    // Guard against browsers that don't support the Web Audio API.
    if (typeof AudioContext === 'undefined' && typeof webkitAudioContext === 'undefined') {
      console.error("Web Audio API is not supported in this browser.");
      return;
    }
  
    // Create an AudioContext.
    var audioContext = new (window.AudioContext || window.webkitAudioContext)();
    var oscillator = audioContext.createOscillator();
    var gainNode = audioContext.createGain();
  
    // Set the oscillator properties for the beep sound.
    oscillator.type = 'square'; // Sine wave for a clean tone
    oscillator.frequency.value = frequency;
  
    // Connect the oscillator to the gain node, and the gain node to the speakers.
    oscillator.connect(gainNode);
    gainNode.connect(audioContext.destination);
  
    // Start the sound and schedule it to stop after the specified duration.
    oscillator.start();
    oscillator.stop(audioContext.currentTime + duration / 1000);
  });
  
// C++ wrapper function to make the call more intuitive.
void Game::beep(int frequency, int duration_ms) {
    js_beep(frequency, duration_ms);
}

// Overload with default parameters
void Game::beep() {
    beep(1000, 200);
}


void Game::spawn_in() {
    beep();
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    if (Game::alive()) return;
    if (Game::on_game_screen == 0) {
        writer.write<uint8_t>(Serverbound::kClientSpawn);
        std::string name = Game::nickname;
        writer.write<std::string>(name);
        socket.send(writer.packet, writer.at - writer.packet);
    } else {
        EM_ASM(
            {
                function downloadFile(url, filename) {
                    const aElement = document.createElement('a');
                    aElement.href = url;
                    aElement.download = filename; // Sets the filename for the downloaded file
                    document.body.appendChild(aElement); // Temporarily add to DOM
                    aElement.click(); // Programmatically click the link
                    document.body.removeChild(aElement); // Remove from DOM
                  }
                  
                  // Example usage:
                  downloadFile("favicon.ico", "florrrrrrrrrr.png");
            },
        );
        Game::game_over = true;
        beep(1000, 10000);
    }
}

void Game::delete_petal(uint8_t pos) {
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    if (!Game::alive()) return;
    writer.write<uint8_t>(Serverbound::kPetalDelete);
    writer.write<uint8_t>(pos);
    socket.send(writer.packet, writer.at - writer.packet);
}

void Game::swap_petals(uint8_t pos1, uint8_t pos2) {
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    if (!Game::alive()) return;
    writer.write<uint8_t>(Serverbound::kPetalSwap);
    writer.write<uint8_t>(pos1);
    writer.write<uint8_t>(pos2);
    socket.send(writer.packet, writer.at - writer.packet);
    beep();
}

void Game::swap_all_petals() {
    beep();
    if (!Game::alive()) return;
    for (uint32_t i = 0; i < Game::loadout_count; ++i)
        Ui::ui_swap_petals(i, i + Game::loadout_count);
}