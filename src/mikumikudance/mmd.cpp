#include "wv_source.hpp"
#include "mikumikudance/mmd.hpp"
#include <mathutil/uvec.h>
#include <pragma/game/game.h>
#include <pragma/model/vertex.h>
#include <pragma/lua/ldefinitions.h>
#include <pragma/lua/libraries/lfile.h>
#include <pragma/model/modelmesh.h>
#include <pragma/model/model.h>
#include <pragma/engine.h>
#include <pragma/networkstate/networkstate.h>
#include <pragma/asset/util_asset.hpp>
#include <sharedutils/util_string.h>
#include <sharedutils/util_file.h>
#include <sharedutils/util_path.hpp>
#include <fsys/filesystem.h>
#include <util_mmd.hpp>
#include <luainterface.hpp>
#include <util_image.hpp>
#include <util_texture_info.hpp>
#include <sharedutils/util.h>

extern DLLENGINE Engine *engine;

#pragma optimize("",off)

static std::unordered_map<std::string,std::string> jpMorphNameToEnglish = {
	// See https://www.deviantart.com/xoriu/art/MMD-Facial-Expressions-Chart-341504917
	// Key is japanese name in UTF8

	// Eyes
	{"\xe3\x81\xbe\xe3\x81\xb0\xe3\x81\x9f\xe3\x81\x8d","Blink"},
	{"\xe7\xac\x91\xe3\x81\x84","Smile"},
	{"\xe3\x82\xa6\xe3\x82\xa3\xe3\x83\xb3\xe3\x82\xaf","Wink"},
	{"\xe3\x82\xa6\xe3\x82\xa3\xe3\x83\xb3\xe3\x82\xaf\xe5\x8f\xb3","Wink-a"},
	{"\xe3\x82\xa6\xe3\x82\xa3\xe3\x83\xb3\xe3\x82\xaf\xef\xbc\x92","Wink-b"},
	{"\xef\xbd\xb3\xef\xbd\xa8\xef\xbe\x9d\xef\xbd\xb8\xef\xbc\x92\xe5\x8f\xb3","Wink-c"},
	{"\xe3\x81\xaa\xe3\x81\x94\xe3\x81\xbf","Howawa"},
	{"\xe3\x81\xaf\xe3\x81\x85","> <"},
	{"\xe3\x81\xb3\xe3\x81\xa3\xe3\x81\x8f\xe3\x82\x8a","Ha!!!"},
	{"\xe3\x81\x98\xe3\x81\xa8\xe7\x9b\xae","Jito-eye"},
	{"\xef\xbd\xb7\xef\xbe\x98\xef\xbd\xaf","Kiri-eye"},
	{"\xe3\x81\xaf\xe3\x81\xa1\xe3\x82\x85\xe7\x9b\xae","O O"},
	{"\xe6\x98\x9f\xe7\x9b\xae","EyeStar"},
	{"\xe3\x81\xaf\xe3\x81\x81\xe3\x81\xa8","EyeHeart"},
	{"\xe7\x9e\xb3\xe5\xb0\x8f","EyeSmall"},
	{"\xe7\x9e\xb3\xe7\xb8\xa6\xe6\xbd\xb0\xe3\x82\x8c","EyeSmall-v"},
	{"\xe5\x85\x89\xe4\xb8\x8b","EyeUnderli"},
	{"\xe6\x81\x90\xe3\x82\x8d\xe3\x81\x97\xe3\x81\x84\xe5\xad\x90\xef\xbc\x81","EyeFunky"},
	{"\xe3\x83\x8f\xe3\x82\xa4\xe3\x83\xa9\xe3\x82\xa4\xe3\x83\x88\xe6\xb6\x88","EyeHi-off"},
	{"\xe6\x98\xa0\xe3\x82\x8a\xe8\xbe\xbc\xe3\x81\xbf\xe6\xb6\x88","EyeRef-off"},
	{"\xe5\x96\x9c\xe3\x81\xb3","Joy"},
	{"\xe3\x82\x8f\xe3\x81\x89\x3f\x21","Wao?!"},
	{"\xe3\x81\xaa\xe3\x81\x94\xe3\x81\xbf\xcf\x89","Howawa w"},
	{"\xe6\x82\xb2\xe3\x81\x97\xe3\x82\x80","Wail"},
	{"\xe6\x95\xb5\xe6\x84\x8f","Hostility"},

	// Mouth
	{"\xe3\x81\x82","a"},
	{"\xe3\x81\x84","i"},
	{"\xe3\x81\x86","u"},
	{"\xe3\x81\x88","e"},
	{"\xe3\x81\x8a","o"},
	{"\xe3\x81\x82\xef\xbc\x92","a 2"},
	{"\xe3\x82\x93","n"},
	{"\xe2\x96\xb2","Mouse_1"},
	{"\xe2\x88\xa7","Mouse_2"},
	{"\xe2\x96\xa1","_"},
	{"\xe3\x83\xaf","Wa"},
	{"\xcf\x89","Omega"},
	{"\xcf\x89\xe2\x96\xa1","w_"},
	{"\xe3\x81\xab\xe3\x82\x84\xe3\x82\x8a","Niyari"},
	{"\xe3\x81\xab\xe3\x82\x84\xe3\x82\x8a\xef\xbc\x92","Niyari2"},
	{"\xe3\x81\xab\xe3\x81\xa3\xe3\x81\x93\xe3\x82\x8a","Smile"},
	{"\xe3\x81\xba\xe3\x82\x8d\xe3\x81\xa3","Pero"},
	{"\xe3\x81\xa6\xe3\x81\xb8\xe3\x81\xba\xe3\x82\x8d","Bero-tehe"},
	{"\xe3\x81\xa6\xe3\x81\xb8\xe3\x81\xba\xe3\x82\x8d\xef\xbc\x92","Bero-tehe2"},
	{"\xe5\x8f\xa3\xe8\xa7\x92\xe4\xb8\x8a\xe3\x81\x92","MouseUP"},
	{"\xe5\x8f\xa3\xe8\xa7\x92\xe4\xb8\x8b\xe3\x81\x92","MouseDW"},
	{"\xe5\x8f\xa3\xe6\xa8\xaa\xe5\xba\x83\xe3\x81\x92","MouseWD"},
	{"\xe6\xad\xaf\xe7\x84\xa1\xe3\x81\x97\xe4\xb8\x8a","ToothAnon"},
	{"\xe6\xad\xaf\xe7\x84\xa1\xe3\x81\x97\xe4\xb8\x8b","ToothBnon"},

	// Brow
	{"\xe7\x9c\x9f\xe9\x9d\xa2\xe7\x9b\xae","Serious"},
	{"\xe5\x9b\xb0\xe3\x82\x8b","Trouble"},
	{"\xe3\x81\xab\xe3\x81\x93\xe3\x82\x8a","Smily"},
	{"\xe6\x80\x92\xe3\x82\x8a","Get angry"},
	{"\xe4\xb8\x8a","UP"},
	{"\xe4\xb8\x8b","Down"}
};
static std::string get_morph_name(const std::string &jpName,const std::optional<std::string> &name={})
{
	auto morphName = jpName;
	auto it = jpMorphNameToEnglish.find(jpName.data());
	if(it != jpMorphNameToEnglish.end())
		morphName = it->second;
	if(morphName.empty() && name.has_value())
		morphName = *name;
	return morphName;
}

