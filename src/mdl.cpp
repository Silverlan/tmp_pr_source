#include <datasystem.h>
#include <pragma/debug/debug_performance_profiler.hpp>
#include <pragma/level/mapinfo.h>
#include "mdl.h"
#include "mdl_sequence.h"
#include "mdl_attachment.h"
#include "mdl_animation.h"
#include "mdl_bone.h"
#include "mdl_hitboxset.h"
#include "vtx.h"
#include "vvd.h"
#include "phy.h"
#include "ani.h"
#include "wv_source.hpp"
#include <pragma/networkstate/networkstate.h>
#include <pragma/model/modelmesh.h>
#include <pragma/model/animation/activities.h>
#include <fsys/filesystem.h>
#include <pragma/model/model.h>
#include <stack>
#include <unordered_set>
#include <unordered_map>
#include <pragma/physics/collisionmesh.h>
#include <sharedutils/util_file.h>
#include <sharedutils/util_string.h>
#include <sharedutils/util_path.hpp>
#include <pragma/engine.h>
#include <pragma/model/animation/vertex_animation.hpp>
#include <pragma/game/game_limits.h>
#include <mathutil/transform.hpp>
#include <panima/skeleton.hpp>
#include <panima/bone.hpp>

extern DLLNETWORK Engine *engine;

static const std::unordered_map<std::string,Activity> translateActivities = {
	{"ACT_RESET",Activity::Invalid},
	{"ACT_IDLE",Activity::Idle},
	{"ACT_TRANSITION",Activity::Invalid},
	{"ACT_COVER",Activity::Invalid},
	{"ACT_COVER_MED",Activity::Invalid},
	{"ACT_COVER_LOW",Activity::Invalid},
	{"ACT_WALK",Activity::Walk},
	{"ACT_WALK_AIM",Activity::Invalid},
	{"ACT_WALK_CROUCH",Activity::Invalid},
	{"ACT_WALK_CROUCH_AIM",Activity::Invalid},
	{"ACT_RUN",Activity::Run},
	{"ACT_RUN_AIM",Activity::Invalid},
	{"ACT_RUN_CROUCH",Activity::Invalid},
	{"ACT_RUN_CROUCH_AIM",Activity::Invalid},
	{"ACT_RUN_PROTECTED",Activity::Invalid},
	{"ACT_SCRIPT_CUSTOM_MOVE",Activity::Invalid},
	{"ACT_RANGE_ATTACK1",Activity::RangeAttack1},
	{"ACT_RANGE_ATTACK2",Activity::RangeAttack2},
	{"ACT_RANGE_ATTACK1_LOW",Activity::RangeAttack3},
	{"ACT_RANGE_ATTACK2_LOW",Activity::RangeAttack4},
	{"ACT_DIESIMPLE",Activity::Invalid},
	{"ACT_DIEBACKWARD",Activity::Invalid},
	{"ACT_DIEFORWARD",Activity::Invalid},
	{"ACT_DIEVIOLENT",Activity::Invalid},
	{"ACT_DIERAGDOLL",Activity::Invalid},
	{"ACT_FLY",Activity::Invalid},
	{"ACT_HOVER",Activity::Invalid},
	{"ACT_GLIDE",Activity::Invalid},
	{"ACT_SWIM",Activity::Invalid},
	{"ACT_JUMP",Activity::Jump},
	{"ACT_HOP",Activity::Invalid},
	{"ACT_LEAP",Activity::Invalid},
	{"ACT_LAND",Activity::Invalid},
	{"ACT_CLIMB_UP",Activity::Invalid},
	{"ACT_CLIMB_DOWN",Activity::Invalid},
	{"ACT_CLIMB_DISMOUNT",Activity::Invalid},
	{"ACT_SHIPLADDER_UP",Activity::Invalid},
	{"ACT_SHIPLADDER_DOWN",Activity::Invalid},
	{"ACT_STRAFE_LEFT",Activity::Invalid},
	{"ACT_STRAFE_RIGHT",Activity::Invalid},
	{"ACT_ROLL_LEFT",Activity::Invalid},
	{"ACT_ROLL_RIGHT",Activity::Invalid},
	{"ACT_TURN_LEFT",Activity::TurnLeft},
	{"ACT_TURN_RIGHT",Activity::TurnRight},
	{"ACT_CROUCH",Activity::Crouch},
	{"ACT_CROUCHIDLE",Activity::CrouchIdle},
	{"ACT_STAND",Activity::Invalid},
	{"ACT_USE",Activity::Invalid},
	{"ACT_SIGNAL1",Activity::Invalid},
	{"ACT_SIGNAL2",Activity::Invalid},
	{"ACT_SIGNAL3",Activity::Invalid},
	{"ACT_SIGNAL_ADVANCE",Activity::Invalid},
	{"ACT_SIGNAL_FORWARD",Activity::Invalid},
	{"ACT_SIGNAL_GROUP",Activity::Invalid},
	{"ACT_SIGNAL_HALT",Activity::Invalid},
	{"ACT_SIGNAL_LEFT",Activity::Invalid},
	{"ACT_SIGNAL_RIGHT",Activity::Invalid},
	{"ACT_SIGNAL_TAKECOVER",Activity::Invalid},
	{"ACT_LOOKBACK_RIGHT",Activity::Invalid},
	{"ACT_LOOKBACK_LEFT",Activity::Invalid},
	{"ACT_COWER",Activity::Invalid},
	{"ACT_SMALL_FLINCH",Activity::Invalid},
	{"ACT_BIG_FLINCH",Activity::Invalid},
	{"ACT_MELEE_ATTACK1",Activity::MeleeAttack1},
	{"ACT_MELEE_ATTACK2",Activity::MeleeAttack2},
	{"ACT_RELOAD",Activity::Invalid},
	{"ACT_RELOAD_LOW",Activity::Invalid},
	{"ACT_ARM",Activity::Invalid},
	{"ACT_DISARM",Activity::Invalid},
	{"ACT_PICKUP_GROUND",Activity::Invalid},
	{"ACT_PICKUP_RACK",Activity::Invalid},
	{"ACT_IDLE_ANGRY",Activity::Invalid},
	{"ACT_IDLE_RELAXED",Activity::Invalid},
	{"ACT_IDLE_STIMULATED",Activity::Invalid},
	{"ACT_IDLE_AGITATED",Activity::Invalid},
	{"ACT_WALK_RELAXED",Activity::Invalid},
	{"ACT_WALK_STIMULATED",Activity::Invalid},
	{"ACT_WALK_AGITATED",Activity::Invalid},
	{"ACT_RUN_RELAXED",Activity::Invalid},
	{"ACT_RUN_STIMULATED",Activity::Invalid},
	{"ACT_RUN_AGITATED",Activity::Invalid},
	{"ACT_IDLE_AIM_RELAXED",Activity::Invalid},
	{"ACT_IDLE_AIM_STIMULATED",Activity::Invalid},
	{"ACT_IDLE_AIM_AGITATED",Activity::Invalid},
	{"ACT_WALK_AIM_RELAXED",Activity::Invalid},
	{"ACT_WALK_AIM_STIMULATED",Activity::Invalid},
	{"ACT_WALK_AIM_AGITATED",Activity::Invalid},
	{"ACT_RUN_AIM_RELAXED",Activity::Invalid},
	{"ACT_RUN_AIM_STIMULATED",Activity::Invalid},
	{"ACT_RUN_AIM_AGITATED",Activity::Invalid},
	{"ACT_WALK_HURT",Activity::Invalid},
	{"ACT_RUN_HURT",Activity::Invalid},
	{"ACT_SPECIAL_ATTACK1",Activity::Invalid},
	{"ACT_SPECIAL_ATTACK2",Activity::Invalid},
	{"ACT_COMBAT_IDLE",Activity::Invalid},
	{"ACT_WALK_SCARED",Activity::Invalid},
	{"ACT_RUN_SCARED",Activity::Invalid},
	{"ACT_VICTORY_DANCE",Activity::Invalid},
	{"ACT_DIE_HEADSHOT",Activity::Invalid},
	{"ACT_DIE_CHESTSHOT",Activity::Invalid},
	{"ACT_DIE_GUTSHOT",Activity::Invalid},
	{"ACT_DIE_BACKSHOT",Activity::Invalid},
	{"ACT_FLINCH_HEAD",Activity::FlinchHead},
	{"ACT_FLINCH_CHEST",Activity::FlinchChest},
	{"ACT_FLINCH_STOMACH",Activity::FlinchStomach},
	{"ACT_FLINCH_LEFTARM",Activity::FlinchLeftArm},
	{"ACT_FLINCH_RIGHTARM",Activity::FlinchRightArm},
	{"ACT_FLINCH_LEFTLEG",Activity::FlinchLeftLeg},
	{"ACT_FLINCH_RIGHTLEG",Activity::FlinchRightLeg},
	{"ACT_FLINCH_PHYSICS",Activity::FlinchGeneric},
	{"ACT_IDLE_ON_FIRE",Activity::Invalid},
	{"ACT_WALK_ON_FIRE",Activity::Invalid},
	{"ACT_RUN_ON_FIRE",Activity::Invalid},
	{"ACT_RAPPEL_LOOP",Activity::Invalid},
	{"ACT_180_LEFT",Activity::Invalid},
	{"ACT_180_RIGHT",Activity::Invalid},
	{"ACT_90_LEFT",Activity::Invalid},
	{"ACT_90_RIGHT",Activity::Invalid},
	{"ACT_STEP_LEFT",Activity::Invalid},
	{"ACT_STEP_RIGHT",Activity::Invalid},
	{"ACT_STEP_BACK",Activity::Invalid},
	{"ACT_STEP_FORE",Activity::Invalid},
	{"ACT_GESTURE_RANGE_ATTACK1",Activity::Invalid},
	{"ACT_GESTURE_RANGE_ATTACK2",Activity::Invalid},
	{"ACT_GESTURE_MELEE_ATTACK1",Activity::Invalid},
	{"ACT_GESTURE_MELEE_ATTACK2",Activity::Invalid},
	{"ACT_GESTURE_RANGE_ATTACK1_LOW",Activity::Invalid},
	{"ACT_GESTURE_RANGE_ATTACK2_LOW",Activity::Invalid},
	{"ACT_MELEE_ATTACK_SWING_GESTURE",Activity::Invalid},
	{"ACT_GESTURE_SMALL_FLINCH",Activity::Invalid},
	{"ACT_GESTURE_BIG_FLINCH",Activity::Invalid},
	{"ACT_GESTURE_FLINCH_BLAST",Activity::Invalid},
	{"ACT_GESTURE_FLINCH_HEAD",Activity::Invalid},
	{"ACT_GESTURE_FLINCH_CHEST",Activity::Invalid},
	{"ACT_GESTURE_FLINCH_STOMACH",Activity::Invalid},
	{"ACT_GESTURE_FLINCH_LEFTARM",Activity::Invalid},
	{"ACT_GESTURE_FLINCH_RIGHTARM",Activity::Invalid},
	{"ACT_GESTURE_FLINCH_LEFTLEG",Activity::Invalid},
	{"ACT_GESTURE_FLINCH_RIGHTLEG",Activity::Invalid},
	{"ACT_GESTURE_TURN_LEFT",Activity::Invalid},
	{"ACT_GESTURE_TURN_RIGHT",Activity::Invalid},
	{"ACT_GESTURE_TURN_LEFT45",Activity::Invalid},
	{"ACT_GESTURE_TURN_RIGHT45",Activity::Invalid},
	{"ACT_GESTURE_TURN_LEFT90",Activity::Invalid},
	{"ACT_GESTURE_TURN_RIGHT90",Activity::Invalid},
	{"ACT_GESTURE_TURN_LEFT45_FLAT",Activity::Invalid},
	{"ACT_GESTURE_TURN_RIGHT45_FLAT",Activity::Invalid},
	{"ACT_GESTURE_TURN_LEFT90_FLAT",Activity::Invalid},
	{"ACT_GESTURE_TURN_RIGHT90_FLAT",Activity::Invalid},

	{"ACT_VM_IDLE",Activity::VmIdle},
	{"ACT_VM_DRAW",Activity::VmDeploy},
	{"ACT_VM_HOLSTER",Activity::VmHolster},
	{"ACT_VM_PRIMARYATTACK",Activity::VmPrimaryAttack},
	{"ACT_VM_SECONDARYATTACK",Activity::VmSecondaryAttack},
	{"ACT_VM_RELOAD",Activity::VmReload},

	{"ACT_GESTURE_TURN_LEFT",Activity::GestureTurnLeft},
	{"ACT_GESTURE_TURN_RIGHT",Activity::GestureTurnRight}
};

// TODO: Match these with materialsystem/src/impl_surfacematerials.h
static const std::unordered_map<std::string,std::string> surfMatTranslationTable = {
	{"default","generic"},
	{"default_silent","generic"},
	{"floatingstandable","generic"},
	{"item","generic"},
	{"ladder","generic"},
	{"no_decal","generic"},
	{"player","generic"},
	{"player_control_clip","generic"},

	{"baserock","rock"},
	{"boulder","rock"},
	{"brick","brick"},
	{"concrete","concrete"},
	{"concrete_block","concrete"},
	{"gravel","brick"},
	{"rock","rock"},

	{"canister","metal"},
	{"chain","metal"},
	{"chainlink","metal"},
	{"combine_metal","metal"},
	{"crowbar","metal"},
	{"floating_metal_barrel","metal"},
	{"grenade","metal"},
	{"gunship","metal"},
	{"metal","metal"},
	{"metal_barrel","metal"},
	{"metal_bouncy","metal"},
	{"metal_box","metal"},
	{"metal_seafloorcar","metal"},
	{"metalgrate","metal"},
	{"metalpanel","metal"},
	{"metalvent","metal"},
	{"metalvehicle","metal"},
	{"paintcan","metal"},
	{"popcan","metal"},
	{"roller","metal"},
	{"slipperymetal","metal"},
	{"solidmetal","metal"},
	{"strider","metal"},
	{"weapon","metal"},

	{"wood","wood"},
	{"wood_box","wood"},
	{"wood_crate","wood"},
	{"wood_furniture","wood"},
	{"wood_lowdensity","wood"},
	{"wood_plank","wood"},
	{"wood_panel","wood"},
	{"wood_solid","wood"},

	{"dirt","dirt"},
	{"grass","grass"},
	{"gravel","gravel"},
	{"mud","dirt"},
	{"quicksand","dirt"},
	{"sand","dirt"},
	{"slipperyslime","dirt"},
	{"antlionsand","dirt"},

	{"slime","water"},
	{"water","water"},
	{"wade","water"},

	{"ice","ice"},
	{"snow","snow"},

	{"alienflesh","flesh"},
	{"antlion","flesh"},
	{"armorflesh","flesh"},
	{"bloodyflesh","flesh"},
	{"flesh","flesh"},
	{"foliage","plant"},
	{"watermelon","plant"},
	{"zombieflesh","flesh"},

	{"asphalt","concrete"},
	{"glass","glass"},
	{"glassbottle","glass"},
	{"combine_glass","glass"},
	{"tile","ceramic"},
	{"paper","generic"},
	{"papercup","generic"},
	{"cardboard","wood"},
	{"plaster","plaster"},
	{"plastic_barrel","plastic"},
	{"plastic_barrel_buoyant","plastic"},
	{"plastic_box","plastic"},
	{"plastic","plastic"},
	{"rubber","generic"},
	{"rubbertire","generic"},
	{"slidingrubbertire","generic"},
	{"slidingrubbertire_front","generic"},
	{"slidingrubbertire_rear","generic"},
	{"jeeptire","generic"},
	{"brakingrubbertire","generic"},

	{"carpet","carpet"},
	{"ceiling_tile","ceramic"},
	{"computer","plastic"},
	{"pottery","ceramic"}
};

static const std::unordered_map<int32_t,AnimationEvent::Type> translateAnimEvent = {
	{3,AnimationEvent::Type::EmitSound}
};

Vector3 import::mdl::transform_coordinate_system(const Vector3 &v)
{
	auto r = v;
	auto y = r.y;
	r.y = r.z;
	r.z = -y;
	return r;
}

