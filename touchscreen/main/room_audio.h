#pragma once
#include <stdbool.h>
void room_audio_start(void);
void room_audio_laundry_done(int key);
bool room_audio_notify_enabled(int key);
bool room_audio_set_notify(int key,bool enabled);
bool room_audio_test(void);
unsigned room_audio_get_volume(void);
bool room_audio_set_volume(unsigned percent);