static std::unordered_map<std::string,std::string> jpBoneNameToEnglish = {
	// See https://learnmmd.com/http:/learnmmd.com/mmd-bone-reference-charts/ and https://github.com/syoyo/MMDLoader/blob/6b9b4bcf3c3736d1ebe4e77125ef4c1671ba4027/pmd_reader.cc
	// Key is japanese name in UTF8
	{"\xe5\x85\xa8\xe3\x81\xa6\xe3\x81\xae\xe8\xa6\xaa","mother"},
	{"\xe3\x82\xb0\xe3\x83\xab\xe3\x83\xbc\xe3\x83\x96","groove"},
	{"\xe3\x82\xbb\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc","center"},
	{"\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab","upper_body"},
	{"\xe9\xa6\x96","neck"},
	{"\xe9\xa0\xad","head"},
	{"\xe5\xb7\xa6\xe8\xa2\x96\xe5\x85\x88","cuff_L"},
	{"\xe5\xb7\xa6\xe8\xa2\x96","sleeve_L"},
	{"\xe5\xb7\xa6\xe7\x9b\xae","eye_L"},
	{"\xe4\xb8\x8b\xe5\x8d\x8a\xe8\xba\xab","lower_body"},
	{"\xe5\xb7\xa6\xe8\x82\xa9","shoulder L"},
	{"\xe5\xb7\xa6\xe8\x85\x95","arm_L"},
	{"\xe5\xb7\xa6\xe3\x81\xb2\xe3\x81\x98","elbow_L"},
	{"\xe5\xb7\xa6\xe6\x89\x8b\xe9\xa6\x96","wrist_L"},
	{"\xe5\xb7\xa6\xe8\xa6\xaa\xe6\x8c\x87\xef\xbc\x91","thumb1_L"},
	{"\xe5\xb7\xa6\xe8\xa6\xaa\xe6\x8c\x87\xef\xbc\x92","thumb2_L"},
	{"\xe5\xb7\xa6\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x91","fore1_L"},
	{"\xe5\xb7\xa6\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x92","fore2_L"},
	{"\xe5\xb7\xa6\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x93","fore3_L"},
	{"\xe5\xb7\xa6\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x91","middle1_L"},
	{"\xe5\xb7\xa6\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x92","middle2_L"},
	{"\xe5\xb7\xa6\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x93","middle3_L"},
	{"\xe5\xb7\xa6\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x91","third1_L"},
	{"\xe5\xb7\xa6\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x92","third2_L"},
	{"\xe5\xb7\xa6\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x93","third3_L"},
	{"\xe5\xb7\xa6\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x91","little1L"},
	{"\xe5\xb7\xa6\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x92","little2L"},
	{"\xe5\xb7\xa6\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x93","little3L"},
	{"\xe5\xb7\xa6\xe8\xb6\xb3","leg_L"},
	{"\xe5\xb7\xa6\xe3\x81\xb2\xe3\x81\x96","knee_L"},
	{"\xe5\xb7\xa6\xe8\xb6\xb3\xe9\xa6\x96","ankle_L"},
	{"\xe4\xb8\xa1\xe7\x9b\xae","eyes"},
	
	{"\xe5\x8f\xb3\xe8\xa2\x96\xe5\x85\x88","cuff_R"},
	{"\xe5\x8f\xb3\xe8\xa2\x96","sleeve_R"},
	{"\xe5\x8f\xb3\xe7\x9b\xae","eye_R"},
	{"\xe5\x8f\xb3\xe8\x82\xa9","shoulderR"},
	{"\xe5\x8f\xb3\xe8\x85\x95","arm_R"},
	{"\xe5\x8f\xb3\xe3\x81\xb2\xe3\x81\x98","elbow_R"},
	{"\xe5\x8f\xb3\xe6\x89\x8b\xe9\xa6\x96","wrist_R"},
	{"\xe5\x8f\xb3\xe8\xa6\xaa\xe6\x8c\x87\xef\xbc\x91","thumb1_R"},
	{"\xe5\x8f\xb3\xe8\xa6\xaa\xe6\x8c\x87\xef\xbc\x92","thumb2_R"},
	{"\xe5\x8f\xb3\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x91","fore1_R"},
	{"\xe5\x8f\xb3\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x92","fore2_R"},
	{"\xe5\x8f\xb3\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x93","fore3_R"},
	{"\xe5\x8f\xb3\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x91","middle1_R"},
	{"\xe5\x8f\xb3\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x92","middle2_R"},
	{"\xe5\x8f\xb3\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x93","middle3_R"},
	{"\xe5\x8f\xb3\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x91","third1_R"},
	{"\xe5\x8f\xb3\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x92","third2_R"},
	{"\xe5\x8f\xb3\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x93","third3_R"},
	{"\xe5\x8f\xb3\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x91","little1R"},
	{"\xe5\x8f\xb3\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x92","little2R"},
	{"\xe5\x8f\xb3\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x93","little3R"},
	{"\xe5\x8f\xb3\xe8\xb6\xb3","leg_R"},
	{"\xe5\x8f\xb3\xe3\x81\xb2\xe3\x81\x96","knee_R"},
	{"\xe5\x8f\xb3\xe8\xb6\xb3\xe9\xa6\x96","ankle_R"},

	{"\xe5\xb7\xa6\xe8\xb6\xb3\xef\xbc\xa9\xef\xbc\xab","leg_IK_L"},
	{"\xe5\xb7\xa6\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88\xef\xbc\xa9\xef\xbc\xab","toe_IK_L"},
	{"\xe5\x8f\xb3\xe8\xb6\xb3\xef\xbc\xa9\xef\xbc\xab","leg_IK_R"},
	{"\xe5\x8f\xb3\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88\xef\xbc\xa9\xef\xbc\xab","toe_IK_R"}

#if 0
	{"\xe4\xb8\x8b\xe5\x8d\x8a\xe8\xba\xab\xe5\x85\x88",""}, // Lowerbodyfirst
	{"\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab\x32",""}, // Upperbody2
	{"\xe4\xb8\xa1\xe7\x9b\xae",""}, // NieMu?
	{"\xe4\xb8\xa1\xe7\x9b\xae\xe5\x85\x88",""}, // ??
	{"\xe9\xa0\xad\xe5\x85\x88",""}, // Headfirst
	{"\xe3\x83\x98\xe3\x83\x83\xe3\x83\x89\xe5\x85\x83",""}, // ??
	{"\xe5\xb7\xa6\xe8\x82\xa9",""}, // Leftshoulder
	{"\xe5\xb7\xa6\xe8\x85\x95",""}, // Leftwrist
	{"\xe5\xb7\xa6\xe8\x85\x95\xe6\x8d\xa9",""}, // Leftwristswipe
	{"\xe5\xb7\xa6\xe3\x81\xb2\xe3\x81\x98",""}, // ??
	{"\xe5\xb7\xa6\xe6\x89\x8b\xe6\x8d\xa9",""}, // Lefthand
	{"\xe5\xb7\xa6\xe6\x89\x8b\xe9\xa6\x96",""}, // Lefthand?
	{"\xe5\xb7\xa6\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x90",""}, // Leftpinky0
	{"\xe5\xb7\xa6\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x91",""}, // Leftpinky1
	{"\xe5\xb7\xa6\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x92",""}, // Leftpinky2
	{"\xe5\xb7\xa6\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x93",""}, // Leftpinkie3
	{"\xe5\xb7\xa6\xe5\xb0\x8f\xe6\x8c\x87\xe5\x85\x88",""}, // Leftpinkyfirst
	{"\xe5\xb7\xa6\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x90",""}, // Leftdrugfinger0
	{"\xe5\xb7\xa6\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x91",""}, // Leftdrugfinger1
	{"\xe5\xb7\xa6\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x92",""}, // Leftdrugfinger2
	{"\xe5\xb7\xa6\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x93",""}, // Leftdrugfinger3
	{"\xe5\xb7\xa6\xe8\x96\xac\xe6\x8c\x87\xe5\x85\x88",""}, // Leftmedicinefingerfirst
	{"\xe5\xb7\xa6\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x90",""}, // Middleleftfinger0
	{"\xe5\xb7\xa6\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x91",""}, // Middleleftfinger1
	{"\xe5\xb7\xa6\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x92",""}, // Middleleftfinger2
	{"\xe5\xb7\xa6\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x93",""}, // Middleleftfinger3
	{"\xe5\xb7\xa6\xe4\xb8\xad\xe6\x8c\x87\xe5\x85\x88",""}, // Leftmiddlefingerfirst
	{"\xe5\xb7\xa6\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x90",""}, // Leftpersonrefersto0
	{"\xe5\xb7\xa6\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x91",""}, // Leftpersonrefersto1
	{"\xe5\xb7\xa6\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x92",""}, // Leftpersonrefersto2
	{"\xe5\xb7\xa6\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x93",""}, // Leftpersonrefersto3
	{"\xe5\xb7\xa6\xe4\xba\xba\xe6\x8c\x87\xe5\x85\x88",""}, // Lefthandpointsfirst
	{"\xe5\xb7\xa6\xe8\xa6\xaa\xe6\x8c\x87\xef\xbc\x90",""}, // Leftkiss0
	{"\xe5\xb7\xa6\xe8\xa6\xaa\xe6\x8c\x87\xef\xbc\x91",""}, // Leftkiss1
	{"\xe5\xb7\xa6\xe8\xa6\xaa\xe6\x8c\x87\xef\xbc\x92",""}, // Leftkiss2
	{"\xe5\xb7\xa6\xe8\xa6\xaa\xe6\x8c\x87\xe5\x85\x88",""}, // Leftkissfirst
	{"\xe5\x8f\xb3\xe8\x82\xa9",""}, // Rightshoulder
	{"\xe5\x8f\xb3\xe8\x85\x95",""}, // Rightwrist
	{"\xe5\x8f\xb3\xe8\x85\x95\xe6\x8d\xa9",""}, // Rightwristswipe
	{"\xe5\x8f\xb3\xe3\x81\xb2\xe3\x81\x98",""}, // ???
	{"\xe5\x8f\xb3\xe6\x89\x8b\xe6\x8d\xa9",""}, // Righthand
	{"\xe5\x8f\xb3\xe6\x89\x8b\xe9\xa6\x96",""}, // Righthand??
	{"\xe5\x8f\xb3\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x90",""}, // Rightpinky0
	{"\xe5\x8f\xb3\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x91",""}, // Rightpinky1
	{"\xe5\x8f\xb3\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x92",""}, // Rightpinky2
	{"\xe5\x8f\xb3\xe5\xb0\x8f\xe6\x8c\x87\xef\xbc\x93",""}, // Rightpinky3
	{"\xe5\x8f\xb3\xe5\xb0\x8f\xe6\x8c\x87\xe5\x85\x88",""}, // Rightpinkyfirst
	{"\xe5\x8f\xb3\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x90",""}, // Rightdrugfinger0
	{"\xe5\x8f\xb3\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x91",""}, // Rightdrugfinger1
	{"\xe5\x8f\xb3\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x92",""}, // Rightdrugfinger2
	{"\xe5\x8f\xb3\xe8\x96\xac\xe6\x8c\x87\xef\xbc\x93",""}, // Rightdrugfinger3
	{"\xe5\x8f\xb3\xe8\x96\xac\xe6\x8c\x87\xe5\x85\x88",""}, // Rightdrugfirst
	{"\xe5\x8f\xb3\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x90",""}, // Rightmiddlefinger0
	{"\xe5\x8f\xb3\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x91",""}, // Rightmiddlefinger1
	{"\xe5\x8f\xb3\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x92",""}, // Rightmiddlefinger2
	{"\xe5\x8f\xb3\xe4\xb8\xad\xe6\x8c\x87\xef\xbc\x93",""}, // Rightmiddlefinger3
	{"\xe5\x8f\xb3\xe4\xb8\xad\xe6\x8c\x87\xe5\x85\x88",""}, // Rightmiddlefingerfirst
	{"\xe5\x8f\xb3\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x90",""}, // Rightpersonrefersto0
	{"\xe5\x8f\xb3\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x91",""}, // Rightpersonrefersto1
	{"\xe5\x8f\xb3\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x92",""}, // Rightpersonrefersto2
	{"\xe5\x8f\xb3\xe4\xba\xba\xe6\x8c\x87\xef\xbc\x93",""}, // Rightpersonrefersto3
	{"\xe5\x8f\xb3\xe4\xba\xba\xe6\x8c\x87\xe5\x85\x88",""}, // Rightpersonpointsfirst
	{"\xe5\x8f\xb3\xe8\xa6\xaa\xe6\x8c\x87\xef\xbc\x90",""}, // Rightfinger0
	{"\xe5\x8f\xb3\xe8\xa6\xaa\xe6\x8c\x87\xef\xbc\x91",""}, // Rightfinger1
	{"\xe5\x8f\xb3\xe8\xa6\xaa\xe6\x8c\x87\xef\xbc\x92",""}, // Rightfinger2
	{"\xe5\x8f\xb3\xe8\xa6\xaa\xe6\x8c\x87\xe5\x85\x88",""}, // Righthandfingerfirst
	{"\xe5\xb7\xa6\xe8\x83\xb8",""}, // Leftchest
	{"\xe5\xb7\xa6\xe8\x83\xb8\xe5\x85\x88",""}, // Leftchestfirst
	{"\xe5\x8f\xb3\xe8\x83\xb8",""}, // Rightchest
	{"\xe5\x8f\xb3\xe8\x83\xb8\xe5\x85\x88",""}, // Rightchestfirst
	{"\xe5\xb7\xa6\xe8\xb6\xb3",""}, // Leftfoot
	{"\xe5\xb7\xa6\xe3\x81\xb2\xe3\x81\x96",""}, // ???
	{"\xe5\xb7\xa6\xe8\xb6\xb3\xe9\xa6\x96",""}, // Leftfoot
	{"\xe5\xb7\xa6\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88",""}, // ???
	{"\xe5\x8f\xb3\xe8\xb6\xb3",""}, // Rightfoot
	{"\xe5\x8f\xb3\xe3\x81\xb2\xe3\x81\x96",""}, // ???
	{"\xe5\x8f\xb3\xe8\xb6\xb3\xe9\xa6\x96",""}, // Rightfoot
	{"\xe5\x8f\xb3\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88",""}, // Righttoe
	{"\xe5\x8f\xb3\xe8\xb6\xb3\xef\xbc\xa9\xef\xbc\xab",""}, // RightfootIK
	{"\xe5\x8f\xb3\xe8\xb6\xb3\xef\xbc\xa9\xef\xbc\xab\xe5\x85\x88",""}, // RightfootIKdestination
	{"\xe5\xb7\xa6\xe8\xb6\xb3\xef\xbc\xa9\xef\xbc\xab",""}, // LeftfootIK
	{"\xe5\xb7\xa6\xe8\xb6\xb3\xef\xbc\xa9\xef\xbc\xab\xe5\x85\x88",""}, // LeftfootIKdestination
	{"\xe5\xb7\xa6\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88\xef\xbc\xa9\xef\xbc\xab",""}, // LefttoeIK
	{"\xe5\xb7\xa6\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88\xef\xbc\xa9\xef\xbc\xab\xe5\x85\x88",""}, // LefttoeIK?
	{"\xe5\x8f\xb3\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88\xef\xbc\xa9\xef\xbc\xab",""}, // RighttoeIK
	{"\xe5\x8f\xb3\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88\xef\xbc\xa9\xef\xbc\xab\xe5\x85\x88",""}, // RighttoeIK?
	{"\xe3\x82\xb0\xe3\x83\xab\xe3\x83\xbc\xe3\x83\x96\xe5\x85\x88",""}, // Groovedestination
	{"\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab\x32\xe5\x85\x88",""}, // Upperbody2ahead
	{"\xe5\x8f\xb3\xe3\x83\x96\xe3\x83\xa9\xe9\xa6\x96\xe7\xb4\x90\xe5\x85\x88",""}, // Rightbraneckstrap
	{"\xe5\xb7\xa6\xe3\x83\x96\xe3\x83\xa9\xe8\x83\x8c\xe4\xb8\xad\xe7\xb4\x90\xe5\x85\x88",""}, // Leftbrabackstrap
	{"\xe5\x8f\xb3\xe3\x83\x96\xe3\x83\xa9\xe8\x83\x8c\xe4\xb8\xad\xe7\xb4\x90\xe5\x85\x88",""}, // Rightbrabackstrap
	{"\xe3\x82\xb9\xe3\x82\xab\xe3\x83\xbc\xe3\x83\x88\xe3\x82\xbb\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc\xe5\x85\x88",""}, // Skirtcenterdestination
	{"\xe5\xb7\xa6\xe3\x81\xb1\xe3\x82\x93\xe3\x81\xa4\xe7\xb4\x90\xef\xbc\x91\xe5\x85\x88",""}, // Leftpantsstring1tip
	{"\xe5\xb7\xa6\xe3\x81\xb1\xe3\x82\x93\xe3\x81\xa4\xe7\xb4\x90\xef\xbc\x92\xe5\x85\x88",""}, // Leftpantsstring2tip
	{"\xe5\x8f\xb3\xe3\x81\xb1\xe3\x82\x93\xe3\x81\xa4\xe7\xb4\x90\xef\xbc\x91\xe5\x85\x88",""}, // Rightpantsstring1tip
	{"\xe5\x8f\xb3\xe3\x81\xb1\xe3\x82\x93\xe3\x81\xa4\xe7\xb4\x90\xef\xbc\x92\xe5\x85\x88",""}, // Rightpantsstring2tip
	{"\x61\x73\x73\x20\x4c",""}, // assL
	{"\x61\x73\x73\x20\x4c\x32",""}, // assL2
	{"\x61\x73\x73\x20\x52",""}, // assR
	{"\x61\x73\x73\x20\x52\x32",""}, // assR2
	{"\x4c\x4c\x65\x67\x31",""}, // LLeg1
	{"\x4c\x4c\x65\x67\x32",""}, // LLeg2
	{"\x52\x4c\x65\x67\x31",""}, // RLeg1
	{"\x52\x4c\x65\x67\x32",""}, // RLeg2
	{"\xe4\xb8\xa1\xe8\xb6\xb3\xe3\x82\xaa\xe3\x83\x95\xe3\x82\xbb",""}, // Bothfeetoff
	{"\xe5\x8f\xb3\xe8\xb6\xb3\xe3\x82\xaa\xe3\x83\x95\xe3\x82\xbb",""}, // Rightfootoff
	{"\xe5\x8f\xb3\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88\xef\xbc\xa9\xef\xbc\xab\x32",""}, // RighttoeIK2
	{"\xe5\xb7\xa6\xe8\xb6\xb3\xe3\x82\xaa\xe3\x83\x95\xe3\x82\xbb",""}, // Leftfootoff
	{"\xe5\xb7\xa6\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88\xef\xbc\xa9\xef\xbc\xab\x32",""}, // LefttoeIK2
	{"\xe3\x82\xb9\xe3\x82\xab\xe3\x83\xbc\xe3\x83\x88\x30\x30\x30\x30",""}, // Skirt0000
	{"\xe8\x85\xb0\xe3\x82\xad\xe3\x83\xa3\xe3\x83\xb3\xe3\x82\xbb\xe3\x83\xab\xe5\xb7\xa6",""}, // Waistcancelleft
	{"\xe5\xb7\xa6\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88\x5f\x64",""}, // Lefttoe_d
	{"\xe8\x85\xb0\xe3\x82\xad\xe3\x83\xa3\xe3\x83\xb3\xe3\x82\xbb\xe3\x83\xab\xe5\x8f\xb3",""}, // Waistcancelright
	{"\xe5\x8f\xb3\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88\x5f\x64",""}, // Righttoe_d
	{"\xe5\x8f\xb3\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88\xe5\x85\x88",""}, // Righttoe
	{"\xe5\xb7\xa6\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88\xe5\x85\x88",""}, // Lefttoe
	{"\xe9\xa0\xad\x20\x74",""}, // Headt
	{"\x68\x61\x69\x72\x20\x66\x72\x6f\x6e\x74\x20\x6d\x69\x64\x64\x6c\x65",""}, // hairfrontmiddle
	{"\x68\x61\x69\x72\x20\x66\x72\x6f\x6e\x74\x20\x6d\x69\x64\x64\x6c\x65\x20\x54",""}, // hairfrontmiddleT
	{"\x68\x61\x69\x72\x20\x66\x72\x6f\x6e\x74\x20\x6c\x65\x66\x74",""}, // hairfrontleft
	{"\x68\x61\x69\x72\x20\x66\x72\x6f\x6e\x74\x20\x6c\x65\x66\x74\x20\x54",""}, // hairfrontleftT
	{"\x68\x61\x69\x72\x20\x66\x72\x6f\x6e\x74\x20\x72\x69\x67\x68\x74",""}, // hairfrontright
	{"\x68\x61\x69\x72\x20\x66\x72\x6f\x6e\x74\x20\x72\x69\x67\x68\x74\x20\x54",""}, // hairfrontrightT
	{"\x68\x61\x69\x72\x20\x62\x61\x63\x6b",""}, // hairback
	{"\x68\x61\x69\x72\x20\x62\x61\x63\x6b\x20\x54",""}, // hairbackT
#endif
};
static std::string get_bone_name(const std::string &jpName,const std::optional<std::string> &name={})
{
	auto boneName = jpName;
	auto it = jpBoneNameToEnglish.find(jpName.data());
	if(it != jpBoneNameToEnglish.end())
		boneName = it->second;
	if(boneName.empty() && name.has_value())
		boneName = *name;
	return boneName;
}
bool import::import_pmx(NetworkState &nw,Model &mdl,VFilePtr f,const std::optional<std::string> &path)
{
	auto mdlData = mmd::pmx::load(f);
	if(mdlData == nullptr)
		return false;
	auto meshGroup = mdl.GetMeshGroup(0u);
	if(meshGroup == nullptr)
		return false;
	auto mesh = std::shared_ptr<ModelMesh>(nw.CreateMesh());
	meshGroup->AddMesh(mesh);

	std::string matPath = mdlData->characterName;
	ustring::to_lower(matPath);
	ustring::replace(matPath," ","_");
	ustring::replace(matPath,".","_");
	matPath = "models/mmd/" +matPath +'/';
	mdl.AddTexturePath(matPath);

	::util::Path texPath {};
	auto *fr = dynamic_cast<VFilePtrInternalReal*>(f.get());
	if(fr)
	{
		texPath = texPath.CreateFile(fr->GetPath());
		texPath.PopBack();
	}
	
	std::string addonPath = "addons/converted/";
	std::string matPathAbs = addonPath +"materials/" +matPath;
	std::unordered_map<uint32_t,uint32_t> mmdTexIdxToMdlTexIdx;
	mmdTexIdxToMdlTexIdx.reserve(mdlData->textures.size());
	auto *texGroup = mdl.GetTextureGroup(0u);
	texGroup->textures.clear();
	auto importTexture = [&nw,&addonPath,&mdl,texGroup,&texPath,&matPath,matPathAbs,&mdlData,&mmdTexIdxToMdlTexIdx](uint32_t texIdx) -> std::optional<uint32_t> {
		if(texIdx >= mdlData->textures.size())
			return {};
		auto it = mmdTexIdxToMdlTexIdx.find(texIdx);
		if(it != mmdTexIdxToMdlTexIdx.end())
			return it->second;
		auto &tex = mdlData->textures[texIdx];
		auto texFilePath = texPath.GetString() +tex;
		auto fTex = FileManager::OpenSystemFile(texFilePath.c_str(),"rb");
		if(fTex == nullptr)
			Con::cwar<<"WARNING: Unable to open texture '"<<texFilePath<<"'!"<<Con::endl;
		else
		{
			auto imgBuf = uimg::load_image(fTex);
			if(imgBuf == nullptr)
				Con::cwar<<"WARNING: Unable to load image '"<<texFilePath<<"'!"<<Con::endl;
			else
			{
				auto outTexName = ufile::get_file_from_filename(texFilePath);
				ufile::remove_extension_from_filename(outTexName);
				auto absPath = matPathAbs +outTexName;
				uimg::TextureInfo texInfo {};
				texInfo.containerFormat = uimg::TextureInfo::ContainerFormat::DDS;
				if(pragma::asset::exists(nw,matPath +outTexName,pragma::asset::Type::Texture) == false)
				{
					auto result = uimg::save_texture(absPath,*imgBuf,texInfo,false,[&matPathAbs](const std::string &errMsg) {
						Con::cwar<<"WARNING: Unable to save texture '"<<matPathAbs<<"': "<<errMsg<<Con::endl;
					},false /* absFileName */);
					Con::cout<<"Result: "<<result<<Con::endl;
				}
			}
		}
		// Load DDS


		//mdl.AddTexturePath(ufile::get_path_from_filename(tex));
		auto texName = ufile::get_file_from_filename(tex);
		ufile::remove_extension_from_filename(texName);
		auto localMatPath = matPath +texName;

		auto settings = ds::create_data_settings({});
		auto root = std::make_shared<ds::Block>(*settings);
		root->AddValue("texture",Material::ALBEDO_MAP_IDENTIFIER,localMatPath);

		auto &matManager = nw.GetMaterialManager();
		auto *mat = matManager.CreateMaterial(localMatPath,"pbr",root);
		mat->Save(mat->GetName(),addonPath);

		auto mdlTexIdx = import::util::add_texture(nw,mdl,texName,texGroup,true);
		mmdTexIdxToMdlTexIdx[texIdx] = mdlTexIdx;
		return mdlTexIdx;
	};
	auto faceOffset = 0ull;
	std::unordered_map<int32_t,std::shared_ptr<ModelSubMesh>> subMeshes;
	for(auto &mat : mdlData->materials)
	{
		if(mat.textureIndex == -1)
			continue;
		auto it = subMeshes.find(mat.textureIndex);
		if(it == subMeshes.end())
			it = subMeshes.insert(std::make_pair(mat.textureIndex,std::shared_ptr<ModelSubMesh>(nw.CreateSubMesh()))).first;
		auto &subMesh = *it->second;
		auto texIdx = importTexture(mat.textureIndex);
		if(texIdx.has_value())
			subMesh.SetSkinTextureIndex(*texIdx);
		auto &verts = subMesh.GetVertices();
		auto &vertWeights = subMesh.GetVertexWeights();
		auto &tris = subMesh.GetTriangles();

		std::unordered_map<uint16_t,uint16_t> faceTranslationTable;
		tris.reserve(mat.faceCount);
		for(auto i=faceOffset;i<faceOffset +mat.faceCount;++i)
		{
			auto idx = mdlData->faces.at(i);
			auto it = faceTranslationTable.find(idx);
			if(it == faceTranslationTable.end())
			{
				if(verts.size() == verts.capacity())
				{
					verts.reserve(verts.size() +500);
					vertWeights.reserve(verts.capacity());
				}
				verts.push_back({});
				vertWeights.push_back({});
				auto &vMesh = verts.back();
				auto &vw = vertWeights.back();
				auto &v = mdlData->vertices.at(idx);
				vMesh.position = {v.position.at(0),v.position.at(1),v.position.at(2)};
				vMesh.normal = {v.normal.at(0),v.normal.at(1),v.normal.at(2)};
				vMesh.uv = {v.uv.at(0),v.uv.at(1)};

				vw.boneIds = {v.boneIds.at(0),v.boneIds.at(1),v.boneIds.at(2),v.boneIds.at(3)};
				vw.weights = {v.boneWeights.at(0),v.boneWeights.at(1),v.boneWeights.at(2),v.boneWeights.at(3)};

				it = faceTranslationTable.insert(std::make_pair(idx,verts.size() -1)).first;
			}
			tris.push_back(it->second);
		}
		faceOffset += mat.faceCount;

		mesh->AddSubMesh(it->second);
	}

	auto &skeleton = mdl.GetSkeleton();
	auto &bones = skeleton.GetBones();
	bones.clear();
	for(auto &bone : mdlData->bones)
	{
		auto boneName = get_bone_name(bone.nameJp,bone.name);
		auto &jpName = bone.nameJp;
		auto it = jpBoneNameToEnglish.find(jpName.data());
		if(it != jpBoneNameToEnglish.end())
		{
			std::string hexName;
			auto l = jpName.length();
			for(auto i=decltype(l){0u};i<l;i+=2)
			{
				auto v = umath::swap_endian(*reinterpret_cast<int16_t*>(jpName.data() +i));

				std::stringstream stream;
				stream<<std::hex<<v;
				auto hex = stream.str();
				if(i +1 == l)
				{
					hex.pop_back();
					hex.pop_back();
				}

				if(hexName.empty() == false)
					hexName += ' ';
				hexName += hex;
			}
			std::cout<<"Unknown japanese bone name: "<<hexName<<" (UTF8)"<<std::endl;
		}

		auto *mdlBone = new Bone();
		mdlBone->name = boneName;
		auto boneId = skeleton.AddBone(mdlBone);
		if(mdlBone->name.empty())
			mdlBone->name = "bone" +std::to_string(boneId);
	}
	auto &rootBones = skeleton.GetRootBones();
	rootBones.clear();
	auto &reference = mdl.GetReference();
	reference.SetBoneCount(mdlData->bones.size());
	auto boneId = 0u;
	for(auto &bone : mdlData->bones)
	{
		auto parentId = bone.parentBoneIdx;
		if(parentId != -1)
		{
			auto &parent = bones.at(parentId);
			auto &child = bones.at(boneId);
			parent->children.insert(std::make_pair(boneId,child));
			child->parent = parent;
		}
		else
			rootBones.insert(std::make_pair(boneId,bones.at(boneId)));
		reference.SetBonePosition(boneId,Vector3(bone.position.at(0),bone.position.at(1),bone.position.at(2)));
		reference.SetBoneOrientation(boneId,uquat::identity());
		++boneId;
	}
	auto frame = Frame::Create(reference);
	auto anim = Animation::Create();
	anim->AddFrame(frame);
	mdl.AddAnimation("reference",anim);

	auto numBones = mdlData->bones.size();
	std::vector<uint32_t> boneList(numBones);
	for(auto i=decltype(numBones){0};i<numBones;++i)
		boneList.at(i) = i;
	anim->SetBoneList(boneList);
	anim->Localize(skeleton);

	/*const char kLegName[4 + 1] = {0x89, 0x45, 0x91, 0xAB, 0x00}; // +1 for \0
	for(auto &bone : mdlData->bones)
	{
		if(memcmp(kLegName,bone.nameJp.data(),4) == 0)
			std::cout<<"Found leg!"<<std::endl;
	}*/

	mdl.GenerateBindPoseMatrices();
	mdl.Update(ModelUpdateFlags::All);
	return true;
}
int import::import_pmx(lua_State *l)
{
	auto &f = *Lua::CheckFile(l,1);
	auto &mdl = Lua::Check<Model>(l,2);
	auto fptr = f.GetHandle();
	auto r = import_pmx(*engine->GetNetworkState(l),mdl,fptr);
	Lua::PushBool(l,r);
	return 1;
}

