function render_favorite(spec,render_details)
	local text = ""

	if spec.name ~= nil then
		text = text .. engine.formspec_escape(spec.name:trim())

--		if spec.description ~= nil and
--			engine.formspec_escape(spec.description):trim() ~= "" then
--			text = text .. " (" .. engine.formspec_escape(spec.description) .. ")"
--		end
	else
		if spec.address ~= nil then
			text = text .. spec.address:trim()

			if spec.port ~= nil then
				text = text .. ":" .. spec.port
			end
		end
	end

	if not render_details then
		return text
	end

	local details = ""
	if spec.password == true then
		details = details .. "*"
	else
		details = details .. "_"
	end

	if spec.creative then
		details = details .. "C"
	else
		details = details .. "_"
	end

	if spec.damage then
		details = details .. "D"
	else
		details = details .. "_"
	end

	if spec.pvp then
		details = details .. "P"
	else
		details = details .. "_"
	end
	details = details .. " "

	local playercount = ""

	if spec.clients ~= nil and
		spec.clients_max ~= nil then
		playercount = string.format("%03d",spec.clients) .. "/" ..
						string.format("%03d",spec.clients_max) .. " "
	end

	return playercount .. engine.formspec_escape(details) ..  text
end