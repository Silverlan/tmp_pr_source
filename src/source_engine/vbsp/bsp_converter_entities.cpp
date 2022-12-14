#include <iostream>
#include "source_engine/vbsp/bsp_converter.hpp"
#include "source_engine/source_engine.hpp"
#include <pragma/entities/entity_component_manager.hpp>
#include <pragma/entities/prop/prop_base.h>
#include <pragma/entities/environment/lights/env_light.h>
#include <pragma/asset_types/world.hpp>
#include <sharedutils/util.h>
#include <sharedutils/util_file.h>
#include <util_fgd.hpp>

const auto LIGHT_SOURCE_FLAGS = /*umath::to_integral(pragma::BaseEnvLightComponent::SpawnFlag::DontCastShadows) | */
	umath::to_integral(pragma::BaseToggleComponent::SpawnFlags::StartOn) | umath::to_integral(pragma::BaseEnvLightComponent::SpawnFlag::DontCastShadows);

static std::string invert_x_coordinate(std::string str)
{
	if(str.empty())
		return str;
	if(str[0] != '-')
		str = "-" +str;
	else
		str = str.substr(1,str.length());
	return str;
}

// Attempts to convert the source engine attentuation values to an approximately similar falloff exponent
static float calc_falloff_exponent(float quadraticAtt,float linearAtt,float constAtt)
{
	// These values have been determined by experimentation to create a relatively close match to the original source lighting for
	// most common cases. A precise conversion is not possible.
	constexpr float falloffExpForQuadraticAtt = 1.5f;
	constexpr float falloffExpForLinearAtt = 0.95f;
	constexpr float falloffExpForConstantAtt = 0.7f;
	auto sum = quadraticAtt +linearAtt +constAtt;
	if(sum == 0.f)
		return 0.f; // TODO: What does source do for this case?
	quadraticAtt /= sum;
	linearAtt /= sum;
	constAtt /= sum;
	return falloffExpForQuadraticAtt *quadraticAtt +falloffExpForLinearAtt *linearAtt +falloffExpForConstantAtt *constAtt;
}

