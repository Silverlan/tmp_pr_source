#include <datasystem.h>
#include <pragma/debug/debug_performance_profiler.hpp>
#include <pragma/level/mapinfo.h>
#include "nif.hpp"
#include <pragma/networkstate/networkstate.h>
#include <mathutil/uvec.h>
#include <pragma/model/animation/skeleton.h>
#include <pragma/model/model.h>
#include <pragma/model/modelmesh.h>
#include <sharedutils/util_string.h>
#include <sharedutils/util_file.h>
#include <util_archive.hpp>
#include <niflib.h>
#include <obj/NiObject.h>
#include <obj/NiNode.h>
#include <obj/NiTriShape.h>
#include <obj/NiTriShapeData.h>
#include <obj/NiTriStrips.h>
#include <obj/NiTriStripsData.h>
#include <obj/NiExtraData.h>
#include <obj/NiSkinInstance.h>
#include <obj/NiSkinData.h>
#include <obj/NiControllerSequence.h>
#include <obj/NiInterpolator.h>
#include <obj/NiTransformInterpolator.h>
#include <obj/NiBSplineCompTransformInterpolator.h>
#include <obj/NiTransformData.h>
#include <obj/NiStringPalette.h>
#include <obj/NiTimeController.h>
#include <obj/BSLightingShaderProperty.h>
#include <obj/BSShaderPPLightingProperty.h>
#include <obj/BSShaderTextureSet.h>
#include <obj/NiTexturingProperty.h>
#include <obj/NiMaterialProperty.h>
#include <obj/NiSourceTexture.h>
#include <fsys/filesystem_stream.hpp>
#include <sharedutils/util_path.hpp>
#include <pragma/game/game_resources.hpp>
#include <materialmanager.h>

struct membuf : std::streambuf
{
    membuf(char* begin, char* end) {
        this->setg(begin, begin, end);
    }
};

#pragma optimize("",off)
static Vector3 nif_vector_to_engine_vector(const Niflib::Vector3 &v) {return {v.x,v.y,v.z};}
static Quat nif_quat_to_engine_quat(const Niflib::Quaternion &q)
{
	auto r = Quat(q.w,q.x,q.y,q.z);
	uquat::normalize(r);
	return r;
}

static std::shared_ptr<Bone> nif_node_to_engine_bone(const Niflib::Ref<Niflib::NiNode> &node,Skeleton &skeleton)
{
	auto name = node->GetName();
	auto &skBones = skeleton.GetBones();
	auto it = std::find_if(skBones.begin(),skBones.end(),[&name](const std::shared_ptr<Bone> &bone) {
		return (bone->name == name) ? true : false;
	});
	assert(it != skBones.end());
	return (it != skBones.end()) ? *it : nullptr;
}

const float FramesPerSecond = 15.f;//30.0f;
const float FramesIncrement = 1.0f/30.0f;
const int TicksPerFrame = 1;//160;//GetTicksPerFrame();

inline uint32_t TimeToFrame(float t) {
   return uint32_t(t * FramesPerSecond * TicksPerFrame);
}
inline float FrameToTime(uint32_t t) {
   return float(t) / (FramesPerSecond * TicksPerFrame);
}

