set(CMAKE_SYSTEM_NAME Generic)

cmake_minimum_required(VERSION 3.16)

project(gardn-client)
include_directories(..)
set(SINGLEPLAYER ON)

set(SRCS
    Assets/Flower.cc
    Assets/Mob.cc
    Assets/Petal.cc
    Assets/Web.cc
    Render/RenderDrop.cc
    Render/RenderFlower.cc
    Render/RenderHealth.cc
    Render/RenderMob.cc
    Render/RenderName.cc
    Render/RenderPetal.cc
    Render/RenderWeb.cc
    Render/Renderer.cc
    Debug.cc
    DOM.cc
    Game.cc
    Input.cc
    Main.cc
    Network.cc
    Particle.cc
    Rendering.cc
    Setup.cc
    Simulation.cc
    Socket.cc
    StaticData.cc
    Storage.cc
    Ui/InGame/InputFreeze.cc
    Ui/InGame/Leaderboard.cc
    Ui/InGame/LevelBar.cc
    Ui/InGame/LoadoutPetal.cc
    Ui/InGame/LoadoutSlot.cc
    Ui/InGame/Map.cc
    Ui/InGame/Mobile.cc
    Ui/InGame/OverlevelTimer.cc
    Ui/InGame/Tooltip.cc
    Ui/TitleScreen/Changelog.cc
    Ui/TitleScreen/DeathScreen.cc
    Ui/TitleScreen/MainScreen.cc
    Ui/TitleScreen/MobGallery.cc
    Ui/TitleScreen/PetalGallery.cc
    Ui/TitleScreen/Settings.cc
    Ui/TitleScreen/StatScreen.cc
    Ui/Button.cc
    Ui/Choose.cc
    Ui/Container.cc
    Ui/DynamicText.cc
    Ui/Element.cc
    Ui/Extern.cc
    Ui/ScrollContainer.cc
    Ui/StaticIcon.cc
    Ui/StaticText.cc
    Ui/TextInput.cc
    Ui/Window.cc
    Ui/Sound.cc
    ../Helpers/Math.cc
    ../Helpers/UTF8.cc
    ../Helpers/Vector.cc
    ../Shared/Arena.cc
    ../Shared/Binary.cc
    ../Shared/Config.cc
    ../Shared/Entity.cc
    ../Shared/EntityDef.cc
    ../Shared/Map.cc
    ../Shared/Simulation.cc
    ../Shared/StaticData.cc
)

set(CMAKE_CXX_COMPILER "em++")
set(CMAKE_CXX_FLAGS "-DCLIENTSIDE=1 -std=c++20")

option(SINGLEPLAYER "Build for singleplayer mode with web worker server" OFF)

if (USE_CODEPOINT_LEN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CODEPOINT_LEN=1")
endif()
if(SINGLEPLAYER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSINGLEPLAYER=1")
endif()
if(DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG=1 -gdwarf-4 -sNO_DISABLE_EXCEPTION_CATCHING")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math --closure=1")
endif()

add_link_options(-sEXPORTED_RUNTIME_METHODS=stringToNewUTF8)
add_link_options(-sEXPORTED_FUNCTIONS=_main,_key_event,_mouse_event,_touch_event,_wheel_event,_clipboard_event,_loop,_on_message)

add_executable(gardn-client ${SRCS})
set(CMAKE_EXECUTABLE_SUFFIX ".js")

# Copy public files to build directory
file(COPY ${CMAKE_SOURCE_DIR}/public/index.html DESTINATION ${CMAKE_BINARY_DIR})
if(SINGLEPLAYER)
    set(SERVER_BUILD_DIR ${CMAKE_BINARY_DIR}/server-build)
    set(SERVER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/../Server)
    
    # Find em++ compiler
    find_program(EMXX_COMPILER em++ PATHS $ENV{EMSDK}/upstream/emscripten $ENV{PATH})
    if(NOT EMXX_COMPILER)
        set(EMXX_COMPILER "em++")
    endif()
    find_program(EMCC_COMPILER emcc PATHS $ENV{EMSDK}/upstream/emscripten $ENV{PATH})
    if(NOT EMCC_COMPILER)
        set(EMCC_COMPILER "emcc")
    endif()
    
    # Build server WASM as a custom target
    add_custom_target(build-server-wasm
        COMMAND ${CMAKE_COMMAND} -S ${SERVER_SOURCE_DIR} -B ${SERVER_BUILD_DIR}
            -DWASM_SERVER=ON
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_C_COMPILER=${EMCC_COMPILER}
            -DCMAKE_CXX_COMPILER=${EMXX_COMPILER}
            -DCMAKE_SYSTEM_NAME=Generic
            -DCMAKE_C_COMPILER_WORKS=1
            -DCMAKE_CXX_COMPILER_WORKS=1
        COMMAND ${CMAKE_COMMAND} --build ${SERVER_BUILD_DIR} --target gardn-server
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SERVER_BUILD_DIR}/gardn-server.js
            ${CMAKE_BINARY_DIR}/gardn-server.js
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SERVER_BUILD_DIR}/gardn-server.wasm
            ${CMAKE_BINARY_DIR}/gardn-server.wasm
        COMMENT "Building and copying server WASM for singleplayer mode"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    
    # Make client depend on server WASM build
    add_dependencies(gardn-client build-server-wasm)
    
    file(COPY ${CMAKE_SOURCE_DIR}/public/server-worker.js DESTINATION ${CMAKE_BINARY_DIR})
endif()
