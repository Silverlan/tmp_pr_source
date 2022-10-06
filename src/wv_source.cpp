#include "mdl.h"
#include "wv_source.hpp"
#include "nif.hpp"
#include "fbx.h"
#include "mikumikudance/mmd.hpp"
#include "source_engine/source_engine.hpp"
#include "source_engine/vbsp/bsp_converter.hpp"
#include "source_engine/vmf/util_vmf.hpp"
#include "source2/source2.hpp"
#include <pragma/lua/libraries/lfile.h>
#include <pragma/pragma_module.hpp>
#include <pragma/ishared.hpp>
#include <pragma/engine.h>
#include <pragma/networkstate/networkstate.h>
#include <pragma/game/game.h>
#include <pragma/util/util_game.hpp>
#include <pragma/asset/util_asset.hpp>
#include <mathutil/uquat.h>
#include <functional>
#include <pragma/model/model.h>
#include <mathutil/eulerangles.h>
#include <fsys/filesystem.h>
#include <util_archive.hpp>
#include <sharedutils/util_file.h>
#include <luasystem.h>
#include <luainterface.hpp>
#include <smdmodel.h>
#include <pragma/physics/collisionmesh.h>
#include <pragma/model/modelmesh.h>
#include <pragma/asset_types/world.hpp>
#include <pragma/asset/util_asset.hpp>
#include <pragma/game/game_resources.hpp>
#include <pragma/asset/util_asset.hpp>
#include <util_image.hpp>
#include <util_texture_info.hpp>
#include <VTFFile.h>
#include <udm.hpp>
#include <panima/bone.hpp>
#include <panima/skeleton.hpp>
#include <material_manager2.hpp>

#if 0
#pragma comment(lib,"libfbxsdk-md.lib")
#pragma comment(lib,"lua51.lib")
#pragma comment(lib,"luasystem.lib")
#pragma comment(lib,"luabind.lib")
#pragma comment(lib,"sharedutils.lib")
#pragma comment(lib,"mathutil.lib")
#pragma comment(lib,"vfilesystem.lib")
#pragma comment(lib,"shared.lib")
#pragma comment(lib,"ishared.lib")
#pragma comment(lib,"materialsystem.lib")
#pragma comment(lib,"util_archive.lib")
#endif
extern DLLNETWORK Engine *engine;

#include <game_mount_info.hpp>
static bool load_mounted_games()
{
	std::string err;
	auto udmData = util::load_udm_asset("cfg/mounted_games.udm",&err);
	if(udmData == nullptr)
		return false;
	auto &data = *udmData;
	auto udm = data.GetAssetData().GetData();

	std::vector<uarch::GameMountInfo> mountedGames {};
	mountedGames.reserve(udm.GetChildCount());
	std::unordered_map<std::string,int32_t> priorities;
	for(auto pair : udm.ElIt())
	{
		mountedGames.push_back({});
		auto &gameInfo = mountedGames.back();
		gameInfo.identifier = pair.key;
		
		auto &child = pair.property;
		child["localization_name"](gameInfo.localizationName);
		child["enabled"](gameInfo.enabled);
		child["priority"](gameInfo.priority);

		auto udmSteam = child["steam"];
		if(udmSteam)
		{
			auto &steamSettings = gameInfo.steamSettings = uarch::SteamSettings{};
			steamSettings->appId = std::numeric_limits<uarch::SteamSettings::AppId>::max();
			udmSteam["app_id"](steamSettings->appId);
			auto udmGamePath = udmSteam["game_path"];
			if(udmGamePath)
				steamSettings->gamePaths.push_back(udmGamePath.ToValue<udm::String>(""));
			else
				udmSteam["game_paths"].GetBlobData(steamSettings->gamePaths);
			udmSteam["mount_workshop"](steamSettings->mountWorkshop);
		}

		auto udmEngine = child["engine"];
		if(udmEngine)
		{
			auto *engineSettings = gameInfo.SetEngine(uarch::engine_name_to_enum(udmEngine["name"].ToValue<udm::String>("")));
			if(engineSettings)
			{
				auto udmArchives = udmEngine["archives"];

				// Array
				for(auto it=udmArchives.begin<udm::String>();it!=udmArchives.end<udm::String>();++it)
				{
					auto &archName = *it;
					switch(gameInfo.gameEngine)
					{
					case uarch::GameEngine::SourceEngine:
					{
						auto &vpkInfo = (static_cast<uarch::SourceEngineSettings&>(*engineSettings).vpkList[archName] = {});
						break;
					}
					case uarch::GameEngine::Source2:
					{
						auto &vpkInfo = (static_cast<uarch::Source2Settings&>(*engineSettings).vpkList[archName] = {});
						break;
					}
					case uarch::GameEngine::Gamebryo:
					{
						(static_cast<uarch::GamebryoSettings&>(*engineSettings).bsaList[archName] = {});
						break;
					}
					case uarch::GameEngine::CreationEngine:
					{
						(static_cast<uarch::CreationEngineSettings&>(*engineSettings).ba2List[archName] = {});
						break;
					}
					}
				}

				// Element
				for(auto pair : udmArchives.ElIt())
				{
					auto archName = std::string{pair.key};

					switch(gameInfo.gameEngine)
					{
					case uarch::GameEngine::SourceEngine:
					{
						auto &vpkInfo = (static_cast<uarch::SourceEngineSettings&>(*engineSettings).vpkList[archName] = {});
						pair.property["root_dir"](vpkInfo.rootDir);
						break;
					}
					case uarch::GameEngine::Source2:
					{
						auto &vpkInfo = (static_cast<uarch::Source2Settings&>(*engineSettings).vpkList[archName] = {});
						pair.property["root_dir"](vpkInfo.rootDir);
						break;
					}
					case uarch::GameEngine::Gamebryo:
					{
						(static_cast<uarch::GamebryoSettings&>(*engineSettings).bsaList[archName] = {});
						break;
					}
					case uarch::GameEngine::CreationEngine:
					{
						(static_cast<uarch::CreationEngineSettings&>(*engineSettings).ba2List[archName] = {});
						break;
					}
					}
				}
			}
		}
	}

	for(auto &gameInfo : mountedGames)
		uarch::mount_game(gameInfo);
	return true;
}

