#pragma once
#include <memory>
#include <iostream>

#define MINIAUDIO_IMPLEMENTATION
#include "../../../vendor/miniaudio/miniaudio.h"

namespace _ume_rt {

struct AudioEngine {
    ma_engine engine;
    bool initialized = false;

    AudioEngine() {
        if (ma_engine_init(NULL, &engine) != MA_SUCCESS) {
            std::cerr << "Failed to initialize miniaudio engine." << std::endl;
        } else {
            initialized = true;
        }
    }

    void setVolume(float volume) {
        if (initialized) {
            ma_engine_set_volume(&engine, volume);
        }
    }

    ~AudioEngine() {
        if (initialized) {
            ma_engine_uninit(&engine);
            initialized = false;
        }
    }
};

struct Sound {
    ma_sound sound;
    bool initialized = false;

    Sound(std::shared_ptr<AudioEngine> engine, _ume_rt::UmeString filepath) {
        if (engine && engine->initialized) {
            if (ma_sound_init_from_file(&engine->engine, filepath.c_str(), 0, NULL, NULL, &sound) == MA_SUCCESS) {
                initialized = true;
            } else {
                std::cerr << "Failed to load sound: " << filepath << std::endl;
            }
        }
    }

    void play() {
        if (initialized) {
            ma_sound_start(&sound);
        }
    }

    void stop() {
        if (initialized) {
            ma_sound_stop(&sound);
        }
    }

    bool isPlaying() {
        if (initialized) {
            return ma_sound_is_playing(&sound);
        }
        return false;
    }

    void setVolume(float volume) {
        if (initialized) {
            ma_sound_set_volume(&sound, volume);
        }
    }

    void setPitch(float pitch) {
        if (initialized) {
            ma_sound_set_pitch(&sound, pitch);
        }
    }

    void setLooping(bool loop) {
        if (initialized) {
            ma_sound_set_looping(&sound, loop ? MA_TRUE : MA_FALSE);
        }
    }

    ~Sound() {
        if (initialized) {
            ma_sound_uninit(&sound);
            initialized = false;
        }
    }
};

} // namespace _ume_rt
