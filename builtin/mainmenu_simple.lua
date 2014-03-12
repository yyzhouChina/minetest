print = engine.debug
math.randomseed(os.time())
os.setlocale("C", "numeric")

local errorfct = error
error = function(text)
	print(debug.traceback(""))
	errorfct(text)
end

local scriptpath = engine.get_scriptdir()

mt_color_grey  = "#AAAAAA"
mt_color_blue  = "#0000DD"
mt_color_green = "#00DD00"
mt_color_dark_green = "#003300"

--for all other colors ask sfan5 to complete his worK!

dofile(scriptpath .. DIR_DELIM .. "misc_helpers.lua")
dofile(scriptpath .. DIR_DELIM .. "tabbuilder.lua")
dofile(scriptpath .. DIR_DELIM .. "mainmenu_common.lua")
dofile(scriptpath .. DIR_DELIM .. "async_event.lua")

local defaulttexturedir = engine.get_texturepath_share() .. DIR_DELIM .. "base" ..
					DIR_DELIM .. "pack" .. DIR_DELIM

local maintabid

function main_get_formspec(name, tabdata, tabsize)
	local retval = ""

	local render_details = dump(engine.setting_getbool("public_serverlist"))

	retval = retval ..
		"label[0,3.0;".. fgettext("Address/Port") .. "]"..
		"label[8,0.5;".. fgettext("Name/Password") .. "]" ..
		"field[0.25,3.25;5.5,0.5;te_address;;" ..engine.setting_get("address") .."]" ..
		"field[5.75,3.25;2.25,0.5;te_port;;" ..engine.setting_get("remote_port") .."]" ..
		"checkbox[8,-0.25;cb_public_serverlist;".. fgettext("Public Serverlist") .. ";" ..
		render_details .. "]"

	retval = retval ..
		"button[8,2.5;4,1.5;btn_mp_connect;".. fgettext("Connect") .. "]" ..
		   "field[8.75,1.5;3.5,0.5;te_name;;" ..engine.setting_get("name") .."]" ..
		"pwdfield[8.75,2.3;3.5,0.5;te_pwd;]"

	--favourites
	retval = retval ..
		"textlist[-0.05,0.0;7.55,2.75;favourites;"

	if #tabdata.favorites > 0 then
		retval = retval .. render_favorite(tabdata.favorites[1],render_details)

		for i=2,#tabdata.favorites,1 do
			retval = retval .. "," .. render_favorite(tabdata.favorites[i],render_details)
		end
	end

	if tabdata.fav_selected ~= nil then
		retval = retval .. ";" .. tabdata.fav_selected .. "]"
	else
		retval = retval .. ";0]"
	end

	-- separator
	retval = retval ..
		"box[-0.3,3.75;12.4,0.1;#FFFFFF]"

	-- checkboxes
	retval = retval ..
		"checkbox[2.5,3.9;cb_creative;".. fgettext("Creative Mode") .. ";" ..
		dump(engine.setting_getbool("creative_mode")) .. "]"..
		"checkbox[6.5,3.9;cb_damage;".. fgettext("Enable Damage") .. ";" ..
		dump(engine.setting_getbool("enable_damage")) .. "]"
	-- buttons
	retval = retval ..
--		"button[0.0,4.5;6,1.5;btn_reset_world;" .. fgettext("Reset World") .. "]" ..
--		"button[0.0,3.0;6,1.5;btn_modstore;" .. fgettext("Modstore") .. "]" ..
		"button[3.0,4.5;6,1.5;btn_start_singleplayer;" .. fgettext("Start Singleplayer") .. "]"

	return retval
end

--------------------------------------------------------------------------------
local function asyncOnlineFavourites()
	local maintabdata = tabbuilder.get_tabdata(maintabid)
	maintabdata.favorites = {}
	engine.handle_async(
		function(param)
			return engine.get_favorites("online")
		end,
		nil,
		function(result)
			maintabdata.favorites = result
			engine.event_handler("Refresh")
		end
		)
end

--------------------------------------------------------------------------------

function main_button_handler(fields,name,tabdata)
	if fields["btn_start_singleplayer"] then
		gamedata.selected_world	= gamedata.worldindex
		gamedata.singleplayer	= true
		engine.start()
	end

	if fields["favourites"] ~= nil then
		local event = engine.explode_textlist_event(fields["favourites"])

		if event.type == "CHG" then
			if event.index <= #tabdata.favorites then
				local address = tabdata.favorites[event.index].address
				local port = tabdata.favorites[event.index].port

				if address ~= nil and
					port ~= nil then
					engine.setting_set("address",address)
					engine.setting_set("remote_port",port)
				end

				tabdata.fav_selected = event.index
			end
		end
		return
	end

	if fields["cb_public_serverlist"] ~= nil then
		engine.setting_set("public_serverlist", fields["cb_public_serverlist"])

		if engine.setting_getbool("public_serverlist") then
			asyncOnlineFavourites()
		else
			local maintabdata = tabbuilder.get_tabdata(maintabid)
			maintabdata.favorites = engine.get_favorites("local")
		end
		return
	end

	if fields["btn_mp_connect"] ~= nil or
		fields["key_enter"] ~= nil then

		gamedata.playername		= fields["te_name"]
		gamedata.password		= fields["te_pwd"]
		gamedata.address		= fields["te_address"]
		gamedata.port			= fields["te_port"]

		local fav_idx = engine.get_textlist_index("favourites")

		if fav_idx ~= nil and fav_idx <= #tabdata.favorites and
			tabdata.favorites[fav_idx].address == fields["te_address"] and
			tabdata.favorites[fav_idx].port    == fields["te_port"] then

			gamedata.servername			= tabdata.favorites[fav_idx].name
			gamedata.serverdescription	= tabdata.favorites[fav_idx].description
		else
			gamedata.servername			= ""
			gamedata.serverdescription	= ""
		end

		gamedata.selected_world = 0

		engine.setting_set("address",fields["te_address"])
		engine.setting_set("remote_port",fields["te_port"])

		engine.start()
		return
	end