static bool get_transform_data(Niflib::ControllerLink &lnk,const string &name,Niflib::NiKeyframeDataRef &outData,Vector3 &p,Quat &q,float &s)
{
   //Control *c = GetTMController(name);
  // if (NULL == c)
  //    return false;

   if (lnk.interpolator){
	   auto interpType = lnk.interpolator->GetType().GetTypeName();
	   std::cout<<"Type: "<<interpType<<std::endl;
	   auto *interpolator = static_cast<Niflib::NiInterpolator*>(lnk.interpolator);
	   if(dynamic_cast<Niflib::NiTransformInterpolator*>(interpolator) != nullptr)
	   {
		   Niflib::NiTransformInterpolator *interp = static_cast<Niflib::NiTransformInterpolator*>(static_cast<Niflib::NiInterpolator*>(lnk.interpolator));
         if (Niflib::NiTransformDataRef data = interp->GetData()){
            outData = Niflib::StaticCast<Niflib::NiKeyframeData>(data);

            // Set initial conditions
            p = nif_vector_to_engine_vector(interp->GetTranslation());
            q = nif_quat_to_engine_quat(interp->GetRotation());
            s = interp->GetScale();
            return true;
         }
      } else if(dynamic_cast<Niflib::NiBSplineTransformInterpolator*>(interpolator) != nullptr)
	  {
		 Niflib::NiBSplineTransformInterpolator *interp = static_cast<Niflib::NiBSplineTransformInterpolator*>(static_cast<Niflib::NiInterpolator*>(lnk.interpolator));
         int npoints = interp->GetNumControlPoints();

         if (npoints > 3)
         {
            Niflib::NiKeyframeDataRef data = static_cast<Niflib::NiKeyframeData*>(Niflib::NiKeyframeData::Create());
            data->SetRotateType(Niflib::QUADRATIC_KEY);
            data->SetTranslateType(Niflib::QUADRATIC_KEY);
            data->SetScaleType(Niflib::QUADRATIC_KEY);
            data->SetTranslateKeys( interp->SampleTranslateKeys(npoints, 3) );
            data->SetQuatRotateKeys( interp->SampleQuatRotateKeys(npoints, 3) );
            data->SetScaleKeys( interp->SampleScaleKeys(npoints, 3) );

            outData = data;

            p = nif_vector_to_engine_vector(interp->GetTranslation());
            q = nif_quat_to_engine_quat(interp->GetRotation());
            s = interp->GetScale();
            return true;
         }
      }
   } else if (lnk.controller) {
      /*if (Niflib::NiTransformControllerRef tc = Niflib::DynamicCast<Niflib::NiTransformController>(lnk.controller)) {
         if (Niflib::NiTransformInterpolatorRef interp = tc->GetInterpolator()) {
            if (Niflib::NiTransformDataRef data = interp->GetData()){
               outData = Niflib::StaticCast<Niflib::NiKeyframeData>(data);

               p = nif_vector_to_engine_vector(interp->GetTranslation());
               q = nif_quat_to_engine_quat(interp->GetRotation(), true);
               s = interp->GetScale();
               return true;
            }
         }
      } else if (Niflib::NiKeyframeControllerRef kfc = Niflib::DynamicCast<Niflib::NiKeyframeController>(lnk.controller)) {
         if (Niflib::NiKeyframeDataRef data = kfc->GetData()) {
            outData = data;
            // values will not be used in transforms
            p = Vector3(FloatNegINF, FloatNegINF, FloatNegINF);
            q = Quat(FloatNegINF, FloatNegINF, FloatNegINF, FloatNegINF);
            s = FloatNegINF;
            return true;
         }
	  }*/
   }
   return false;
}