uint32_t import::util::add_texture(NetworkState &nw,Model &mdl,const std::string &name,TextureGroup *optTexGroup,bool forceAddToTexGroup)
{
	auto fname = name;
	ufile::remove_extension_from_filename(fname,pragma::asset::get_supported_extensions(pragma::asset::Type::Material,pragma::asset::FormatType::All));
	fname = pragma::asset::get_normalized_path(name,pragma::asset::Type::Material);
	auto &meta = mdl.GetMetaInfo();
	auto it = std::find(meta.textures.begin(),meta.textures.end(),fname);
	auto idx = 0u;
	if(it != meta.textures.end())
		idx = it -meta.textures.begin();
	else
	{
		std::optional<std::string> foundPath {};
		for(auto &path : mdl.GetMetaInfo().texturePaths)
		{
			auto texPath = path +fname;
			if(pragma::asset::exists(texPath,pragma::asset::Type::Material))
			{
				foundPath = texPath;
				break;
			}
		}
		if(foundPath.has_value() == false)
		{
			for(auto &path : mdl.GetMetaInfo().texturePaths)
			{
				auto texPath = path +fname;
				if(::util::port_file(&nw,"materials/" +texPath +".vmt"))
				{
					foundPath = texPath;
					break;
				}
			}
		}
		idx = mdl.AddTexture(fname,foundPath.has_value() ? nw.LoadMaterial(*foundPath) : nw.GetMaterialManager().GetErrorMaterial());
	}

	auto *texGroup = optTexGroup ? optTexGroup : mdl.GetTextureGroup(0);
	if(texGroup == nullptr)
		texGroup = mdl.CreateTextureGroup();
	if(forceAddToTexGroup || std::find(texGroup->textures.begin(),texGroup->textures.end(),idx) == texGroup->textures.end())
		texGroup->textures.push_back(idx);
	return idx;
}

void import::util::port_model_texture_assets(NetworkState &nw,Model &mdl)
{
	if(engine->ShouldMountExternalGameResources() == false)
		return;
	// Immediately attempt to port all of the materials / textures associated with the model
	for(auto &matName : mdl.GetTextures())
	{
		for(auto &lookupPath : mdl.GetTexturePaths())
		{
			auto matPath = lookupPath +matName;
			if(pragma::asset::exists(matPath,pragma::asset::Type::Material) || FileManager::Exists("materials/" +matPath +".vmt") || FileManager::Exists("materials/" +matPath +".vmat_c"))
				break;
			if(nw.PortMaterial(matPath +"." +std::string{pragma::asset::FORMAT_MATERIAL_ASCII}))
				break;
		}
	}
}

static bool write_data(const std::string &fpath,const std::vector<uint8_t> &data)
{
	auto path = ufile::get_path_from_filename(fpath);
	FileManager::CreatePath(path.c_str());
	auto fOut = FileManager::OpenFile<VFilePtrReal>(fpath.c_str(),"wb");
	if(fOut == nullptr)
		return false;
	fOut->Write(data.data(),data.size());
	return true;
}

/// SMD
static uint32_t add_bone(const SMDModel::Node &node,panima::Skeleton &skeleton,std::shared_ptr<panima::Bone> *bone=nullptr)
{
	Model mdl;
	auto &bones = skeleton.GetBones();
	auto it = std::find_if(bones.begin(),bones.end(),[&node](const std::shared_ptr<panima::Bone> &bone) {
		return (node.name == bone->name) ? true : false;
		});
	if(it == bones.end())
	{
		auto *ptrBone = new panima::Bone;
		ptrBone->name = node.name;
		skeleton.AddBone(ptrBone);
		it = bones.end() -1;
	}
	if(bone != nullptr)
		*bone = *it;
	return it -bones.begin();
}

static void update_skeletal_hierarchy(SMDModel &smd,panima::Skeleton &skeleton)
{
	auto &nodes = smd.GetNodes();
	for(auto &node : nodes)
	{
		if(node.parent == -1)
			continue;
		auto bone = skeleton.GetBone(skeleton.LookupBone(node.name));
		auto parentBone = skeleton.GetBone(skeleton.LookupBone(nodes.at(node.parent).name));
		bone.lock()->parent = parentBone;
		parentBone.lock()->children[bone.lock()->ID] = bone.lock();
	}
	for(auto &bone : skeleton.GetBones())
	{
		if(bone->parent.expired() == false)
			continue;
		skeleton.GetRootBones()[bone->ID] = bone;
	}
}

static void add_nodes_to_skeleton(SMDModel &smd,panima::Skeleton &skeleton,pragma::animation::Animation *anim)
{
	auto &smdSkeleton = smd.GetSkeleton();
	auto &smdNodes = smd.GetNodes();
	auto &bones = skeleton.GetBones();
	auto *boneList = (anim != nullptr) ? &anim->GetBoneList() : nullptr;
	if(anim)
		anim->ReserveBoneIds(anim->GetBoneCount() +smdNodes.size());
	for(auto i=decltype(smdNodes.size()){0};i<smdNodes.size();++i)
	{
		auto &node = smdNodes[i];
		auto boneId = add_bone(node,skeleton);
		if(boneList != nullptr)
			anim->AddBoneId(boneId);
	}
	update_skeletal_hierarchy(smd,skeleton);
}

static std::vector<uint32_t> get_skeleton_translation_table(SMDModel &smd,Model &mdl)
{
	auto &nodes = smd.GetNodes();
	auto &skeleton = mdl.GetSkeleton();
	auto &bones = skeleton.GetBones();

	std::vector<uint32_t> t;
	t.reserve(nodes.size());
	for(auto &node : nodes)
	{
		auto it = std::find_if(bones.begin(),bones.end(),[&node](const std::shared_ptr<panima::Bone> &bone) {
			return (node.name == bone->name) ? true : false;
			});
		t.push_back((*it)->ID);
	}
	return t;
}

static void generate_reference(SMDModel &smd,Model &mdl)
{
	auto animId = mdl.LookupAnimation("reference");
	if(animId == -1)
	{
		mdl.AddAnimation("reference",pragma::animation::Animation::Create());
		animId = mdl.LookupAnimation("reference");
	}

	auto &smdFrame = smd.GetFrames().front();
	auto anim = mdl.GetAnimation(animId);
	auto frame = anim->GetFrame(0);
	if(frame == nullptr)
	{
		frame = Frame::Create(smdFrame.transforms.size());
		anim->AddFrame(frame);
	}

	auto &nodes = smd.GetNodes();
	auto &skeleton = mdl.GetSkeleton();
	for(auto i=decltype(smdFrame.transforms.size()){0};i<smdFrame.transforms.size();++i)
	{
		auto &t = smdFrame.transforms[i];
		auto &node = nodes[i];
		auto boneId = add_bone(node,skeleton);
		frame->SetBoneCount(umath::max(frame->GetBoneCount(),boneId +1));

		auto &pos = t.position;
		auto &rot = t.rotation;
		//to_local_bone_system(smdFrame,node,pos,rot);

		frame->SetBonePosition(boneId,pos);
		frame->SetBoneOrientation(boneId,rot);

		auto m = glm::toMat4(t.rotation);
		m = glm::translate(m,t.position);
		mdl.SetBindPoseBoneMatrix(boneId,glm::inverse(m));
	}
	update_skeletal_hierarchy(smd,skeleton);
	auto refFrame = Frame::Create(*frame);
	frame->Localize(*anim,mdl.GetSkeleton());
	mdl.SetReference(refFrame);
}

