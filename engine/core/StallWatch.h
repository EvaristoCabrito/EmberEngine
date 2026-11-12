#pragma once

namespace ember {

// The main thread stuck (a frame, a screen change or the way out that does not end; Windows then calls the game "not
// responding" and closes it with no word in the log, the user's three hangs of 2026-11-12): after `seconds` without a beat,
// where it is goes to the log — the step it last named and its call stack — and again while it stays there. It only reads:
// nothing the game does changes.
void startStallWatch(double seconds = 3.0);  // on the main thread, once
void stallBeat();                            // the main thread, once a turn of its loop
void stallStep(const char* step);            // a string literal: what the main thread is about to do

}  // namespace ember
