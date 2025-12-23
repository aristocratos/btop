// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <atomic>
#include <csignal>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>

using std::array;
using std::atomic;
using std::deque;
using std::string;

/* The input functions rely on the following termios parameters being set:
        Non-canonical mode (c_lflags & ~(ICANON))
        VMIN and VTIME (c_cc) set to 0
        These will automatically be set when running Term::init() from btop_tools.cpp
*/

//* Functions and variables for handling keyboard and mouse input
namespace Input {

    struct Mouse_loc {
        int line, col, height, width;
    };

    //? line, col, height, width
    extern std::unordered_map<string, Mouse_loc> mouse_mappings;

    //* Signal mask used during polling read
    extern sigset_t signal_mask;

    extern atomic<bool> polling;

    //* Mouse column and line position
    extern array<int, 2> mouse_pos;

    //* Last entered key
    extern deque<string> history;

    //* Poll keyboard & mouse input for <timeout> ms and return input availability as a bool
    bool poll(const uint64_t timeout = 0);

    //* Get a key or mouse action from input
    string get();

    //* Wait until input is available and return key
    string wait();

    //* Interrupt poll/wait
    void interrupt();

    //* Clears last entered key
    void clear();

    //* Process actions for input <key>
    void process(const std::string_view key);

} // namespace Input