static void to_local_bone_system(const SMDModel::Frame &frame,const SMDModel::Node &node,Vector3 &pos,Quat &rot)
{
	auto parentId = node.parent;
	if(parentId != -1)
	{
		auto &tParent = frame.transforms[parentId];
		pos -= tParent.position;
		auto inv = uquat::get_inverse(tParent.rotation);
		uvec::rotate(&pos,inv);

		rot = inv *rot;
	}
}

static void update_reference(Model &mdl,pragma::animation::Animation &anim) // Adds all new bones to the reference pose
{
	auto numBones = mdl.GetBoneCount();
	auto &animBoneList = anim.GetBoneList();
	auto &reference = mdl.GetReference();
	for(auto i=decltype(numBones){0};i<numBones;++i)
	{
		auto *mat = mdl.GetBindPoseBoneMatrix(i);
		if(mat == nullptr)
		{
			reference.SetBoneCount(umath::max(reference.GetBoneCount(),i +1));
			auto it = std::find(animBoneList.begin(),animBoneList.end(),i);
			if(it != animBoneList.end())
			{
				auto animBoneId = it -animBoneList.begin();
				auto &frame = *anim.GetFrame(0);
				auto &pos = *frame.GetBonePosition(animBoneId);
				auto &rot = *frame.GetBoneOrientation(animBoneId);

				reference.SetBonePosition(i,pos);
				reference.SetBoneOrientation(i,rot);

				auto m = glm::toMat4(rot);
				m = glm::translate(m,pos);
				mdl.SetBindPoseBoneMatrix(i,glm::inverse(m));
			}
			else
			{
				reference.SetBonePosition(i,{});
				reference.SetBoneOrientation(i,{});
				mdl.SetBindPoseBoneMatrix(i,{});
			}
		}
	}
}

static void generate_animation(SMDModel &smd,Model &mdl,pragma::animation::Animation &anim)
{
	auto &smdFrames = smd.GetFrames();
	auto numBones = anim.GetBoneCount();
	auto &nodes = smd.GetNodes();
	for(auto &smdFrame : smdFrames)
	{
		auto frame = Frame::Create(numBones);
		for(auto i=decltype(smdFrame.transforms.size()){0};i<smdFrame.transforms.size();++i)
		{
			auto &node = nodes[i];
			auto &t = smdFrame.transforms[i];

			auto pos = t.position;
			auto rot = t.rotation;
			to_local_bone_system(smdFrame,node,pos,rot);

			frame->SetBonePosition(i,pos);
			frame->SetBoneOrientation(i,rot);
		}
		anim.AddFrame(frame);
	}
	update_reference(mdl,anim);
}

static bool load_smd(NetworkState *nw,const std::string &name,Model &mdl,SMDModel &smd,bool bCollision,std::vector<std::string> &textures)
{
	auto *game = nw->GetGameState();
	auto &meshes = smd.GetMeshes();
	if(bCollision == true)
	{
		for(auto &mesh : meshes)
		{
			auto colMesh = CollisionMesh::Create(game);
			for(auto &tri : mesh.triangles)
			{
				for(uint8_t i=0;i<3;++i)
					colMesh->AddVertex(tri.vertices[i].position);
			}
			colMesh->Update();
			mdl.AddCollisionMesh(colMesh);
		}
		mdl.Update();
		return true;
	}
	auto anim = pragma::animation::Animation::Create();
	mdl.GetSkeleton().GetBones().clear();
	mdl.GetSkeleton().GetRootBones().clear();
	add_nodes_to_skeleton(smd,mdl.GetSkeleton(),anim.get());
	if(!meshes.empty())
	{
		auto mdlMesh = std::shared_ptr<ModelMesh>(nw->CreateMesh())->shared_from_this();
		auto boneTranslation = get_skeleton_translation_table(smd,mdl);

		auto numMats = mdl.GetMaterials().size();
		textures.reserve(textures.size() +smd.GetMeshes().size());
		for(auto &mesh : smd.GetMeshes())
		{
			auto subMesh = std::shared_ptr<ModelSubMesh>(nw->CreateSubMesh());
			auto &verts = subMesh->GetVertices();
			for(auto &tri : mesh.triangles)
			{
				std::array<uint32_t,3> triangleIndices = {0,0,0};
				for(uint8_t i=0;i<3;++i)
				{
					auto &triVert = tri.vertices[i];
					auto it = std::find_if(verts.begin(),verts.end(),[&triVert](const umath::Vertex &v) {
						return (umath::abs(triVert.position.x -v.position.x) <= umath::VERTEX_EPSILON && umath::abs(triVert.position.y -v.position.y) <= umath::VERTEX_EPSILON && umath::abs(triVert.position.z -v.position.z) <= umath::VERTEX_EPSILON) ? true : false;
						});
					if(it == verts.end())
					{
						auto vertId = subMesh->AddVertex({triVert.position,{triVert.uv.x,-triVert.uv.y},triVert.normal});
						triangleIndices[i] = subMesh->GetVertexCount() -1;

						umath::VertexWeight w {};
						uint8_t c = 0;
						for(auto &pair : triVert.weights)
						{
							if(pair.first != -1)
							{
								w.boneIds[c] = boneTranslation[pair.first];
								w.weights[c] = pair.second;
								if(++c == 4)
									break;
							}
						}
						subMesh->SetVertexWeight(vertId,w);

						it = verts.end() -1;
					}
					else
						triangleIndices[i] = it -verts.begin();
				}
				subMesh->AddTriangle(triangleIndices[0],triangleIndices[1],triangleIndices[2]);
			}
			textures.push_back(mesh.texture);
			auto texId = import::util::add_texture(*nw,mdl,mesh.texture);
			subMesh->SetSkinTextureIndex(texId);
			subMesh->Update();
			mdlMesh->AddSubMesh(subMesh);
		}
		mdlMesh->Update();
		auto meshGroup = mdl.GetMeshGroup(0);
		if(meshGroup == nullptr)
			meshGroup = mdl.AddMeshGroup("reference");
		meshGroup->AddMesh(mdlMesh);
		generate_reference(smd,mdl);
	}
	generate_animation(smd,mdl,*anim);
	mdl.AddAnimation(name,anim);
	return true;
}
///