/*std::string sjis2utf8(const std::string& sjis) {
	std::string utf8_string;

	LPCCH pSJIS = (LPCCH)sjis.c_str();
	int utf16size = ::MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, pSJIS, -1, 0, 0);
	if (utf16size != 0) {
		LPWSTR pUTF16 = new WCHAR[utf16size];
		if (::MultiByteToWideChar(CP_ACP, 0, (LPCCH)pSJIS, -1, pUTF16, utf16size) != 0) {
			int utf8size = ::WideCharToMultiByte(CP_UTF8, 0, pUTF16, -1, 0, 0, 0, 0);
			if (utf8size != 0) {
				LPTSTR pUTF8 = new TCHAR[utf8size + 16];
				ZeroMemory(pUTF8, utf8size + 16);
				if (::WideCharToMultiByte(CP_UTF8, 0, pUTF16, -1, pUTF8, utf8size, 0, 0) != 0) {
					utf8_string = std::string(pUTF8);
				}
				delete pUTF8;
			}
		}
		delete pUTF16;
	}
	return utf8_string;
}*/

static std::string sj2utf8(const std::vector<uint8_t> &convTable,const std::string &input)
{
	// See https://stackoverflow.com/a/33170901/2482983
    std::string output(3 * input.length(), ' '); //ShiftJis won't give 4byte UTF8, so max. 3 byte per input char are needed
    size_t indexInput = 0, indexOutput = 0;

    while(indexInput < input.length())
    {
        char arraySection = ((uint8_t)input[indexInput]) >> 4;

        size_t arrayOffset;
        if(arraySection == 0x8) arrayOffset = 0x100; //these are two-byte shiftjis
        else if(arraySection == 0x9) arrayOffset = 0x1100;
        else if(arraySection == 0xE) arrayOffset = 0x2100;
        else arrayOffset = 0; //this is one byte shiftjis

        //determining real array offset
        if(arrayOffset)
        {
            arrayOffset += (((uint8_t)input[indexInput]) & 0xf) << 8;
            indexInput++;
            if(indexInput >= input.length()) break;
        }
        arrayOffset += (uint8_t)input[indexInput++];
        arrayOffset <<= 1;

        //unicode number is...
        uint16_t unicodeValue = (convTable[arrayOffset] << 8) | convTable[arrayOffset + 1];

        //converting to UTF8
        if(unicodeValue < 0x80)
        {
            output[indexOutput++] = unicodeValue;
        }
        else if(unicodeValue < 0x800)
        {
            output[indexOutput++] = 0xC0 | (unicodeValue >> 6);
            output[indexOutput++] = 0x80 | (unicodeValue & 0x3f);
        }
        else
        {
            output[indexOutput++] = 0xE0 | (unicodeValue >> 12);
            output[indexOutput++] = 0x80 | ((unicodeValue & 0xfff) >> 6);
            output[indexOutput++] = 0x80 | (unicodeValue & 0x3f);
        }
    }

    output.resize(indexOutput); //remove the unnecessary bytes
    return output;
}