bool import::load_nif(NetworkState *nw,std::shared_ptr<::Model> &mdl,const std::string &fpath)
{
	Niflib::NifInfo info {};
	Niflib::NiObjectRef obj = nullptr;
	fsys::Stream stream(fpath.c_str());
	if(stream.isvalid() == true)
		obj = Niflib::ReadNifTree(stream,&info);
	else
	{
		auto bsaPath = fpath;
		//ustring::replace(bsaPath,"models\\","meshes\\");
		std::vector<uint8_t> data {};
		auto r = uarch::load(bsaPath,data);
		if(r == false)
			return false;
		/*auto pair = FileManager::AddVirtualFile(bsaPath,reinterpret_cast<char*>(data.data()),data.size(),false);
		if(pair.first == nullptr || pair.second == nullptr)
			return false;

		pair.first->Remove(pair.second);*/

		auto *pdata = reinterpret_cast<char*>(const_cast<uint8_t*>(data.data()));
		membuf buf(pdata,pdata +data.size());
		std::istream bufStream(&buf);
		try
		{
			obj = Niflib::ReadNifTree(bufStream,&info);
		}
		catch(const std::exception &e)
		{
			std::cout<<"Ex: "<<e.what()<<std::endl;
		}
	}
	if(obj == nullptr)
		return false;

	auto &skeleton = mdl->GetSkeleton();

	// Animation
	if(obj->GetType().GetTypeName() == "NiControllerSequence")
	{
		auto *seq = static_cast<Niflib::NiControllerSequence*>(static_cast<Niflib::NiObject*>(obj));
		float start = seq->GetStartTime();
		float stop = seq->GetStopTime();
		//seq->
		//std::cout<<"Time: "<<start<<"; "<<stop<<std::endl;
		//for(;;);
		/*fsys::Stream stream(fpath.c_str());
		auto list = Niflib::ReadNifList(stream,&info);
		for(auto &el : list)
		{
			std::cout<<"Element: "<<static_cast<Niflib::NiObject*>(el)->GetType().GetTypeName()<<std::endl;
		}
		for(;;);*/

		//auto *seq = static_cast<Niflib::NiControllerSequence*>(static_cast<Niflib::NiObject*>(obj));
		std::cout<<"Adding animation "<<seq->GetName()<<std::endl;
		if(seq->GetName() == "SpecialIdle_Eat")
		{
			std::cout<<"Test..."<<std::endl;
		}
		auto refAnim = mdl->GetAnimation(0);
		auto reference = refAnim->GetFrame(0);
		auto anim = Animation::Create();
		auto &bones = skeleton.GetBones();
		auto numBones = reference->GetBoneCount();//skeleton.GetBoneCount();
		anim->ReserveBoneIds(anim->GetBoneCount() +numBones);
		for(auto i=decltype(numBones){0};i<numBones;++i)
			anim->AddBoneId(i);
		
		std::vector<std::shared_ptr<Frame>> frames;
		const auto fSetBoneTransform = [&frames,&anim,reference,numBones](uint32_t frameId,uint32_t boneId,const Vector3 *pos,const Quat *rot) {
			if(frameId >= frames.size());
			{
				for(auto i=frames.size();i<=frameId;++i)
				{
					frames.push_back(Frame::Create(*reference));
					anim->AddFrame(frames.back());
				}
			}
			auto &frame = frames.at(frameId);
			if(pos != nullptr)
				frame->SetBonePosition(boneId,*pos);
			if(rot != nullptr)
				frame->SetBoneOrientation(boneId,*rot);
		};
		for(auto ref : seq->GetControllerData())
		{
			if(ref.interpolator == nullptr)
				continue;

				 string name = ref.targetName;
				 if (name.empty() && ref.nodeNameOffset >= 0) {
					Niflib::NiStringPaletteRef strings = ref.stringPalette;
					name = strings->GetSubStr(ref.nodeNameOffset);
				 }
				 if (name.empty()) {
					name = ref.nodeName;
				 }
				 if (name.empty())
					continue;


			auto it = std::find_if(bones.begin(),bones.end(),[&name](const std::shared_ptr<Bone> &bone) {
				return (bone->name == name) ? true : false;
			});
			if(it == bones.end())
				continue; // TODO
			auto boneId = it -bones.begin();

				 string type = ref.controllerType;
				 if (type.empty()) {
					Niflib::NiStringPaletteRef strings = ref.stringPalette;
					type = strings->GetSubStr(ref.controllerTypeOffset);
				 }
				 if (type.empty()) {
					if (ref.controller != NULL) {
					   type = ref.controller->GetType().GetTypeName();
					}
				 }
				 if (type.empty())
					continue;
				 std::cout<<"Controller type: "<<type<<std::endl;
				 if(type == "NiTransformController")
				 {
					Niflib::NiKeyframeDataRef data;
					Vector3 p; Quat q; float s;
					//std::cout<<"Bone: "<<(*it)->name<<std::endl;
					try
					{
						if (get_transform_data(ref, name, data, p, q, s)) {
							//auto transKeys = data->GetTranslateKeys();
							//std::cout<<"Translate key count: "<<transKeys.size()<<std::endl;
							//fSetBoneTransform(0,boneId,p,q); // TODO
							auto transKeys = data->GetTranslateKeys();
							//for(auto &key : transKeys)
							for(auto i=decltype(transKeys.size()){0};i<transKeys.size();++i)
							{
								auto &key = transKeys.at(i);
								auto tpos = TimeToFrame(key.time);
								auto pos = nif_vector_to_engine_vector(key.data);
								fSetBoneTransform(tpos,boneId,&pos,nullptr);
							}

							if(data->GetRotateType() == Niflib::KeyType::XYZ_ROTATION_KEY)
							{

								auto rotXKeys = data->GetXRotateKeys();
								auto rotYKeys = data->GetYRotateKeys();
								auto rotZKeys = data->GetZRotateKeys();
								auto size = umath::max(umath::max(rotXKeys.size(),rotYKeys.size()),rotZKeys.size());
								for(auto i=decltype(size){0};i<size;++i)
								{
									auto x = (i < rotXKeys.size()) ? rotXKeys.at(i) : Niflib::Key<float>{};
									auto y = (i < rotYKeys.size()) ? rotYKeys.at(i) : Niflib::Key<float>{};
									auto z = (i < rotZKeys.size()) ? rotZKeys.at(i) : Niflib::Key<float>{};
									assert(x.time == y.time && y.time == z.time);
									std::array<float,3> values = {x.data,y.data,z.data};
									auto trot = TimeToFrame(x.time);
									//auto rot = uquat::create(EulerAngles(y.data,z.data,x.data));
									static std::array<uint8_t,3> order = {2,0,1};
									static std::array<float,3> signs = {1.f,1.f,1.f};
									// TODO: RAD TO ANG
									auto rot = uquat::create(EulerAngles(
										umath::rad_to_deg(values.at(order.at(0)) *signs.at(0)),
										umath::rad_to_deg(values.at(order.at(1)) *signs.at(1)),
										umath::rad_to_deg(values.at(order.at(2))) *signs.at(2))
									);
									fSetBoneTransform(trot,boneId,nullptr,&rot);
								}
							}
							else
							{
								auto rotKeys = data->GetQuatRotateKeys();
								//for(auto &key : rotKeys)
								for(auto i=decltype(rotKeys.size()){0};i<rotKeys.size();++i)
								{
									auto &key = rotKeys.at(i);
									auto trot = TimeToFrame(key.time);
									auto rot = nif_quat_to_engine_quat(key.data);
									fSetBoneTransform(trot,boneId,nullptr,&rot);
								}
							}
							//std::cout<<"Size: "<<transKeys.size()<<","<<rotKeys.size()<<" ("<<p.x<<","<<p.y<<","<<p.z<<")"<<std::endl;

							//AddValues
						   /*PosRotScaleNode(n, p, q, s, prsDefault, TimeToFrame(time));
						   if (ai.AddValues(c, data, time)) {
							  minTime = min(minTime, start);
							  maxTime = max(maxTime, stop);
							  ok = true;
						   }*/
						}
					}
					catch(const std::exception &e)
					{
						std::cout<<"EXXX: "<<e.what()<<std::endl;
					}
				 }
			/*if(ref.interpolator == nullptr)
				continue;
			std::string nodeName = ref.nodeName.c_str();
			auto it = std::find_if(bones.begin(),bones.end(),[&nodeName](const std::shared_ptr<Bone> &bone) {
				return (bone->name == nodeName) ? true : false;
			});
			if(it == bones.end())
				continue; // TODO
			auto boneId = it -bones.begin();
			auto type = ref.interpolator->GetType().GetTypeName();
			if(type == "NiTransformInterpolator")
			{
				auto *ip = static_cast<Niflib::NiTransformInterpolator*>(static_cast<Niflib::NiInterpolator*>(ref.interpolator));
				auto tdata = ip->GetData();
				if(tdata != nullptr)
				{
					auto translations = tdata->GetTranslateKeys();
					auto rotations = tdata->GetQuatRotateKeys(); // TODO: Same size as translations?
					for(auto i=decltype(translations.size()){0};i<translations.size();++i)
					{
						auto &pos = (i < translations.size()) ? translations.at(i).data : Niflib::Vector3{};
						auto &rot = (i < rotations.size()) ? rotations.at(i).data : Niflib::Quaternion{};
						fSetBoneTransform(i,boneId,Vector3(pos.x,pos.y,pos.z),Quat(rot.w,rot.x,rot.y,rot.z));
					}
				}
			}
			else if(type == "NiBSplineCompTransformInterpolator")
			{
				auto *ip = static_cast<Niflib::NiBSplineCompTransformInterpolator*>(static_cast<Niflib::NiInterpolator*>(ref.interpolator));
				auto translations = ip->GetTranslateControlData();
				auto rotations = ip->GetQuatRotateControlData(); // TODO: Same size as translations?
				for(auto i=decltype(translations.size()){0};i<translations.size();++i)
				{
					auto &pos = (i < translations.size()) ? translations.at(i) : Niflib::Vector3{};
					auto rot = (i < rotations.size()) ? rotations.at(i) : Niflib::Quaternion{};
					fSetBoneTransform(i,boneId,Vector3(pos.x,pos.y,pos.z),Quat(rot.w,rot.x,rot.y,rot.z));
				}
			}*/
		}
		mdl->AddAnimation(seq->GetName(),anim);
		std::cout<<"Animation has been added!"<<std::endl;
		return true;
	}

	auto refs = obj->GetRefs();
	std::cout<<"NumRefs: "<<refs.size()<<std::endl;
	struct TriObject
	{
		Niflib::NiAVObject *object = nullptr;
		std::shared_ptr<ModelMesh> mesh = nullptr;
		bool bsGeometry = false;
	};
	std::vector<TriObject> triObjects {};
	std::vector<Niflib::NiNode*> nodes {};
	for(auto &ref : refs)
	{
		std::cout<<"-----------------------------------------------"<<std::endl;
		auto &type = ref->GetType();
		auto name = type.GetTypeName();
		if(name == "NiNode")
		{
			auto *node = static_cast<Niflib::NiNode*>(static_cast<Niflib::NiObject*>(ref));
			nodes.push_back(node);
			//node->
		}
		else if(name == "NiTriShape" || name == "NiTriStrips")
		{
			triObjects.push_back({});
			auto &o = triObjects.back();
			o.object = static_cast<Niflib::NiAVObject*>(static_cast<Niflib::NiObject*>(ref));
			o.bsGeometry = false;
		}
		else if(name == "BSTriShape" || name == "BSMeshLODTriShape" || name == "BSSubIndexTriShape")
		{
			//Niflib::BSSubIndexTriShape *x;
			//x->GetBoneWeights()
			//x->
			triObjects.push_back({});
			auto &o = triObjects.back();
			o.object = static_cast<Niflib::NiAVObject*>(static_cast<Niflib::NiObject*>(ref));
			o.bsGeometry = true;
		}
	}

	// Generate bones
	auto &refPose = mdl->GetReference();
	auto &rootBones = skeleton.GetRootBones();
	auto &skBones = skeleton.GetBones();
	std::function<void(Niflib::NiNode*,std::shared_ptr<Bone>)> fInsertBone = nullptr;
	fInsertBone = [&fInsertBone,&skeleton,&rootBones,&refPose,&skBones](Niflib::NiNode *node,std::shared_ptr<Bone> parent) {
		std::shared_ptr<Bone> skBone = nullptr;
		auto name = node->GetName();
		auto it = std::find_if(skBones.begin(),skBones.end(),[&name](const std::shared_ptr<Bone> &bone) {
			return (bone->name == name) ? true : false;
		});
		if(it != skBones.end())
			skBone = *it;
		else
		{
			auto *pBone = new Bone();
			pBone->name = name;
			auto boneId = skeleton.AddBone(pBone);
			skBone = skeleton.GetBone(boneId).lock();
			auto t = node->GetLocalTranslation();
			auto r = node->GetLocalRotation().AsQuaternion();
			auto s = node->GetLocalScale();
			refPose.SetBoneCount(refPose.GetBoneCount() +1);
			refPose.SetBonePosition(boneId,nif_vector_to_engine_vector(t));
			refPose.SetBoneOrientation(boneId,Quat(r.w,r.x,r.y,r.z));

			auto pair = std::make_pair(boneId,skBone);
			if(parent != nullptr)
			{
				parent->children.insert(pair);
				skBone->parent = parent;
			}
			else
				rootBones.insert(pair);
		}
		for(auto &child : node->GetChildren())
		{
			if(child->GetType().GetTypeName() != "NiNode")
				continue;
			fInsertBone(static_cast<Niflib::NiNode*>(static_cast<Niflib::NiObject*>(child)),skBone);
		}
	};
	if(triObjects.empty() == true)
	{
		// Import bones from nodes instead
		auto numBones = nodes.size();
		skeleton.GetBones().reserve(numBones);
		for(auto &node : nodes)
			fInsertBone(node,nullptr);
	}
	else
	{
		for(auto &o : triObjects)
		{
			std::vector<Niflib::Ref<Niflib::NiNode>> bones {};
			/*if(o.bsGeometry == true)
			{
				auto *triShape = static_cast<Niflib::BSTriShape*>(o.object);
				auto skin = triShape->GetSkin();
				if(skin != nullptr)
				{
					auto *skinInstance = dynamic_cast<Niflib::BSSkin__Instance*>(static_cast<Niflib::NiObject*>(skin));
					if(skinInstance != nullptr)
						bones = skinInstance->GetBones();
				}
			}
			else*/ // TODO
			{
				auto *geometry = static_cast<Niflib::NiGeometry*>(o.object);
				auto skin = geometry->GetSkinInstance();
				if(skin != nullptr)
					bones = skin->GetBones();
			}
			auto numBones = bones.size();
			if(numBones > refPose.GetBoneCount())
				refPose.SetBoneCount(numBones);
			skeleton.GetBones().reserve(numBones);
			for(auto &bone : bones)
				fInsertBone(bone,nullptr);
		}
	}

	// Build bone hierarchy
	/*for(auto &bone : skeleton.GetBones())
	{
		if(bone->parent.lock() == nullptr)
			rootBones.insert(std::remove_reference_t<decltype(rootBones)>::value_type(bone->ID,bone));
		else
			bone->parent.lock()->children.insert(std::remove_reference_t<decltype(rootBones)>::value_type(bone->ID,bone));
	}*/

	auto fGetMaterials = [&mdl,nw](Niflib::NiAVObject &o) -> std::vector<uint32_t> {
		std::vector<uint32_t> meshMaterials;
		const auto fConvertTextureName = [](std::string &tex) -> util::Path {
			if(ustring::substr(tex,0,9) == "textures\\")
				tex = ustring::substr(tex,9);
			auto path = ufile::get_path_from_filename(tex);
			tex = ufile::get_file_from_filename(tex);
			ufile::remove_extension_from_filename(tex);
			return path;
		};

		const auto fAddMaterial = [&fConvertTextureName,&mdl,&meshMaterials,nw](std::string albedoMap,const std::optional<std::string> &optNormalMap) {
			auto albedoPath = fConvertTextureName(albedoMap);
			if(albedoPath.IsEmpty())
				return;

			::util::port_file(nw,("materials/" +albedoPath +albedoMap +".dds").GetString());

			auto settings = ds::create_data_settings({});
			auto root = std::make_shared<ds::Block>(*settings);
			root->AddValue("texture",Material::ALBEDO_MAP_IDENTIFIER,(albedoPath +albedoMap).GetString());

			std::cout<<"Adding texture "<<albedoMap<<" ("<<albedoPath<<")..."<<std::endl;
			mdl->AddTexturePath(albedoPath.GetString());
			meshMaterials.push_back(mdl->AddTexture(albedoMap,nullptr));
			// TODO: Import dds directly!!

			if(optNormalMap.has_value())
			{
				auto normalMap = *optNormalMap;
				auto normalPath = fConvertTextureName(normalMap);
				root->AddValue("texture",Material::NORMAL_MAP_IDENTIFIER,(normalPath +normalMap).GetString());
			}

			auto &matManager = nw->GetMaterialManager();
			auto *mat = matManager.CreateMaterial((albedoPath +albedoMap).GetString(),"pbr",root);
			mat->Save(mat->GetName(),"addons/imported/");
		};

		// Find materials
		auto props = o.GetProperties();
		for(auto &prop : props)
		{
			Niflib::Ref<Niflib::BSShaderTextureSet> texSet = nullptr;
			auto *propPPLighting = dynamic_cast<Niflib::BSShaderPPLightingProperty*>(static_cast<Niflib::NiProperty*>(prop));
			if(propPPLighting != nullptr)
				texSet = propPPLighting->GetTextureSet();
			else
			{
				auto *propLighting = dynamic_cast<Niflib::BSLightingShaderProperty*>(static_cast<Niflib::NiProperty*>(prop));
				if(propLighting != nullptr)
					texSet = propLighting->GetTextureSet();
				else
				{
					auto *propTexturing = dynamic_cast<Niflib::NiTexturingProperty*>(static_cast<Niflib::NiProperty*>(prop));
					if(propTexturing != nullptr)
					{
						auto &baseMap = propTexturing->GetTexture(Niflib::TexType::BASE_MAP);
						auto &baseSource = baseMap.source;
						if(baseSource != nullptr)
						{
							auto &bumpMap = propTexturing->GetTexture(Niflib::TexType::BUMP_MAP); // Unused
							auto &darkMap = propTexturing->GetTexture(Niflib::TexType::DARK_MAP); // Unused
							auto &detailMap = propTexturing->GetTexture(Niflib::TexType::DETAIL_MAP); // Unused
							auto &glossMap = propTexturing->GetTexture(Niflib::TexType::GLOSS_MAP); // Unused
							auto &normalMap = propTexturing->GetTexture(Niflib::TexType::NORMAL_MAP);
							auto &decal0Map = propTexturing->GetTexture(Niflib::TexType::DECAL_0_MAP); // Unused
							auto &decal1Map = propTexturing->GetTexture(Niflib::TexType::DECAL_1_MAP); // Unused
							auto &decal2Map = propTexturing->GetTexture(Niflib::TexType::DECAL_2_MAP); // Unused
							auto &decal3Map = propTexturing->GetTexture(Niflib::TexType::DECAL_3_MAP); // Unused
							auto &normalSource = normalMap.source;
							fAddMaterial(static_cast<Niflib::NiSourceTexture*>(baseSource)->GetTextureFileName(),(normalSource != nullptr) ? static_cast<Niflib::NiSourceTexture*>(normalMap.source)->GetTextureFileName() : "");
						}
					}
				}
			}
			if(texSet == nullptr)
				continue;
			auto &textures = texSet->GetTextures();
			// Note: This seems to work for Fallout: NV assets? TODO: Test it with other games
			if(textures.size() > 0)
			{
				auto baseMap = textures.front();//texSet->GetTexture(Niflib::TexType::BASE_MAP);
				auto normalMap = (textures.size() > 1) ? textures.at(1) : std::optional<std::string>{};//texSet->GetTexture(Niflib::TexType::NORMAL_MAP);
				fAddMaterial(baseMap,normalMap);
			}
		}
		//
		return meshMaterials;
	};

	auto fAddMesh = [&mdl,&fGetMaterials,nw](const std::string &name,Niflib::NiGeometry *geometry,const Vector3 &offset) {
		//if(name.length() > 9 && name.substr(name.length() -9) == "Activator")
		//	return;
		auto meshMaterials = fGetMaterials(*geometry);

		std::vector<Niflib::Vector3> verts {};
		std::vector<Niflib::Vector3> normals {};
		std::vector<Niflib::TexCoord> texCoords {};
		std::vector<Niflib::Triangle> triangles {};
		int32_t geoMatId = -1;
		/*if(o.bsGeometry == true)
		{
		auto *triShape = static_cast<Niflib::BSTriShape*>(o.object);
		auto *subTriShape = dynamic_cast<Niflib::BSSubIndexTriShape*>(triShape);
		if(subTriShape != nullptr)
		{
		auto &segments = subTriShape->GetSegments();
		for(auto i=decltype(segments.size()){0};i<segments.size();++i)
		{
		auto &segment = segments.at(i);
		if(segment.numRecords == 0)
		{
		auto subTriangles = subTriShape->GetTriangles(i);
		triangles.reserve(triangles.size() +subTriangles.size());
		for(auto &tri : subTriangles)
		triangles.push_back(tri);
		}
		else
		{
		for(auto j=decltype(segment.numRecords){0};j<segment.numRecords;++j)
		{
		auto subTriangles = subTriShape->GetTriangles(i,j);
		triangles.reserve(triangles.size() +subTriangles.size());
		for(auto &tri : subTriangles)
		triangles.push_back(tri);
		}
		}
		}
		}
		// Materials
		verts = triShape->GetVertices();
		normals = triShape->GetNormals();
		try
		{
		texCoords = triShape->GetUVSet();
		}
		catch(...) {}
		geoMatId = 0; // TODO
		}
		else*/ // TODO
		{
			auto data = static_cast<Niflib::NiTriShapeData*>(static_cast<Niflib::NiGeometryData*>(geometry->GetData()));
			verts = data->GetVertices();
			normals = data->GetNormals();
			if(data->GetUVSetCount() > 0)
				texCoords = data->GetUVSet(0);
			triangles = data->GetTriangles();
			geoMatId = geometry->GetActiveMaterial();
		}

		auto matId = (geoMatId >= meshMaterials.size()) ? 0 : meshMaterials.at(geoMatId);
		auto subMesh = std::shared_ptr<ModelSubMesh>(nw->CreateSubMesh());
		subMesh->SetSkinTextureIndex(matId);
		auto &meshVerts = subMesh->GetVertices();
		auto &meshIndices = subMesh->GetTriangles();

		meshVerts.reserve(verts.size());
		for(auto i=decltype(verts.size()){0};i<verts.size();++i)
		{
			auto &v = verts.at(i);
			auto *uv = (i < texCoords.size()) ? &texCoords.at(i) : nullptr;
			Vertex vert {};
			vert.position = nif_vector_to_engine_vector(v) +offset;
			vert.normal = (i < normals.size()) ? nif_vector_to_engine_vector(normals.at(i)) : Vector3{};
			if(uv)
				vert.uv = {uv->u,uv->v};
			meshVerts.push_back(vert);
		}

		meshIndices.reserve(triangles.size() *3);
		for(auto &tri : triangles)
		{
			meshIndices.push_back(tri.v1);
			meshIndices.push_back(tri.v2);
			meshIndices.push_back(tri.v3);
		}

		auto mesh = std::shared_ptr<ModelMesh>(nw->CreateMesh());
		mesh->AddSubMesh(subMesh);
		auto group = mdl->AddMeshGroup(name);
		group->AddMesh(mesh);
		std::cout<<"TriStrips Name: "<<name<<std::endl;
		std::cout<<"Vertices: "<<verts.size()<<std::endl;
		std::cout<<"Triangles: "<<triangles.size()<<std::endl;
	};
	
	std::function<void(Niflib::NiNode&)> fIterateNodes = nullptr;
	fIterateNodes = [&fIterateNodes,&fAddMesh](Niflib::NiNode &node) {
		for(auto &child : node.GetChildren())
		{
			if(dynamic_cast<Niflib::NiGeometry*>(&*child))
				fAddMesh(node.GetName(),dynamic_cast<Niflib::NiGeometry*>(&*child),{});
			auto *childNode = dynamic_cast<Niflib::NiNode*>(&*child);
			if(childNode == nullptr)
				continue;
			fIterateNodes(*childNode);
		}
	};
	for(auto *node : nodes)
		fIterateNodes(*node);
	// Generate meshes
	for(auto &o : triObjects)
	{
		auto *geometry = static_cast<Niflib::NiGeometry*>(o.object);
		auto offset = nif_vector_to_engine_vector(o.object->GetLocalTranslation());
		fAddMesh(o.object->GetName(),geometry,offset);
	}

	// Update vertex weights
	//auto numBonesSk = skeleton.GetBoneCount();
	for(auto &o : triObjects)
	{
		auto &mesh = o.mesh;
		if(mesh == nullptr)
			continue;
		auto &subMesh = mesh->GetSubMeshes().front();
		auto &meshVertWeights = subMesh->GetVertexWeights();
		auto vertCount = subMesh->GetVertexCount();
		meshVertWeights.resize(vertCount,VertexWeight(Vector4i(-1,-1,-1,-1),Vector4(0.f,0.f,0.f,0.f)));
		std::vector<uint8_t> vertWeightCount(vertCount,0);
		std::vector<Niflib::Ref<Niflib::NiNode>> bones {};
		if(o.bsGeometry == true)
		{
			auto *pGeometry = dynamic_cast<Niflib::NiGeometry*>(o.object);
			if(pGeometry != nullptr)
			{
				if(pGeometry->IsSkin() == true)
					pGeometry->NormalizeSkinWeights();
				auto pSkinInstance = pGeometry->GetSkinInstance();
				if(pSkinInstance)
				{
					bones = pSkinInstance->GetBones();
					auto skinData = pSkinInstance->GetSkinData();
					if(skinData)
					{
						for(auto i=decltype(vertCount){0};i<vertCount;++i)
						{
							auto &vw = meshVertWeights.at(i);
							auto skinWeights = skinData->GetBoneWeights(i);
							for(auto i=decltype(skinWeights.size()){0u};i<umath::min(skinWeights.size(),static_cast<size_t>(4));++i)
							{
								vw.weights[i] = skinWeights.at(i).weight;
								vw.boneIds[i] = skinWeights.at(i).index;
							}
						}
					}
				}
			}
		}
		else
		{
			auto *geometry = static_cast<Niflib::NiGeometry*>(o.object);
			auto skin = geometry->GetSkinInstance();
			if(skin != nullptr)
			{
				bones = skin->GetBones();

				auto skinData = skin->GetSkinData();
				auto numBones = skinData->GetBoneCount();
				for(auto i=decltype(numBones){0};i<numBones;++i)
				{
					auto &bone = bones.at(i);
					auto name = bone->GetName();
					auto it = std::find_if(skBones.begin(),skBones.end(),[&name](const std::shared_ptr<Bone> &bone) {
						return (bone->name == name) ? true : false;
					});
					auto boneId = (*it)->ID;
					auto weights = skinData->GetBoneWeights(i);
					for(auto &weight : weights)
					{
						auto vertId = weight.index;
						auto &count = vertWeightCount.at(vertId);
						if(count >= 4)
							continue;
						auto &vw = meshVertWeights.at(vertId);
						vw.weights[count] = weight.weight;
						vw.boneIds[count] = boneId;
						++count;
					}
				}
			}
		}
	}
	
	mdl->Update(ModelUpdateFlags::All);
	return true;
}
#pragma optimize("",on)