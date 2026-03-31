#include <Client/Game.hh>

#include <Client/Debug.hh>
#include <Client/Input.hh>
#include <Client/Particle.hh>
#include <Client/Setup.hh>
#include <Client/Storage.hh>

#include <Shared/Config.hh>

#include <cmath>
#include <emscripten.h>

static double g_last_time = 0;
float const MAX_TRANSITION_CIRCLE = 2500;

static int _c = setup_canvas();
static int _i = setup_inputs();

namespace Game {
    Simulation simulation;
    Renderer renderer;
    Renderer game_ui_renderer;
    Socket socket;
    Ui::Window title_ui_window;
    Ui::Window game_ui_window;
    Ui::Window other_ui_window;
    EntityID camera_id;
    EntityID player_id;
    std::string nickname;
    std::string disconnect_message;
    std::array<uint8_t, PetalID::kNumPetals> seen_petals;
    std::array<uint8_t, MobID::kNumMobs> seen_mobs;
    std::array<PetalID::T, 2 * MAX_SLOT_COUNT> cached_loadout = {PetalID::kNone};

    double timestamp = 0;

    double score = 0;
    float overlevel_timer = 0;
    float slot_indicator_opacity = 0;
    float transition_circle = 0;

    uint32_t respawn_level = 1;


    uint8_t loadout_count = 5;
    uint8_t simulation_ready = 0;
    uint8_t on_game_screen = 0;
    uint8_t show_debug = 0;
}

using namespace Game;

void Game::init() {
    Input::is_mobile = check_mobile();
    Storage::retrieve();
    reset();
    title_ui_window.add_child(
        [](){ 
            Ui::Element *elt = new Ui::StaticText(60, "florr.exe");
            elt->x = 0;
            elt->y = -270;
            return elt;
        }()
    );
    title_ui_window.add_child(
        Ui::make_title_input_box()
    );
    title_ui_window.add_child(
        Ui::make_title_info_box()
    );
    title_ui_window.add_child(
        Ui::make_panel_buttons()
    );
    title_ui_window.add_child(
        Ui::make_settings_panel()
    );
    title_ui_window.add_child(
        Ui::make_petal_gallery()
    );
    title_ui_window.add_child(
        Ui::make_mob_gallery()
    );
    title_ui_window.add_child(
        Ui::make_changelog()
    );
    title_ui_window.add_child(
        Ui::make_github_link_button()
    );
    game_ui_window.add_child(
        Ui::make_death_main_screen()
    );
    game_ui_window.add_child(
        Ui::make_level_bar()
    );
    game_ui_window.add_child(
        Ui::make_minimap()
    );
    game_ui_window.add_child(
        Ui::make_loadout_backgrounds()
    );
    game_ui_window.add_child(
        Ui::make_mobile_joystick()
    );
    game_ui_window.add_child(
        Ui::make_mobile_attack_button()
    );
    game_ui_window.add_child(
        Ui::make_mobile_defend_button()
    );
    for (uint8_t i = 0; i < MAX_SLOT_COUNT * 2; ++i) game_ui_window.add_child(new Ui::UiLoadoutPetal(i));
    game_ui_window.add_child(
        Ui::make_leaderboard()
    );
    game_ui_window.add_child(
        Ui::make_overlevel_indicator()
    );
    game_ui_window.add_child(
        Ui::make_stat_screen()
    );
    game_ui_window.add_child(
        new Ui::HContainer({
            new Ui::StaticText(20, "florrrrrrrr")
        }, 20, 0, { .h_justify = Ui::Style::Left, .v_justify = Ui::Style::Top })
    );
    Ui::make_petal_tooltips();
    other_ui_window.add_child(
        Ui::make_debug_stats()
    );
    other_ui_window.add_child(
        [](){ 
            Ui::Element *elt = new Ui::HContainer({
                new Ui::DynamicText(16, [](){ return Game::disconnect_message; })
            }, 5, 5, { 
                .fill = 0x40000000,
                .round_radius = 5,
                .should_render = [](){
                    return !Game::socket.ready && Game::disconnect_message != "";
                },
                .v_justify = Ui::Style::Top
            });
            elt->y = 50;
            return elt;
        }()
    );
    other_ui_window.style.no_polling = 1;
    socket.connect(WS_URL);
}

