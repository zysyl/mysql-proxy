function normalize_query(tokens)
	local n_q = ""

	for i, token in ipairs(tokens) do
		-- normalize the query
		if token["token_name"] == "TK_COMMENT" then
		elseif token["token_name"] == "TK_LITERAL" then
			n_q = n_q .. "`" .. token.text .. "` "
		elseif token["token_name"] == "TK_STRING" then
			n_q = n_q .. "? "
		elseif token["token_name"] == "TK_NUMBER" then
			n_q = n_q .. "? "
		elseif token["token_name"] == "TK_FLOAT" then
			n_q = n_q .. "? "
		else
			n_q = n_q .. token.text:upper() .. " "
		end
	end

	return n_q
end

function read_query(packet) 
	if packet:byte() == proxy.COM_QUERY then
		local tokens = proxy.tokenize(packet:sub(2))

		-- just for debug
		for i, token in ipairs(tokens) do
			-- print the token and what we know about it
			print(i .. ": " .. " { " .. token["token_name"] .. ", " .. token["text"] .. " }" )
		end

		print("normalized query: " .. normalize_query(tokens))
	end
end
