#ifndef __MMD_HPP__
#define __MMD_HPP__

#include <fsys/filesystem.h>
#include <optional>
#include <string>

struct lua_State;
class NetworkState;
class Model;
namespace import
{
	bool import_pmx(NetworkState &nw,Model &mdl,VFilePtr f,const std::optional<std::string> &path={});
	int import_pmx(lua_State *l);
	int import_vmd(lua_State *l);
};

#endif