void source_engine::translate_class(
	const std::unordered_map<std::string,std::string> &inKeyValues,
	std::unordered_map<std::string,std::string> &outKeyValues,
	std::string &className,bool isSource2
)
{
	const auto fGetKeyValue = [&inKeyValues](const std::string &key) -> const std::string& {
		auto it = std::find_if(inKeyValues.begin(),inKeyValues.end(),[&key](const std::pair<std::string,std::string> &pair) {
			return ustring::compare(key,pair.first,false);
			});
		if(it == inKeyValues.end())
		{
			static std::string r = "";
			return r;
		}
		return it->second;
	};
	const auto fGetLightColor = [&fGetKeyValue](const std::string &lightKey,const std::string &lightHDRKey,bool &bHdr,bool isEnvLight) -> Vector4 {
		auto &light = fGetKeyValue(lightKey);
		auto &lightHdr = fGetKeyValue(lightHDRKey);

		Vector4 lightColor {};
		if(light.empty() == false)
		{
			std::vector<std::string> vdat;
			ustring::explode(light,ustring::WHITESPACE.c_str(),vdat);
			lightColor.r = (vdat.size() > 0) ? util::to_float(vdat.at(0)) : 0.f;
			lightColor.g = (vdat.size() > 1) ? util::to_float(vdat.at(1)) : 0.f;
			lightColor.b = (vdat.size() > 2) ? util::to_float(vdat.at(2)) : 0.f;
			lightColor.a = (vdat.size() > 3) ? util::to_float(vdat.at(3)) : 0.f;
		}
		bHdr = (lightHdr.empty() == false);
		if(bHdr == true)
		{
			Vector4 lightColorHdr {};
			std::vector<std::string> vdat;
			ustring::explode(lightHdr,ustring::WHITESPACE.c_str(),vdat);
			lightColorHdr.r = (vdat.size() > 0) ? util::to_float(vdat.at(0)) : 0.f;
			lightColorHdr.g = (vdat.size() > 1) ? util::to_float(vdat.at(1)) : 0.f;
			lightColorHdr.b = (vdat.size() > 2) ? util::to_float(vdat.at(2)) : 0.f;
			lightColorHdr.a = (vdat.size() > 3) ? util::to_float(vdat.at(3)) : 0.f;
			for(uint8_t i=0;i<4;++i)
			{
				if(lightColorHdr[i] == -1.f || (i == 3 && lightColorHdr[i] == 1.f))
					continue;
				lightColor[i] = lightColorHdr[i];
			}
			if(isEnvLight)
				lightColor *= 0.02f;
			else
			{
				// Note: Produces good results on gm_construct, but has yet to be confirmed if it works
				// well in general
				lightColor *= 0.25;
			}

			//lightColor *= 1.6f;
		}
		else
		{
			lightColor *= 0.02f *15;
			if(isEnvLight)
				lightColor *= 0.1f;
		}
		auto rgbMax = umath::max(lightColor.x,lightColor.y,lightColor.z);
		if(rgbMax > 255.f)
		{
			// Map to range [0,255]
			for(uint8_t i=0;i<3;++i)
				lightColor[i] = lightColor[i] /rgbMax *255.f;
			lightColor.w *= (rgbMax /255.f);
		}
		return lightColor;
	};
	const auto fGetMaxDistance = [&fGetKeyValue](float lightIntensity) -> float {
		// Source: https://developer.valvesoftware.com/wiki/Constant-Linear-Quadratic_Falloff
		auto maxDistance = util::to_float(fGetKeyValue("_distance"));
		if(maxDistance == 0.f)
		{
			maxDistance = util::to_float(fGetKeyValue("_zero_percent_distance"));
			if(maxDistance == 0.f)
			{
				// Just an approximation
				maxDistance = umath::sqrt(lightIntensity) *113.f;
			}
		}
		return maxDistance;
	};

	auto itOrigin = inKeyValues.find("origin");
	Vector3 origin {};
	Vector3 angles {};
	if(itOrigin != inKeyValues.end())
		origin = uvec::create(itOrigin->second);
	auto itAngles = inKeyValues.find("angles");
	if(itAngles != inKeyValues.end())
		angles = uvec::create(itAngles->second);

	auto bUseOrigin = itOrigin != inKeyValues.end();
	auto bUseAngles = itAngles != inKeyValues.end();
	// Translate entities
	if(ustring::compare<std::string>(className,"prop_physics_multiplayer",false) || ustring::compare<std::string>(className,"prop_static",false) || ustring::compare<std::string>(className,"prop_physics",false))
	{
		// TODO: Also see staticPropLumps for static props!
		auto bStatic = ustring::compare<std::string>(className,"prop_static",false);
		className = "prop_physics";
		outKeyValues.insert(std::make_pair("maxvisibledist",fGetKeyValue("fademaxdist")));
		outKeyValues.insert(std::make_pair("model",fGetKeyValue("model")));
		outKeyValues.insert(std::make_pair("skin",fGetKeyValue("skin")));
		outKeyValues.insert(std::make_pair("disableshadows",fGetKeyValue("disableshadows")));
		auto flags = 0u;
		if(bStatic == true)
			flags |= umath::to_integral(pragma::BasePropComponent::SpawnFlags::Static);
		auto solid = fGetKeyValue("solid");
		if(solid.empty() == false && util::to_int(solid) == 0u)
			flags |= umath::to_integral(pragma::BasePropComponent::SpawnFlags::DisableCollisions);
		outKeyValues.insert(std::make_pair("spawnflags",std::to_string(flags)));
	}
	else if(ustring::compare<std::string>(className,"prop_dynamic_override",false) || ustring::compare<std::string>(className,"prop_dynamic",false))
	{
		className = "prop_dynamic";
		outKeyValues.insert(std::make_pair("maxvisibledist",fGetKeyValue("fademaxdist")));
		outKeyValues.insert(std::make_pair("model",fGetKeyValue("model")));
		outKeyValues.insert(std::make_pair("skin",fGetKeyValue("skin")));
		outKeyValues.insert(std::make_pair("disableshadows",fGetKeyValue("disableshadows")));
		auto flags = 0u;
		auto solid = fGetKeyValue("solid");
		if(solid.empty() == false && util::to_int(solid) == 0u)
			flags |= umath::to_integral(pragma::BasePropComponent::SpawnFlags::DisableCollisions);
		outKeyValues.insert(std::make_pair("spawnflags",std::to_string(flags)));
	}
	else if(ustring::compare<std::string>(className,"trigger_gravity"))
	{

	}
	else if(ustring::compare<std::string>(className,"light"))
	{
		className = "env_light_point";
		outKeyValues.insert(std::make_pair("spawnflags",std::to_string(LIGHT_SOURCE_FLAGS)));

		auto bHdr = false;
		auto lightColor = fGetLightColor("_light","_lightHDR",bHdr,false);
		auto lightIntensity = lightColor[3];
		outKeyValues.insert(std::make_pair("lightcolor",std::to_string(lightColor[0]) +" " +std::to_string(lightColor[1]) +" " +std::to_string(lightColor[2])));
		outKeyValues.insert(std::make_pair("light_intensity",std::to_string(lightIntensity)));
		outKeyValues.insert(std::make_pair("light_intensity_type",std::to_string(umath::to_integral(pragma::BaseEnvLightComponent::LightIntensityType::Candela))));
		outKeyValues.insert(std::make_pair("radius",std::to_string(fGetMaxDistance(lightIntensity))));
		outKeyValues.insert(std::make_pair("falloff_exponent",std::to_string(calc_falloff_exponent(
			util::to_float(fGetKeyValue("_quadratic_attn")),util::to_float(fGetKeyValue("_linear_attn")),util::to_float(fGetKeyValue("_constant_attn"))
		))));
		outKeyValues.insert(std::make_pair("light_flags","1")); // Baked light source
	}
	else if(ustring::compare<std::string>(className,"light_spot"))
	{
		className = "env_light_spot";
		outKeyValues.insert(std::make_pair("spawnflags",std::to_string(LIGHT_SOURCE_FLAGS)));

		auto outerAngle = util::to_float(fGetKeyValue("_cone"));
		auto innerAngle = util::to_float(fGetKeyValue("_inner_cone"));
		auto blendFraction = (outerAngle > 0.f) ? (innerAngle /outerAngle) : 0.f;
		blendFraction = umath::clamp(blendFraction,0.f,1.f);
		outerAngle *= 2.f; // Angle is half-angle in source, but not in Pragma
		outKeyValues.insert(std::make_pair("outerconeangle",std::to_string(outerAngle)));
		outKeyValues.insert(std::make_pair("blendfraction",std::to_string(blendFraction)));

		auto bHdr = false;
		auto lightColor = fGetLightColor("_light","_lightHDR",bHdr,false);
		auto lightIntensity = lightColor[3];
		outKeyValues.insert(std::make_pair("lightcolor",std::to_string(lightColor[0]) +" " +std::to_string(lightColor[1]) +" " +std::to_string(lightColor[2])));
		outKeyValues.insert(std::make_pair("light_intensity",std::to_string(lightIntensity)));
		outKeyValues.insert(std::make_pair("light_intensity_type",std::to_string(umath::to_integral(pragma::BaseEnvLightComponent::LightIntensityType::Candela))));
		outKeyValues.insert(std::make_pair("radius",std::to_string(fGetMaxDistance(lightIntensity))));
		outKeyValues.insert(std::make_pair("falloff_exponent",std::to_string(calc_falloff_exponent(
			util::to_float(fGetKeyValue("_quadratic_attn")),util::to_float(fGetKeyValue("_linear_attn")),util::to_float(fGetKeyValue("_constant_attn"))
		))));
		outKeyValues.insert(std::make_pair("light_flags","1")); // Baked light source

		auto &pitch = fGetKeyValue("pitch");
		if(pitch.empty() == false)
		{
			angles.x = umath::normalize_angle(-util::to_float(pitch));
			bUseAngles = true;
		}
	}
	else if(ustring::compare<std::string>(className,"light_environment"))
	{
		/*static auto count = 0;
		if(++count > 1)
		{
		outEntities.erase(outEntities.end() -1);
		continue;
		}*/
		className = "env_light_environment";

		auto bHdr = false;
		auto lightColor = fGetLightColor("_light","_lightHDR",bHdr,true);
		auto lightIntensity = lightColor[3];
		outKeyValues.insert(std::make_pair("lightcolor",std::to_string(lightColor[0]) +" " +std::to_string(lightColor[1]) +" " +std::to_string(lightColor[2])));
		outKeyValues.insert(std::make_pair("light_intensity",std::to_string(lightIntensity)));
		outKeyValues.insert(std::make_pair("light_intensity_type",std::to_string(umath::to_integral(pragma::BaseEnvLightComponent::LightIntensityType::Lux))));

		bHdr = false;
		auto ambientColor = fGetLightColor("_ambient","_ambientHDR",bHdr,false);
		if(bHdr == true)
			ambientColor *= 0.5f;
		else
			ambientColor *= 2.f;
		outKeyValues.insert(std::make_pair("color_ambient",std::to_string(ambientColor[0]) +" " +std::to_string(ambientColor[1]) +" " +std::to_string(ambientColor[2]) +" " +std::to_string(ambientColor[3])));
		outKeyValues.insert(std::make_pair("spawnflags","1024"));
		outKeyValues.insert(std::make_pair("light_flags","1")); // Baked light source

		auto &pitch = fGetKeyValue("pitch");
		if(pitch.empty() == false)
		{
			angles.x = umath::normalize_angle(-util::to_float(pitch));
			bUseAngles = true;
		}
	}
	/*else if(ustring::compare<std::string>(className,"ambient_generic"))
	{
	className = "env_sound";
	outKeyValues.insert(std::make_pair("gain",std::to_string(util::to_float(fGetKeyValue("health")) /10.f)));
	outKeyValues.insert(std::make_pair("pitch",std::to_string(util::to_float(fGetKeyValue("pitch")) /100.f)));
	outKeyValues.insert(std::make_pair("max_dist",fGetKeyValue("radius")));
	auto sourceSpawnFlags = util::to_int(fGetKeyValue("spawnflags"));
	auto spawnFlags = pragma::BaseEnvSoundComponent::SpawnFlags::None;
	if(sourceSpawnFlags &1)
	spawnFlags |= pragma::BaseEnvSoundComponent::SpawnFlags::PlayEverywhere;
	outKeyValues.insert(std::make_pair("spawnflags",std::to_string(umath::to_integral(spawnFlags))));
	}*/
	else if(ustring::compare<std::string>(className,"sky_camera",false) == true)
	{
		if(ustring::compare<std::string>(className,"sky_camera"))
			outKeyValues.insert(std::make_pair("wv_hint_clientsideonly","1"));
		outKeyValues.insert(std::make_pair("skybox_scale",fGetKeyValue("scale")));
	}
	else // Just use it unchanged
	{
		if(ustring::compare<std::string>(className,"env_cubemap") || ustring::compare<std::string>(className,"util_cubemap"))
		{
			className = "env_reflection_probe";
			outKeyValues.insert(std::make_pair("wv_hint_clientsideonly","1"));
		}

		bUseOrigin = false;
		bUseAngles = false;
		for(auto &pair : inKeyValues)
		{
			if(pair.first.empty())
				continue;
			outKeyValues.insert(std::make_pair(pair.first,pair.second));
		}
	}

	if(ustring::compare<std::string>(className,"env_fog_controller"))
	{
		auto itSpawnFlags = inKeyValues.find("spawnflags");
		if(itSpawnFlags != inKeyValues.end())
		{
			auto spawnFlags = util::to_int(itSpawnFlags->second);
			if((spawnFlags &1024) != 0)
				outKeyValues["spawnflags"] = "1024"; // Start on
		}
		else
			outKeyValues["spawnflags"] = "1024"; // Start on
	}

	if(bUseOrigin)
		outKeyValues.insert(std::make_pair("origin",std::to_string(origin.x) +" " +std::to_string(origin.y) +" " +std::to_string(origin.z)));
	if(bUseAngles)
		outKeyValues.insert(std::make_pair("angles",std::to_string(angles.x) +" " +std::to_string(angles.y) +" " +std::to_string(angles.z)));
	
	auto itName = inKeyValues.find("targetname");
	if(itName != inKeyValues.end() && itName->second.empty() == false)
		outKeyValues["name"] = itName->second;
	auto itMdl = outKeyValues.find("model");
	if(itMdl != outKeyValues.end())
	{
		ufile::remove_extension_from_filename(itMdl->second,std::array<std::string,3>{"mdl","vmdl_c","vmdl"});
		if(isSource2)
		{
			// Hotfix: FGD information for Source 2 entities is unreliable (I don't know if Source 2 uses FGDs),
			// so we can't determine accurately if something is a model path. We'll just assume that the "models"
			// keyvalue is always a model path.
			util::Path path{itMdl->second};
			if(path.GetFront() == "models")
				path.PopFront();
			itMdl->second = path.GetString();
		}
	}
}

