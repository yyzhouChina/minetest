tabbuilder = {}
tabbuilder.current_tab = engine.setting_get("main_menu_tab")
tabbuilder.old_tab = tabbuilder.current_tab
tabbuilder.current_dialog = nil
tabbuilder.last_tab_index = 1
tabbuilder.tablist = {}
tabbuilder.on_tab_change_callbacks = {}
tabbuilder.x = -0.3
tabbuilder.y = -0.99


--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
-- Init functions
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
function tabbuilder.register_on_tab_change_callback(cbf)
	table.insert(tabbuilder.on_tab_change_callbacks,cbf)
end

--------------------------------------------------------------------------------
function tabbuilder.set_tab_pos(x,y)
	assert(x ~= nil and y ~= nil and tonumber(x) ~= nil and tonumber(y) ~= nil)
	tabbuilder.x = x
	tabbuilder.y = y
end

--------------------------------------------------------------------------------
function tabbuilder.register_tab(
		name, caption, tabsize, cbf_formspec, cbf_button_handler, cbf_events)

	assert(tabsize == nil or (type(tabsize) == table and tabsize.x ~= nil and tabsize.y ~= nil))
	assert(cbf_formspec ~= nil and type(cbf_formspec) == "function")
	assert(cbf_button_handler == nil or type(cbf_button_handler) == "function")
	assert(cbf_events == nil or type(cbf_events) == "function")

	local newtab = {
		name = name,
		caption = caption,
		button_handler = cbf_button_handler,
		event_handler = cbf_events,
		get_formspec = cbf_formspec,
		tabsize = tabsize,
		tabdata = {}
	}

	table.insert(tabbuilder.tablist,newtab)

	if tabbuilder.current_tab == nil and #tabbuilder.tablist == 1 then
		tabbuilder.current_tab = newtab.name
		tabbuilder.old_tab = tabbuilder.current_tab
		tabbuilder.last_tab_index = 1
	end

	return #tabbuilder.tablist
end

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
-- Runtime functions
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
function tabbuilder.getformspec()

	local formspec = ""

	-- don't show tab if a dialog is active
	if tabbuilder.current_dialog ~= nil then
		print "dialog active"
	else
		local tsize = tabbuilder.tablist[tabbuilder.last_tab_index].tabsize or
						{width=12, height=5.2}
		formspec = formspec .. "size[" .. tsize.width .. "," .. tsize.height .. "]"
		formspec = formspec .. tabbuilder.tab_header()
		formspec = formspec ..
				tabbuilder.tablist[tabbuilder.last_tab_index].get_formspec(
					tabbuilder.tablist[tabbuilder.last_tab_index].name,
					tabbuilder.tablist[tabbuilder.last_tab_index].tabdata,
					tabbuilder.tablist[tabbuilder.last_tab_index].tabsize
					)
	end

	return formspec
end

--------------------------------------------------------------------------------
function tabbuilder.handle_buttons(fields)
	if tabbuilder.handle_tab_buttons(fields) then
		print("tab changed")
		return true
	end

	print("Current tab: " .. tabbuilder.last_tab_index)

	if tabbuilder.tablist[tabbuilder.last_tab_index].button_handler ~= nil then
		print("have tab buttonhandler")
		local retval =
			tabbuilder.tablist[tabbuilder.last_tab_index].button_handler(
					fields,
					tabbuilder.tablist[tabbuilder.last_tab_index].name,
					tabbuilder.tablist[tabbuilder.last_tab_index].tabdata
					)

		return true
		--TODO evaluate return data
	end
end

--------------------------------------------------------------------------------
function tabbuilder.handle_events(event)
end

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
-- internal functions
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

function tabbuilder.tab_header()

	local toadd = ""

	for i=1,#tabbuilder.tablist,1 do

		if toadd ~= "" then
			toadd = toadd .. ","
		end

		toadd = toadd .. tabbuilder.tablist[i].caption
	end
	return "tabheader[" .. tabbuilder.x .. "," .. tabbuilder.y .. ";main_tab;" ..
			toadd ..";" .. tabbuilder.last_tab_index .. ";true;false]"
end

--------------------------------------------------------------------------------
function tabbuilder.handle_tab_buttons(fields)

	--save tab selection to config file
	if fields["main_tab"] then
		local index = tonumber(fields["main_tab"])
		tabbuilder.last_tab_index = index
		tabbuilder.current_tab = tabbuilder.tablist[index].name
		engine.setting_set("main_menu_tab",tabbuilder.current_tab)
		print("tab changed to: " .. tabbuilder.current_tab)
	end

	--handle tab changes
	if tabbuilder.current_tab ~= tabbuilder.old_tab then
		for i=1,#tabbuilder.on_tab_change_callbacks,1 do
			if type(tabbuilder.on_tab_change_callbacks[i] == "function") then
				tabbuilder.on_tab_change_callbacks(
					tabbuilder.current_tab,
					tabbuilder.old_tab)
			end
		end
		tabbuilder.old_tab = tabbuilder.current_tab
		return true
	end

	return false
end

--------------------------------------------------------------------------------
function tabbuilder.get_tabdata(tabid)
	if tabid > 0 and tabid <= #tabbuilder.tablist then
		return tabbuilder.tablist[tabid].tabdata
	end
	return nil
end