#if 0
static void register_importers()
{
	auto &assetManager = engine->GetAssetManager();
	pragma::asset::AssetManager::ImporterInfo importerInfo {};
	importerInfo.name = "Source Engine Model";
	assetManager.RegisterImporter(importerInfo,pragma::asset::Type::Model,[](VFilePtr f,const std::optional<std::string> &filePath,std::string &outErrMsg) -> std::unique_ptr<pragma::asset::IAssetWrapper> {
		if(filePath.has_value() == false)
			return nullptr;
		std::string ext;
		ufile::get_extension(*filePath,&ext);
		if(ustring::compare(ext,"mdl",false) == false)
			return nullptr;
		auto path = ufile::get_path_from_filename(*filePath);
		auto mdlName = ufile::get_file_from_filename(*filePath);
		const std::array<std::string,7> extensions = {
			"dx80.vtx",
			"dx90.vtx",
			"mdl",
			"phy",
			"sw.vtx",
			"vvd",
			"ani"
		};
		std::optional<std::string> sourcePath = {};
		std::unordered_map<std::string,VFilePtr> files;
		for(auto &ext : extensions)
		{
			auto subPath = path +mdlName +"." +ext;
			auto f = FileManager::OpenFile(subPath.c_str(),"rb");
			if(f == nullptr)
				f = uarch::load(subPath,(ext == "mdl") ? &sourcePath : nullptr);
			
			if(f != nullptr)
				files[ext] = f;
		}
		if(files.find("dx90.vtx") != files.end())
			files["vtx"] = files["dx90.vtx"];
		else if(files.find("dx80.vtx") != files.end())
			files["vtx"] = files["dx80.vtx"];
		else if(files.find("sw.vtx") != files.end())
			files["vtx"] = files["sw.vtx"];

		if(sourcePath.has_value())
			Con::cout<<"Found model in '"<<*sourcePath<<"'! Porting..."<<Con::endl;

		std::vector<std::string> textures;
		auto r = ::import::load_mdl(nw,files,fCreateModel,fCallback,true,textures,optLog);
		if(r == nullptr)
			return false;
		return fCallback(r,path,mdlName);
	});
}
#endif

static VTFImageFormat get_vtf_image_format(pragma::asset::VtfInfo::Format format)
{
	switch(format)
	{
	case pragma::asset::VtfInfo::Format::Bc1:
		return VTFImageFormat::IMAGE_FORMAT_DXT1;
	case pragma::asset::VtfInfo::Format::Bc1a:
		return VTFImageFormat::IMAGE_FORMAT_DXT1;//VTFImageFormat::IMAGE_FORMAT_DXT1_ONEBITALPHA;
	case pragma::asset::VtfInfo::Format::Bc2:
		return VTFImageFormat::IMAGE_FORMAT_DXT3;
	case pragma::asset::VtfInfo::Format::Bc3:
		return VTFImageFormat::IMAGE_FORMAT_DXT5;
	case pragma::asset::VtfInfo::Format::R8G8B8A8_UNorm:
		return VTFImageFormat::IMAGE_FORMAT_RGBA8888;
	case pragma::asset::VtfInfo::Format::R8G8_UNorm:
		return VTFImageFormat::IMAGE_FORMAT_UV88;
	case pragma::asset::VtfInfo::Format::R16G16B16A16_SFloat:
		return VTFImageFormat::IMAGE_FORMAT_RGBA16161616F;
	case pragma::asset::VtfInfo::Format::R32G32B32A32_SFloat:
		return VTFImageFormat::IMAGE_FORMAT_RGBA32323232F;
	case pragma::asset::VtfInfo::Format::A8B8G8R8_UNorm_Pack32:
		return VTFImageFormat::IMAGE_FORMAT_ABGR8888;
	case pragma::asset::VtfInfo::Format::B8G8R8A8_UNorm:
		return VTFImageFormat::IMAGE_FORMAT_BGRA8888;
	}
	static_assert(umath::to_integral(pragma::asset::VtfInfo::Format::Count) == 10,"Update this implementation when new format types have been added!");
}
static std::optional<uimg::TextureInfo::InputFormat> get_texture_info_input_format(pragma::asset::VtfInfo::Format format)
{
	switch(format)
	{
	case pragma::asset::VtfInfo::Format::R8G8B8A8_UNorm:
		return uimg::TextureInfo::InputFormat::R8G8B8A8_UInt;
	case pragma::asset::VtfInfo::Format::R16G16B16A16_SFloat:
		return uimg::TextureInfo::InputFormat::R16G16B16A16_Float;
	case pragma::asset::VtfInfo::Format::R32G32B32A32_SFloat:
		return uimg::TextureInfo::InputFormat::R32G32B32A32_Float;
	}
	static_assert(umath::to_integral(pragma::asset::VtfInfo::Format::Count) == 10,"Update this implementation when new format types have been added!");
	return {};
}
static std::optional<uimg::TextureInfo::OutputFormat> get_texture_info_output_format(pragma::asset::VtfInfo::Format format)
{
	switch(format)
	{
	case pragma::asset::VtfInfo::Format::Bc1:
		return uimg::TextureInfo::OutputFormat::BC1;
	case pragma::asset::VtfInfo::Format::Bc1a:
		return uimg::TextureInfo::OutputFormat::BC1a;
	case pragma::asset::VtfInfo::Format::Bc2:
		return uimg::TextureInfo::OutputFormat::BC2;
	case pragma::asset::VtfInfo::Format::Bc3:
		return uimg::TextureInfo::OutputFormat::BC3;
	}
	static_assert(umath::to_integral(pragma::asset::VtfInfo::Format::Count) == 10,"Update this implementation when new format types have been added!");
	return {};
}

