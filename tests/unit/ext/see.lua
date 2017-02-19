local function ReadInt(str)
	local t = {}
	for w in str:gmatch(".") do table.insert(t, w:byte()) end
	return t[1]+t[2]*256+t[3]*256^2+t[4]*256^3
end

local function num_args(func)
	local ok = pcall(function() string.dump(func) end)
	if not ok then return "?" end
	local dump = string.dump(func)
	--Set up the cursor
	local cursor = 13
	local offset = ReadInt(dump:sub(cursor, cursor + 4))
	cursor = cursor + 13 + offset
	--Get the Parameters
	local numParams = dump:sub(cursor, cursor + 1):byte()
	cursor = cursor + 1
	--Get the Variable-Args flag (whether there's a ... in the param)
	local varFlag = dump:sub(cursor, cursor + 1):byte()

	local ret = tostring(numParams)
	if varFlag > 1 then ret = ret .. "+" end

	return ret
end

local int = 4
if os.getenv("OS") ~= "Windows_NT" then int = 8 end

function get_args(func)
	local once = true
	local function _get_args(dump, cursor)
		--Set up the cursor
		if not cursor then
			cursor = 1
			--Skip Header Stuff
			cursor = cursor + 12
		end
		--Skip Source Name
		local offset = ReadInt(dump:sub(cursor, cursor + int))
		cursor = cursor + int
		local sourceName = dump:sub(cursor, cursor+offset)
		cursor = cursor + offset
		--Skip Line Defined
		cursor = cursor + 4
		--Skip Last Line Defined
		cursor = cursor + 4
		--Get the Upvalues
		local numUpvalues = dump:sub(cursor, cursor + 1):byte()
		cursor = cursor + 1
		--Get the Parameters
		local numParams = dump:sub(cursor, cursor + 1):byte()
		cursor = cursor + 1
		--Get the Variable-Args flag (whether there's a ... in the param)
		local varFlag = dump:sub(cursor, cursor + 1):byte()
		cursor = cursor + 1
		--Skip the Stacksizes
		cursor = cursor + 1
		--CODE:
		--~Skip the entire code...
		local offset = ReadInt(dump:sub(cursor, cursor + 4))
		cursor = cursor + 4
		cursor = cursor + offset*4
		--CONSTANTS:
		--~Skip the entire constant section...
		local offset = ReadInt(dump:sub(cursor, cursor + 4))
		cursor = cursor + 4
		for i=1,offset do

			local tp = dump:sub(cursor, cursor):byte()
			cursor = cursor + 1
			if tp == 1 then
				--Boolean
				cursor = cursor + 1
			elseif tp == 3 then
				--Number
				local off = ReadInt(dump:sub(cursor, cursor + 8))
				cursor = cursor + 8
			elseif tp == 4 then
				--String
				local off = ReadInt(dump:sub(cursor, cursor + int))
				cursor = cursor + int
				cursor = cursor + off
			else
				-- Pass on nil
			end
		end
		--FUNCTIONS:
		--Reget...
		local offset = ReadInt(dump:sub(cursor, cursor + 4))
		cursor = cursor + 4
		for i=1, offset do
			local _
			_, cursor = _get_args(dump, cursor)
		end
		--LINES:
		--~Skip the entire Line section...
		local offset = ReadInt(dump:sub(cursor, cursor + 4))
		cursor = cursor + 4
		cursor = cursor + offset*4
		--LOCALS:
		--~Get Locals
		local offset = ReadInt(dump:sub(cursor, cursor + 4))
		cursor = cursor + 4
		local locals = {}
		local params = {}
		for i=1,offset do
			local off = ReadInt(dump:sub(cursor, cursor + int))
			cursor = cursor + int
			local varname = dump:sub(cursor, cursor+off)
			table.insert(locals,varname)
			if i <= numParams then table.insert(params,varname) end
			cursor = cursor + off
			cursor = cursor + 4
			cursor = cursor + 4
		end
		if varFlag > 1 then
			table.insert(params, "...")
		end
		--UPVALUES:
		--~Get Upvalues
		local offset = ReadInt(dump:sub(cursor, cursor + 4))
		cursor = cursor + 4
		for i=1,offset do
			local off = ReadInt(dump:sub(cursor, cursor + int))
			cursor = cursor + int
			cursor = cursor + off
		end
		return params, cursor
	end
	local ok = pcall(function() string.dump(func) end)
	if not ok then return {"?"} end
	local dump = string.dump(func)
	local ret = {_get_args(dump)}
	return ret[1]
end
function string.join(self, table, ...)
	local str = string.format(("%s"..self):rep(#table), unpack(table))
	return str:sub(1, #str-#self)
end

-- See
function see(object, query)
	if not query then query = "*" end
	local function _keys(Table)
		local tab = {}
		for k in pairs(Table) do
			if type(k) == "string" then
				table.insert(tab, k)
			end
		end
		return tab
	end
	local function _len(Table)
		local n = 0
		for i in pairs(Table) do
			n = n + 1
		end
		return n
	end
	local function _just(str, limit)
		if not limit then limit = 20 end

		if #str <= limit then
			return str .. string.rep(" ", limit-#str)
		else
			return str .. string.rep(" ", math.ceil(#str/limit)*limit-#str)
		end
	end
	local function dump(list, limit)
		if not limit then limit = 20 end
		local str = "{"
		local seen = {}
		local i = 1
		for _,v in ipairs(list) do
			if i > limit then str = str .. "..., "; break end
			if type(v) ~= "table" then
				if type(v) == "string" then
					if #v > 10 then v = v:sub(1,10) .. "..." end
					str = str .. '"'..v..'"' .. ", "
				elseif type(v) == "number" then
					v = tostring(v)
					if #v > 5 then v = v:sub(1,5) .. "..." end
					str = str .. v .. ", "
				elseif type(v) == "function" then
					str = str .. string.format("function(%s)",table.concat(get_args(v), ", ")) .. ", "
				else
					str = str .. tostring(v) .. ", "
				end
				i = i + 1
			else
				str = str .. dump(v, limit-i) .. ", "
				i = i + #v

			end
			seen[_] = true
		end
		if #list > 0 then
			str = str:sub(1, #str-2)
		end
		local trim = false
	--~ 	for k,v in pairs(list) do
	--~ 		if not seen[k] then
	--~ 			trim = true
	--~ 			if type(v) ~= "table" then
	--~ 				str = str .. tostring(k) .. " = " .. tostring(v) .. ", "
	--~ 			else
	--~ 				str = str .. tostring(k) .. " = " .. dump(v) .. ", "
	--~ 			end
	--~ 		end
	--~ 	end
		if trim then str = str:sub(1, #str-2) end
		str = str .. "}"
		return str
	end
	local function _introspect(Table, prefix, postfix, translate)
		local keys = _keys(Table)
		table.sort(keys)
		if not translate then translate = {} end

		local matches = {}

		for _,k in ipairs(keys) do
			--Setup Query Condition:
			if type(k) == "table" then
				k = string.format("table[%s]",_len(k))
			end
			if query and type(k) ~= "number" then
				if type(query) ~= "string"then
					--Pass
				else
					--Cases:
					-- name -> abs match
					-- name* -> relative match
					-- *name -> relative match
					if (k):match(query:gsub("*", ".*")) then
						local pre = "."
						local post = ""

						local Type = type(Table[k])
						if (Type == "table") then
							post = string.format("[%s]", _len(Table[k]))
						elseif (Type == "function") then
							local del = ", "
							post = string.format("(%s)",del:join(get_args(Table[k])))
						elseif (Type == "string") then
							local ellipses = "..."
							if #Table[k] <= 10 then ellipses = "" end
							post = string.format(" = \"%s%s\"",Table[k]:sub(1,10),ellipses)
						elseif (Type == "number") then
							local this = tostring(Table[k])
							local ellipses = "..."
							if #this <= 10 then ellipses = "" end
							post = string.format(" = %s%s",this:sub(1,10), ellipses)
						end

						if prefix and not translate[k] then pre = prefix else pre = "" end
						if postfix and translate[k] then post = postfix end

						if translate[k] then k = translate[k] end
						local format = string.format("%s%s%s",pre,k,post)
						table.insert(matches,format)
					end
				end
			end
		end
		local n = 0
		local str = ""
		local conwidth = conwidth or 80
		local columns = columns or 4
		local slots = math.floor(conwidth/columns)
		for _,key in ipairs(matches) do
			local spaces = math.floor(#key/slots)+1
			n = n + spaces
			if n > columns then print(str); str = ""; n = spaces end
			str = str .. key .. string.rep(" ", spaces*slots-#key)
		end
		if #str > 0 then print(str) end
	end
	if not level then level = 0 end
	if type(object) == "table" then
		--print "+ Table Elements"
		_introspect(object, ".")
		local d = dump(object)
		if #d > 2 then print(d) end
	elseif type(object) == "function" then
		print(string.format("function(%s)",string.join(", ",get_args(object))))
	else
		print("@"..string.upper(type(object)),object)
	end
	local _mt = getmetatable(object)
	local _mt_translate = {
		__index = "[]",
		__call = "()",
		__add = "+",
		__sub = "-",
		__mul = "*",
		__div = "/",
		__mod = "%",
		__pow = "^",
		__unm = "-self",
		__concat = "self .. ''",
		__len = "#",
		__eq = "==",
		__lt = "<",
		__le = "<=",
		__newindex = "self[] = obj",
		__gc = "GC",
		__tostring = "tostring()",
		__tonumber = "tonumber()",
	}
	if _mt then
		print "\nMetatable"
		_introspect(_mt, "mt.", "", _mt_translate)
	end

	local _return = newproxy(true)
	local __mt = getmetatable(_return)
	__mt.__index = function(self, key)
		if type(object) ~= "table" then return end
		if object[key] then
			print("\n@"..tostring(key))
			return see(object[key])
		end
	end
	__mt.__call = function(self)
		return self
	end
	return _return
end
return see