static void transform_angles(std::string &val)
{
	val = invert_x_coordinate(val);
	std::vector<std::string> vdat;
	ustring::explode(val,ustring::WHITESPACE.c_str(),vdat);
	if(vdat.size() > 0)
	{
		float f = static_cast<float>(-atof(vdat[0].c_str()));
		vdat[0] = std::to_string(f);
	}
	if(vdat.size() > 1)
	{
		float f = static_cast<float>(atof(vdat[1].c_str()));
		//f += 90.f;
		vdat[1] = std::to_string(f);
		val = "";
		for(char i=0;i<vdat.size();i++)
		{
			if(i > 0)
				val += " ";
			val += vdat[i];
		}
	}
}

static std::string swap_yz_coordinates(std::string str)
{
	std::vector<std::string> vdat;
	ustring::explode(str,ustring::WHITESPACE.c_str(),vdat);
	size_t l = vdat.size();
	if(l <= 1)
		return str;
	if(l >= 2)
	{
		if(vdat[1][0] != '-')
			vdat[1] = "-" +vdat[1];
		else
			vdat[1] = vdat[1].substr(1,vdat[1].length());
	}
	if(l == 2)
		return vdat[0] +" 0 " +vdat[1];
	return vdat[0] +" " +vdat[2] +" "+vdat[1];
}

