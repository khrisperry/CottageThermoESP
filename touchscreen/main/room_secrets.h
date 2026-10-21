#pragma once
// Local-only credential include. Never place the real token in this tracked file.
#if !defined(ROOM_HA_TEST) && __has_include("../secrets.h")
#include "../secrets.h"
#endif
#ifndef COTTAGE_HA_URL
#define COTTAGE_HA_URL "https://cottageha.greenledges.net:8123"
#endif
#ifndef COTTAGE_HA_TOKEN
#define COTTAGE_HA_TOKEN ""
#endif