namespace source_engine
{
	bool compile_model(const std::string &qcPath,const std::optional<std::string> &game,std::string &outErr);
	bool open_model_in_hlmv(const std::string &mdlPath,const std::optional<std::string> &game,std::string &outErr);
	bool open_model_in_file_explorer(const std::string &mdlPath,const std::optional<std::string> &game,std::string &outErr);
	bool extract_asset_files(const std::unordered_map<std::string,std::string> &files,const std::string &game,std::string &outErr);
	std::optional<std::string> find_game_with_sdk_tools(std::string &outErr);
};
#include <sharedutils/util.h>
#include <util_archive.hpp>
static std::optional<std::string> locate_hlmv(const std::optional<std::string> &game,std::string &outErr)
{
	std::vector<std::string> files;
	const std::string hlmvPath = "../bin/hlmv.exe";
	if(uarch::find_files(hlmvPath,&files,nullptr,true,game) == false || files.empty())
	{
		outErr = "Unable to locate \"" +hlmvPath;
		if(game.has_value())
			outErr += " for game \"" +*game +"\"";
		outErr += "!";
		return {};
	}
	return std::move(files.front());
}
static std::optional<std::string> locate_studiomdl(const std::optional<std::string> &game,std::string &outErr)
{
	std::vector<std::string> files;
	const std::string studiomdlPath = "../bin/studiomdl.exe";
	if(uarch::find_files(studiomdlPath,&files,nullptr,true,game) == false || files.empty())
	{
		outErr = "Unable to locate \"" +studiomdlPath;
		if(game.has_value())
			outErr += " for game \"" +*game +"\"";
		outErr += "!";
		return {};
	}
	return std::move(files.front());
}
bool source_engine::compile_model(const std::string &qcPath,const std::optional<std::string> &game,std::string &outErr)
{
	auto absStudiomdlExe = locate_studiomdl(game,outErr);
	if(absStudiomdlExe.has_value() == false)
		return false;
	std::vector<std::string> files;

	std::string absQcPath;
	if(FileManager::FindAbsolutePath(qcPath,absQcPath) == false)
	{
		outErr = "Unable to locate QC \"" +qcPath +"\"!";
		return false;
	}

	uint32_t exitCode;
	std::vector<std::string> args = {"-nop4"};
	if(game.has_value())
	{
		std::vector<std::string> gamePaths;
		if(uarch::get_mounted_game_paths(*game,gamePaths) == false)
		{
			outErr = "Unable to locate game paths for game \"" +*game +"\"!";
			return false;
		}
		for(auto &gamePath : gamePaths)
		{
			auto gameInfoPath = gamePath +"gameinfo.txt";
			if(FileManager::ExistsSystem(gameInfoPath))
			{
				files.push_back(gameInfoPath);
				break;
			}
		}
		if(files.empty())
		{
			outErr = "Unable to locate \"gameinfo.txt";
			if(game.has_value())
				outErr += " for game \"" +*game +"\"";
			outErr += "!";
			return false;
		}
		args.push_back("-game \"" +ufile::get_path_from_filename(files[0]) +"\"");
	}
	auto result = util::start_and_wait_for_command(("\"" +*absStudiomdlExe +"\" " +ustring::implode(args) +" \"" +absQcPath +"\"").c_str(),nullptr,&exitCode);
	if(result == false)
	{
		if(exitCode != EXIT_SUCCESS)
		{
			outErr = "studiomdl.exe failed with error code " +std::to_string(exitCode) +"!";
			return false;
		}
		outErr = "Unable to open studiomdl.exe!";
	}
	return result;
}
bool source_engine::open_model_in_hlmv(const std::string &mdlPath,const std::optional<std::string> &game,std::string &outErr)
{
	auto absHlmvPath = locate_hlmv(game,outErr);
	if(absHlmvPath.has_value() == false)
		return false;
	std::vector<std::string> files;
	if(uarch::find_files("models/" +mdlPath,&files,nullptr,true,game) == false || files.empty())
	{
		outErr = "Unable to locate \"" +mdlPath;
		if(game.has_value())
			outErr += " for game \"" +*game +"\"";
		outErr += "!";
		return false;
	}
	auto &absMdlPath = files[0];
	auto result = util::start_process(("\"" +*absHlmvPath +"\"").c_str(),"\"" +absMdlPath +"\"",true);
	if(result == false)
	{
		outErr = "Unable to open hlmv.exe!";
		return false;
	}
	return result;
}
bool source_engine::open_model_in_file_explorer(const std::string &mdlPath,const std::optional<std::string> &game,std::string &outErr)
{
	std::vector<std::string> files;
	if(uarch::find_files("models/" +mdlPath,&files,nullptr,true,game) == false || files.empty())
	{
		outErr = "Unable to locate \"" +mdlPath;
		if(game.has_value())
			outErr += " for game \"" +*game +"\"";
		outErr += "!";
		return false;
	}
	auto &absMdlPath = files.front();
	util::open_path_in_explorer(ufile::get_path_from_filename(absMdlPath),ufile::get_file_from_filename(absMdlPath));
	return true;
}
bool source_engine::extract_asset_files(const std::unordered_map<std::string,std::string> &files,const std::string &game,std::string &outErr)
{
	std::vector<std::string> gamePaths;
	if(uarch::get_mounted_game_paths(game,gamePaths) == false || gamePaths.empty())
	{
		outErr = "Unable to locate game paths for game \"" +game +"\"!";
		return false;
	}
	auto &gamePath = gamePaths.front();
	std::vector<std::string> failed;
	for(auto &pair : files)
	{
		std::string absPath;
		if(FileManager::FindAbsolutePath(pair.first,absPath) == false)
		{
			failed.push_back(pair.first);
			continue;
		}
		auto relPath = util::Path::CreateFile(pair.second);
		auto dstPath = gamePath +relPath;
		FileManager::CreateSystemPath(gamePath,ufile::get_path_from_filename(relPath.GetString()).c_str());
		auto result = FileManager::CopySystemFile(absPath.c_str(),dstPath.GetString().c_str());
		if(result)
			continue;
		failed.push_back(pair.second);
	}
	if(!failed.empty())
	{
		outErr = "Failed to copy " +std::to_string(failed.size()) +" asset files:";
		for(auto &f : failed)
			outErr += '\n' +f;
		return false;
	}
	return true;
}
std::optional<std::string> source_engine::find_game_with_sdk_tools(std::string &outErr)
{
	auto &gameMountInfos = uarch::get_game_mount_infos();
	for(auto &info : gameMountInfos)
	{
		if(info.gameEngine != uarch::GameEngine::SourceEngine)
			continue;
		std::string err;
		auto hlmv = locate_hlmv(info.identifier,err);
		if(hlmv.has_value() == false)
			continue;
		auto studiomdl = locate_studiomdl(info.identifier,err);
		if(studiomdl.has_value() == false)
			continue;
		return info.identifier;
	}
	return {};
}

