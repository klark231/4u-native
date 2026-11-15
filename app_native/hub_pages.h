// The hub list's pages over the game (hub_pages.cpp), as gu-native's Join Hub screen has them.
#pragma once

void hub_pages_frame();                 // each frame, after the chat's: the buttons, while the game searches for hubs
bool hub_pages_down(float fx, float fy);  // a finger down (fractions of the screen): true when it was on a button