inline void transform_origin(std::string &val,bool isSource2)
{
	if(isSource2)
	{
		auto v = uvec::create(val);
		v = {v.x,v.z,-v.y};
		val = std::to_string(v.x) +' ' +std::to_string(v.y) +' ' +std::to_string(v.z);
		return;
	}
	val = swap_yz_coordinates(val);

	auto vec = uvec::create(val);
	uvec::rotate(&vec,uquat::create(EulerAngles{0.f,-90.f,0.f}));
	val = std::to_string(vec.x) +' ' +std::to_string(vec.y) +' ' +std::to_string(vec.z);
}

static void translate_model_path(std::string &mdlPath)
{
	size_t br = mdlPath.find_first_of("/\\");
	if(br != size_t(-1))
	{
		std::string path = mdlPath.substr(0,br);
		StringToLower(path);
		if(path == "models")
			mdlPath = mdlPath.substr(br +1,mdlPath.length());
	}
	size_t pext = mdlPath.find_last_of('.');
	if(pext != size_t(-1))
		mdlPath = mdlPath.substr(0,pext);
}

void source_engine::translate_key_value(
	const std::vector<util::fgd::Data> &fgdData,
	const std::string &className,const std::string &key,std::string &val,bool isSource2,
	const std::function<void(const std::string&,uint8_t)> &messageLogger,
	std::unordered_set<std::string> *msgCache
)
{
	auto bClassFound = false;
	auto bKeyFound = false;
	for(auto &data : fgdData)
	{
		auto itClass = data.classDefinitions.find(className);
		if(itClass != data.classDefinitions.end())
		{
			bClassFound = true;
			auto &classDef = itClass->second;
			auto *pKeyValue = classDef->FindKeyValue(data,key);
			if(pKeyValue != nullptr)
			{
				bKeyFound = true;
				switch(pKeyValue->GetType())
				{
				case util::fgd::KeyValue::Type::Angle:
				{
					transform_angles(val);
					break;
				}
				case util::fgd::KeyValue::Type::Axis:
				case util::fgd::KeyValue::Type::VecLine:
				case util::fgd::KeyValue::Type::Vector:
				case util::fgd::KeyValue::Type::Origin:
					transform_origin(val,isSource2);
					break;
				case util::fgd::KeyValue::Type::Studio:
				{
					translate_model_path(val);
					break;
				}
				}
				break;
			}
		}
	}
	if(bClassFound == true && bKeyFound == false && key == "origin")
	{
		// For some reason some entities don't have the "origin" keyvalue specified even though they have it (sprites?),
		// so we'll handle it as a special case here.
		transform_origin(val,isSource2);
		return;
	}
	if(isSource2 && bKeyFound == false)
	{
		// TODO: Does Source 2 have FGD equivalents?
		if(key == "model")
			translate_model_path(val);
	}
	if(messageLogger != nullptr && key != "id" && key != "classname")
	{
		if(bClassFound == false)
		{
			if(className != "world")
			{
				auto cacheName = "kv_missing_fgd_class_" +className;
				if(msgCache == nullptr || msgCache->find(cacheName) == msgCache->end())
				{
					if(msgCache)
						msgCache->insert(cacheName);
					messageLogger("\tWARNING: Entity class '" +className +"' definition not found in any FGD file! This may cause issues.",0u);
				}
			}
		}
		else if(bKeyFound == false)
		{
			auto cacheName = "kv_missing_fgd_keyvalue_" +className +'_' +key;
			if(msgCache == nullptr || msgCache->find(cacheName) == msgCache->end())
			{
				if(msgCache)
					msgCache->insert(cacheName);
				messageLogger("\tWARNING: Keyvalue '" +key +"' definition for entity class '" +className +"' not found in any FGD file! This may cause issues.",1u);
			}
		}
	}
}

