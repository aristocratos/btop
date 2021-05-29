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
#include <robin_hood.h>

using std::string, std::vector, std::unordered_map, std::array, std::atomic, robin_hood::unordered_flat_map;

namespace Global {

	atomic<bool> stop_all(false);



	const unordered_flat_map<string, unordered_map<string, vector<string>>> Menus = {
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



}



#endif