void transform_local_frame_to_global(const std::vector<std::shared_ptr<import::mdl::Bone>> &bones,Frame &frame)
{
	std::function<void(const std::vector<std::shared_ptr<import::mdl::Bone>>&,const Vector3&,const Quat&)> transform_local_transforms_to_global = nullptr;
	transform_local_transforms_to_global = [&transform_local_transforms_to_global,&frame](const std::vector<std::shared_ptr<import::mdl::Bone>> &bones,const Vector3 &origin,const Quat &originRot) {
		for(auto &bone : bones)
		{
			auto boneId = bone->GetID();
			auto pos = *frame.GetBonePosition(boneId);
			uvec::rotate(&pos,originRot);
			pos += origin;
			frame.SetBonePosition(boneId,pos);

			auto rot = *frame.GetBoneOrientation(boneId);
			rot = originRot *rot;
			frame.SetBoneOrientation(boneId,rot);

			transform_local_transforms_to_global(bone->GetChildren(),pos,rot);
		}
	};
	transform_local_transforms_to_global(bones,{},{});
}

bool import::load_mdl(
	NetworkState *nw,const VFilePtr &f,const std::function<std::shared_ptr<Model>()> &fCreateModel,
	const std::function<bool(const std::shared_ptr<Model>&,const std::string&,const std::string&)> &fCallback,
	bool bCollision,MdlInfo &mdlInfo,std::ostream *optLog
)
{
	auto offset = f->Tell();
	auto &header = mdlInfo.header;
	header = {};
	// Read everything up to 'numlocalseq', since everything up to that point is the same in all headers
	f->Read(&header,offsetof(mdl::studiohdr_t,numlocalseq));
	if(header.version == 37)
	{
		struct Version37Header
		{
			int numanimgroup;
			int animgroupindex;
				
			int numbonedesc;
			int bonedescindex;
				
			int numseq; // sequences
			int seqindex;
			int sequencesindexed; // initialization flag - have the sequences been indexed?
				
			int numseqgroups; // demand loaded sequences
			int seqgroupindex;
				
			int numtextures; // raw textures
			int textureindex;
				
			int numcdtextures; // raw textures search paths
			int cdtextureindex;
				
			int numskinref; // replaceable textures tables
			int numskinfamilies;
			int skinindex;
				
			int numbodyparts;		
			int bodypartindex;
				
			int numattachments; // queryable attachable points
			int attachmentindex;
				
			int numtransitions; // animation node to animation node transition graph
			int transitionindex;
				
			int numflexdesc;
			int flexdescindex;
				
			int numflexcontrollers;
			int flexcontrollerindex;
				
			int numflexrules;
			int flexruleindex;
				
			int numikchains;
			int ikchainindex;
				
			int nummouths;
			int mouthindex;
				
			int numposeparameters;
			int poseparamindex;
				
			int surfacepropindex;
				
			// Key values
			int keyvalueindex;
			int keyvaluesize;
				
			int numikautoplaylocks;
			int ikautoplaylockindex;
				
			float mass; // The collision model mass that jay wanted
			int contents;
			std::array<int,9> unused; // remove as appropriate
		};
		auto v37Header = f->Read<Version37Header>();
		header.numlocalseq = v37Header.numseq;
		header.localseqindex = v37Header.seqindex;

		header.numtextures = v37Header.numtextures;
		header.textureindex = v37Header.textureindex;

		header.numcdtextures = v37Header.numcdtextures;
		header.cdtextureindex = v37Header.cdtextureindex;

		header.numskinref = v37Header.numskinref;
		header.numskinfamilies = v37Header.numskinfamilies;
		header.skinindex = v37Header.skinindex;

		header.numbodyparts = v37Header.numbodyparts;
		header.bodypartindex = v37Header.bodypartindex;

		header.numlocalattachments = v37Header.numattachments;
		header.localattachmentindex = v37Header.attachmentindex;

		header.numflexdesc = v37Header.numflexdesc;
		header.flexdescindex = v37Header.flexdescindex;

		header.numflexcontrollers = v37Header.numflexcontrollers;
		header.flexcontrollerindex = v37Header.flexcontrollerindex;

		header.numflexrules = v37Header.numflexrules;
		header.flexruleindex = v37Header.flexruleindex;

		header.numikchains = v37Header.numikchains;
		header.ikchainindex = v37Header.ikchainindex;

		header.nummouths = v37Header.nummouths;
		header.mouthindex = v37Header.mouthindex;

		header.numlocalposeparameters = v37Header.numposeparameters;
		header.localposeparamindex = v37Header.poseparamindex;

		header.surfacepropindex = v37Header.surfacepropindex;
		header.keyvalueindex = v37Header.keyvalueindex;

		header.numlocalikautoplaylocks = v37Header.numikautoplaylocks;
		header.localikautoplaylockindex = v37Header.ikautoplaylockindex;

		header.mass = v37Header.mass;
		header.contents = v37Header.contents;

		header.numincludemodels = 0;
		header.szanimblocknameindex = 0;
		header.numanimblocks = 0;

		header.numflexcontrollerui = 0;
	}
	else
	{
		auto offset = offsetof(mdl::studiohdr_t,numlocalseq);
		f->Read(reinterpret_cast<uint8_t*>(&mdlInfo.header) +offset,sizeof(mdl::studiohdr_t) -offset);
	}
	auto *id = reinterpret_cast<uint8_t*>(&header.id);

	if(optLog)
		(*optLog)<<"Header ID: "<<id<<"\n";

	if(id[0] != 0x49 || id[1] != 0x44 || id[2] != 0x53 || id[3] != 0x54)
	{
		if(optLog)
			(*optLog)<<"Unknown header ID! Terminating...\n";
		return false;
	}

	if(optLog)
	{
		(*optLog)<<
			"Header information:\n"<<
			"Version: "<<header.version<<"\n"<<
			"Mass: "<<header.mass<<"\n"<<
			"Flags: "<<header.flags<<"\n"<<
			"Number of body parts: "<<header.numbodyparts<<"\n"<<
			"Number of bones: "<<header.numbones<<"\n"<<
			"Number of flex controllers: "<<header.numflexcontrollers<<"\n"<<
			"Number of textures: "<<header.numtextures<<"\n";
	}

	auto eyePos = header.eyeposition;
	// TODO: I'm not sure about this. The scout from TF2 has the eye position (-75.8498688,0,0),
	// So using it for the y-axis should be correct (unless it's relative to something else), but y and z
	// may be wrong.
	//mdlInfo.model.SetEyeOffset({eyePos.z,-eyePos.x,eyePos.y});
	// Update: zombie.mdl (HL Source Zombie) has the eyeposition (0,0,55), so the above is *not* correct
	auto eyePosCorrected = Vector3{eyePos.y,-eyePos.x,eyePos.z};
	mdlInfo.model.SetEyeOffset({eyePosCorrected.x,eyePosCorrected.z,-eyePosCorrected.y});

	if(header.studiohdr2index != 0)
	{
		f->Seek(offset +header.studiohdr2index);
		mdlInfo.header2 = f->Read<mdl::studiohdr2_t>();
	}

	// Read includes
	f->Seek(offset +header.includemodelindex);
	for(auto i=decltype(header.numincludemodels){0};i<header.numincludemodels;++i)
	{
		auto offset = f->Tell();
		auto labelOffset = f->Read<int32_t>();
		auto fileNameOffset = f->Read<int32_t>();
		auto dataOffset = f->Tell();
		if(labelOffset != 0)
		{
			f->Seek(offset +labelOffset);
			auto label = f->ReadString();
		}
		if(fileNameOffset != 0)
		{
			f->Seek(offset +fileNameOffset);
			auto fileName = f->ReadString();
			if(fileName.empty() == false)
			{
				if(optLog)
					(*optLog)<<"Found include model '"<<fileName<<"'! Adding to list of includes...\n";
				auto mdlName = FileManager::GetCanonicalizedPath(fileName);
				if(ustring::substr(mdlName,0,7) == std::string("models") +FileManager::GetDirectorySeparator())
					mdlName = ustring::substr(mdlName,7);
				ufile::remove_extension_from_filename(mdlName);
				auto incPath = mdlName;
				mdlInfo.model.GetMetaInfo().includes.push_back(::util::Path::CreateFile(incPath).GetString());
				if(FileManager::Exists("models\\" +incPath) == false)
				{
					auto r = convert_hl2_model(nw,fCreateModel,fCallback,"models\\",mdlName,optLog);
					if(r == false)
					{
						if(optLog)
							(*optLog)<<"WARNING: Unable to convert include model '"<<fileName<<"'!\n";
					}
				}
			}
		}
		f->Seek(dataOffset);
	}

	auto fAddPath = [&mdlInfo](const std::string &fpath) {
		auto npath = FileManager::GetCanonicalizedPath(fpath);
		if(npath.empty())
			return;
		auto it = std::find_if(mdlInfo.texturePaths.begin(),mdlInfo.texturePaths.end(),[&npath](const std::string &pathOther) {
			return ustring::compare(npath,pathOther,false);
			});
		if(it != mdlInfo.texturePaths.end())
			return;
		mdlInfo.texturePaths.push_back(npath);
	};

	// Read texture paths
	f->Seek(offset +header.cdtextureindex);
	mdlInfo.texturePaths.reserve(mdlInfo.texturePaths.size() +header.numcdtextures);
	for(auto i=decltype(header.numcdtextures){0};i<header.numcdtextures;++i)
	{
		f->Seek(offset +header.cdtextureindex +i *sizeof(int32_t));
		auto strOffset = f->Read<int32_t>();
		f->Seek(offset +strOffset);
		auto stdCdTex = f->ReadString();
		if(optLog)
			(*optLog)<<"Adding texture path '"<<stdCdTex<<"'...\n";
		fAddPath(stdCdTex);
	}
	//

	// Read textures
	f->Seek(offset +header.textureindex);
	mdlInfo.textures.reserve(header.numtextures);
	mdlInfo.texturePaths.reserve(mdlInfo.texturePaths.size() +header.numtextures);
	for(auto i=decltype(header.numtextures){0};i<header.numtextures;++i)
	{
		auto stdTexOffset = f->Tell();
		auto stdTex = f->Read<mdl::mstudiotexture_t>();
		auto postOffset = f->Tell();

		f->Seek(stdTexOffset +stdTex.sznameindex);
		auto name = f->ReadString();

		if(name.empty() == false)
		{
			if(optLog)
				(*optLog)<<"Adding texture '"<<name<<"'...\n";
			mdlInfo.textures.push_back(ufile::get_file_from_filename(name));
			fAddPath(ufile::get_path_from_filename(name));
		}

		f->Seek(postOffset);
	}
	//

	auto itHwm = std::find(mdlInfo.texturePaths.begin(),mdlInfo.texturePaths.end(),"models/player/hvyweapon/hwm/");
	if(itHwm != mdlInfo.texturePaths.end())
	{
		// Note: This is a really ugly hack.
		// The problem is that some Source models use a material lookup path (cdTexturePath) with relative material names,
		// while others use absolute material names. Pragma on the other hand always uses lookup paths, so if it's an absolute
		// name, the path to the material is stripped and added as a lookup path.
		// This generally works fine, except in very rare cases where the absolute path of a material would point to material,
		// where a material of the same name also exists in one of the lookup paths which differ from the absolute path of that material,
		// and where the order of lookup paths is such that that lookup path comes before the stripped absolute path of the material.
		// This happens, for example, with the HWM SFM TF2 models like models/player/hwm/heavy.mdl:
		// 1) It has a lookup path that points to "materials/models/player/hvyweapon", which contains a "hvyweapon_red.vmt" material.
		// 2) It uses the absolute path for the "hvyweapon_red.vmt", which points to "materials/models/player/hvyweapon/hwm/hvyweapon_red.vmt"
		// 3) The absolute path of the material is stripped (-> "materials/models/player/hvyweapon/hwm") and added as a lookup path, but ends up in order AFTER the "materials/models/player/hvyweapon" lookup path
		// 4) As a result, the non-hwm material version from "materials/models/player/hvyweapon" would be used ingame
		// There is no proper solution for this problem (except maybe by getting rid of lookup paths altogether and using absolute paths for all materials),
		// so we'll work around it for the specific models that it affects by changing the order of the lookup paths so the hwm path comes first.
		// There may be other models where this could be a problem, but it's extremely rare and could still easily be fixed by hand.
		auto hwmPath = *itHwm;
		mdlInfo.texturePaths.erase(itHwm);
		mdlInfo.texturePaths.insert(mdlInfo.texturePaths.begin(),hwmPath);
	}

	// Read bones
	f->Seek(offset +header.boneindex);
	auto &bones = mdlInfo.bones;
	bones.reserve(header.numbones);
	for(auto i=decltype(header.numbones){0};i<header.numbones;++i)
	{
		bones.push_back(std::make_shared<mdl::Bone>(header.version,i,f));
		if(optLog)
			(*optLog)<<"Adding bone '"<<bones.back()->GetName()<<"'...\n";
	}
	if(header.numbones > umath::to_integral(GameLimits::MaxBones))
	{
		if(optLog)
			(*optLog)<<"WARNING: Model has "<<header.numbones<<" bones, but the engine only supports up to "<<umath::to_integral(GameLimits::MaxBones)<<"! Expect issues!\n";
	}
	mdl::Bone::BuildHierarchy(bones);

	// Read animation names
	//f->Seek(offset +header.szanimblocknameindex); // 0?
	//auto name = f->ReadString();
	//std::cout<<"Anim Block Name: "<<name<<std::endl;
	//
	
	// Read animations
	std::vector<std::string> aniFileNames;
	auto &animBlocks = mdlInfo.animationBlocks;
	if(header.numanimblocks > 0)
	{
		animBlocks.reserve(header.numanimblocks);
		if(header.szanimblocknameindex > 0)
		{
			auto offset = f->Tell();
			f->Seek(header.szanimblocknameindex);
			auto animBlockName = f->ReadString();
			if(optLog)
				(*optLog)<<"Found animation '"<<animBlockName<<"'!\n";
			aniFileNames.push_back(animBlockName);

			f->Seek(offset);
		}
		if(header.animblockindex > 0)
		{
			f->Seek(offset +header.animblockindex);
			for(auto i=decltype(header.numanimblocks){0};i<header.numanimblocks;++i)
				animBlocks.push_back(f->Read<mdl::mstudioanimblock_t>());
		}
	}

	f->Seek(offset +header.localanimindex);

	auto &animDescs = mdlInfo.animationDescs;
	animDescs.reserve(header.numlocalanim);
	auto animDescOffset = f->Tell();
	for(auto i=decltype(header.numlocalanim){0};i<header.numlocalanim;++i)
	{
		animDescs.push_back(mdl::AnimationDesc(i,f));
		if(optLog)
			(*optLog)<<"Found animation desc '"<<animDescs.back().GetName()<<"' with flags "<<animDescs.back().GetStudioDesc().flags<<"!\n";
	}

	// Read animation sections
	for(auto i=decltype(header.numlocalanim){0};i<header.numlocalanim;++i)
	{
		auto &animDesc = animDescs[i];
		animDesc.ReadAnimationSections(header,animDescOffset +i *sizeof(mdl::mstudioanimdesc_t),f);
	}
	//

	if(optLog)
		(*optLog)<<"Reading animation data...\n";
	for(auto it=animDescs.begin();it!=animDescs.end();++it)
	{
		auto &animDesc = *it;
		animDesc.ReadAnimation(header,animDescOffset,animDescOffset +(it -animDescs.begin()) *sizeof(mdl::mstudioanimdesc_t),f);
	}
	//

	// Load sequences
	if(optLog)
		(*optLog)<<"Reading sequences...\n";
	f->Seek(offset +header.localseqindex);
	auto &sequences = mdlInfo.sequences;
	sequences.reserve(header.numlocalseq);
	for(auto i=decltype(header.numlocalseq){0};i<header.numlocalseq;++i)
		sequences.push_back({f,header.numbones});
	//

	// Read attachments
	if(optLog)
		(*optLog)<<"Reading attachments...\n";
	f->Seek(offset +header.localattachmentindex);
	auto &attachments = mdlInfo.attachments;
	attachments.reserve(header.numlocalattachments);
	for(auto i=decltype(header.numlocalattachments){0};i<header.numlocalattachments;++i)
		attachments.push_back(std::make_shared<mdl::Attachment>(f));
	//

	// Read flex descs
	if(optLog)
		(*optLog)<<"Reading flex descs...\n";
	f->Seek(offset +header.flexdescindex);
	auto &flexDescs = mdlInfo.flexDescs;
	flexDescs.reserve(header.numflexdesc);
	for(auto i=decltype(header.numflexdesc){0};i<header.numflexdesc;++i)
		flexDescs.push_back(mdl::FlexDesc{f});
	//

	// Read flex controllers
	if(optLog)
		(*optLog)<<"Reading flex controllers...\n";
	f->Seek(offset +header.flexcontrollerindex);
	auto &flexControllers = mdlInfo.flexControllers;
	flexControllers.reserve(header.numflexcontrollers);

	// Some models have multiple flex controllers of the same name, this is not supported in Pragma!
	// We'll remove the duplicates here.
	std::vector<uint32_t> flexControllerTranslationTable;
	flexControllerTranslationTable.resize(header.numflexcontrollers);

	for(auto i=decltype(header.numflexcontrollers){0};i<header.numflexcontrollers;++i)
	{
		mdl::FlexController fc {f};
		auto it = std::find_if(flexControllers.begin(),flexControllers.end(),[&fc](const import::mdl::FlexController &fcOther) {
			return fc.GetName() == fcOther.GetName();
		});
		if(it == flexControllers.end())
		{
			flexControllers.push_back(fc);
			flexControllerTranslationTable[i] = flexControllers.size() -1;
			continue;
		}
		*it = fc;
		flexControllerTranslationTable[i] = (it -flexControllers.begin());
	}
	//

	auto getFlexController = [&flexControllers,&flexControllerTranslationTable](int32_t idx) -> import::mdl::FlexController& {
		return flexControllers[flexControllerTranslationTable[idx]];
	};

	// Read flex rules
	if(optLog)
		(*optLog)<<"Reading flex rules...\n";
	f->Seek(offset +header.flexruleindex);
	auto &flexRules = mdlInfo.flexRules;
	flexRules.reserve(header.numflexrules);
	for(auto i=decltype(header.numflexrules){0};i<header.numflexrules;++i)
		flexRules.push_back(mdl::FlexRule{f});
	//

	// Read flex controller uis
	if(optLog)
		(*optLog)<<"Reading flex controller uis...\n";
	f->Seek(offset +header.flexcontrolleruiindex);
	auto &flexControllerUis = mdlInfo.flexControllerUis;
	flexControllerUis.reserve(header.numflexcontrollerui);
	for(auto i=decltype(header.numflexcontrollerui){0};i<header.numflexcontrollerui;++i)
		flexControllerUis.push_back(mdl::FlexControllerUi{f});
	//

	if(flexRules.empty() == false)
	{
		for(auto &flexRule : flexRules)
		{
			auto &flexDesc = flexDescs.at(flexRule.GetFlexId());
			auto &ops = flexRule.GetOperations();
			auto flexName = flexDesc.GetName();
			std::stringstream sop;
			sop<<"%"<<flexName<<" = ";

			struct OpExpression
			{
				OpExpression(const std::string &e="",uint32_t p=0u)
					: expression(e),precedence(p)
				{}
				std::string expression;
				uint32_t precedence = 0u;
			};

			std::stack<OpExpression> opStack {};

			auto fCheckStackCount = [&opStack,&flexName,&optLog](uint32_t required,const std::string &identifier) -> bool {
				if(opStack.size() >= required)
					return true;
				if(optLog)
					(*optLog)<<"WARNING: Unable to evaluate flex operation '"<<identifier<<"' for flex "<<flexName<<"! Skipping...\n";
				opStack.push({"0.0"});
				return false;
			};

			for(auto &op : ops)
			{
				switch(op.type)
				{
					case mdl::FlexRule::Operation::Type::None:
						break;
					case mdl::FlexRule::Operation::Type::Const:
						opStack.push({std::to_string(op.d.value),10u});
						break;
					case mdl::FlexRule::Operation::Type::Fetch:
						opStack.push({getFlexController(op.d.index).GetName(),10u});
						break;
					case mdl::FlexRule::Operation::Type::Fetch2:
						opStack.push({"%" +flexDescs.at(op.d.index).GetName(),10u});
						break;
					case mdl::FlexRule::Operation::Type::Add:
					{
						if(fCheckStackCount(2,"add") == false)
							break;
						auto r = opStack.top();
						opStack.pop();
						auto l = opStack.top();
						opStack.pop();

						opStack.push({l.expression +" + " +r.expression,1u});
						break;
					}
					case mdl::FlexRule::Operation::Type::Sub:
					{
						if(fCheckStackCount(2,"sub") == false)
							break;
						auto r = opStack.top();
						opStack.pop();
						auto l = opStack.top();
						opStack.pop();

						opStack.push({l.expression +" - " +r.expression,1u});
						break;
					}
					case mdl::FlexRule::Operation::Type::Mul:
					{
						if(fCheckStackCount(2,"mul") == false)
							break;
						auto r = opStack.top();
						opStack.pop();
						auto rExpr = std::string{};
						if(r.precedence < 2)
							rExpr = "(" +r.expression +")";
						else
							rExpr = r.expression;

						auto l = opStack.top();
						opStack.pop();
						auto lExpr = std::string{};
						if(l.precedence < 2)
							lExpr = "(" +l.expression +")";
						else
							lExpr = l.expression;

						opStack.push({lExpr +" * "+rExpr,2});
						break;
					}
					case mdl::FlexRule::Operation::Type::Div:
					{
						if(fCheckStackCount(2,"div") == false)
							break;
						auto r = opStack.top();
						opStack.pop();
						auto rExpr = std::string{};
						if(r.precedence < 2)
							rExpr = "(" +r.expression +")";
						else
							rExpr = r.expression;

						auto l = opStack.top();
						opStack.pop();
						auto lExpr = std::string{};
						if(l.precedence < 2)
							lExpr = "(" +l.expression +")";
						else
							lExpr = l.expression;

						opStack.push({lExpr +" / "+rExpr,2});
						break;
					}
					case mdl::FlexRule::Operation::Type::Neg:
					{
						if(fCheckStackCount(1,"neg") == false)
							break;
						auto r = opStack.top();
						opStack.pop();
						opStack.push({"-" +r.expression,10});
						break;
					}
					case mdl::FlexRule::Operation::Type::Exp:
						if(optLog)
							(*optLog)<<"WARNING: Invalid flex rule "<<umath::to_integral(op.type)<<" (Exp)! Skipping...\n";
						break;
					case mdl::FlexRule::Operation::Type::Open:
						if(optLog)
							(*optLog)<<"WARNING: Invalid flex rule "<<umath::to_integral(op.type)<<" (Open)! Skipping...\n";
						break;
					case mdl::FlexRule::Operation::Type::Close:
						if(optLog)
							(*optLog)<<"WARNING: Invalid flex rule "<<umath::to_integral(op.type)<<" (Close)! Skipping...\n";
						break;
					case mdl::FlexRule::Operation::Type::Comma:
						if(optLog)
							(*optLog)<<"WARNING: Invalid flex rule "<<umath::to_integral(op.type)<<" (Comma)! Skipping...\n";
						break;
					case mdl::FlexRule::Operation::Type::Max:
					{
						if(fCheckStackCount(2,"max") == false)
							break;
						auto r = opStack.top();
						opStack.pop();
						auto rExpr = std::string{};
						if(r.precedence < 5)
							rExpr = "(" +r.expression +")";
						else
							rExpr = r.expression;

						auto l = opStack.top();
						opStack.pop();
						auto lExpr = std::string{};
						if(l.precedence < 5)
							lExpr = "(" +l.expression +")";
						else
							lExpr = l.expression;

						opStack.push({" max(" +lExpr +", "+rExpr +")",5});
						break;
					}
					case mdl::FlexRule::Operation::Type::Min:
					{
						if(fCheckStackCount(2,"min") == false)
							break;
						auto r = opStack.top();
						opStack.pop();
						auto rExpr = std::string{};
						if(r.precedence < 5)
							rExpr = "(" +r.expression +")";
						else
							rExpr = r.expression;

						auto l = opStack.top();
						opStack.pop();
						auto lExpr = std::string{};
						if(l.precedence < 5)
							lExpr = "(" +l.expression +")";
						else
							lExpr = l.expression;

						opStack.push({" min(" +lExpr +", "+rExpr +")",5});
						break;
					}
					case mdl::FlexRule::Operation::Type::TwoWay0:
						opStack.push({" (1 - (min(max(" +getFlexController(op.d.index).GetName() + " + 1, 0), 1)))",5});
						break;
					case mdl::FlexRule::Operation::Type::TwoWay1:
						opStack.push({" (min(max(" +getFlexController(op.d.index).GetName() + ", 0), 1))",5});
						break;
					case mdl::FlexRule::Operation::Type::NWay:
					{
						auto &v = getFlexController(op.d.index).GetName();

						auto &valueControllerIndex = opStack.top();
						opStack.pop();
						auto &flValue = getFlexController(ustring::to_int(valueControllerIndex.expression)).GetName();

						auto filterRampW = opStack.top();
						opStack.pop();
						auto filterRampZ = opStack.top();
						opStack.pop();
						auto filterRampY = opStack.top();
						opStack.pop();
						auto filterRampX = opStack.top();
						opStack.pop();

						auto greaterThanX = "min(1, (-min(0, (" + filterRampX.expression + " - " + flValue + "))))";
						auto lessThanY = "min(1, (-min(0, (" + flValue + " - " + filterRampY.expression + "))))";
						auto remapX = "min(max((" + flValue + " - " + filterRampX.expression + ") / (" + filterRampY.expression + " - " + filterRampX.expression + "), 0), 1)";
						auto greaterThanEqualY = "-(min(1, (-min(0, (" + flValue + " - " + filterRampY.expression + ")))) - 1)";
						auto lessThanEqualZ = "-(min(1, (-min(0, (" + filterRampZ.expression + " - " + flValue + ")))) - 1)";
						auto greaterThanZ = "min(1, (-min(0, (" + filterRampZ.expression + " - " + flValue + "))))";
						auto lessThanW = "min(1, (-min(0, (" + flValue + " - " + filterRampW.expression + "))))";
						auto remapZ = "(1 - (min(max((" + flValue + " - " + filterRampZ.expression + ") / (" + filterRampW.expression + " - " + filterRampZ.expression + "), 0), 1)))";

						auto expValue = "((" + greaterThanX + " * " + lessThanY + ") * " + remapX + ") + (" + greaterThanEqualY + " * " + lessThanEqualZ + ") + ((" + greaterThanZ + " * " + lessThanW + ") * " + remapZ + ")";

						opStack.push({"((" + expValue + ") * (" + v + "))",5});
						break;
					}
					case mdl::FlexRule::Operation::Type::Combo:
					{
						if(opStack.empty())
							break;
						auto count = op.d.index;
						if(fCheckStackCount(count,"combo") == false)
							break;
						auto newExpr = OpExpression{};
						auto intExpr = opStack.top();
						opStack.pop();
						newExpr.expression += intExpr.expression;
						for(auto i=decltype(count){1};i<count;++i)
						{
							intExpr = opStack.top();
							opStack.pop();
							newExpr.expression += " * " +intExpr.expression;
						}
						newExpr.expression = "(" +newExpr.expression +")";
						newExpr.precedence = 5u;
						opStack.push(newExpr);
						break;
					}
					case mdl::FlexRule::Operation::Type::Dominate:
					{
						if(opStack.empty())
							break;
						auto count = op.d.index;
						if(fCheckStackCount(count +1,"dominate") == false)
							break;
						auto newExpr = OpExpression{};
						auto intExpr = opStack.top();
						opStack.pop();
						newExpr.expression += intExpr.expression;
						for(auto i=decltype(count){1};i<count;++i)
						{
							intExpr = opStack.top();
							opStack.pop();
							newExpr.expression += " * " +intExpr.expression;
						}
						intExpr = opStack.top();
						opStack.pop();
						newExpr.expression = intExpr.expression +" * (1 - " +newExpr.expression +")";
						newExpr.expression = "(" +newExpr.expression +")";
						newExpr.precedence = 5u;
						opStack.push(newExpr);
						break;
					}
					case mdl::FlexRule::Operation::Type::DMELowerEyelid:
					{
						auto &pCloseLidV = getFlexController(op.d.index);
						auto range = pCloseLidV.GetRange();
						auto flCloseLidVMin = std::to_string(range.first);
						auto flCloseLidVMax = std::to_string(range.second);
						auto flCloseLidV = "(min(max((" + pCloseLidV.GetName() + " - " + flCloseLidVMin + ") / (" + flCloseLidVMax + " - " + flCloseLidVMin + "), 0), 1))";

						auto closeLidIndex = opStack.top();
						opStack.pop();

						auto &pCloseLid = getFlexController(ustring::to_int(closeLidIndex.expression));
						range = pCloseLid.GetRange();
						auto flCloseLidMin = std::to_string(range.first);
						auto flCloseLidMax = std::to_string(range.second);
						auto flCloseLid = "(min(max((" + pCloseLid.GetName() + " - " + flCloseLidMin + ") / (" + flCloseLidMax + " - " + flCloseLidMin + "), 0), 1))";

						opStack.pop();

						auto eyeUpDownIndex = opStack.top();
						opStack.pop();
						auto &pEyeUpDown = getFlexController(ustring::to_int(eyeUpDownIndex.expression));
						range = pCloseLid.GetRange();
						auto flEyeUpDownMin = std::to_string(range.first);
						auto flEyeUpDownMax = std::to_string(range.second);
						auto flEyeUpDown = "(-1 + 2 * (min(max((" + pEyeUpDown.GetName() + " - " + flEyeUpDownMin + ") / (" + flEyeUpDownMax + " - " + flEyeUpDownMin + "), 0), 1)))";

						opStack.push({"(min(1, (1 - " + flEyeUpDown + ")) * (1 - " + flCloseLidV + ") * " + flCloseLid + ")",5});
						break;
					}
					case mdl::FlexRule::Operation::Type::DMEUpperEyelid:
					{
						auto &pCloseLidV = getFlexController(op.d.index);
						auto range = pCloseLidV.GetRange();
						auto flCloseLidVMin = std::to_string(range.first);
						auto flCloseLidVMax = std::to_string(range.second);
						auto flCloseLidV = "(min(max((" + pCloseLidV.GetName() + " - " + flCloseLidVMin + ") / (" + flCloseLidVMax + " - " + flCloseLidVMin + "), 0), 1))";

						auto closeLidIndex = opStack.top();
						opStack.pop();
						auto &pCloseLid = getFlexController(ustring::to_int(closeLidIndex.expression));
						range = pCloseLidV.GetRange();
						auto flCloseLidMin = std::to_string(range.first);
						auto flCloseLidMax = std::to_string(range.second);
						auto flCloseLid = "(min(max((" + pCloseLid.GetName() + " - " + flCloseLidMin + ") / (" + flCloseLidMax + " - " + flCloseLidMin + "), 0), 1))";

						opStack.pop();

						auto eyeUpDownIndex = opStack.top();
						opStack.pop();
						auto &pEyeUpDown = getFlexController(ustring::to_int(eyeUpDownIndex.expression));
						range = pCloseLidV.GetRange();
						auto flEyeUpDownMin = std::to_string(range.first);
						auto flEyeUpDownMax = std::to_string(range.second);
						auto flEyeUpDown = "(-1 + 2 * (min(max((" + pEyeUpDown.GetName() + " - " + flEyeUpDownMin + ") / (" + flEyeUpDownMax + " - " + flEyeUpDownMin + "), 0), 1)))";

						opStack.push({"(min(1, (1 + " + flEyeUpDown + ")) * " + flCloseLidV + " * " + flCloseLid + ")",5});
						break;
					}
				}
			}
			if(opStack.size() == 1)
			{
				sop<<opStack.top().expression;
				//std::cout<<"Final expression: "<<sop.str()<<std::endl;
			}
			else
			{
				if(optLog)
					(*optLog)<<"WARNING: Unable to evaluate flex rule for flex "<<flexName<<"! Skipping...\n";
			}
		}
	}

	// Read hitboxes
	if(optLog)
		(*optLog)<<"Reading hitboxes...\n";
	f->Seek(offset +header.hitboxsetindex);
	auto &hitboxSets = mdlInfo.hitboxSets;
	hitboxSets.reserve(header.numhitboxsets);
	for(auto i=decltype(header.numhitboxsets){0};i<header.numhitboxsets;++i)
		hitboxSets.push_back({f});
	//

	// Read skins
	if(optLog)
		(*optLog)<<"Reading skins...\n";
	f->Seek(offset +header.skinindex);
	auto &skinFamilies = mdlInfo.skinFamilies;
	skinFamilies.reserve(header.numskinfamilies);
	for(auto i=decltype(header.numskinfamilies){0};i<header.numskinfamilies;++i)
	{
		skinFamilies.push_back({});
		auto &skinFamily = skinFamilies.back();
		skinFamily.resize(header.numskinref);
		f->Read(skinFamily.data(),header.numskinref *sizeof(skinFamily.front()));
	}
	//

	// Read body parts
	if(optLog)
		(*optLog)<<"Reading body parts...\n";
	f->Seek(offset +header.bodypartindex);
	auto &bodyParts = mdlInfo.bodyParts;
	bodyParts.reserve(header.numbodyparts);
	for(auto i=decltype(header.numbodyparts){0};i<header.numbodyparts;++i)
		bodyParts.push_back({f});
	//

	// Read pose parameters
	if(optLog)
		(*optLog)<<"Reading pose parameters...\n";
	f->Seek(offset +header.localposeparamindex);
	auto &poseParameters = mdlInfo.poseParameters;
	poseParameters.reserve(header.numlocalposeparameters);
	for(auto i=decltype(header.numlocalposeparameters){0};i<header.numlocalposeparameters;++i)
		poseParameters.push_back({f});
	//

	return true;
}

