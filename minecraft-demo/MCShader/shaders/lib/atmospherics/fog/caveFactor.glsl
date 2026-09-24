#ifndef INCLUDE_CAVE_FACTOR
    #define INCLUDE_CAVE_FACTOR

    float GetCaveFactor() {
        float caveFactor = clamp(1.0 - cameraPosition.y / oceanAltitude, 0.0, 1.0 - eyeBrightnessM);
        #ifdef SULFUR_CAVE_FOG
            caveFactor = mix(caveFactor, 1.0, inSulfurCaves);
        #endif
        return caveFactor;
    }
#endif