int import::import_vmd(lua_State *l)
{
	auto &f = *Lua::CheckFile(l,1);
	//auto &mdl = Lua::Check<Model>(l,2);
	auto fptr = f.GetHandle();
	auto animData = mmd::vmd::load(fptr);
	if(animData == nullptr)
		return 0;
	auto t = luabind::newtable(l);
	t["modelName"] = animData->modelName;
	auto tKeyframes = luabind::newtable(l);
	auto tMorphs = luabind::newtable(l);
	auto tCameras = luabind::newtable(l);
	auto tLights = luabind::newtable(l);
	t["keyframes"] = tKeyframes;
	t["morphs"] = tMorphs;
	t["cameras"] = tCameras;
	t["lights"] = tLights;
	auto strlen = [](const auto &name) {
		auto alen = name.size();
		uint32_t len = 0;
		for(auto i=decltype(alen){0u};i<alen;++i)
		{
			if(name[i] == '\0')
				break;
			++len;
		}
		return len;
	};

	static std::vector<uint8_t> convTable;
	if(convTable.empty())
	{
		auto shiftJisPath = ::util::get_program_path() +"/modules/mount_external/shiftjis.dat";
		auto f = FileManager::OpenSystemFile(shiftJisPath.c_str(),"rb");
		if(f)
		{
			auto len = f->GetSize();
			convTable.resize(len);
			f->Read(convTable.data(),len);
			f = nullptr;
		}
	}

	int32_t idx = 1;
	for(auto &kf : animData->keyframes)
	{
		auto nameJp = sj2utf8(convTable,std::string{kf.boneName.data(),strlen(kf.boneName)});
		auto boneName = get_bone_name(nameJp);

		auto t = luabind::newtable(l);
		t["boneName"] = boneName;
		t["frameIndex"] = kf.frameIndex;
		t["position"] = Vector3{kf.position[0],kf.position[1],kf.position[2]};
		t["rotation"] = Quat{kf.rotation[0],kf.rotation[1],kf.rotation[2],kf.rotation[3]};
		tKeyframes[idx++] = t;
	}
	idx = 1;
	for(auto &morph : animData->morphs)
	{
		auto nameJp = sj2utf8(convTable,std::string{morph.morphName.data(),strlen(morph.morphName)});
		auto t = luabind::newtable(l);
		t["morphName"] = get_morph_name(nameJp);
		t["frameIndex"] = morph.frameIndex;
		t["weight"] = morph.weight;
		tMorphs[idx++] = t;
	}
	// TODO: Cameras, lights
	t.push(l);
	return 1;
}
#pragma optimize("",on)