std::shared_ptr<Model> import::load_mdl(
	NetworkState *nw,const std::unordered_map<std::string,VFilePtr> &files,const std::function<std::shared_ptr<Model>()> &fCreateModel,
	const std::function<bool(const std::shared_ptr<Model>&,const std::string&,const std::string&)> &fCallback,bool bCollision,
	std::vector<std::string> &textures,std::ostream *optLog
)
{
	auto it = files.find("mdl");
	if(it == files.end())
		return nullptr;
	auto ptrMdl = fCreateModel();
	auto &mdl = *ptrMdl;
	MdlInfo mdlInfo(mdl);
	auto r = load_mdl(nw,it->second,fCreateModel,fCallback,bCollision,mdlInfo,optLog);
	if(r == false)
		return nullptr;
	if(optLog)
		(*optLog)<<"Building model...\n";
	it = files.find("ani");
	if(it != files.end())
	{
		if(optLog)
			(*optLog)<<"Loading ani file '"<<it->second<<"'...\n";
		import::mdl::load_ani(it->second,mdlInfo);
		for(auto &animDesc : mdlInfo.animationDescs)
		{
			auto &sectionAnims = animDesc.GetSectionAnimations();
			//std::cout<<"Section Anim Count: "<<sectionAnims.size()<<std::endl;
		}
	}

	// Transform animations
	auto &anims = mdlInfo.animations;
	auto &animDescs = mdlInfo.animationDescs;
	anims.resize(animDescs.size());
	for(auto i=decltype(animDescs.size()){0};i<animDescs.size();++i)
	{
		auto &animDesc = animDescs[i];

		anims[i] = animDesc.CalcAnimation(mdlInfo);
	}
	//

	auto &header = mdlInfo.header;
	mdl.SetMass(header.mass);
	textures = mdlInfo.textures;
	for(auto &texPath : mdlInfo.texturePaths)
		mdl.AddTexturePath(texPath);
	auto &texPaths = mdl.GetTexturePaths();
	std::vector<uint32_t> textureTranslations;
	textureTranslations.reserve(textures.size());
	for(auto &tex : textures)
	{
		// Some models (e.g. tf2 models) contain the absolute texture path for some reason.
		// We'll strip it here if it's in one of our known texture paths.
		auto texPath = FileManager::GetCanonicalizedPath(tex);
		auto it = std::find_if(texPaths.begin(),texPaths.end(),[&texPath](const std::string &path) {
			return (ustring::substr(texPath,0,path.length()) == path) ? true : false;
		});
		if(it != texPaths.end())
			texPath = ustring::substr(texPath,it->length());
		textureTranslations.push_back(import::util::add_texture(*nw,mdl,texPath));
	}

	auto &skeleton = mdl.GetSkeleton();
	auto &skelBones = skeleton.GetBones();
	auto &bones = mdlInfo.bones;
	for(auto &bone : bones)
	{
		auto *mdlBone = new panima::Bone();
		mdlBone->name = bone->GetName();
		skeleton.AddBone(mdlBone);
	}
	
	for(auto &pp : mdlInfo.poseParameters)
	{
		auto start = pp.GetStart();
		auto end = pp.GetEnd();
		auto loopPoint = pp.GetLoop();
		auto loop = (loopPoint > start && loopPoint < end) ? false : true;
		if(ustring::compare<std::string>(pp.GetName(),"move_yaw",false)) // TODO: Dirty hack. Some models (e.g. gman.mdl) have no loop point for controllers that SHOULD have loop points (e.g. move_yaw). What to do about this? (Could change range of move_yaw from [0,360] to [-180,180] in-engine.)
			loop = true;
		mdl.AddBlendController(pp.GetName(),start,end,loop);
	}

	auto reference = pragma::animation::Animation::Create();
	mdl.AddAnimation("reference",reference); // Reference always has to be the first animation!
	auto bHasReferenceAnim = false;

	std::vector<decltype(anims.size())> animsWithSequences;
	animsWithSequences.reserve(mdlInfo.sequences.size());
	std::unordered_map<uint32_t,uint32_t> seqAnims; // Map sequence to animation
	std::vector<std::string> animNames(anims.size());
	auto fApplyAnimFlags = [](pragma::animation::Animation &anim,int32_t flags) {
		if(flags &STUDIO_LOOPING)
			anim.AddFlags(FAnim::Loop);
		if(flags &STUDIO_AUTOPLAY)
			anim.AddFlags(FAnim::Autoplay | FAnim::Loop); // Autoplay animations are always looped
		if(flags &STUDIO_DELTA)
			anim.AddFlags(FAnim::Gesture);
	};
	for(auto i=decltype(mdlInfo.sequences.size()){0};i<mdlInfo.sequences.size();++i)
	{
		auto &seq = mdlInfo.sequences.at(i);
		auto &animIndices = seq.GetAnimationIndices();
		auto &name = seq.GetName();
		auto &activityName = seq.GetActivityName();
		if(animIndices.empty())
			continue;
		auto animIdx = animIndices.front();
		if(animIdx >= animNames.size())
		{
			if(optLog)
				(*optLog)<<"WARNING: Sequence '"<<name<<"' references animation with index "<<animIdx<<", but only "<<animNames.size()<<" animations exist! Ignoring...\n";
			continue;
		}
		animNames.at(animIdx) = name;
		std::shared_ptr<pragma::animation::Animation> anim = nullptr;
		auto it = std::find(animsWithSequences.begin(),animsWithSequences.end(),static_cast<decltype(anims.size())>(animIdx));
		if(it == animsWithSequences.end())
		{
			animsWithSequences.push_back(animIdx);
			anim = anims[animIdx]; // Use existing anim
		}
		else // There was already a sequence with this animation; Copy it
		{
			anim = anims[animIdx];
			anim = pragma::animation::Animation::Create(*anim);//,Animation::ShareMode::Frames);
		}
		
		auto itAct = translateActivities.find(seq.GetActivityName());
		if(itAct != translateActivities.end() && itAct->second != Activity::Invalid)
			anim->SetActivity(itAct->second);
		else
			anim->SetActivity(static_cast<Activity>(pragma::animation::Animation::GetActivityEnumRegister().RegisterEnum(activityName)));
		anim->SetActivityWeight(seq.GetActivityWeight());
		anim->SetFadeInTime(seq.GetFadeInTime());
		anim->SetFadeOutTime(seq.GetFadeOutTime());
		anim->SetRenderBounds(seq.GetMin(),seq.GetMax());
		anim->GetBoneWeights() = seq.GetWeights();

		fApplyAnimFlags(*anim,seq.GetFlags());

		auto numFrames = anim->GetFrameCount();
		for(auto &ev : seq.GetEvents())
		{
			auto *animEv = new AnimationEvent();
			animEv->eventID = static_cast<AnimationEvent::Type>(ev.GetEvent());
			auto itTranslate = translateAnimEvent.find(umath::to_integral(animEv->eventID));
			if(itTranslate != translateAnimEvent.end())
				animEv->eventID = itTranslate->second;

			animEv->arguments.push_back(ev.GetOptions());
			auto frame = umath::round(ev.GetCycle() *numFrames);
			anim->AddEvent(frame,animEv);
		}
		auto newAnimId = mdl.AddAnimation(name,anim);
		seqAnims.insert(std::make_pair(i,newAnimId));
		if(name == "reference")
			bHasReferenceAnim = true;
	}

	for(auto i=decltype(anims.size()){0};i<anims.size();++i)
	{
		auto &anim = anims[i];
		if(anim == nullptr)
			continue;
		auto it = std::find(animsWithSequences.begin(),animsWithSequences.end(),i);
		if(it == animsWithSequences.end()) // Only add this animation if it hasn't already been added through a sequence
		{
			auto &name = animDescs.at(i).GetName();
			mdl.AddAnimation(name,anim);
			auto &desc = animDescs.at(i).GetStudioDesc();
			fApplyAnimFlags(*anim,desc.flags);
			if(name == "reference")
				bHasReferenceAnim = true;
			animNames.at(i) = name;
		}
	}

	// Blend controllers
	auto &mdlAnims = mdl.GetAnimations();
	for(auto i=decltype(mdlInfo.sequences.size()){0};i<mdlInfo.sequences.size();++i)
	{
		auto &seq = mdlInfo.sequences.at(i);
		auto &pp = seq.GetPoseParameter();
		if(pp.numBlends < 2)
			continue;
		auto it = seqAnims.find(i);
		if(it == seqAnims.end())
			continue;
		auto animIdx = it->second;
		auto &mdlAnim = mdlAnims.at(animIdx);

		auto animIndices = seq.GetAnimationIndices();
		for(auto &idx : animIndices)
		{
			auto &name = animNames.at(idx);
			idx = mdl.LookupAnimation(name);
		}
		/*auto animId = animIndices.at(i);
		if(animId == -1)
			continue;
		auto &name = animNames.at(animId);
		auto animDstId = mdl.LookupAnimation(name);*/


		if(animIndices.size() == 9)
		{
			if(pp.paramIdx.at(0) != -1 && pp.paramIdx.at(1) != -1)
			{
				auto ppMoveYIdx = pp.paramIdx.at(0);
				auto ppMoveXIdx = pp.paramIdx.at(1);
				// Seems to be always the same order?
				auto sw = animIndices.at(0);
				auto s = animIndices.at(1);
				auto se = animIndices.at(2);
				auto w = animIndices.at(3);
				auto c = animIndices.at(4);
				auto e = animIndices.at(5);
				auto nw = animIndices.at(6);
				auto n = animIndices.at(7);
				auto ne = animIndices.at(8);

				auto fPrint = [&mdl](const std::string &identifier,uint32_t i) {
					Con::cout<<identifier<<": "<<mdl.GetAnimationName(i)<<Con::endl;
				};
				fPrint("sw",sw);
				fPrint("s",s);
				fPrint("se",se);
				fPrint("w",w);
				fPrint("c",c);
				fPrint("e",e);
				fPrint("nw",nw);
				fPrint("n",n);
				fPrint("ne",ne);

				uint8_t iParam = 0;
				auto bcIdx = pp.paramIdx.at(iParam);
				auto start = pp.start.at(iParam);
				auto end = pp.end.at(iParam);

				auto &bc = mdlAnim->SetBlendController(bcIdx);
				const auto flags = STUDIO_AUTOPLAY | STUDIO_DELTA;
				auto bAutoplayGesture = (seq.GetFlags() &flags) == flags;
                std::array<uint32_t,9> blends = {
                                    static_cast<uint32_t>(s),
                                    static_cast<uint32_t>(se),
                                    static_cast<uint32_t>(e),
                                    static_cast<uint32_t>(ne),
                                    static_cast<uint32_t>(n),
                                    static_cast<uint32_t>(nw),
                                    static_cast<uint32_t>(w),
                                    static_cast<uint32_t>(sw),
                                    static_cast<uint32_t>(s)
                                    //s,sw,w,nw,n,ne,e,se,s
                                    //n,ne,e,se,s,sw,w,nw,n
                                };
				auto numBlends = blends.size(); // pp.numBlends
				for(auto i=decltype(numBlends){0};i<numBlends;++i)
				{
					auto animId = blends.at(i);//animIndices.at(i);
					//if(animId == -1)
					//	continue;
					//auto &name = animNames.at(animId);
					//auto animDstId = mdl.LookupAnimation(name);
					auto animDstId = animId;
					//std::cout<<"Assigning animation "<<mdl.GetAnimationName(animDstId)<<std::endl;
					if(animDstId == -1)
						continue;
					if(bAutoplayGesture == true)
						mdl.GetAnimation(animDstId)->AddFlags(FAnim::Loop);
					bc.transitions.push_back({});
					auto &t = bc.transitions.back();
					t.animation = animDstId;
					auto f = (i /static_cast<float>(numBlends -1));
					t.transition = umath::lerp(start,end,f);
					Con::cout<<animDstId<<": "<<t.transition<<Con::endl;
				}
				bc.animationPostBlendTarget = c;
				bc.animationPostBlendController = pp.paramIdx.at(1);
			}
		}
		else
		{
			// PP1: n, ne, e, se, s, sw, w, nw
			// PP2: -1 -> c?

			for(auto iParam=decltype(pp.paramIdx.size()){0u};iParam<pp.paramIdx.size();++iParam)
			{
				auto bcIdx = pp.paramIdx.at(iParam);
				if(bcIdx == -1)
					continue;
				// TODO: Add support for multiple blend controllers
				auto start = pp.start.at(iParam);
				auto end = pp.end.at(iParam);
				auto &bc = mdlAnim->SetBlendController(bcIdx);

				const auto flags = STUDIO_AUTOPLAY | STUDIO_DELTA;
				auto bAutoplayGesture = (seq.GetFlags() &flags) == flags;
				// If this is an autoplay gesture, all animations associated with this one should probably loop

				/*std::cout<<"Name: "<<seq.GetName()<<std::endl;
				for(auto idx : animIndices)
				{
					if(idx == -1)
						continue;
					auto &animName = animDescs.at(idx).GetName();
					auto &seqName = animNames.at(idx);
					std::cout<<"\tChild anim "<<idx<<": "<<animName<<" ("<<seqName<<")"<<std::endl;
				}*/
				// Note: The first animation in the blend array seems to be the base animation, which we don't need since we already
				// have that information. Instead, we'll just skip it altogether.
				// TODO: I'm not sure if this works for all models, but it works for the Scout model from TF2, which has the following pose parameters:
				/*
				move_y:
				[0]	488	short -> name = "run_PRIMARY"
				[1]	481	short -> name = "a_runS_PRIMARY"
				[2]	482	short -> name = "a_runSE_PRIMARY"
				[3]	487	short -> name = "a_runW_PRIMARY"
				[4]	480	short -> name = "a_runCenter_PRIMARY"
				[5]	483	short -> name = "a_runE_PRIMARY"
				[6]	486	short -> name = "a_runNW_PRIMARY"
				[7]	485	short -> name = "a_runN_PRIMARY"
				[8]	484	short -> name = "a_runNE_PRIMARY"

				move_x:
				[0]	488	short -> name = "run_PRIMARY"
				[1]	481	short -> name = "a_runS_PRIMARY"
				[2]	482	short -> name = "a_runSE_PRIMARY"
				[3]	487	short -> name = "a_runW_PRIMARY"
				[4]	480	short -> name = "a_runCenter_PRIMARY"
				[5]	483	short -> name = "a_runE_PRIMARY"
				[6]	486	short -> name = "a_runNW_PRIMARY"
				[7]	485	short -> name = "a_runN_PRIMARY"
				[8]	484	short -> name = "a_runNE_PRIMARY"
				*/
				for(auto i=decltype(pp.numBlends){0};i<pp.numBlends;++i)
				{
					auto animId = animIndices.at(i);
					if(animId == -1)
						continue;
					//std::cout<<"Assigning animation "<<mdl.GetAnimationName(animId)<<std::endl;
					if(animId == -1)
						continue;
					if(bAutoplayGesture == true)
						mdl.GetAnimation(animId)->AddFlags(FAnim::Loop);
					bc.transitions.push_back({});
					auto &t = bc.transitions.back();
					t.animation = animId;
					auto f = (pp.numBlends > 1) ? (i /static_cast<float>(pp.numBlends -1)) : 0.f;
					t.transition = umath::lerp(start,end,f);
				}
			}
		}
	}
	//

	// Flex Controllers
	auto &fcs = mdl.GetFlexControllers();
	fcs.resize(mdlInfo.flexControllers.size());
	auto flexIdx = 0u;
	for(auto &fc : mdlInfo.flexControllers)
	{
		auto &outFc = fcs.at(flexIdx++);
		outFc.name = fc.GetName();
		auto range = fc.GetRange();
		outFc.min = range.first;
		outFc.max = range.second;
	}
	//

	// Flexes
	auto &flexes = mdl.GetFlexes();
	flexes.reserve(mdlInfo.flexDescs.size());
	flexIdx = 0u;
	for(auto &flexDesc : mdlInfo.flexDescs)
	{
		flexes.push_back(Flex{flexDesc.GetName()});
		auto &flex = flexes.back();
		auto it = std::find_if(mdlInfo.flexRules.begin(),mdlInfo.flexRules.end(),[flexIdx](const import::mdl::FlexRule &rule) {
			return (rule.GetFlexId() == flexIdx) ? true : false;
		});
		if(it != mdlInfo.flexRules.end())
		{
			auto &rule = *it;
			auto &ops = flex.GetOperations();
			auto &ruleOps = rule.GetOperations();
			ops.reserve(ruleOps.size());
			for(auto &ruleOp : ruleOps)
			{
				ops.push_back({});
				auto &op = ops.back();
				op.type = static_cast<Flex::Operation::Type>(ruleOp.type);
				op.d.value = ruleOp.d.value;
			}
		}
		++flexIdx;
	}
	//

	auto &rootBones = skeleton.GetRootBones();
	if(header.numbones > 0)
		rootBones.clear();

	for(auto i=decltype(header.numbones){0};i<header.numbones;++i)
	{
		auto &skelBone = skelBones[i];
		auto &origBone = bones[i];

		auto &origParent = origBone->GetParent();
		if(origParent != nullptr)
		{
			auto &skelParent = skelBones[origParent->GetID()];
			skelBone->parent = skelParent;
			skelParent->children[i] = skelBone;
		}
		else
			rootBones[i] = skelBone;
	}

	auto refId = mdl.LookupAnimation("reference");
	std::shared_ptr<Frame> frameReference = nullptr;
	auto bGlobalizeReference = false;
	if(refId == -1 || bHasReferenceAnim == false)
	{
		if(optLog)
			(*optLog)<<"WARNING: No reference animation found; Attempting to generate reference from default bone transforms...\n";
		reference = pragma::animation::Animation::Create();
		mdl.AddAnimation("reference",reference);
	}
	else
	{
		// Note: We're not actually using the model's existing reference animation and instead generate our own,
		// since that is more reliable in most cases.
		// This can, however, cause issues as well, for example:
		// Model: https://steamcommunity.com/sharedfiles/filedetails/?id=2360039741
		// The model has a deformed reference pose, which doesn't match the actual bone poses.
		// To correct it, the model also has an auto-playing layered animation,which effectively cancels out
		// the deformation caused by the reference pose.
		// However, since we're generating our own reference pose (which doesn't have the deformation),
		// the layered animation will cause a deformation of its own.
		// A temporary work-around is to stop the layered animation on all actors using the model,
		// but a proper fix should be added in the future.

		reference = mdl.GetAnimation(refId);
		//frameReference = reference->GetFrame(0);
		//bGlobalizeReference = true;
	}
	reference->GetFrames().clear();

	std::vector<uint16_t> refBoneList;
	refBoneList.reserve(header.numbones);
	for(auto i=decltype(header.numbones){0};i<header.numbones;++i)
		refBoneList.push_back(i);
	reference->SetBoneList(refBoneList);

	frameReference = Frame::Create(header.numbones);
	for(auto i=decltype(header.numbones){0};i<header.numbones;++i)
	{
		auto &origBone = bones[i];
		auto &pos = origBone->GetPos();
		auto &ang = origBone->GetAngles();
		//auto rot = origBone->GetRot();//glm::toQuat(fAngToMat(ang));
		//auto m = mdl::util::euler_angles_to_matrix(ang);
		//auto rot = mdl::util::get_rotation(m);
		//auto rot = Quat{0.f,ang.x,ang.y,ang.z};
		//uquat::normalize(rot);
		//auto rot = Quat{0.0,ang.x,ang.y,ang.z};
		auto rot = import::mdl::quaternion_to_euler_angles(origBone->GetRot());
		Vector3 bonePos {-pos.x,pos.y,pos.z};
		Quat boneRot {0.0,rot.x,rot.y,rot.z};

		frameReference->SetBonePosition(i,bonePos);
		frameReference->SetBoneOrientation(i,boneRot);
	}
	reference->AddFrame(frameReference);

	/*auto &anims = mdl.GetAnimations();
	//for(auto i=decltype(anims.size()){0};i<anims.size() -1;++i) // Last animation is reference (TODO: Is that a problem?)
	{
		auto i = mdl.LookupAnimation("attack1");
	//	auto rotAdd = uquat::create(EulerAngles(90,180.f,0));
		auto &anim = anims[i];
		for(auto &frame : anim->GetFrames())
		{
			frame->Globalize(anim.get(),&skeleton);
			for(auto j=decltype(header.numbones){0};j<header.numbones;++j)
			{
				auto &pos = *frame->GetBonePosition(j);
				//uvec::rotate(&pos,rotAdd);
				auto &rot = *frame->GetBoneOrientation(j);
				//rot = rotAdd *rot;
				//rot.w = -rot.w;
				//rot.y = -rot.y;
			}
			frame->Localize(anim.get(),&skeleton);
		}
	}*/


	/*auto bFirst = true;
	for(auto &anim : mdl.GetAnimations())
	{
		if(bFirst == true)
		{
			bFirst = false;
			continue;
		}
		for(auto &frame : anim->GetFrames())
		{
			for(auto i=0;i<header.numbones;++i)
			{
				frame->SetBonePosition(i,*refPose->GetBonePosition(i));
				frame->SetBoneOrientation(i,*refPose->GetBoneOrientation(i));
			}
			//frame->Localize(anim.get(),&skeleton);
		}
	}*/ // ????????

	//auto mesh = std::shared_ptr<ModelMesh>(nw->CreateMesh());
	//auto subMesh = std::shared_ptr<ModelSubMesh>(nw->CreateSubMesh());
	//auto &triangles = subMesh->GetTriangles();
	//auto &verts = subMesh->GetVertices();
	//auto &vertWeights = subMesh->GetVertexWeights();

	std::vector<umath::Vertex> vvdVerts;
	std::vector<umath::VertexWeight> vvdVertWeights;
	it = files.find("vvd");
	if(it != files.end())
	{
		import::mdl::load_vvd(it->second,vvdVerts,vvdVertWeights,mdlInfo.fixedLodVertexIndices);
		if(mdlInfo.header.flags &STUDIOHDR_FLAGS_STATIC_PROP)
		{
			for(auto &v : vvdVerts)
			{
				umath::swap(v.position.x,v.position.y);
				umath::negate(v.position.y);

				umath::swap(v.normal.x,v.normal.y);
				umath::negate(v.normal.y);
			}
		}
	}

	struct MeshBodyPartInfo
	{
		MeshBodyPartInfo(ModelMeshGroup &mg,ModelMesh &m,ModelSubMesh &sm,uint32_t bpIdx,uint32_t mIdx,uint32_t msIdx)
			: meshGroup(mg),mesh(m),subMesh(sm),bodyPartIdx(bpIdx),mdlIdx(mIdx),meshIdx(msIdx)
		{}
		ModelMeshGroup &meshGroup;
		ModelMesh &mesh;
		ModelSubMesh &subMesh;
		uint32_t bodyPartIdx = std::numeric_limits<uint32_t>::max();
		uint32_t mdlIdx = std::numeric_limits<uint32_t>::max();
		uint32_t meshIdx = std::numeric_limits<uint32_t>::max();
	};
	auto &bodyParts = mdlInfo.bodyParts;
	std::vector<MeshBodyPartInfo> meshToBodyPart {};
	std::vector<mdl::vtx::VtxBodyPart> vtxBodyParts;
	it = files.find("vtx");
	std::vector<std::shared_ptr<ModelSubMesh>> skipSharedMeshTransforms {}; // Skip vertex transformations for these meshes, because they use shared vertices from other meshes

	auto fMaxEyeDeflection = 30.f;
	if(mdlInfo.header2.has_value() && mdlInfo.header2->flMaxEyeDeflection != 0.f)
		fMaxEyeDeflection = umath::rad_to_deg(mdlInfo.header2->flMaxEyeDeflection);
	mdl.SetMaxEyeDeflection(fMaxEyeDeflection);

	// TODO: Add LOD info
	std::vector<uint32_t> bgBaseMeshGroups = {};
	std::vector<uint32_t> fixedLod0IndicesToPragmaModelIndices {};
	if(mdlInfo.fixedLodVertexIndices.empty() == false)
		fixedLod0IndicesToPragmaModelIndices.resize(mdlInfo.fixedLodVertexIndices.front().size());
	else
		fixedLod0IndicesToPragmaModelIndices.resize(vvdVerts.size());
	if(it != files.end())
	{
		auto bVTX = import::mdl::load_vtx(it->second,vtxBodyParts);
		if(bVTX == true && !vtxBodyParts.empty())
		{
			uint32_t bodyPartVertexIndexStart = 0;
			for(auto bodyPartIdx=decltype(vtxBodyParts.size()){0};bodyPartIdx<vtxBodyParts.size();++bodyPartIdx)
			{
				auto &vtxBodyPart = vtxBodyParts[bodyPartIdx]; // aBodyPart
				auto &vtxModels = vtxBodyPart.models;

				auto &bodyPart = bodyParts[bodyPartIdx];
				auto &models = bodyPart.GetModels();

				auto bgName = bodyPart.GetName();
				if(mdl.GetBodyGroupId(bgName) != -1)
				{
					// Bodygroup with this name already exists!
					// Source allows multiple bodygroups with the same name,
					// but Pragma does not, so we'll have to rename the bodygroup
					// to something else.
					std::string newBgName;
					uint32_t bgIndex = 2;
					do
					{
						newBgName = bgName +std::to_string(bgIndex++);
					}
					while(mdl.GetBodyGroupId(newBgName) != -1);
					bgName = std::move(newBgName);
				}
				auto &bgroup = mdl.AddBodyGroup(bgName);
				for(auto mdlIdx=decltype(vtxModels.size()){0};mdlIdx<vtxModels.size();++mdlIdx)
				{
					auto &vtxModel = vtxModels[mdlIdx]; // aVtxModel
					auto &model = models[mdlIdx];

					for(auto &bpEyeball : model.eyeballs)
					{
						auto &stdEyeball = bpEyeball.stdEyeball;

						const auto fConvertVertex = [](const Vector3 &v) {
							return Vector3{v.x,-v.z,v.y};
						};

						Eyeball eyeball {};
						eyeball.name = bpEyeball.name;
						eyeball.boneIndex = stdEyeball.bone;
						eyeball.origin = fConvertVertex(stdEyeball.org);
						eyeball.zOffset = stdEyeball.zoffset;
						eyeball.radius = stdEyeball.radius;
						eyeball.up = fConvertVertex(stdEyeball.up);
						eyeball.forward = fConvertVertex(stdEyeball.forward);
						eyeball.irisMaterialIndex = stdEyeball.texture; // This appears to be always 0, will be reassigned via the meshes below

						eyeball.irisScale = stdEyeball.iris_scale;

						eyeball.upperLid.raiserFlexIndex = stdEyeball.upperflexdesc[0];
						eyeball.upperLid.neutralFlexIndex = stdEyeball.upperflexdesc[1];
						eyeball.upperLid.lowererFlexIndex = stdEyeball.upperflexdesc[2];
						eyeball.upperLid.raiserValue = stdEyeball.uppertarget[0];
						eyeball.upperLid.neutralValue = stdEyeball.uppertarget[1];
						eyeball.upperLid.lowererValue = stdEyeball.uppertarget[2];
						eyeball.upperLid.lidFlexIndex = stdEyeball.upperlidflexdesc;

						eyeball.lowerLid.raiserFlexIndex = stdEyeball.lowerflexdesc[0];
						eyeball.lowerLid.neutralFlexIndex = stdEyeball.lowerflexdesc[1];
						eyeball.lowerLid.lowererFlexIndex = stdEyeball.lowerflexdesc[2];
						eyeball.lowerLid.raiserValue = stdEyeball.lowertarget[0];
						eyeball.lowerLid.neutralValue = stdEyeball.lowertarget[1];
						eyeball.lowerLid.lowererValue = stdEyeball.lowertarget[2];
						eyeball.lowerLid.lidFlexIndex = stdEyeball.lowerlidflexdesc;

						// These are always the same in Source
						eyeball.maxDilationFactor = 1.f;
						eyeball.irisUvRadius = 0.2f;

						mdl.AddEyeball(eyeball);
					}
					
					for(auto lodIdx=decltype(vtxModel.lods.size()){0};lodIdx<vtxModel.lods.size();++lodIdx)
					{
						auto bgLodName = bgName +"_" +((lodIdx > 0) ? (std::string("lod") +std::to_string(lodIdx)) : "reference");
						auto subId = 2u;
						while(mdl.GetMeshGroup(bgLodName) != nullptr)
							bgLodName = bgName +std::to_string(subId++) +"_" +((lodIdx > 0) ? (std::string("lod") +std::to_string(lodIdx)) : "reference");
						uint32_t meshGroupId = std::numeric_limits<uint32_t>::max();
						auto meshGroup = mdl.AddMeshGroup(bgLodName,meshGroupId);
						if(lodIdx == 0u)
						{
							auto it = std::find(bgroup.meshGroups.begin(),bgroup.meshGroups.end(),meshGroupId);
							if(it == bgroup.meshGroups.end())
								bgroup.meshGroups.push_back(meshGroupId);
							if(mdlIdx == 0u)
							{
								it = std::find(bgBaseMeshGroups.begin(),bgBaseMeshGroups.end(),meshGroupId);
								if(it == bgBaseMeshGroups.end())
									bgBaseMeshGroups.push_back(meshGroupId);
							}
						}
						auto lodMesh = std::shared_ptr<ModelMesh>(nw->CreateMesh());

						auto &vtxLod = vtxModel.lods[lodIdx];
						for(auto meshIdx=decltype(vtxLod.meshes.size()){0};meshIdx<vtxLod.meshes.size();++meshIdx)
						{
							auto lodSubMesh = std::shared_ptr<ModelSubMesh>(nw->CreateSubMesh());
							auto &verts = lodSubMesh->GetVertices();
							auto &vertWeights = lodSubMesh->GetVertexWeights();
							std::unordered_map<uint16_t,uint16_t> newIndices;

							auto &vtxMesh = vtxLod.meshes[meshIdx];
							auto &mesh = model.meshes[meshIdx];
							if(mesh.stdMesh.materialtype == 1)
							{
								auto &eyeball = *mdl.GetEyeball(mesh.stdMesh.materialparam);
								eyeball.irisMaterialIndex = mesh.stdMesh.material;
							}

							if(lodIdx == 0u)
							{
								if(meshToBodyPart.size() == meshToBodyPart.capacity())
									meshToBodyPart.reserve(meshToBodyPart.size() +50);
								meshToBodyPart.push_back({*meshGroup,*lodMesh,*lodSubMesh,static_cast<uint32_t>(bodyPartIdx),static_cast<uint32_t>(mdlIdx),static_cast<uint32_t>(meshIdx)});
							}

							lodSubMesh->SetSkinTextureIndex(mesh.stdMesh.material);
							auto meshVertexIndexStart = mesh.stdMesh.vertexoffset;
							for(auto &stripGroup : vtxMesh.stripGroups)
							{
								lodSubMesh->ReserveIndices(lodSubMesh->GetIndexCount() +stripGroup.indices.size());
								for(auto i=decltype(stripGroup.indices.size()){0};i<stripGroup.indices.size();i++)
								{
									auto vtxIdx0 = stripGroup.indices[i];
									auto &vtxVert = stripGroup.stripVerts[vtxIdx0];
									auto vertIdx = vtxVert.origMeshVertID +meshVertexIndexStart +bodyPartVertexIndexStart;
									auto originalIdx = vertIdx;
									if(mdlInfo.fixedLodVertexIndices.empty() == false)
									{
										auto &lodVertIndices = mdlInfo.fixedLodVertexIndices.front();
										vertIdx = lodVertIndices[vertIdx];
									}
									auto fixedIdx = vertIdx;
									auto it = newIndices.find(vertIdx);
									if(it == newIndices.end()) // New unique vertex
									{
										if(vertIdx < vvdVerts.size() && vertIdx < vvdVertWeights.size())
										{
											auto &vvdVert = vvdVerts[vertIdx];
											auto &vvdWeight = vvdVertWeights[vertIdx];
											verts.push_back(vvdVert);
											vertWeights.push_back(vvdWeight);
										}
										else
										{
											// Invalid vertex?
											// (Happens for https://sfmlab.com/project/32816/ , reason unknown)
											verts.push_back({});
											vertWeights.push_back({});
										}

										vertIdx = newIndices[vertIdx] = verts.size() -1;
									}
									else
										vertIdx = it->second;
									if(lodIdx == 0)
									{
										// Invalid vertex?
										// (Happens for https://sfmlab.com/project/32816/ , reason unknown)
										if(fixedIdx < fixedLod0IndicesToPragmaModelIndices.size())
											fixedLod0IndicesToPragmaModelIndices.at(fixedIdx) = vertIdx;
									}
									lodSubMesh->AddIndex(vertIdx);
								}
							}

							// Swap triangle winding order
							lodSubMesh->VisitIndices([](auto *indexData,uint32_t numIndices) {
								for(auto i=decltype(numIndices){};i<numIndices;i+=3)
									umath::swap(indexData[i],indexData[i +1]);
							});

							lodMesh->AddSubMesh(lodSubMesh);
							lodSubMesh->Update();
						}
						meshGroup->AddMesh(lodMesh);
						lodMesh->Update();

						if(lodIdx > 0)
						{
							std::unordered_map<uint32_t,uint32_t> meshIds = {};
							auto meshGroupIdx = lodIdx;
							meshIds[0] = meshGroupIdx;
							mdl.AddLODInfo(lodIdx,vtxLod.header.switchPoint,meshIds);
						}
					}
					bodyPartVertexIndexStart += model.vertexCount;
				}
			}
			/*for(auto lod=decltype(bodyPart.lodMeshes.size()){0};lod<bodyPart.lodMeshes.size();++lod)
			{
				auto &bodyPartLod = bodyPart.lodMeshes[lod];

				auto meshGroup = mdl.AddMeshGroup((lod > 0) ? (std::string("lod") +std::to_string(lod)) : "reference");
				auto lodMesh = std::shared_ptr<ModelMesh>(nw->CreateMesh());
				auto lodSubMesh = std::shared_ptr<ModelSubMesh>(nw->CreateSubMesh());

				lodSubMesh->GetTriangles() = bodyPartLod.indices;
				if(mdlInfo.fixedLodVertexIndices.empty())
				{
					if(lod == 0)
					{
						lodSubMesh->GetVertices() = vvdVerts;
						lodSubMesh->GetVertexWeights() = vvdVertWeights;
						rootMesh = lodSubMesh;
					}
					else
					{
						lodSubMesh->SetShared(*rootMesh,ModelSubMesh::ShareMode::All & ~ModelSubMesh::ShareMode::Triangles);
						skipSharedMeshTransforms.push_back(lodSubMesh);
					}
				}
				else
				{
					auto &fixedVerts = mdlInfo.fixedLodVertexIndices[lod];
					auto &verts = lodSubMesh->GetVertices();
					auto &vertWeights= lodSubMesh->GetVertexWeights();
					verts.reserve(fixedVerts.size());
					vertWeights.reserve(fixedVerts.size());
					for(auto idx : fixedVerts)
					{
						verts.push_back(vvdVerts[idx]);
						vertWeights.push_back(vvdVertWeights[idx]);
					}
				}
				
				lodSubMesh->SetTexture(0); // TODO? CHECK MULTIPLE TEX MODELS
				lodMesh->AddSubMesh(lodSubMesh);
				meshGroup->AddMesh(lodMesh);

				if(lod > 0)
				{
					std::unordered_map<uint32_t,uint32_t> meshIds = {};
					meshIds[0] = lod;
					mdl.AddLODInfo(lod,meshIds);
				}
				lodSubMesh->Update();
				lodMesh->Update();
			}*/
		}
	}

	// Vertex Animations
	struct PairInfo
	{
		PairInfo(ModelMesh &m,ModelSubMesh &sm)
			: mesh(m),subMesh(sm)
		{}
		ModelMesh &mesh;
		ModelSubMesh &subMesh;
		uint32_t pairId = std::numeric_limits<uint32_t>::max();
		std::vector<Vector3> pairTransforms = {};
	};
	std::unordered_map<uint32_t,PairInfo> flexPairs;
	auto bpIdx = 0u;
	uint32_t mdlVertexOffset = 0u;
	for(auto &bp : mdlInfo.bodyParts)
	{
		auto mdlIdx = 0u;
		for(auto &bpMdl : bp.GetModels())
		{
			auto meshIdx = 0u;
			for(auto &mesh : bpMdl.meshes)
			{
				auto it = std::find_if(meshToBodyPart.begin(),meshToBodyPart.end(),[bpIdx,mdlIdx,meshIdx](const MeshBodyPartInfo &info) {
					return (bpIdx == info.bodyPartIdx && mdlIdx == info.mdlIdx && meshIdx == info.meshIdx) ? true : false;
				});
				if(it == meshToBodyPart.end())
				{
					++meshIdx;
					continue;
				}
				auto &info = *it;
				auto &meshFlexes = mesh.flexes;
				for(auto &meshFlex : meshFlexes)
				{
					auto &stdFlex = meshFlex.stdFlex;
					auto isWrinkle = (stdFlex.vertanimtype == import::mdl::StudioVertAnimType_t::STUDIO_VERT_ANIM_WRINKLE);
					auto &vertexTransforms = meshFlex.vertAnim;
					auto &stdFlexDesc = mdlInfo.flexDescs.at(stdFlex.flexdesc);
					auto vertAnim = mdl.AddVertexAnimation("flex_" +stdFlexDesc.GetName());
					auto meshFrame = vertAnim->AddMeshFrame(info.mesh,info.subMesh);
					meshFrame->SetFlagEnabled(MeshVertexFrame::Flags::HasNormals);
					if(stdFlex.flexdesc < flexes.size())
					{
						auto &flex = flexes.at(stdFlex.flexdesc);
						flex.SetVertexAnimation(*vertAnim);
					}

					auto *flexLowerer = mdlInfo.model.GetFlex(stdFlexDesc.GetName() +"_lowerer");
					auto *flexRaiser = mdlInfo.model.GetFlex(stdFlexDesc.GetName() +"_raiser");
					if(flexLowerer != nullptr && flexRaiser != nullptr) // Probably an eyelid?
					{
						// Assign frame 0 to lowerer and frame 1 to raiser, that seems to do the trick
						if(flexLowerer != nullptr)
							flexLowerer->SetVertexAnimation(*vertAnim,0);

						if(flexRaiser != nullptr)
							flexRaiser->SetVertexAnimation(*vertAnim,1);
					}
					/*auto *flexNeutral = mdlInfo.model.GetFlex(stdFlexDesc.GetName() +"_neutral");
					if(flexNeutral != nullptr)
					{
						auto &meshAnim = *vertAnim->GetMeshAnimation(info.subMesh);
						if(flexNeutral != nullptr && meshAnim.GetFrames().size() >= 2)
							flexNeutral->SetVertexAnimation(*vertAnim,meshAnim,*meshAnim.GetFrame(1));
					}*/

					VertexAnimation *pairVertAnim = nullptr;
					MeshVertexFrame *pairMeshFrame = nullptr;
					if(stdFlex.flexpair != 0)
					{
						auto &stdFlexDescPair = mdlInfo.flexDescs.at(stdFlex.flexpair);
						pairVertAnim = mdl.AddVertexAnimation("flex_" +stdFlexDescPair.GetName()).get();
						pairMeshFrame = pairVertAnim->AddMeshFrame(info.mesh,info.subMesh).get();
						if(stdFlex.flexpair < flexes.size())
						{
							auto &flex = flexes.at(stdFlex.flexpair);
							flex.SetVertexAnimation(*pairVertAnim);
						}
					}
					
					for(auto &t : vertexTransforms)
					{
						auto fixedVertIdx = t->index +mesh.stdMesh.vertexoffset +mdlVertexOffset;
						if(mdlInfo.fixedLodVertexIndices.empty() == false)
							fixedVertIdx = mdlInfo.fixedLodVertexIndices.front().at(fixedVertIdx);
						
						if(fixedVertIdx < fixedLod0IndicesToPragmaModelIndices.size())
						{
							auto vertIdx = fixedLod0IndicesToPragmaModelIndices.at(fixedVertIdx);

							auto &vvdVert = vvdVerts.at(fixedVertIdx);
							auto side = t->side /255.f;
							auto scale = 1.f -side;
							float vertAnimFixedPointScale;
							if(header.flags &STUDIOHDR_FLAGS_VERT_ANIM_FIXED_POINT_SCALE)
								vertAnimFixedPointScale = header.flVertAnimFixedPointScale;
							else
								vertAnimFixedPointScale = 1.f /4'096.f;

							Vector3 v{float16::Convert16bitFloatTo32bits(t->flDelta.at(1)),float16::Convert16bitFloatTo32bits(t->flDelta.at(2)),float16::Convert16bitFloatTo32bits(t->flDelta.at(0))};
							v = {v.z,v.y,-v.x}; // Determined by testing
							if(pairMeshFrame != nullptr)
							{
								pairMeshFrame->SetVertexPosition(vertIdx,v *(1.f -scale));
								v *= scale; // TODO: Unsure if this is correct
							}
							meshFrame->SetVertexPosition(vertIdx,v);

							Vector3 n {float16::Convert16bitFloatTo32bits(t->flNDelta.at(1)),float16::Convert16bitFloatTo32bits(t->flNDelta.at(2)),float16::Convert16bitFloatTo32bits(t->flNDelta.at(0))};
							n = {n.z,n.y,-n.x}; // TODO: I have no idea if this is correct
							if(pairMeshFrame != nullptr)
							{
								auto npair = n *(1.f -scale); // TODO: I'm even less sure that this is correct
								pairMeshFrame->SetVertexNormal(vertIdx,npair);
								n *= scale; // TODO: Same here
							}
							meshFrame->SetVertexNormal(vertIdx,n);

							if(stdFlex.vertanimtype == import::mdl::StudioVertAnimType_t::STUDIO_VERT_ANIM_WRINKLE)
							{
								auto &wrinkle = static_cast<import::mdl::mstudiovertanim_wrinkle_t&>(*t);
								auto wrinkleDelta = wrinkle.wrinkledelta *vertAnimFixedPointScale;

								//auto wrinkleDelta = float16::Convert16bitFloatTo32bits(wrinkle.wrinkledelta);
								if(wrinkleDelta != 0.f)
									meshFrame->SetFlagEnabled(MeshVertexFrame::Flags::HasDeltaValues);
								meshFrame->SetDeltaValue(vertIdx,wrinkleDelta);
							}
							else if(stdFlex.vertanimtype == import::mdl::StudioVertAnimType_t::STUDIO_VERT_ANIM_NORMAL)
							{

							}
							// TODO: The delta values (pos/norm) should have to be multiplied by vertAnimFixedPointScale, but
							// for some reason they don't?
						}
						else
						{
							if(optLog)
								(*optLog)<<"WARNING: Missing flex vertex "<<fixedVertIdx<<" for flex "<<stdFlexDesc.GetName()<<"! Skipping...\n";
						}
					}
				}
				++meshIdx;
			}
			mdlVertexOffset += bpMdl.vertexCount;
			++mdlIdx;
		}
		++bpIdx;
	}

	//for(auto &pair : flexPairs)
	//{
	//	auto &pairInfo = pair.second; // TODO: Use these vertices
	//	auto &stdFlexDesc0 = mdlInfo.flexDescs.at(pair.first);
	//	auto &stdFlexDesc1 = mdlInfo.flexDescs.at(pairInfo.pairId);
	//	auto va1 = mdl.AddVertexAnimation(stdFlexDesc1.GetName());
	//	auto *va0 = mdl.GetVertexAnimation(stdFlexDesc0.GetName());
	//	if(va0 == nullptr || va1 == nullptr)
	//		continue;
	//	auto &meshAnims1 = va1->GetMeshAnimations();
	//	auto maIdx = 0u;
	//	for(auto &ma0 : (*va0)->GetMeshAnimations())
	//	{
	//		auto frameIdx = 0u;
	//		for(auto &frame0 : ma0->GetFrames())
	//		{
	//			std::shared_ptr<MeshVertexFrame> frame1 = nullptr;
	//			if(maIdx < meshAnims1.size())
	//			{
	//				auto &ma1 = meshAnims1.at(maIdx);
	//				auto &frames1 = ma1->GetFrames();
	//				if(frameIdx < frames1.size())
	//					frame1 = frames1.at(frameIdx);
	//			}
	//			if(frame1 == nullptr)
	//				frame1 = va1->AddMeshFrame(*ma0->GetMesh(),*ma0->GetSubMesh());

	//			auto numVerts = umath::min(frame0->GetVertices().size(),frame1->GetVertices().size());
	//			for(auto i=decltype(numVerts){0};i<numVerts;++i)
	//			{
	//				Vector3 v0;
	//				Vector3 v1;
	//				if(frame0->GetVertexPosition(i,v0) == false || frame1->GetVertexPosition(i,v1) == false)
	//					continue;
	//				frame1->SetVertexPosition(i,v0);
	//				/*if(v0.x < 0.f)
	//					frame1->SetVertexPosition(i,Vector3{});
	//				else
	//				{
	//					frame1->SetVertexPosition(i,v0);
	//					frame0->SetVertexPosition(i,Vector3{});
	//				}*/
	//			}
	//			++frameIdx;
	//		}
	//		++maIdx;
	//	}
	//}
	//


	/*if(mdlInfo.fixedLodVertexIndices.empty())
	{
		verts = vvdVerts;
		vertWeights = vvdVertWeights;
	}
	else
	{
		auto &fixedVerts = mdlInfo.fixedLodVertexIndices.front(); // TODO: What about other LODs?
		verts.reserve(fixedVerts.size());
		vertWeights.reserve(fixedVerts.size());
		for(auto idx : fixedVerts)
		{
			verts.push_back(vvdVerts[idx]);
			vertWeights.push_back(vvdVertWeights[idx]);
		}
	}*/

	std::unordered_map<std::string,std::vector<std::pair<std::string,float>>> defaultPhonemes = {
			{"sh",{
					{"puckerer",0},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0.400000006},
					{"left_funneler",0.400000006},
					{"right_funneler",0.400000006},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"<sil>",{
					{"puckerer",0},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"ow",{
					{"left_puckerer",0.6000000238},
					{"right_puckerer",0.6000000238},
					{"bite",0},
					{"left_mouth_drop",0.5400000215},
					{"right_mouth_drop",0.5400000215},
					{"tightener",0.5299999714},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.4699999988},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"r2",{
					{"left_puckerer",0.75},
					{"right_puckerer",0.75},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.25},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"th",{
					{"puckerer",0},
					{"bite",0.1000000015},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0.1299999952},
					{"left_funneler",0.5},
					{"right_funneler",0.5},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"left_part",0.2899999917},
					{"right_part",0.2899999917},
					{"chin_raiser",0.2599999905},
			}},
			{"b",{
					{"puckerer",0},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",1},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.1000000015},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"r3",{
					{"left_puckerer",1},
					{"right_puckerer",1},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.25},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"iy",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"left_stretcher",0.7799999714},
					{"right_stretcher",0.7799999714},
					{"presser",0},
					{"jaw_clencher",0.5500000119},
					{"left_funneler",0.375},
					{"right_funneler",0.375},
					{"jaw_drop",0.150000006},
					{"upper_raiser",0},
					{"left_part",1},
					{"right_part",1},
					{"chin_raiser",0},
			}},
			{"r",{
					{"left_puckerer",0.5099999905},
					{"right_puckerer",0.5099999905},
					{"bite",0},
					{"left_mouth_drop",0.0500000007},
					{"right_mouth_drop",0.0500000007},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"t",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"left_stretcher",0.4399999976},
					{"right_stretcher",0.4399999976},
					{"presser",0},
					{"jaw_clencher",1},
					{"left_funneler",0.2199999988},
					{"right_funneler",0.2199999988},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"left_part",1},
					{"right_part",1},
					{"chin_raiser",0},
			}},
			{"ao",{
					{"left_puckerer",0.5},
					{"right_puckerer",0.5},
					{"bite",0},
					{"left_mouth_drop",0.6000000238},
					{"right_mouth_drop",0.6000000238},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"left_funneler",0.25},
					{"right_funneler",0.25},
					{"jaw_drop",0.375},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"aa2",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.5},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"n",{
					{"puckerer",0},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"left_stretcher",0.3000000119},
					{"right_stretcher",0.3000000119},
					{"presser",0},
					{"jaw_clencher",0.200000003},
					{"funneler",0},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"left_part",0.8899999857},
					{"right_part",0.8899999857},
					{"chin_raiser",0},
			}},
			{"aa",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.400000006},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"g",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.150000006},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"w",{
					{"left_puckerer",0.8000000119},
					{"right_puckerer",0.8000000119},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0.4499999881},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"left_funneler",0.1000000015},
					{"right_funneler",0.075000003},
					{"jaw_drop",0.3249999881},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"d2",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0.4499999881},
					{"funneler",0},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"left_part",1},
					{"right_part",1},
					{"chin_raiser",0},
			}},
			{"ih2",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",0.8799999952},
					{"right_mouth_drop",0.8799999952},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.3249999881},
					{"upper_raiser",0},
					{"left_part",0.400000006},
					{"right_part",0.400000006},
					{"chin_raiser",0},
			}},
			{"hh2",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",0.6499999762},
					{"right_mouth_drop",0.6499999762},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.5},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"f",{
					{"puckerer",0},
					{"bite",0.7400000095},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"left_funneler",0.1000000015},
					{"right_funneler",0},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"y",{
					{"puckerer",0},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0.349999994},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0.3000000119},
					{"funneler",0},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"left_part",1},
					{"right_part",1},
					{"chin_raiser",0},
			}},
			{"v",{
					{"puckerer",0},
					{"bite",1},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"zh",{
					{"puckerer",0},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0.5500000119},
					{"left_funneler",0.5},
					{"right_funneler",0.5},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"nx",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"left_stretcher",0.400000006},
					{"right_stretcher",0.400000006},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.1000000015},
					{"upper_raiser",0},
					{"left_part",0.4499999881},
					{"right_part",0.4499999881},
					{"chin_raiser",0},
			}},
			{"c",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0.25},
					{"funneler",0},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"left_part",0.8500000238},
					{"right_part",0.8500000238},
					{"chin_raiser",0},
			}},
			{"ey",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"left_stretcher",0.3000000119},
					{"right_stretcher",0.3000000119},
					{"presser",0},
					{"jaw_clencher",0},
					{"left_funneler",0.349999994},
					{"right_funneler",0.3249999881},
					{"jaw_drop",0.150000006},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"g2",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.150000006},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"hh",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",0.5},
					{"right_mouth_drop",0.5},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.400000006},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"eh",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"left_stretcher",0.6000000238},
					{"right_stretcher",0.6000000238},
					{"presser",0},
					{"jaw_clencher",0},
					{"left_funneler",0.25},
					{"right_funneler",0.25},
					{"jaw_drop",0.200000003},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"jh",{
					{"left_puckerer",1},
					{"right_puckerer",1},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0.349999994},
					{"left_funneler",0.150000006},
					{"right_funneler",0.150000006},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"left_part",1},
					{"right_part",1},
					{"chin_raiser",0},
			}},
			{"z",{
					{"puckerer",0},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"left_stretcher",0.5},
					{"right_stretcher",0.5},
					{"presser",0},
					{"jaw_clencher",0.9900000095},
					{"funneler",0},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"left_part",1},
					{"right_part",1},
					{"chin_raiser",0},
			}},
			{"d",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"left_funneler",0.25},
					{"right_funneler",0.25},
					{"jaw_drop",0.1000000015},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"ih",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.200000003},
					{"upper_raiser",0},
					{"left_part",0.400000006},
					{"right_part",0.400000006},
					{"chin_raiser",0},
			}},
			{"m",{
					{"puckerer",0},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",1},
					{"jaw_clencher",0},
					{"funneler",0.1000000015},
					{"jaw_drop",0.150000006},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"ch",{
					{"left_puckerer",0.9499999881},
					{"right_puckerer",0.9499999881},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0.6499999762},
					{"left_funneler",0.400000006},
					{"right_funneler",0.400000006},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"ax",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",0.5},
					{"right_mouth_drop",0.5},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"left_funneler",0.25},
					{"right_funneler",0.25},
					{"jaw_drop",0.400000006},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"ae",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",0.8000000119},
					{"right_mouth_drop",0.8000000119},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.349999994},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"l2",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.150000006},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"er",{
					{"left_puckerer",0.9300000072},
					{"right_puckerer",0.9300000072},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"left_funneler",0.3799999952},
					{"right_funneler",0},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"left_part",0.6000000238},
					{"right_part",0.6000000238},
					{"chin_raiser",0},
			}},
			{"uw",{
					{"left_puckerer",0.9499999881},
					{"right_puckerer",0.8999999762},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.200000003},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"ah",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.3000000119},
					{"upper_raiser",0},
					{"left_part",0.25},
					{"right_part",0.25},
					{"chin_raiser",0},
			}},
			{"ax2",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",0.5099999905},
					{"right_mouth_drop",0.5099999905},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"left_funneler",0.25},
					{"right_funneler",0.25},
					{"jaw_drop",0.5},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"l",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",1},
					{"right_mouth_drop",1},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.150000006},
					{"upper_raiser",0},
					{"left_part",0.5400000215},
					{"right_part",0.5400000215},
					{"chin_raiser",0},
			}},
			{"dh",{
					{"puckerer",0},
					{"bite",0},
					{"left_mouth_drop",0.75},
					{"right_mouth_drop",0.75},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"left_funneler",0.200000003},
					{"right_funneler",0.200000003},
					{"jaw_drop",0.150000006},
					{"left_upper_raiser",0.150000006},
					{"right_upper_raiser",0.150000006},
					{"left_part",0.400000006},
					{"right_part",0.400000006},
					{"chin_raiser",0},
			}},
			{"uh",{
					{"left_puckerer",0.75},
					{"right_puckerer",0.75},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"funneler",0},
					{"jaw_drop",0.224999994},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"er2",{
					{"left_puckerer",0.5},
					{"right_puckerer",0.5},
					{"bite",0},
					{"left_mouth_drop",0.5},
					{"right_mouth_drop",0.5},
					{"tightener",0},
					{"stretcher",0},
					{"presser",0},
					{"jaw_clencher",0},
					{"left_funneler",0.25},
					{"right_funneler",0.25},
					{"jaw_drop",0.25},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"s",{
					{"puckerer",0},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"left_stretcher",0.6999999881},
					{"right_stretcher",0.6999999881},
					{"presser",0},
					{"jaw_clencher",0.5500000119},
					{"left_funneler",0.375},
					{"right_funneler",0.375},
					{"jaw_drop",0},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
			{"p",{
					{"puckerer",0},
					{"bite",0},
					{"mouth_drop",0},
					{"tightener",0},
					{"stretcher",0},
					{"presser",1},
					{"jaw_clencher",0},
					{"funneler",0.1000000015},
					{"jaw_drop",0.1000000015},
					{"upper_raiser",0},
					{"part",0},
					{"chin_raiser",0},
			}},
	};
	auto &phonemeMap = mdlInfo.model.GetPhonemeMap();
	for(auto &pair : defaultPhonemes)
	{
		auto &flexControllers = phonemeMap.phonemes.insert(std::make_pair(pair.first,PhonemeInfo{})).first->second.flexControllers;
		flexControllers.reserve(pair.second.size());
		for(auto &pair : pair.second)
			flexControllers.insert(pair);
	}

	// Transform coordinate systems
	mdlInfo.ConvertTransforms(skipSharedMeshTransforms,reference.get());
	//

	// Load physics
	std::vector<mdl::phy::PhyCollisionData> collisionObjects {};
	std::unordered_map<int32_t,mdl::phy::PhyRagdollConstraint> constraints {};
	it = files.find("phy");
	if(it != files.end())
	{
		if(import::mdl::load_phy(it->second,collisionObjects,constraints) == true)
		{
			auto bFirst = true;
			auto colMeshOffset = mdl.GetCollisionMeshCount();
			std::vector<std::shared_ptr<CollisionMesh>> colMeshes;
			colMeshes.reserve(collisionObjects.size());
			for(auto &colObj : collisionObjects)
			{
				auto colMesh = CollisionMesh::Create(nw->GetGameState());

				int32_t boneId = -1;
				std::shared_ptr<mdl::Bone> bone = nullptr;
				if(!colObj.faceSections.empty())
				{
					auto &face = colObj.faceSections.front();
					boneId = face.boneIdx;
					if(boneId >= 0 && boneId < bones.size())
						bone = bones[boneId]; // TODO Faces with different bones?
					else
						Con::cwar<<"WARNING: Physics has reference to bone "<<boneId<<", which does not exist! Ignoring..."<<Con::endl;
				}
				auto &verts = colMesh->GetVertices();
				verts.reserve(colObj.vertices.size());

				// Transformation
				/*auto pos = Vector3{}; // firstAnimDescFrameLine.position;
				auto rot = Vector3{}; // firstAnimDescFrameLine.rotation

				auto mat = EulerAngles{rot.x,rot.y,rot.z +umath::deg_to_rad(-90.f)}.ToMatrix();
				mat = glm::translate(Vector3{pos.y,-pos.x,pos.z}) *mat;
				// TODO: Verify that order is correct (position not rotated!)

				auto fMatrixInvert = [](const Mat4 &in,Mat4 &out) {
					out[0][0] = in[0][0];
					out[1][0] = in[0][1];
					out[2][0] = in[0][2];

					out[0][1] = in[1][0];
					out[1][1] = in[1][1];
					out[2][1] = in[1][2];

					out[0][2] = in[2][0];
					out[1][2] = in[2][1];
					out[2][2] = in[2][2];

					Vector3 temp {};
					temp.x = in[3][0];
					temp.y = in[3][1];
					temp.z = in[3][2];

					out[3][0] = -uvec::dot(temp,Vector3{out[0][0],out[1][0],out[2][0]});
					out[3][1] = -uvec::dot(temp,Vector3{out[0][1],out[1][1],out[2][1]});
					out[3][2] = -uvec::dot(temp,Vector3{out[0][2],out[1][2],out[2][2]});
				};
				Mat4 invMat {};
				fMatrixInvert(mat,invMat);*/
				// ???????
				// See
				// Public Sub MatrixInvert(ByVal in_matrixColumn0 As SourceVector, ByVal in_matrixColumn1 As SourceVector, ByVal in_matrixColumn2 As SourceVector, ByVal in_matrixColumn3 As SourceVector, ByRef out_matrixColumn0 As SourceVector, ByRef out_matrixColumn1 As SourceVector, ByRef out_matrixColumn2 As SourceVector, ByRef out_matrixColumn3 As SourceVector)
				// SourceSmdFile44.vb -> Private Sub ProcessTransformsForPhysics()
				//



				for(auto &v : colObj.vertices)
				{
					auto vpos = v.first;
					if(boneId == -1 || bones.size() <= 1) // TODO Seems to work for most models, but why is this needed?
					{
						const auto v = static_cast<float>(umath::sin(M_PI_4));
						const auto rot = Quat{0.f,v,0.f,v}; // Rotation 180 degree on pitch axis and -90 degree on yaw axis
						//vpos = vpos *rot;
					}
					if(colObj.sourcePhyIsCollision == true)
					{
						auto rot180 = uquat::create(EulerAngles(0,180,0)); // Rotation by 180 degree on yaw-axis; Works on most props, not sure why this is needed
						//vpos = vpos *rot180;
					}

					auto t = mdl::util::transform_physics_vertex(bone,vpos,colObj.sourcePhyIsCollision);
					if(colObj.sourcePhyIsCollision == true)
					{
						boneId = -1;
						umath::swap(t.y,t.z);
						umath::negate(t.z);

						umath::negate(t.y);
						// boneId == 0 is meaningless if sourcePhyIsCollision is true (= collision mesh has NO parent)
						//umath::swap(t.y,t.z);
						//umath::negate(t.z);
						/*if(mdlInfo.header.flags &STUDIOHDR_FLAGS_STATIC_PROP)
						{
							uvec::rotate(&t,uquat::create(EulerAngles(0.f,-90.f,0.f)));

							auto rot = uquat::create(EulerAngles(0,0,-90));
							uvec::rotate(&t,rot);
							t.x = -t.x;

							uvec::rotate(&t,uquat::get_inverse(uquat::create(EulerAngles(0.f,-90.f,0.f))));
						}
						else if(boneId == -1 || bones.size() <= 1)
						{
							umath::swap(t.y,t.z);
							umath::negate(t.z);
							t.y = -t.y;
						}
						else
						{
							uvec::rotate(&t,uquat::create(EulerAngles(0.f,-90.f,0.f)));

							auto rot = uquat::create(EulerAngles(0,90,-90));
							uvec::rotate(&t,rot);
							t.y = -t.y;

							uvec::rotate(&t,uquat::get_inverse(uquat::create(EulerAngles(0.f,-90.f,0.f))));
						}*/
					}
					else
					{
						umath::swap(t.y,t.z);
						umath::negate(t.z);
					}
					
					// Perfect fit for phycannon:
					//umath::negate(t.y);

					// Perfect fit for smg:
					//umath::negate(t.x);
					//umath::negate(t.y);
					//umath::negate(t.z);

					verts.push_back(t);
				}

				// There are very few models where the physics mesh will be rotated incorrectly by either 90 or 180 degrees on some axes for whatever reason. (e.g. weapons/w_smg1.mdl)
				// There seems to be no obvious way to find the correct rotation, so the code below will just rotate the collision mesh by all posibilities and select the one which
				// encompasses the visible geometry the closest - which is usually the correct one.
				// If it's not the correct one, the model will have to be rotated manually.
				if(colObj.sourcePhyIsCollision == true)
				{
					const std::array<float,5> testAngles = {-180.f,-90.f,0.f,90.f,180.f};
					auto smallestDistanceSum = std::numeric_limits<float>::max();
					auto smallestCandidate = EulerAngles(0,0,0);
					for(auto p : testAngles)
					{
						for(auto y : testAngles)
						{
							for(auto r : testAngles)
							{
								auto ang = EulerAngles(p,y,r);
								auto rot = uquat::create(ang);
								auto distSum = 0;
								for(auto v : verts)
								{
									uvec::rotate(&v,rot);
									auto dClosest = std::numeric_limits<float>::max();
									auto meshGroup = mdl.GetMeshGroup(0);
									if(meshGroup != nullptr)
									{
										for(auto &mesh : meshGroup->GetMeshes())
										{
											for(auto &subMesh : mesh->GetSubMeshes())
											{
												auto &verts = subMesh->GetVertices();
												subMesh->VisitIndices([&](auto *indexData,uint32_t numIndices) {
													for(auto i=decltype(numIndices){0};i<numIndices;i+=3)
													{
														auto &v0 = verts.at(indexData[i]);
														auto &v1 = verts.at(indexData[i +1]);
														auto &v2 = verts.at(indexData[i +2]);
														Vector3 r;
														umath::geometry::closest_point_on_triangle_to_point(v0.position,v1.position,v2.position,v,&r);
														auto d = uvec::distance(v,r);
														if(d < dClosest)
															dClosest = d;
													}
												});
											}
										}
									}
									distSum += dClosest;
								}
								if(distSum < smallestDistanceSum)
								{
									smallestDistanceSum = distSum;
									smallestCandidate = ang;
								}
								else if(distSum < smallestDistanceSum +0.1f) // Approximately equal; Use whichever angle-set requires the least amount of rotations
								{
									if((umath::abs(ang.p) +umath::abs(ang.y) +umath::abs(ang.r)) < (umath::abs(smallestCandidate.p) +umath::abs(smallestCandidate.y) +umath::abs(smallestCandidate.r)))
									{
										smallestDistanceSum = distSum;
										smallestCandidate = ang;
									}
								}
							}
						}
					}
					if(smallestDistanceSum != std::numeric_limits<float>::max() && smallestCandidate != EulerAngles(0,0,0))
					{
						auto rot = uquat::create(smallestCandidate);
						for(auto &v : verts)
							uvec::rotate(&v,rot);
					}
				}
				
				colMesh->Centralize();
				auto surfProp = colObj.keyValues.surfaceProp;
				ustring::to_lower(surfProp);
				auto itSurfMat = surfMatTranslationTable.find(surfProp);
				if(itSurfMat != surfMatTranslationTable.end())
					surfProp = itSurfMat->second;
				colMesh->SetSurfaceMaterial(surfProp);
				colMesh->SetBoneParent(boneId);
				colMesh->Update();
				colMesh->SetVolume(colObj.keyValues.volume);
				colMesh->SetMass(colObj.keyValues.mass);
				mdl.AddCollisionMesh(colMesh);
				if(bFirst == true)
				{
					bFirst = false;
					mdl.SetMass(colObj.keyValues.mass);
				}
			}
			for(auto &pair : constraints)
			{
				auto &ragdollConstraint = pair.second;
				if(ragdollConstraint.childIndex >= colMeshes.size() || ragdollConstraint.parentIndex >= colMeshes.size())
					continue;
				auto &child = colMeshes[ragdollConstraint.childIndex];
				auto &parent = colMeshes[ragdollConstraint.parentIndex];
				if(child->GetBoneParent() < 0 || parent->GetBoneParent() < 0)
					continue;
				auto &joint = mdl.AddJoint(JointType::DOF,child->GetBoneParent(),parent->GetBoneParent());

				auto angLimitL = EulerAngles(ragdollConstraint.xmin,-ragdollConstraint.zmax,ragdollConstraint.ymin);
				auto angLimitU = EulerAngles(ragdollConstraint.xmax,-ragdollConstraint.zmin,ragdollConstraint.ymax);

				std::stringstream ssAngLimitL;
				ssAngLimitL<<angLimitL.p<<" "<<angLimitL.y<<" "<<angLimitL.r;

				std::stringstream ssAngLimitU;
				ssAngLimitU<<angLimitU.p<<" "<<angLimitU.y<<" "<<angLimitU.r;

				joint.args = {
					{"ang_limit_l",ssAngLimitL.str()},
					{"ang_limit_u",ssAngLimitU.str()}
				};

				// Deprecated
				/*auto &joint = mdl.AddJoint(JOINT_TYPE_CONETWIST,ragdollConstraint.childIndex,ragdollConstraint.parentIndex);
				joint.args = {
					{"sp1l",std::to_string(-ragdollConstraint.xmax)},
					{"sp1u",std::to_string(-ragdollConstraint.xmin)},
					{"sp2l",std::to_string(-ragdollConstraint.zmax)},
					{"sp2u",std::to_string(-ragdollConstraint.zmin)},
					{"tsl",std::to_string(-ragdollConstraint.ymax)},
					{"tsu",std::to_string(-ragdollConstraint.ymin)},
					//{"sftn",""},
					//{"bias",""},
					//{"rlx",""}
				};*/
			}
		}
	}
	//

	// Add LODs
	/*if(bodyPart != nullptr)
	{
		for(auto lod=decltype(bodyPart->lodMeshes.size()){1};lod<bodyPart->lodMeshes.size();++lod)
		{
			auto &bodyPartLod = bodyPart->lodMeshes[lod];
			auto meshGroup = mdl.AddMeshGroup(std::string("lod") +std::to_string(lod));
			auto lodMesh = std::shared_ptr<ModelMesh>(nw->CreateMesh());
			auto lodSubMesh = std::shared_ptr<ModelSubMesh>(nw->CreateSubMesh());
			lodSubMesh->SetShared(*subMesh,ModelSubMesh::ShareMode::All & ~ModelSubMesh::ShareMode::Triangles);
			lodSubMesh->GetTriangles() = bodyPartLod.indices;
			lodSubMesh->SetTexture(0);
			lodMesh->AddSubMesh(lodSubMesh);
			meshGroup->AddMesh(lodMesh);

			std::unordered_map<uint32_t,uint32_t> meshIds = {};
			meshIds[0] = lod;
			mdl.AddLODInfo(lod,meshIds);
			lodSubMesh->Update();
			lodMesh->Update();
		}
	}*/

	// Add root bone
	/*auto rootBone = std::make_shared<Bone>();
	rootBone->name = "root";
	rootBone->ID = 0u;

	for(auto it=skelBones.begin();it!=skelBones.end();++it)
	{
		(*it)->ID += 1u;
		if((*it)->parent.expired())
		{
			(*it)->parent = rootBone;
			rootBone->children.insert(std::make_pair((*it)->ID,*it));
		}
		auto oldChildren = (*it)->children;
		(*it)->children.clear();
		for(auto &pair : oldChildren)
			(*it)->children.insert(std::make_pair(pair.first +1u,pair.second));
	}
	skelBones.insert(skelBones.begin(),rootBone);

	const auto fAddRootBoneToFrame = [](Frame &frame) {
		auto &transforms = frame.GetBoneTransforms();
		auto &scales = frame.GetBoneScales();
		transforms.insert(transforms.begin(),OrientedPoint{Vector3{},uquat::identity()});
		if(scales.empty() == false)
			scales.insert(scales.begin(),Vector3{1.f,1.f,1.f});
	};

	const auto fAddRootBoneToAnim = [&fAddRootBoneToFrame](Animation &anim) {
		auto &boneList = const_cast<std::vector<uint32_t>&>(anim.GetBoneList());
		for(auto &id : boneList)
			++id;
		boneList.insert(boneList.begin(),0u);
		for(auto &frame : anim.GetFrames())
			fAddRootBoneToFrame(*frame);
	};

	for(auto &anim : mdl.GetAnimations())
		fAddRootBoneToAnim(*anim);
	rootBones = {
		{0u,rootBone}
	};

	for(auto &meshGroup : mdl.GetMeshGroups())
	{
		for(auto &mesh : meshGroup->GetMeshes())
		{
			for(auto &subMesh : mesh->GetSubMeshes())
			{
				auto &vertWeights = subMesh->GetVertexWeights();
				for(auto &vw : vertWeights)
				{
					for(auto i=0u;i<4u;++i)
					{
						auto &id = vw.boneIds[i];
						if(id == -1)
							continue;
						++id;
					}
				}
			}
		}
	}

	for(auto &att : mdl.GetAttachments())
		++att.bone;
	for(auto &colMesh : mdl.GetCollisionMeshes())
	{
		auto boneId = colMesh->GetBoneParent();
		if(boneId != -1)
			colMesh->SetBoneParent(boneId +1u);
	}
	auto &hitboxes = mdl.GetHitboxes();
	auto oldHitboxes = hitboxes;
	hitboxes.clear();
	for(auto &pair : oldHitboxes)
		hitboxes.insert(std::make_pair(pair.first +1u,pair.second));*/
	//

	auto &baseMeshes = mdl.GetBaseMeshes();
	baseMeshes.clear();
	//baseMeshes = bgBaseMeshGroups;

	// TODO: Some models mustn't have a base mesh set (like "models/a_hat_in_time/mustache_girl.mdl"), but
	// some others might? Keep this under observation!
	//baseMeshes.push_back(0);
	//

	// Generate reference
	auto refPose = Frame::Create(*frameReference);
	for(auto i=decltype(bones.size()){0};i<bones.size();++i)
	{
		auto &pos = *refPose->GetBonePosition(i);
		auto &rot = *refPose->GetBoneOrientation(i);

		auto m = glm::toMat4(rot);
		m = glm::translate(m,pos);
		mdl.SetBindPoseBoneMatrix(i,glm::inverse(m));
	}
	reference->Localize(skeleton);
	mdl.SetReference(refPose);
	//

	// Build bind pose
	for(auto i=decltype(header.numbones){0};i<header.numbones;++i)
	{
		auto &pos = *refPose->GetBonePosition(i);
		auto &rot = *refPose->GetBoneOrientation(i);

		auto m = glm::toMat4(rot);
		m = glm::translate(m,pos);
		mdl.SetBindPoseBoneMatrix(i,glm::inverse(m));
	}
	//

	// Attachments
	for(auto &att : mdlInfo.attachments)
	{
		auto &t = att->GetTransform();
		auto tmpAng = mdl::util::convert_rotation_matrix_to_degrees(t[0][0],t[1][0],t[2][0],t[0][1],t[1][1],t[2][1],t[2][2]);

		// Transform to source system
		auto ang = EulerAngles(
			tmpAng.y,
			-tmpAng.r,
			-tmpAng.p
		);

		// Transform to pragma system; These transformations are strange, but they seem to work for all models
		ang.p = -ang.p;
		ang.y = -ang.y;
		auto rot = uquat::create(EulerAngles(0,90,0)) *uquat::create(ang);
		//auto matAng = mdl::util::euler_angles_to_matrix(ang);
		//auto rot = mdl::util::get_rotation(matAng);
		
		Vector3 pos(t[0][3],t[1][3],t[2][3]);
		umath::swap(pos.y,pos.z);
		umath::negate(pos.y);

		mdl.AddAttachment(att->GetName(),att->GetBone(),pos,rot);
	}
	//

	// Hitboxes
	for(auto &hbSet : mdlInfo.hitboxSets)
	{
		for(auto &hb : hbSet.GetHitboxes())
		{
			auto hg = (hb.groupId != 10) ? static_cast<HitGroup>(hb.groupId) : HitGroup::Gear;
			mdl.AddHitbox(hb.boneId,hg,hb.boundingBox.first,hb.boundingBox.second);
		}
	}
	//

	// Skins
	auto texGroupIdx = 0u;
	for(auto &skinFamily : mdlInfo.skinFamilies)
	{
		auto *texGroup = mdl.GetTextureGroup(texGroupIdx++);
		if(texGroup == nullptr)
			texGroup = mdl.CreateTextureGroup();
		auto &textures = texGroup->textures;
		textures.clear();
		textures.reserve(skinFamily.size());
		for(auto idx : skinFamily)
		{
			auto texId = textureTranslations.at(idx);
			textures.push_back(texId);
		}
	}
	//

	// For some reason skin families in source contain a reference to all textures for each family, regardless of whether they're actually
	// used by any of the meshes. We only care about the textures that are actually in use, so we'll discard the others.
	auto *texGroup = mdl.GetTextureGroup(0);
	if(texGroup)
	{
		// Check which materials are in use by a mesh and which ones aren't
		std::unordered_set<uint32_t> materialIndicesInUse {};
		for(auto &meshGroup : mdl.GetMeshGroups())
		{
			for(auto &mesh : meshGroup->GetMeshes())
			{
				for(auto &subMesh : mesh->GetSubMeshes())
				{
					auto texIdx = subMesh->GetSkinTextureIndex();
					materialIndicesInUse.insert(texIdx);
				}
			}
		}

		// Map old material indices to new ones
		std::unordered_map<uint32_t,uint32_t> oldMatIdxToNewIdx {};
		std::vector<uint32_t> matIndicesToRemove {};
		uint32_t newMatIdx = 0u;
		for(auto oldMatIdx=decltype(texGroup->textures.size()){0u};oldMatIdx<texGroup->textures.size();++oldMatIdx)
		{
			if(materialIndicesInUse.find(oldMatIdx) != materialIndicesInUse.end())
				oldMatIdxToNewIdx.insert(std::make_pair(oldMatIdx,newMatIdx++));
			else
				matIndicesToRemove.insert(matIndicesToRemove.begin(),oldMatIdx);
		}

		// Remove unused material indices
		for(auto &texGroup : mdl.GetTextureGroups())
		{
			for(auto idx : matIndicesToRemove)
			{
				// Indices are in reverse order (highest to lowest), so we can just remove the elements
				// from the containers.
				texGroup.textures.erase(texGroup.textures.begin() +idx);
			}
		}

		// Apply new material indices to meshes
		for(auto &meshGroup : mdl.GetMeshGroups())
		{
			for(auto &mesh : meshGroup->GetMeshes())
			{
				for(auto &subMesh : mesh->GetSubMeshes())
				{
					auto matIdx = subMesh->GetSkinTextureIndex();
					auto itNewIndex = oldMatIdxToNewIdx.find(matIdx);
					if(itNewIndex != oldMatIdxToNewIdx.end())
						subMesh->SetSkinTextureIndex(itNewIndex->second);
				}
			}
		}

		for(auto &eyeball : mdl.GetEyeballs())
		{
			auto itNewIndex = oldMatIdxToNewIdx.find(eyeball.irisMaterialIndex);
			if(itNewIndex != oldMatIdxToNewIdx.end())
				eyeball.irisMaterialIndex = itNewIndex->second;
		}
	}

	// Make sure eyeball index references match with Pragma
	for(auto &eyeball : mdl.GetEyeballs())
	{
		auto &boneIndex = eyeball.boneIndex;
		if(boneIndex < bones.size())
		{
			auto &bone = bones.at(boneIndex);
			boneIndex = skeleton.LookupBone(bone->GetName());
		}

		auto &irisMaterialIndex = eyeball.irisMaterialIndex;
		if(irisMaterialIndex < textureTranslations.size())
			irisMaterialIndex = textureTranslations.at(irisMaterialIndex);
		auto fTranslateFlex = [&mdlInfo,&mdl](int32_t &flexIdx) {
			if(flexIdx >= mdlInfo.flexDescs.size())
				return;
			auto &flexDesc = mdlInfo.flexDescs.at(flexIdx);
			uint32_t pragmaIdx;
			if(mdl.GetFlexId(flexDesc.GetName(),pragmaIdx))
				flexIdx = pragmaIdx;
		};
		fTranslateFlex(eyeball.lowerLid.raiserFlexIndex);
		fTranslateFlex(eyeball.lowerLid.neutralFlexIndex);
		fTranslateFlex(eyeball.lowerLid.lowererFlexIndex);
		fTranslateFlex(eyeball.lowerLid.lidFlexIndex);
		fTranslateFlex(eyeball.upperLid.raiserFlexIndex);
		fTranslateFlex(eyeball.upperLid.neutralFlexIndex);
		fTranslateFlex(eyeball.upperLid.lowererFlexIndex);
		fTranslateFlex(eyeball.upperLid.lidFlexIndex);
	}

	auto isStaticProp = (mdlInfo.header.flags &STUDIOHDR_FLAGS_STATIC_PROP) != 0;
	/*if(isStaticProp == false && skeleton.GetBoneCount() <= 1)
	{
		// We'll just assume this is a static prop...
		isStaticProp = true;
	}*/
	umath::set_flag(mdl.GetMetaInfo().flags,Model::Flags::Inanimate,isStaticProp);

	if(isStaticProp)
	{
		// Source replaces all bones in static props with "static_prop" and adds vertex weights for it to all vertices.
		// We don't want it to create any unnecessary vertex weight buffers in Pragma, so we'll clear all the vertex weights.
		for(auto &meshGroup : mdl.GetMeshGroups())
		{
			for(auto &mesh : meshGroup->GetMeshes())
			{
				for(auto &subMesh : mesh->GetSubMeshes())
					subMesh->GetVertexWeights().clear();
			}
		}
	}

	// Optimize meshes
	/*if(optLog)
		(*optLog)<<"Optimizing meshes!\n";
	for(auto &meshGroup : mdl.GetMeshGroups())
	{
		for(auto &mesh : meshGroup->GetMeshes())
		{
			for(auto &subMesh : mesh->GetSubMeshes())
				subMesh->Optimize();
		}
	}*/

	//mdl.Update(ModelUpdateFlags::AllData);
	//auto rot = uquat::create(EulerAngles(0.f,90.f,0.f));
	//mdl.Rotate(rot);

	mdl.Update(ModelUpdateFlags::AllData);

	import::util::port_model_texture_assets(*nw,mdl);

	if(optLog)
		(*optLog)<<"Done!\n";

	return ptrMdl;
}
