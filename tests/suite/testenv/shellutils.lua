--[[ $%BEGINLICENSE%$
 Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; version 2 of the
 License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA

 $%ENDLICENSE%$ --]]

module("testenv.shellutils", package.seeall)

VERBOSE = tonumber(os.getenv("VERBOSE")) or 0

---
-- turn a option-table into a string 
--
-- the values are encoded and quoted for the shell
--
-- @param tbl a option table
-- @param sep the seperator, defaults to a space
function options_tostring(tbl, sep) 
	-- default value for sep 
	sep = sep or " "
	
	assert(type(tbl) == "table")
	assert(type(sep) == "string")

	local s_tbl = { }
	for k, v in pairs(tbl) do
		local values
		-- if the value is a table, repeat the option
		if type(v) == "table" then
			values = v
		else
			values = { v }
		end

		for tk, tv in pairs(values) do
			if tv == true then
				table.insert(s_tbl, "--" .. k)
			else
				local enc_value = tv:gsub("\\", "\\\\"):gsub("\"", "\\\"")
				table.insert(s_tbl, "--" .. k .. "=\"" .. enc_value .. "\"")
			end
		end
	end
	local s = table.concat(s_tbl, " ")
	-- print_verbose(" option: " .. s)

	return s
end

--- turns an option table into a string of environment variables
--
function env_tostring(tbl)
	assert(type(tbl) == "table")

	local s = ""
	for k, v in pairs(tbl) do
		local enc_value = v:gsub("\\", "\\\\"):gsub("\"", "\\\"")
		s = s .. k .. "=\"" .. enc_value .. "\" "
	end

	return s
end

---
-- print_verbose()
--
-- prints a message if either the DEBUG or VERBOSE variables
-- are set.
--
-- @param msg the message being printed
-- @param min_level the minimum verbosity level for printing the message (default 1)
function print_verbose(msg, min_level)
	min_level = min_level or 1
	if (VERBOSE >= min_level) then
		print (msg)
	end
end