class Model;
class NetworkState;
extern "C" {
	bool PRAGMA_EXPORT export_vtf(
		const std::string &fileName,const std::function<const uint8_t*(uint32_t,uint32_t)> &pfGetImgData,uint32_t width,uint32_t height,uint32_t szPerPixel,
		uint32_t numLayers,uint32_t numMipmaps,bool cubemap,const pragma::asset::VtfInfo &vtfTexInfo,const std::function<void(const std::string&)> &errorHandler,
		bool absoluteFileName
	)
	{
		auto srcFormat = vtfTexInfo.inputFormat;
		auto dstFormat = vtfTexInfo.outputFormat;
		std::function<const uint8_t*(uint32_t,uint32_t)> fGetImgData = pfGetImgData;
		static_assert(umath::to_integral(pragma::asset::VtfInfo::Format::Count) == 10,"Update this implementation when new format types have been added!");
		if(srcFormat != dstFormat && (
			dstFormat == pragma::asset::VtfInfo::Format::Bc1 || dstFormat == pragma::asset::VtfInfo::Format::Bc1a ||
			dstFormat == pragma::asset::VtfInfo::Format::Bc2 || dstFormat == pragma::asset::VtfInfo::Format::Bc3
		))
		{
			std::vector<std::vector<std::vector<uint8_t>>> compressedData;
			
			auto texInfoInputFormat = get_texture_info_input_format(srcFormat);
			auto texInfoOutputFormat = get_texture_info_output_format(dstFormat);
			assert(texInfoInputFormat.has_value() && texInfoOutputFormat.has_value());
			if(texInfoInputFormat.has_value() == false || texInfoOutputFormat.has_value() == false)
				return false;

			uimg::TextureSaveInfo texSaveInfo {};
			auto &texInfo = texSaveInfo.texInfo;
			texInfo.inputFormat = *texInfoInputFormat;
			texInfo.outputFormat = *texInfoOutputFormat;
			if(umath::is_flag_set(vtfTexInfo.flags,pragma::asset::VtfInfo::Flags::NormalMap))
				texInfo.SetNormalMap();
			if(umath::is_flag_set(vtfTexInfo.flags,pragma::asset::VtfInfo::Flags::Srgb))
				texInfo.flags |= uimg::TextureInfo::Flags::SRGB;
			texSaveInfo.width = width;
			texSaveInfo.height = height;
			texSaveInfo.szPerPixel = szPerPixel;
			texSaveInfo.numLayers = numLayers;
			texSaveInfo.numMipmaps = numMipmaps;
			texSaveInfo.cubemap = cubemap;
			auto result = uimg::compress_texture(
				compressedData,[&pfGetImgData](uint32_t level,uint32_t mip,std::function<void()> &deleter) -> const uint8_t* {
					deleter = nullptr;
					return pfGetImgData(level,mip);
				},texSaveInfo,errorHandler
			);
			if(result == false)
				return false;
			fGetImgData = [compressedData{std::move(compressedData)}](uint32_t layer,uint32_t mip) -> const uint8_t* {
				return compressedData[layer][mip].data();
			};
			srcFormat = dstFormat;
		}

		auto vtfSrcFormat = get_vtf_image_format(srcFormat);
		auto vtfDstFormat = get_vtf_image_format(dstFormat);

		VTFLib::CVTFFile f {};
		auto result = f.Create(width,height,1u /* frames */,numLayers,1u /* slices */,vtfSrcFormat,false /* thumbnail */,numMipmaps > 1 /* mipmaps */,false /* nullImageData */);
		if(result == false)
			return false;
		for(auto i=decltype(numLayers){0u};i<numLayers;++i)
		{
			for(auto j=decltype(numMipmaps){0u};j<numMipmaps;++j)
			{
				auto *data = const_cast<vlByte*>(fGetImgData(i,j));
				// TODO: Smallest mipmap has a size of 0 after compression? Why?
				if(data == nullptr)
					continue;
				f.SetData(0u /* frameIndex */,i,0u /* sliceIndex */,j,data);
			}
		}
		if(vtfSrcFormat != vtfDstFormat)
			f = VTFLib::CVTFFile{f,vtfDstFormat};
		if(umath::is_flag_set(vtfTexInfo.flags,pragma::asset::VtfInfo::Flags::Srgb))
			f.SetFlag(VTFImageFlag::TEXTUREFLAGS_SRGB,true);
		if(umath::is_flag_set(vtfTexInfo.flags,pragma::asset::VtfInfo::Flags::NormalMap))
			f.SetFlag(VTFImageFlag::TEXTUREFLAGS_NORMAL,true);
		if(umath::is_flag_set(vtfTexInfo.flags,pragma::asset::VtfInfo::Flags::GenerateMipmaps))
			f.GenerateMipmaps();
		f.GenerateThumbnail();
		return f.Save((util::get_program_path() +'/' +fileName).c_str());
	}
	bool PRAGMA_EXPORT pragma_attach(std::string &errMsg)
	{
		pragma::asset::AssetManager::ImporterInfo importerInfo {};
		importerInfo.name = "MikuMikuDance";
		importerInfo.fileExtensions = {{"pmx",false}};
		engine->GetAssetManager().RegisterImporter(importerInfo,pragma::asset::Type::Model,[](Game &game,ufile::IFile &f,const std::optional<std::string> &mdlPath,std::string &errMsg) -> std::unique_ptr<pragma::asset::IAssetWrapper> {
			auto mdl = game.CreateModel();
			if(import::import_pmx(*game.GetNetworkState(),*mdl,f,mdlPath) == false)
				return nullptr;
			auto wrapper = std::make_unique<pragma::asset::ModelAssetWrapper>();
			wrapper->SetModel(*mdl);
			return wrapper;
		});
		return true;
	}
	void PRAGMA_EXPORT pragma_initialize_lua(Lua::Interface &lua)
	{
		auto &libSteamWorks = lua.RegisterLibrary("import",{
            {"import_pmx",static_cast<int32_t(*)(lua_State*)>([](lua_State *l) {
                return import::import_pmx(l);
            })},
            {"import_vmd",static_cast<int32_t(*)(lua_State*)>([](lua_State *l) {
                return import::import_vmd(l);
            })}
        });
		auto &libSourceEngine = lua.RegisterLibrary("source_engine",{
			{"compile_model",static_cast<int32_t(*)(lua_State*)>([](lua_State *l) -> int32_t {
				std::string qcPath = Lua::CheckString(l,1);
				std::optional<std::string> game {};
				if(Lua::IsSet(l,2))
					game = Lua::CheckString(l,2);
				std::string err;
				auto result = source_engine::compile_model(qcPath,game,err);
				Lua::Push(l,result);
				if(result == false)
				{
					Lua::PushString(l,err);
					return 2;
				}
				return 1;
			})},
			{"open_model_in_hlmv",static_cast<int32_t(*)(lua_State*)>([](lua_State *l) {
				std::string mdlPath = Lua::CheckString(l,1);
				std::optional<std::string> game {};
				if(Lua::IsSet(l,2))
					game = Lua::CheckString(l,2);
				std::string err;
				auto result = source_engine::open_model_in_hlmv(mdlPath,game,err);
				Lua::Push(l,result);
				if(result == false)
				{
					Lua::PushString(l,err);
					return 2;
				}
				return 1;
			})},
			{"open_model_in_file_explorer",static_cast<int32_t(*)(lua_State*)>([](lua_State *l) {
				std::string mdlPath = Lua::CheckString(l,1);
				std::optional<std::string> game {};
				if(Lua::IsSet(l,2))
					game = Lua::CheckString(l,2);
				std::string err;
				auto result = source_engine::open_model_in_file_explorer(mdlPath,game,err);
				Lua::Push(l,result);
				if(result == false)
				{
					Lua::PushString(l,err);
					return 2;
				}
				return 1;
			})},
			{"extract_asset_files",static_cast<int32_t(*)(lua_State*)>([](lua_State *l) {
				auto tFiles = luabind::object{l,luabind::from_stack(l,1)};
				std::unordered_map<std::string,std::string> files;
				Lua::table_to_map<std::string,std::string>(l,tFiles,1,files);
				std::string game = Lua::CheckString(l,2);
				std::string err;
				auto result = source_engine::extract_asset_files(files,game,err);
				Lua::Push(l,result);
				if(result == false)
				{
					Lua::PushString(l,err);
					return 2;
				}
				return 1;
			})},
			{"find_game_with_sdk_tools",static_cast<int32_t(*)(lua_State*)>([](lua_State *l) {
				std::string err;
				auto gameIdentifier = source_engine::find_game_with_sdk_tools(err);
				if(!gameIdentifier.has_value())
				{
					Lua::PushBool(l,false);
					return 1;
				}
				Lua::PushString(l,*gameIdentifier);
				return 1;
			})}
		});
	}
	PRAGMA_EXPORT void initialize_archive_manager()
	{
		load_mounted_games();
		uarch::set_verbose(engine->IsVerbose());
		uarch::initialize();
	}
	PRAGMA_EXPORT void close_archive_manager() {uarch::close();}
	PRAGMA_EXPORT void find_files(const std::string &path,std::vector<std::string> *outFiles,std::vector<std::string> *outDirectories) {uarch::find_files(path,outFiles,outDirectories);}
	PRAGMA_EXPORT bool get_mounted_game_priority(const std::string &game,int32_t &outPriority)
	{
		auto prio = uarch::get_mounted_game_priority(game);
		if(!prio.has_value())
			return false;
		outPriority = *prio;
		return true;
	}
	PRAGMA_EXPORT void set_mounted_game_priority(const std::string &game,int32_t priority) {uarch::set_mounted_game_priority(game,priority);}
	PRAGMA_EXPORT void open_archive_file(const std::string &path,VFilePtr &f,const std::optional<std::string> &gameIdentifier)
	{
		f = FileManager::OpenFile(path.c_str(),"rb");
		if(f == nullptr)
			f = uarch::load(path,nullptr,gameIdentifier);
	}
	PRAGMA_EXPORT bool extract_resource(NetworkState *nw,const std::string &fpath,const std::string &outputPath)
	{
		auto f = uarch::load(fpath);
		auto fv = std::dynamic_pointer_cast<VFilePtrInternalVirtual>(f);
		if(fv != nullptr)
		{
			auto data = fv->GetData();
			if(data == nullptr)
				return false;
			return write_data(outputPath,*data);
		}
		auto fr = std::dynamic_pointer_cast<VFilePtrInternalReal>(f);
		if(fr != nullptr)
		{
			std::vector<uint8_t> data(fr->GetSize());
			fr->Read(data.data(),data.size());
			return write_data(outputPath,data);
		}
		return false;
	}
	PRAGMA_EXPORT bool convert_smd(NetworkState &nw,Model &mdl,VFilePtr &smdFile,const std::string &animName,bool isCollisionMesh,std::vector<std::string> &outTextures)
	{
		auto smd = SMDModel::Load(smdFile);
		if(smd == nullptr)
			return false;
		return load_smd(&nw,animName,mdl,*smd,isCollisionMesh,outTextures);
	}
	PRAGMA_EXPORT bool convert_lightmap_data_to_bsp_luxel_data(NetworkState &nw,const std::string &mapPath,const uimg::ImageBuffer &imgBuf,uint32_t atlasWidth,uint32_t atlasHeight,std::string &outErrMsg)
	{
		return pragma::asset::vbsp::BSPConverter::ConvertLightmapToBSPLuxelData(nw,mapPath,imgBuf,atlasWidth,atlasHeight,outErrMsg);
	}
	PRAGMA_EXPORT bool convert_source2_map(Game &game,const std::string &path)
	{
		std::optional<std::string> sourcePath = {};
		auto fullPath = path;
		ufile::remove_extension_from_filename(fullPath);
		fullPath += ".vwrld_c"; // TODO: vmap_c?
		auto f = FileManager::OpenFile(fullPath.c_str(),"rb");
		if(f == nullptr)
			f = uarch::load(fullPath,&sourcePath);
		if(f == nullptr)
			return false;
		auto worldData = source2::convert::convert_map(game,f,path);
		if(worldData == nullptr)
			return false;
		std::string errMsg;
		return worldData->Write(path,&errMsg);
	}
	PRAGMA_EXPORT bool convert_hl2_map(Game &game,const std::string &path)
	{
		auto bspConverter = pragma::asset::vbsp::BSPConverter::Open(game,path);
		if(bspConverter == nullptr)
		{
			auto vmfPath = path;
			ufile::remove_extension_from_filename(vmfPath);
			vmfPath += ".vmf";
			if(FileManager::Exists(vmfPath) == false)
				return false;
			Con::cout<<"Found VMF version of map: '"<<vmfPath<<"'! Compiling..."<<Con::endl;
			Con::cwar<<"----------- VMF Compile LOG -----------"<<Con::endl;
			auto r = vmf::load(*game.GetNetworkState(),vmfPath,[](const std::string &msg) {
				Con::cout<<"> "<<msg<<Con::endl;
				});
			Con::cwar<<"---------------------------------------"<<Con::endl;
			return r == vmf::ResultCode::Success;
		}
		Con::cout<<"Found BSP version of map: '"<<path<<"'! Converting..."<<Con::endl;
		bspConverter->SetMessageLogger([](const std::string &msg) {
			Con::cout<<"> "<<msg<<Con::endl;
			});
		bspConverter->StartConversion();

		return true;
	}
	PRAGMA_EXPORT bool convert_source2_model(
		NetworkState *nw,const std::function<std::shared_ptr<Model>()> &fCreateModel,
		const std::function<bool(const std::shared_ptr<Model>&,const std::string&,const std::string&)> &fCallback,
		const std::string &path,const std::string &mdlName,std::ostream *optLog
	)
	{
		std::optional<std::string> sourcePath = {};
		auto fullPath = path +mdlName +".vmdl_c";
		auto f = FileManager::OpenFile(fullPath.c_str(),"rb");
		if(f == nullptr)
			f = uarch::load(fullPath,&sourcePath);

		if(f == nullptr)
			return false;

		if(sourcePath.has_value())
			Con::cout<<"Found model in '"<<*sourcePath<<"'! Porting..."<<Con::endl;

		std::vector<std::string> textures;
		auto r = ::import::load_source2_mdl(*nw->GetGameState(),f,fullPath,fCallback,true,textures,optLog);
		if(r == nullptr)
			return false;
		return fCallback(r,path,mdlName);
	}
	PRAGMA_EXPORT bool convert_hl2_model(
		NetworkState *nw,const std::function<std::shared_ptr<Model>()> &fCreateModel,
		const std::function<bool(const std::shared_ptr<Model>&,const std::string&,const std::string&)> &fCallback,
		const std::string &path,const std::string &mdlName,std::ostream *optLog
	)
	{
		const std::array<std::string,7> extensions = {
			"dx80.vtx",
			"dx90.vtx",
			"mdl",
			"phy",
			"sw.vtx",
			"vvd",
			"ani"
		};
		std::optional<std::string> sourcePath = {};
		std::unordered_map<std::string,VFilePtr> files;
		for(auto &ext : extensions)
		{
			auto subPath = path +mdlName +"." +ext;
			auto f = FileManager::OpenFile(subPath.c_str(),"rb");
			if(f == nullptr)
				f = uarch::load(subPath,(ext == "mdl") ? &sourcePath : nullptr);
			
			if(f != nullptr)
				files[ext] = f;
		}
		if(files.find("dx90.vtx") != files.end())
			files["vtx"] = files["dx90.vtx"];
		else if(files.find("dx80.vtx") != files.end())
			files["vtx"] = files["dx80.vtx"];
		else if(files.find("sw.vtx") != files.end())
			files["vtx"] = files["sw.vtx"];

		if(sourcePath.has_value())
			Con::cout<<"Found model in '"<<*sourcePath<<"'! Porting..."<<Con::endl;

		std::vector<std::string> textures;
		auto r = ::import::load_mdl(nw,files,fCreateModel,fCallback,true,textures,optLog);
		if(r == nullptr)
			return false;
		return fCallback(r,path,mdlName);
	}
	PRAGMA_EXPORT bool convert_nif_model(NetworkState *nw,const std::function<std::shared_ptr<Model>()> &fCreateModel,const std::function<bool(const std::shared_ptr<Model>&,const std::string&,const std::string&)> &fCallback,const std::string &pathRoot,const std::string &mdlName)
	{
		auto mdl = fCreateModel();
		auto fpath = pathRoot +mdlName +".nif";
		auto path = ufile::get_path_from_filename(fpath);
		::import::load_nif(nw,mdl,path +"skeleton.nif"); // Attempt to load skeleton before loading actual mesh (To retrieve correct bone hierarchy)
		if(::import::load_nif(nw,mdl,fpath) == false)
			return false;

		auto numMeshGroups = mdl->GetMeshGroupCount();
		for(auto i=decltype(numMeshGroups){0};i<numMeshGroups;++i)
			mdl->GetBaseMeshes().push_back(i);

		auto refAnim = pragma::animation::Animation::Create();
		auto &skeleton = mdl->GetSkeleton();
		auto numBones = skeleton.GetBoneCount();
		auto &boneList = refAnim->GetBoneList();
		refAnim->ReserveBoneIds(refAnim->GetBoneCount() +numBones);
		for(auto i=decltype(numBones){0};i<numBones;++i)
			refAnim->AddBoneId(i);
		auto refFrame = Frame::Create(mdl->GetReference());
		refAnim->AddFrame(refFrame);
		mdl->AddAnimation("reference",refAnim);
		//refFrame->Localize(*refAnim,skeleton);
		//mdl->GetReference().Localize(*refAnim,skeleton);
		mdl->GetReference().Globalize(*refAnim,skeleton);

		//mdl->GenerateBindPoseMatrices();

		std::function<void(const std::string&,uint32_t)> fLoadAnimations = nullptr;
		fLoadAnimations = [&fLoadAnimations,nw,&mdl](const std::string &path,uint32_t depth) {
			std::vector<std::string> files;
			FileManager::FindFiles((path +"*.kf").c_str(),&files,nullptr);
			uarch::find_files(path +"*kf",&files,nullptr);
			// TODO: Find in archive
			for(auto &f : files)
			{
				try
				{
					::import::load_nif(nw,mdl,path +f);
				}
				catch(const std::exception &e)
				{
					std::cout<<"ex: "<<e.what()<<std::endl;
				}
			}
			if(depth == 0)
				return;
			std::vector<std::string> dirs;
			FileManager::FindFiles((path +"*").c_str(),nullptr,&dirs);
			uarch::find_files(path +"*",nullptr,&dirs);
			for(auto &dir : dirs)
				fLoadAnimations(path +dir +"\\",depth -1);
		};
		fLoadAnimations(path,4);

		auto &textures = mdl->GetTextures();
		auto *texGroup = mdl->CreateTextureGroup();
		texGroup->textures.reserve(textures.size());
		for(auto i=decltype(textures.size()){0};i<textures.size();++i)
			texGroup->textures.push_back(i); // TODO: Generate material files
		return fCallback(mdl,pathRoot,mdlName);
	}
};