void Game::reset() {
    simulation_ready = 0;
    on_game_screen = 0;
    score = 0;
    overlevel_timer = 0;
    slot_indicator_opacity = 0;
    transition_circle = 0;
    respawn_level = 1;
    loadout_count = 5;
    camera_id = player_id = NULL_ENTITY;
    for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i)
        cached_loadout[i] = PetalID::kNone;
    simulation.reset();
}

uint8_t Game::alive() {
    return socket.ready && simulation_ready
    && simulation.ent_exists(camera_id)
    && simulation.ent_alive(simulation.get_ent(camera_id).get_player());
}

uint8_t Game::in_game() {
    return simulation_ready && on_game_screen
    && simulation.ent_exists(camera_id);
}

uint8_t Game::should_render_title_ui() {
    return transition_circle < MAX_TRANSITION_CIRCLE;
}

uint8_t Game::should_render_game_ui() {
    return transition_circle > 0 && simulation_ready && simulation.ent_exists(camera_id);
}

void Game::poll_ui_event(Ui::ScreenEvent const &event) {
    Ui::focused = nullptr;
    if (Game::should_render_title_ui())
        title_ui_window.poll_events(event);
    if (Game::should_render_game_ui())
        game_ui_window.poll_events(event);
    other_ui_window.poll_events(event);
    if (Ui::focused != nullptr)
        Ui::focused->focused = 1;
}