end

--------------------------------------------------------------------------------
function init_globals()
	--init gamedata
	gamedata.worldindex = 0

	local worldlist = engine.get_worlds()

	local found_singleplayerworld = false

	for i=1,#worldlist,1 do
		if worldlist[i].name == "singleplayerworld" then
			found_singleplayerworld = true
			gamedata.worldindex = i
		end
	end

	if not found_singleplayerworld then
		engine.create_world("singleplayerworld", 1)

		local worldlist = engine.get_worlds()

		for i=1,#worldlist,1 do
			if worldlist[i].name == "singleplayerworld" then
				gamedata.worldindex = i
			end
		end
	end

	maintabid = tabbuilder.register_tab("main",fgettext("Main"),nil,
		main_get_formspec,main_button_handler)

	local maintabdata = tabbuilder.get_tabdata(maintabid)

	maintabdata.favorites = {}
	maintabdata.fav_selected = 0

	asyncOnlineFavourites()

	tabbuilder.register_tab("adv",fgettext("Advanced"),nil,function() return "" end)

	tabbuilder.register_tab("credits",fgettext("Credits"),nil,
		function ()
			local logofile = defaulttexturedir .. "logo.png"
			return	"vertlabel[0,-0.5;CREDITS]" ..
				"label[0.5,3;Minetest " .. engine.get_version() .. "]" ..
				"label[0.5,3.3;http://minetest.net]" ..
				"image[0.5,1;" .. engine.formspec_escape(logofile) .. "]" ..
				"textlist[3.5,-0.25;8.5,5.8;list_credits;" ..
				"#FFFF00" .. fgettext("Core Developers") .."," ..
				"Perttu Ahola (celeron55) <celeron55@gmail.com>,"..
				"Ryan Kwolek (kwolekr) <kwolekr@minetest.net>,"..
				"PilzAdam <pilzadam@minetest.net>," ..
				"Ilya Zhuravlev (xyz) <xyz@minetest.net>,"..
				"Lisa Milne (darkrose) <lisa@ltmnet.com>,"..
				"Maciej Kasatkin (RealBadAngel) <mk@realbadangel.pl>,"..
				"proller <proler@gmail.com>,"..
				"sfan5 <sfan5@live.de>,"..
				"kahrl <kahrl@gmx.net>,"..
				"sapier,"..
				"ShadowNinja <shadowninja@minetest.net>,"..
				"Nathanael Courant (Nore/Novatux) <nore@mesecons.net>,"..
				"BlockMen,"..
				","..
				"#FFFF00" .. fgettext("Active Contributors") .. "," ..
				"Vanessa Ezekowitz (VanessaE) <vanessaezekowitz@gmail.com>,"..
				"Jurgen Doser (doserj) <jurgen.doser@gmail.com>,"..
				"Jeija <jeija@mesecons.net>,"..
				"MirceaKitsune <mirceakitsune@gmail.com>,"..
				"dannydark <the_skeleton_of_a_child@yahoo.co.uk>,"..
				"0gb.us <0gb.us@0gb.us>,"..
				"," ..
				"#FFFF00" .. fgettext("Previous Contributors") .. "," ..
				"Guiseppe Bilotta (Oblomov) <guiseppe.bilotta@gmail.com>,"..
				"Jonathan Neuschafer <j.neuschaefer@gmx.net>,"..
				"Nils Dagsson Moskopp (erlehmann) <nils@dieweltistgarnichtso.net>,"..
				"Constantin Wenger (SpeedProg) <constantin.wenger@googlemail.com>,"..
				"matttpt <matttpt@gmail.com>,"..
				"JacobF <queatz@gmail.com>,"..
				";0;true]"
			end
	)
end

--------------------------------------------------------------------------------
function update_menu()

	local formspec

	-- handle errors
	if gamedata.errormessage ~= nil then
		formspec = "size[12,3.2]" ..
			"textarea[1,1;10,2;;ERROR: " ..
			engine.formspec_escape(gamedata.errormessage) ..
			";]"..
			"button[4.5,2.5;3,0.5;btn_error_confirm;" .. fgettext("Ok") .. "]"
	else
		formspec = tabbuilder.getformspec()
	end

	print("Setting formspec to: " .. formspec)
	engine.update_formspec(formspec)
end

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
-- initialize callbacks
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
engine.button_handler = function(fields)
	print("Buttonhandler: tab: " .. tabbuilder.current_tab .. " fields: " .. dump(fields))

	if fields["btn_error_confirm"] then
		gamedata.errormessage = nil
		update_menu()
		return
	end

	if tabbuilder.handle_buttons(fields) then
		update_menu()
	end
end

--------------------------------------------------------------------------------
engine.event_handler = function(event)
	tabbuilder.handle_events(event)

	if event == "Refresh" then
		update_menu()
	end
end

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
-- menu startup
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
init_globals()
update_menu()