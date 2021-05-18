/* Copyright 2021 Aristocratos (jakob@qvantnet.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

indent = tab
tab-size = 4
*/

#ifndef _btop_globs_included_
#define _btop_globs_included_ 1

#include <string>
#include <vector>
#include <array>
#include <atomic>
#include <unordered_map>

using std::string, std::vector, std::unordered_map, std::array, std::atomic;

namespace Global {

	atomic<bool> stop_all(false);

	const unordered_map<string, string> Default_theme = {
		{ "main_bg", "#00" },
		{ "main_fg", "#cc" },
		{ "title", "#ee" },
		{ "hi_fg", "#969696" },
		{ "selected_bg", "#7e2626" },
		{ "selected_fg", "#ee" },
		{ "inactive_fg", "#40" },
		{ "graph_text", "#60" },
		{ "meter_bg", "#40" },
		{ "proc_misc", "#0de756" },
		{ "cpu_box", "#3d7b46" },
		{ "mem_box", "#8a882e" },
		{ "net_box", "#423ba5" },
		{ "proc_box", "#923535" },
		{ "div_line", "#30" },
		{ "temp_start", "#4897d4" },
		{ "temp_mid", "#5474e8" },
		{ "temp_end", "#ff40b6" },
		{ "cpu_start", "#50f095" },
		{ "cpu_mid", "#f2e266" },
		{ "cpu_end", "#fa1e1e" },
		{ "free_start", "#223014" },
		{ "free_mid", "#b5e685" },
		{ "free_end", "#dcff85" },
		{ "cached_start", "#0b1a29" },
		{ "cached_mid", "#74e6fc" },
		{ "cached_end", "#26c5ff" },
		{ "available_start", "#292107" },
		{ "available_mid", "#ffd77a" },
		{ "available_end", "#ffb814" },
		{ "used_start", "#3b1f1c" },
		{ "used_mid", "#d9626d" },
		{ "used_end", "#ff4769" },
		{ "download_start", "#231a63" },
		{ "download_mid", "#4f43a3" },
		{ "download_end", "#b0a9de" },
		{ "upload_start", "#510554" },
		{ "upload_mid", "#7d4180" },
		{ "upload_end", "#dcafde" },
		{ "process_start", "#80d0a3" },
		{ "process_mid", "#dcd179" },
		{ "process_end", "#d45454" }
	};

	const unordered_map<string, unordered_map<string, vector<string>>> Menus = {
		{ "options", {
			{ "normal", {
				"┌─┐┌─┐┌┬┐┬┌─┐┌┐┌┌─┐",
				"│ │├─┘ │ ││ ││││└─┐",
				"└─┘┴   ┴ ┴└─┘┘└┘└─┘"
				} },
			{ "selected", {
				"╔═╗╔═╗╔╦╗╦╔═╗╔╗╔╔═╗",
				"║ ║╠═╝ ║ ║║ ║║║║╚═╗",
				"╚═╝╩   ╩ ╩╚═╝╝╚╝╚═╝"
				} }
		} },
		{ "help", {
			{ "normal", {
				"┬ ┬┌─┐┬  ┌─┐",
				"├─┤├┤ │  ├─┘",
				"┴ ┴└─┘┴─┘┴  "
				} },
			{ "selected", {
				"╦ ╦╔═╗╦  ╔═╗",
				"╠═╣║╣ ║  ╠═╝",
				"╩ ╩╚═╝╩═╝╩  "
				} }
		} },
		{ "quit", {
			{ "normal", {
				"┌─┐ ┬ ┬ ┬┌┬┐",
				"│─┼┐│ │ │ │ ",
				"└─┘└└─┘ ┴ ┴ "
				} },
			{ "selected", {
				"╔═╗ ╦ ╦ ╦╔╦╗ ",
				"║═╬╗║ ║ ║ ║  ",
				"╚═╝╚╚═╝ ╩ ╩  "
				} }
		} }
	};

	//? Units for floating_humanizer function
	const array<string, 11> Units_bit = {"bit", "Kib", "Mib", "Gib", "Tib", "Pib", "Eib", "Zib", "Yib", "Bib", "GEb"};
	const array<string, 11> Units_byte = {"Byte", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB", "BiB", "GEB"};

}

namespace Symbols {
	const string h_line			= "─";
	const string v_line			= "│";
	const string left_up		= "┌";
	const string right_up		= "┐";
	const string left_down		= "└";
	const string right_down		= "┘";
	const string title_left		= "┤";
	const string title_right	= "├";
	const string div_up			= "┬";
	const string div_down		= "┴";

	const array<string, 10> superscript = { "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹" };
}

#endif