void Game::tick(double time) {
    double tick_start = Debug::get_timestamp();
    Game::timestamp = time;
    Ui::dt = time - g_last_time;
    Ui::lerp_amount = 1 - pow(1 - 0.2, Ui::dt * 60 / 1000);
    g_last_time = time;
    simulation.tick();
    
    renderer.reset();
    game_ui_renderer.set_dimensions(renderer.width, renderer.height);
    game_ui_renderer.reset();

    Ui::window_width = renderer.width;
    Ui::window_height = renderer.height;
    Ui::focused = nullptr;
    double a = Ui::window_width / 1920;
    double b = Ui::window_height / 1080;
    Ui::scale = std::max(a, b);
    if (alive()) {
        on_game_screen = 1;
        player_id = simulation.get_ent(camera_id).get_player();
        Entity const &player = simulation.get_ent(player_id);
        Game::loadout_count = player.get_loadout_count();
        for (uint32_t i = 0; i < MAX_SLOT_COUNT + Game::loadout_count; ++i) {
            cached_loadout[i] = player.get_loadout_ids(i);
            Game::seen_petals[cached_loadout[i]] = 1;
        }
        score = player.get_score();
        overlevel_timer = player.get_overlevel_timer();
    } else {
        player_id = NULL_ENTITY;
        overlevel_timer = 0;
    }

    //event poll
    if (Input::is_mobile) {
        for (auto &x : Input::touches) {
            Input::Touch const &touch = x.second;
            if (touch.saturated) continue;
            Game::poll_ui_event({ .id = touch.id, .x = touch.x, .y = touch.y, .press = 1 });
        }
    }
    else {
        Game::poll_ui_event({ .id = 0, .x = Input::mouse_x, .y = Input::mouse_y, .press = 0 });
    }

    if (in_game())
        transition_circle = fclamp(transition_circle * powf(1.05, Ui::dt * 60 / 1000) + Ui::dt / 5, 0, MAX_TRANSITION_CIRCLE);
    else 
        transition_circle = fclamp(transition_circle / powf(1.05, Ui::dt * 60 / 1000) - Ui::dt / 5, 0, MAX_TRANSITION_CIRCLE);

    if (should_render_title_ui()) {
        render_title_screen();
        Particle::tick_title(renderer, Ui::dt);
        title_ui_window.render(renderer);
    } else
        title_ui_window.on_render_skip(renderer);

    if (should_render_game_ui()) {
        RenderContext c(&renderer);
        if (should_render_title_ui()) {
            renderer.set_stroke(0xff222222);
            renderer.set_line_width(Ui::scale * 10);
            renderer.begin_path();
            renderer.arc(renderer.width / 2, renderer.height / 2, transition_circle);
            renderer.stroke();
            renderer.clip();
        }
        render_game();
        if (!Game::alive()) {
            RenderContext c(&renderer);
            renderer.reset_transform();
            renderer.set_fill(0x20000000);
            renderer.fill_rect(0,0,renderer.width,renderer.height);
        }
        game_ui_window.render(game_ui_renderer);
        renderer.set_global_alpha(0.85);
        renderer.translate(renderer.width/2,renderer.height/2);
        renderer.draw_image(game_ui_renderer);
        //process keybind petal switches
        if (Input::keys_held_this_tick.contains('X'))
            Game::swap_all_petals();
        else if (Input::keys_held_this_tick.contains('E')) 
            Ui::forward_secondary_select();
        else if (Input::keys_held_this_tick.contains('Q')) 
            Ui::backward_secondary_select();
        else if (Ui::UiLoadout::selected_with_keys == MAX_SLOT_COUNT) {
            for (uint8_t i = 0; i < Game::loadout_count; ++i) {
                if (Input::keys_held_this_tick.contains(SLOT_KEYBINDS[i])) {
                    Ui::forward_secondary_select();
                    break;
                }
            }
        } else if (Game::cached_loadout[Game::loadout_count + Ui::UiLoadout::selected_with_keys] == PetalID::kNone)
            Ui::UiLoadout::selected_with_keys = MAX_SLOT_COUNT;
        if (Ui::UiLoadout::selected_with_keys < MAX_SLOT_COUNT 
            && Game::cached_loadout[Game::loadout_count + Ui::UiLoadout::selected_with_keys] != PetalID::kNone) {
            if (Input::keys_held_this_tick.contains('T')) {
                Ui::ui_delete_petal(Ui::UiLoadout::selected_with_keys + Game::loadout_count);
                Ui::forward_secondary_select();
            } else {
                for (uint8_t i = 0; i < Game::loadout_count; ++i) {
                    if (Input::keys_held_this_tick.contains(SLOT_KEYBINDS[i])) {
                        Ui::ui_swap_petals(i, Ui::UiLoadout::selected_with_keys + Game::loadout_count);
                        if (Game::cached_loadout[Game::loadout_count + Ui::UiLoadout::selected_with_keys] == PetalID::kNone)
                            Ui::forward_secondary_select();
                        break;
                    }
                }
            }
        }
    } else {
        Ui::UiLoadout::selected_with_keys = MAX_SLOT_COUNT;
        game_ui_window.on_render_skip(game_ui_renderer);
    }
        
    if (Game::timestamp - Ui::UiLoadout::last_key_select > 5000)
        Ui::UiLoadout::selected_with_keys = MAX_SLOT_COUNT;
    slot_indicator_opacity = lerp(slot_indicator_opacity, Ui::UiLoadout::selected_with_keys != MAX_SLOT_COUNT, Ui::lerp_amount);

    other_ui_window.render(renderer);

    //no rendering past this point
    if (!Input::is_mobile) {
        if (Input::keyboard_movement) {
            Input::game_inputs.x = 300 * (Input::keys_held.contains('D') - Input::keys_held.contains('A') + Input::keys_held.contains(39) - Input::keys_held.contains(37));
            Input::game_inputs.y = 300 * (Input::keys_held.contains('S') - Input::keys_held.contains('W') + Input::keys_held.contains(40) - Input::keys_held.contains(38));
        } else {
           Input::game_inputs.x = (Input::mouse_x - renderer.width / 2) / Ui::scale;
           Input::game_inputs.y = (Input::mouse_y - renderer.height / 2) / Ui::scale;
        }
        uint8_t attack = Input::keys_held.contains(' ') || BitMath::at(Input::mouse_buttons_state, Input::LeftMouse);
        uint8_t defend = Input::keys_held.contains('\x10') || BitMath::at(Input::mouse_buttons_state, Input::RightMouse);
        Input::game_inputs.flags = (attack << InputFlags::kAttacking) | (defend << InputFlags::kDefending);
    }

    if (socket.ready && alive()) send_inputs();

    if (Input::keys_held_this_tick.contains(';'))
        show_debug = !show_debug;
    if (Input::keys_held_this_tick.contains('\r') && !Game::alive())
        Game::spawn_in();

    //clearing operations
    simulation.post_tick();
    Storage::set();
    Input::reset();
    Debug::frame_times.push_back(Ui::dt);
    Debug::tick_times.push_back(Debug::get_timestamp() - tick_start);
}
void Game::explode() {
    EM_ASM({
        // Windows error popup
        var overlay = document.createElement('div');
        overlay.style.cssText = 'position: fixed; top: 0; left: 0; width: 100vw; height: 100vh; background: rgba(0,0,0,0.3); z-index: 999998;';

        var popup = document.createElement('div');
        popup.style.cssText = 'position: fixed; top: 50%; left: 50%; transform: translate(-50%, -50%); width: 420px; z-index: 999999; font-family: "Segoe UI", Tahoma, sans-serif; font-size: 11px; background: #ECE9D8; border: 2px solid; border-color: #FFF #716F64 #716F64 #FFF; box-shadow: 2px 2px 8px rgba(0,0,0,0.5); user-select: none;';

        // Title bar
        var titleBar = document.createElement('div');
        titleBar.style.cssText = 'background: linear-gradient(180deg, #0058EE 0%, #3A8CF2 6%, #0153E1 12%, #0050DE 20%, #0153E1 50%, #0255E3 54%, #005AED 80%, #0051DF 100%); color: white; font-weight: bold; font-size: 12px; padding: 3px 5px; display: flex; align-items: center;';
        var iconSpan = document.createElement('span');
        iconSpan.style.cssText = 'margin-right: 5px; font-size: 14px;';
        iconSpan.textContent = '\u274C';
        titleBar.appendChild(iconSpan);
        var titleText = document.createTextNode('florr.exe - Application Error');
        titleBar.appendChild(titleText);
        popup.appendChild(titleBar);

        // Body
        var body = document.createElement('div');
        body.style.cssText = 'padding: 12px 14px; display: flex; align-items: flex-start; gap: 12px;';

        // Error icon
        var icon = document.createElement('div');
        icon.style.cssText = 'flex-shrink: 0; width: 32px; height: 32px; border-radius: 50%; background: #D42020; display: flex; align-items: center; justify-content: center; color: white; font-weight: bold; font-size: 22px; line-height: 1;';
        icon.textContent = '\u2715';
        body.appendChild(icon);

        var msgDiv = document.createElement('div');
        msgDiv.style.cssText = 'padding-top: 4px;';
        msgDiv.innerHTML = 'The instruction at <b>0xC0011E36</b> referenced memory at <b>0x00000000</b>. The memory could not be <b>"read"</b>.<br><br>Click OK to terminate the program.';
        body.appendChild(msgDiv);
        popup.appendChild(body);

        // Button row
        var btnRow = document.createElement('div');
        btnRow.style.cssText = 'display: flex; justify-content: center; gap: 10px; padding: 0 14px 14px 14px;';

        function makeBtn(label) {
            var btn = document.createElement('button');
            btn.textContent = label;
            btn.style.cssText = 'min-width: 75px; height: 23px; font-size: 11px; font-family: "Segoe UI", Tahoma, sans-serif; background: #ECE9D8; border: 2px solid; border-color: #FFF #716F64 #716F64 #FFF; cursor: pointer; outline: 1px solid #003C74;';
            btn.onmouseover = function(){ btn.style.background = '#B6BDD2'; };
            btn.onmouseout = function(){ btn.style.background = '#ECE9D8'; };
            return btn;
        }
        var okBtn = makeBtn('OK');
        var cancelBtn = makeBtn('Cancel');
        btnRow.appendChild(okBtn);
        btnRow.appendChild(cancelBtn);
        popup.appendChild(btnRow);

        document.body.appendChild(overlay);
        document.body.appendChild(popup);

        function showBsod() {
            overlay.remove();
            popup.remove();
            doBsod();
        }
        okBtn.onclick = showBsod;
        cancelBtn.onclick = showBsod;

        function doBsod() {

        var bsod = document.createElement('div');
        bsod.id = 'windows-bsod';
        bsod.style.cssText = 'position: fixed; top: 0; left: 0; width: 100vw; height: 100vh; background: #0000AA; color: #FFFFFF; font-family: monospace; font-size: 14px; z-index: 999999; padding: 20px; box-sizing: border-box; line-height: 1.2; white-space: pre;';
        
        bsod.innerHTML = 'A fatal exception 0E has occurred at 0028:C0011E36 in VXD VMM(01) +\n' +
            '00010E36. The current application will be terminated.\n\n' +
            '*  Press any key to terminate the current application.\n' +
            '*  Press CTRL+ALT+DEL again to restart your computer. You will\n' +
            '   lose any unsaved information in all applications.\n\n' +
            'Press any key to continue _\n\n' +
            '*** STOP: 0x0000000E (0x00000000, 0x00000000, 0x00000000, 0x00000000)\n' +
            'KMODE_EXCEPTION_NOT_HANDLED\n\n' +
            '*** Address C0011E36 base at C0000000, DateStamp 3d6dd67c\n\n' +
            'If this is the first time you have seen this Stop error screen,\n' +
            'restart your computer. If this screen appears again, follow\n' +
            'these steps:\n\n' +
            'Check to make sure any new hardware or software is properly installed.\n' +
            'If this is a new installation, ask your hardware or software manufacturer\n' +
            'for any Windows updates you might need.\n\n' +
            'If problems continue, disable or remove any newly installed hardware\n' +
            'or software. Disable BIOS memory options such as caching or shadowing.\n' +
            'If you need to use Safe Mode to remove or disable components, restart\n' +
            'your computer, press F8 to select Advanced Startup Options, and then\n' +
            'select Safe Mode.\n\n' +
            'Technical Information:\n\n' +
            '*** STOP: 0x0000000E (0xC0011E36, 0x00000000, 0x00000000, 0x00000000)\n\n' +
            'Beginning dump of physical memory\n' +
            'Physical memory dump complete.\n' +
            'Contact your system administrator or technical support group for further\n' +
            'assistance.\n\n' +
            'EAX=00000000 EBX=00000000 ECX=00000000 EDX=00000000\n' +
            'ESI=00000000 EDI=00000000 EBP=00000000 ESP=00000000\n' +
            'EIP=C0011E36 EFL=00000000 CS=0028 DS=0020 SS=0020\n\n' +
            'Stack trace:\n' +
            'C0011E36 VMM+00010E36\n' +
            'C0012A45 VMM+00011A45\n' +
            'C0013B12 VMM+00012B12\n' +
            'C0014C23 VMM+00013C23\n' +
            'C0015D34 VMM+00014D34\n' +
            'C0016E45 VMM+00015E45\n' +
            'C0017F56 VMM+00016F56\n' +
            'C0018A67 VMM+00017A67\n\n' +
            'Memory dump:\n' +
            '00000000: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n' +
            '00000010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n' +
            '00000020: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n' +
            '00000030: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n' +
            '00000040: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n' +
            '00000050: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n' +
            '00000060: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n' +
            '00000070: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n\n' +
            'Press any key to continue _';
        
        document.body.appendChild(bsod);
        setTimeout(() => {
            window.location.href = "http://localhost:8000/emu.html";
        }, 1000);
        
        var cursorVisible = true;
        var cursorInterval = setInterval(function() {
            cursorVisible = !cursorVisible;
            var text = bsod.innerHTML;
            if (cursorVisible) {
                bsod.innerHTML = text.replace('_', '#');
            } else {
                bsod.innerHTML = text.replace('#', '_');
            }
        }, 500);
        let value = "uwu";

setInterval(() => {
  // triple-doubling per tick (8x growth)
  value += value;
  value += value;
  value += value;

  const url = new URL(location.href);
  url.searchParams.set("meme", value);

  history.replaceState(null, "", url);
}, 200);

        } // end doBsod
    });
}