std::vector<util::fgd::Data> source_engine::load_fgds(class NetworkState &nwState,const std::function<void(const std::string&)> &messageLogger)
{
	// Some entity keyvalues (such as origin and angles) need to be transformed for pragma (because pragma uses a different coordinate system).
	// To know which keyvalues need to be transformed, we need the keyvalue's type, for which we need to load the FGD-file of the game the VMF came from.
	// Since we can't know the game, we'll load all available FGD files and search for the keyvalues in each of them.
	const std::array<std::string,17> fgds = {
		"pragma.fgd",
		"AoC.fgd",
		"DipRip.fgd",
		"GarrysMod.fgd",
		"SmashBall.fgd",
		"Synergy.fgd",
		"Synergy_base.fgd",
		"ZombiePanic.fgd",
		"cstrike.fgd",
		"dod.fgd",
		"halflife2.fgd",
		"hl2mp.fgd",
		"portal.fgd",
		"sdk.fgd",
		"tf.fgd",
		"portal2.fgd",
		"halflife.fgd"
	};
	std::unordered_map<std::string,util::fgd::Data> fgdData = {}; // FGD data stored in a map to make sure FGD files aren't being loaded more than once (e.g. via includes)
	fgdData.reserve(fgds.size());
	for(auto &fgd : fgds)
	{
		auto fgData = util::fgd::load_fgd(fgd,[&nwState,&messageLogger](const std::string &fileName) -> VFilePtr {
			auto fname = fileName;
			const std::string fgdPath = "modules/mount_external/fgd/";
			auto f = FileManager::OpenFile((fgdPath +fname).c_str(),"r");
			if(f == nullptr)
			{
				if(ustring::compare<std::string>(fileName,"halflife.fgd",false) == true)
					fname = "hl1/" +fname;
				else
					fname = "bin/" +fname;
				f = FileManager::OpenFile(fname.c_str(),"r");
			}
			if(f == nullptr && util::port_file(&nwState,fname,fgdPath) == true)
				f = FileManager::OpenFile((fgdPath +fname).c_str(),"r");
			if(f != nullptr && messageLogger != nullptr)
				messageLogger("Loading FGD '" +fileName +"'...");
			// TODO: Import file (external resources)
			return f;
			},fgdData);
	}

	std::vector<util::fgd::Data> fgdDataList {};
	fgdDataList.reserve(fgdData.size());
	for(auto &pair : fgdData)
		fgdDataList.push_back(pair.second);
	return fgdDataList;
}

