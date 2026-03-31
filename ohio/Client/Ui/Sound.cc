#include <emscripten.h>
#include <iostream>
#include <Client/Game.hh>

int last_beep_time = 0;

void Beep(int frequency, int duration) {
    if (Game::timestamp - last_beep_time < 100) return;
    last_beep_time = Game::timestamp;
    EM_ASM({
        const audioContext = new AudioContext();
        const gainNode = audioContext.createGain();
        gainNode.gain.value = 0.5;
        const oscillator = audioContext.createOscillator();
        oscillator.type = 'square';
        oscillator.frequency.setValueAtTime($0, audioContext.currentTime);
        oscillator.connect(gainNode);
        gainNode.connect(audioContext.destination);
        oscillator.start();
        oscillator.stop(audioContext.currentTime + ($1 / 1000));
        return 0;
    }, frequency, duration);
}