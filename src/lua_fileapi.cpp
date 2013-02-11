/*
Minetest-c55
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "lua_fileapi.h"
#include "porting.h"
#include "filesys.h"
#include "log.h"
#include <vector>

#define method(class, name) {#name, class::l_##name}

FileRef::FileRef() {
	m_file = 0;
	m_writable = false;
}

FileRef::~FileRef() {
}

FileRef* FileRef::checkobject(lua_State *L, int narg)
{
	luaL_checktype(L, narg, LUA_TUSERDATA);
	void *ud = luaL_checkudata(L, narg, FileRef::className);
	if(!ud) luaL_typerror(L, narg, FileRef::className);
	return *(FileRef**)ud;  // unbox pointer
}

// garbage collector
int FileRef::gc_object(lua_State *L) {
	FileRef *o = *(FileRef **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}

// Creates an FileRef and leaves it on top of stack
// Not callable from Lua; all references are created on the C side.
bool FileRef::open(std::string path, std::string mode) {
	if (mode == "t") {
		m_file = new std::fstream(path.c_str(), std::ios_base::trunc | std::ios::out | std::ios::in);
		m_writable = true;
	}
	else if (mode == "w") {
		m_file = new std::fstream(path.c_str(), std::ios_base::ate | std::ios::out | std::ios::in);
		m_writable = true;
	}
	else {
		//open read only by default
		m_file = new std::fstream(path.c_str(), std::ios_base::in);
		m_writable = false;
	}

	if (m_file != 0) {
		return true;
	}

	return false;
}

int FileRef::l_delete(lua_State *L) {
	std::string filename = luaL_checkstring(L, 1);
	std::string type = luaL_checkstring(L, 2);

	if (checkFilename(filename,type)) {
		std::string complete_path = getFilename(filename,type,L);
		if (fs::DeleteSingleFileOrEmptyDirectory(complete_path)) {
			lua_pushboolean(L, true);
			return 1;
		}
	}
	lua_pushboolean(L, false);
	return 1;
}

//open file
int FileRef::l_open(lua_State *L) {

	std::string filename = luaL_checkstring(L, 1);
	std::string type = luaL_checkstring(L, 2);
	std::string mode = luaL_checkstring(L, 3);

	if (checkFilename(filename,type)) {

		std::string complete_path = getFilename(filename,type,L);

		if (complete_path == "" ) {
			errorstream << "Invalid file type specified on opening file" << std::endl;
			lua_pushnil(L);
			return 1;
		}
		FileRef* ref = new FileRef();
		*(void **)(lua_newuserdata(L,sizeof(void *))) = ref;
		luaL_getmetatable(L,className);
		lua_setmetatable(L,-2);

		if (!ref->open(complete_path,mode)) {
			//TODO add error message
			errorstream << "unable to open file" << std::endl;
		}
	}
	else {
		lua_pushnil(L);
	}

	return 1;
}

int FileRef::l_close(lua_State *L) {
	FileRef* file = checkobject(L, 1);

	if (file != 0) {
		file->m_file->close();
		delete file->m_file;
		file->m_file = 0;
	}
	return 0;
}

int FileRef::l_getline(lua_State *L) {
	FileRef* file = checkobject(L, 1);

	if ((file != 0) &&
		(file->m_file->is_open())){
		std::string line;
		getline(*(file->m_file),line);
		lua_pushstring(L, line.c_str());
		return 1;
	}

	lua_pushnil(L);
	return 1;
}

int FileRef::l_seek(lua_State *L) {
	FileRef* file = checkobject(L, 1);
	int seekto = luaL_checkint(L,2);

	if ((file != 0) &&
		(file->m_file->is_open())){
		file->m_file->seekg(seekto);

		if (file->m_file->tellg() == seekto) {
			lua_pushboolean(L, true);
			return 1;
		}
	}

	lua_pushboolean(L, false);
	return 1;
}

int FileRef::l_write(lua_State *L) {
	FileRef* file = checkobject(L, 1);
	std::string data = luaL_checkstring(L, 2);

	if ((file->m_writable) &&
		(file->m_file->is_open()) &&
		(data != "")){
		file->m_file->write(data.c_str(),data.size());
		lua_pushboolean(L, true);
	}
	else {
		errorstream << "file not writable or no data:" << data << std::endl;
		lua_pushboolean(L, false);
	}
	return 1;
}

int FileRef::l_read(lua_State *L) {
	FileRef* file = checkobject(L, 1);

	if ((file != 0) &&
		(file->m_file->is_open())){
		std::string retval = "";
		std::string toappend = "";

		while ( file->m_file->good() ) {
			getline (*(file->m_file),toappend);
			retval += toappend;
			retval += "\n";
		}

		lua_pushstring(L, retval.c_str());
		return 1;
	}

	lua_pushnil(L);
	return 1;
}

std::string FileRef::getFilename(std::string filename,std::string type,lua_State *L) {
	std::string complete_path = "";
	if (type == "world") {
		complete_path = get_server(L)->getWorldPath();
		complete_path += DIR_DELIM;
		complete_path += filename;
	} else if (type == "user") {
		complete_path = porting::path_user;
		complete_path += DIR_DELIM;
		complete_path += filename;
	}

	return complete_path;
}

bool FileRef::checkFilename(std::string filename,std::string type) {

	//dir delim is not allowed in filenames at all
	if (filename.find(DIR_DELIM) != std::string::npos)
		return false;

	//check for special files
	if (type == "world") {
		if (filename == "auth.txt") return false;
		if (filename == "env_meta.txt") return false;
		if (filename == "ipban.txt") return false;
		if (filename == "map_meta.txt") return false;
		if (filename == "map.sqlite") return false;
		if (filename == "players") return false;
		if (filename == "rollback.txt") return false;
		if (filename == "world.mt") return false;
		if (filename == "settings.conf") return false;
	}

	if (type == "user") {
		if (filename == "minetest.conf") return false;
	}

	return true;
}

void FileRef::Register(lua_State *L)
	{
		lua_newtable(L);
		int methodtable = lua_gettop(L);
		luaL_newmetatable(L, className);
		int metatable = lua_gettop(L);

		lua_pushliteral(L, "__metatable");
		lua_pushvalue(L, methodtable);
		lua_settable(L, metatable);  // hide metatable from Lua getmetatable()

		lua_pushliteral(L, "__index");
		lua_pushvalue(L, methodtable);
		lua_settable(L, metatable);

		lua_pushliteral(L, "__gc");
		lua_pushcfunction(L, gc_object);
		lua_settable(L, metatable);

		lua_pop(L, 1);  // drop metatable

		luaL_openlib(L, 0, methods, 0);  // fill methodtable
		lua_pop(L, 1);  // drop methodtable

		// Cannot be created from Lua
		//lua_register(L, className, create_object);
	}

const char FileRef::className[] = "FileRef";
const luaL_reg FileRef::methods[] = {
	method(FileRef, close),
	method(FileRef, getline),
	method(FileRef, write),
	method(FileRef, read),
	method(FileRef, seek),
	{0,0}
};