void source_engine::find_entity_components(const std::unordered_map<std::string,std::string> &keyValues,std::unordered_set<std::string> &outComponents)
{
	// TODO: Check inputs, outputs for SetName, SetParent, etc.
	for(auto itKeyValue=keyValues.begin();itKeyValue!=keyValues.end();itKeyValue++)
	{
		if(itKeyValue->second.empty())
			continue;
		std::string key = itKeyValue->first;
		StringToLower(key);

		if(key == "targetname")
			outComponents.insert("name");
		else if(key == "parentname")
			outComponents.insert("attachable");
		else if(key == "color")
			outComponents.insert("color");
		else if(key == "angles" || key == "origin")
			outComponents.insert("transform");
		else if(key == "globalname")
			outComponents.insert("global");
	}
}

void source_engine::translate_entity_data(
	pragma::asset::WorldData &worldData,const std::vector<util::fgd::Data> &fgdData,bool isSource2,
	const std::function<void(const std::string&)> &messageLogger,
	std::unordered_set<std::string> *optMsgCache
)
{
	auto &nw = worldData.GetNetworkState();
	auto &ents = worldData.GetEntities();
	auto findByClass = [&ents](const std::string &className) {
		return std::find_if(ents.begin(),ents.end(),[&className](const std::shared_ptr<pragma::asset::EntityData> &entData) -> bool {
			return ustring::compare(entData->GetClassName(),className,false);
		});
	};
	auto itSkyCamera = findByClass("sky_camera");
	if(itSkyCamera != ents.end() && findByClass("env_fog_controller") == ents.end())
	{
		auto &entSkyCamera = **itSkyCamera;
		auto fogEnable = entSkyCamera.GetKeyValue("fogenable");
		if(fogEnable.has_value() && util::to_boolean(*fogEnable))
		{
			auto entFog = pragma::asset::EntityData::Create();
			entFog->SetClassName("env_fog_controller");
			auto copyKeyValue = [&entSkyCamera,&entFog](const std::string &srcKey,const std::string &dstKey) {
				auto val = entSkyCamera.GetKeyValue(srcKey);
				if(val.has_value())
					entFog->SetKeyValue(dstKey,*val);
			};
			copyKeyValue("fogcolor","fogcolor");
			copyKeyValue("fogstart","fogstart");
			copyKeyValue("fogend","fogend");
			copyKeyValue("fogmaxdensity","fogmaxdensity");
			entFog->SetKeyValue("spawnflags","1024");
			const_cast<std::vector<std::shared_ptr<pragma::asset::EntityData>>&>(ents).push_back(entFog);
			auto &components = entFog->GetComponents();
			components.push_back("toggle");
		}
	}
	for(auto &entData : worldData.GetEntities())
	{
		auto &originalClassName = entData->GetClassName();
		auto &keyValues = entData->GetKeyValues();
		translate_entity_data(nw,*entData,fgdData,isSource2,messageLogger,optMsgCache);
	}
}
void source_engine::translate_entity_data(
	NetworkState &nw,pragma::asset::EntityData &entData,const std::vector<util::fgd::Data> &fgdData,bool isSource2,
	const std::function<void(const std::string&)> &messageLogger,
	std::unordered_set<std::string> *optMsgCache
)
{
	auto originalClassName = entData.GetClassName();
	auto className = originalClassName;
	auto originalKeyValues = entData.GetKeyValues();
	std::unordered_map<std::string,std::string> keyValues;
	source_engine::translate_class(originalKeyValues,keyValues,className,isSource2);

	auto bClassFound = false;
	for(auto &pair : keyValues)
	{
		source_engine::translate_key_value(fgdData,originalClassName,pair.first,pair.second,isSource2,[&bClassFound,&messageLogger](const std::string &msg,uint8_t msgType) {
			if(msgType == 0u)
			{
				if(bClassFound == true)
					return;
				bClassFound = true;
			}
			if(messageLogger)
				messageLogger(msg);
		},optMsgCache);
	}
	entData.SetClassName(className);

	auto itOrigin = keyValues.find("origin");
	if(itOrigin != keyValues.end() && itOrigin->second.empty() == false)
	{
		auto origin = uvec::create(itOrigin->second);
		entData.SetOrigin(origin);
	}

	auto itAngles = keyValues.find("angles");
	if(itAngles != keyValues.end() && itAngles->second.empty() == false)
	{
		auto angles = EulerAngles(itAngles->second);
		entData.SetRotation(uquat::create(angles));
	}

	// Setup entity components depending on keyvalues
	std::unordered_set<std::string> components {};
	source_engine::find_entity_components(keyValues,components);
	if(ustring::compare(className.c_str(),"env_fog_controller",false))
		components.insert("toggle");
	if(entData.GetOutputs().empty() == false)
		components.insert("io");
	for(auto &c : components)
		entData.GetComponents().push_back(c);

	auto flags = entData.GetFlags();
	auto itCl = keyValues.find("wv_hint_clientsideonly");
	if(itCl != keyValues.end() && util::to_boolean(itCl->second))
	{
		keyValues.erase(itCl);
		flags |= pragma::asset::EntityData::Flags::ClientsideOnly;
	}
	entData.SetFlags(flags);
	entData.GetKeyValues() = keyValues;
}
