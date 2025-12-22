// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "btop_input.hpp"

#include <atomic>
#include <bitset>
#include <string>
#include <string_view>
#include <vector>

using std::atomic;
using std::bitset;
using std::string;
using std::vector;

namespace Menu {

    extern atomic<bool> active;
    extern string output;
    extern int signalToSend;
    extern bool redraw;

    //? line, col, height, width
    extern std::unordered_map<string, Input::Mouse_loc> mouse_mappings;

    //* Creates a message box centered on screen
    //? Height of box is determined by size of content vector
    //? Boxtypes: 0 = OK button | 1 = YES and NO with YES selected | 2 = Same as 1 but with NO selected
    //? Strings in content vector is not checked for box width overflow
    class msgBox {
        string box_contents, button_left, button_right;
        int height {};
        int width {};
        int boxtype {};
        int selected {};
        int x {};
        int y {};

    public:
        enum BoxTypes {
            OK,
            YES_NO,
            NO_YES
        };
        enum msgReturn {
            Invalid,
            Ok_Yes,
            No_Esc,
            Select
        };
        msgBox();
        msgBox(int width, int boxtype, const vector<string>& content, std::string_view title);

        //? Draw and return box as a string
        string operator()();

        //? Process input and returns value from enum Ret
        int input(const string& key);

        //? Clears content vector and private strings
        void clear();
        int getX() const {
            return x;
        }
        int getY() const {
            return y;
        }
    };

    extern bitset<8> menuMask;

    //* Enum for functions in vector menuFuncs
    enum Menus {
        SizeError,
        SignalChoose,
        SignalSend,
        SignalReturn,
        Options,
        Help,
        Renice,
        Main
    };

    //* Handles redirection of input for menu functions and handles return codes
    void process(const std::string_view key = "");

    //* Show a menu from enum Menu::Menus
    void show(int menu, int signal = -1);

} // namespace Menu
