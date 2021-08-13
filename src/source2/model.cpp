#include "mdl.h"
#include "wv_source.hpp"
#include "source2/source2.hpp"
#include "source2/pr_impl.hpp"
#include <util_source2.hpp>
#include <source2/resource.hpp>
#include <source2/resource_data.hpp>
#include <source2/resource_edit_info.hpp>
#include <fsys/filesystem.h>
#include <util_archive.hpp>
#include <sharedutils/util_string.h>
#include <sharedutils/util_path.hpp>
#include <pragma/model/modelmesh.h>
#include <pragma/model/animation/vertex_animation.hpp>
#include <pragma/model/model.h>
#include <pragma/model/modelmesh.h>
#include <pragma/physics/collisionmesh.h>
#include <pragma/networkstate/networkstate.h>
#include <pragma/game/game_resources.hpp>
#include <pragma/game/game.h>
#include <pragma/asset_types/world.hpp>
#include <mathutil/vertex.hpp>
#include <util_image_buffer.hpp>
#include <util_image.hpp>
#include <unordered_set>

static uint32_t add_material(NetworkState &nw,Model &mdl,const std::string &mat,std::optional<uint32_t> skinId={})
{
	::util::Path path{mat};
	path.RemoveFileExtension();
	path += ".vmat_c";

	std::string inputPath = path.GetString();
	auto front = path.GetFront();
	path.PopFront();

	auto outputPath = path;
	if(ustring::compare(front,"materials",false) == false)
		outputPath = "models/" +outputPath;

	// Materials in Source 2 can be in arbitrary locations (relative to the game), not necessarily
	// in the materials directory. For this reason we'll have to port the material immediately and then
	// change its path for Pragma.
	::util::port_file(&nw,inputPath,"materials/" +outputPath.GetString());

	auto strPath = outputPath.GetPath();
	auto strFile = outputPath.GetFileName();
	mdl.AddTexturePath(strPath);

	if(skinId.has_value())
	{
		auto *texGroup = mdl.GetTextureGroup(*skinId);
		if(texGroup == nullptr)
			texGroup = mdl.CreateTextureGroup();
		return import::util::add_texture(nw,mdl,strFile,texGroup,true);
	}
	auto *texGroup = mdl.GetTextureGroup(0);
	if(texGroup)
	{
		auto n = texGroup->textures.size();
		auto idx = import::util::add_texture(nw,mdl,strFile,texGroup);
		if(texGroup->textures.size() > n) // Texture was added, so we'll have to add it to the other texture groups as well
		{
			auto &texGroups = mdl.GetTextureGroups();
			for(auto i=decltype(texGroups.size()){1u};i<texGroups.size();++i)
				import::util::add_texture(nw,mdl,strFile,&texGroups.at(i),true);
		}
		return idx;
	}
	return 0;
}

static std::vector<std::shared_ptr<ModelSubMesh>> generate_split_meshes(NetworkState &nw,const source2::impl::MeshData &meshData,const std::vector<uint64_t> &indices)
{
	std::vector<std::shared_ptr<ModelSubMesh>> meshes {};
	auto subMesh = std::shared_ptr<ModelSubMesh>{nw.CreateSubMesh()};
	auto &verts = subMesh->GetVertices();
	auto &tris = subMesh->GetTriangles();
	auto &vertWeights = subMesh->GetVertexWeights();

	verts = meshData.verts;
	vertWeights = meshData.vertWeights;

	if(meshData.lightmapUvs.empty() == false)
		subMesh->AddUVSet("lightmap") = meshData.lightmapUvs;;
	
	tris.reserve(indices.size());
	for(auto idx : indices)
	{
		if(idx > std::numeric_limits<uint16_t>::max())
			idx = std::numeric_limits<uint16_t>::max();
		tris.push_back(idx);
	}
	meshes.push_back(subMesh);
	return meshes;
#if 0
	constexpr auto MAX_VERTEX_COUNT_PER_MESH = std::numeric_limits<uint16_t>::max();
	// Pragma doesn't support a vertex count > MAX_VERTEX_COUNT_PER_MESH,
	// so we'll have to split the mesh
	struct SplitMesh
	{
		std::vector<Vertex> verts {};
		std::vector<VertexWeight> vertWeights {};
		std::vector<Vector2> lightmapUvs {};
	};
	std::vector<SplitMesh> splitMeshes {};

	auto hasVertexWeights = (meshData.vertWeights.empty() == false);
	auto hasLightmapUvs = (meshData.lightmapUvs.empty() == false);

	for(auto i=decltype(indices.size()){0u};i<indices.size();i+=3)
	{
		auto idx0 = indices.at(i);
		auto idx1 = indices.at(i +1);
		auto idx2 = indices.at(i +2);

	}

	auto numVerts = meshData.verts.size();
	auto &prVerts = meshData.verts;
	auto &vertWeights = meshData.vertWeights;
	auto &lightmapUvs = meshData.lightmapUvs;
	auto numSplitMeshes = numVerts /std::numeric_limits<uint16_t>::max() +1;
	splitMeshes.reserve(numSplitMeshes);

	for(auto i=decltype(numVerts){0u};i<numVerts;++i)
	{
		auto meshIdx = i /MAX_VERTEX_COUNT_PER_MESH;
		if(meshIdx >= splitMeshes.size())
		{
			splitMeshes.push_back({});
			auto &splitMesh = splitMeshes.back();
			splitMesh.verts.reserve(MAX_VERTEX_COUNT_PER_MESH);
			if(hasVertexWeights)
				splitMesh.vertWeights.reserve(MAX_VERTEX_COUNT_PER_MESH);
			if(hasLightmapUvs)
				splitMesh.lightmapUvs.reserve(MAX_VERTEX_COUNT_PER_MESH);
		}
		auto &splitMesh = splitMeshes.back();
		splitMesh.verts.push_back(prVerts.at(i));
		if(hasVertexWeights)
			splitMesh.vertWeights.push_back(vertWeights.at(i));

		if(hasLightmapUvs)
			splitMesh.lightmapUvs.push_back(lightmapUvs.at(i));
	}

	std::vector<std::shared_ptr<ModelSubMesh>> meshes {};
	meshes.reserve(splitMeshes.size());
	for(auto &splitMesh : splitMeshes)
	{
		auto mesh = std::shared_ptr<ModelSubMesh>{nw.CreateSubMesh()};
		meshes.push_back(mesh);

		mesh->GetVertices() = splitMesh.verts;
		if(hasVertexWeights)
			mesh->GetVertexWeights() = splitMesh.vertWeights;

		if(hasLightmapUvs)
		{
			auto &uvSet = mesh->AddUVSet("lightmap");
			uvSet = splitMesh.lightmapUvs;
		}
	}
#endif
	return meshes;
}

struct MeshVertexInfo
{
	uint32_t meshGroupId = 0;
	uint32_t meshId = 0;
	uint32_t subMeshId = 0;
	uint32_t vertexId = 0;
};
using BundleVertexData = std::unordered_map<uint32_t,Vector4>;
using BundleData = std::unordered_map<uint32_t,BundleVertexData>;
using MorphData = std::unordered_map<std::string,BundleData>;

static Vector4 vertex_attr_value_to_vec4(const source2::resource::VBIB::VertexBuffer &vbuf,const source2::resource::VBIB::VertexAttribute &attr,uint32_t offset)
{
	std::vector<float> data;
	vbuf.ReadVertexAttribute(offset,attr,data);
	Vector4 result {};
	for(uint8_t i=0;i<data.size();++i)
		result[i] = data.at(i);
	return result;
}

static uint32_t morphDstWidth;
static uint32_t morphDstHeight;
static MorphData prMorphData {};
static std::vector<Vector4> transformedData {};
static std::shared_ptr<uimg::ImageBuffer> imgBuf = nullptr;
static void initialize_scene_objects(
	NetworkState &nw,Model &mdl,uint32_t meshGroupIdx,source2::resource::VBIB &vbib,source2::resource::IKeyValueCollection &data,
	source2::resource::Skeleton *optSkeleton=nullptr,std::optional<int64_t> meshIdx={},
	std::vector<uint64_t> *optVbibVertexBufferOffsets=nullptr,std::vector<std::vector<MeshVertexInfo>> *optVbibVertexIndexToPragmaMeshId=nullptr
)
{
	auto &meshGroup = *mdl.GetMeshGroup(meshGroupIdx);
	auto &vbufs = vbib.GetVertexBuffers();
	auto &ibufs = vbib.GetIndexBuffers();

	auto sceneObjects = data.FindArrayValues<source2::resource::IKeyValueCollection*>("m_sceneObjects");
	for(auto *sceneObject : sceneObjects)
	{
		auto name = sceneObject->FindValue<std::string>("m_name");
		if(name.has_value())
			std::cout<<"Loading mesh '"<<*name<<"'..."<<std::endl;
		auto prMesh = std::shared_ptr<ModelMesh>{nw.CreateMesh()};
		auto prMeshIdx = meshGroup.GetMeshes().size();
		meshGroup.AddMesh(prMesh);
		auto drawCalls = sceneObject->FindArrayValues<source2::resource::IKeyValueCollection*>("m_drawCalls");
		uint64_t globalVertexOffset = 0;
		for(auto *drawCall : drawCalls)
		{
			//auto prSubMesh = std::shared_ptr<ModelSubMesh>{nw.CreateSubMesh()};
			//prMesh->AddSubMesh(prSubMesh);
			//prSubMesh->SetSkinTextureIndex(0);
			auto vertexCount = drawCall->FindValue<int64_t>("m_nVertexCount");
			auto baseVertex = drawCall->FindValue<int64_t>("m_nBaseVertex");
			auto primitiveType = drawCall->FindValue<std::string>("m_nPrimitiveType","");
			auto startIndex = drawCall->FindValue<int64_t>("m_nStartIndex",0);
			auto indexCount = drawCall->FindValue<int64_t>("m_nIndexCount",0);
			auto uvDensity = drawCall->FindValue<double>("m_flUvDensity");
			auto startInstance = drawCall->FindValue<int64_t>("m_nStartInstance");
			auto instanceCount = drawCall->FindValue<int64_t>("m_nInstanceCount");
			auto tintColor = drawCall->FindValue<Vector4>("m_vTintColor");
			auto cullDataIndex = drawCall->FindValue<int64_t>("m_CullDataIndex");
			auto hasBakedLightingFromLightMap = drawCall->FindValue<bool>("m_bHasBakedLightingFromLightMap");
			auto material = drawCall->FindValue<std::string>("m_material");
			util::ScopeGuard sg {[&globalVertexOffset,drawCall,vertexCount]() {globalVertexOffset += *vertexCount;}};

			if(primitiveType != "RENDER_PRIM_TRIANGLES")
			{
				Con::cwar<<"WARNING: Unsupported primitive type '"<<primitiveType<<"'! Skipping mesh..."<<Con::endl;
				continue;
			}

			uint32_t skinTexIdx = 0;
			if(material.has_value())
				skinTexIdx = add_material(nw,mdl,*material);

			source2::impl::MeshData meshData {};
			auto vertexBuffers = drawCall->FindArrayValues<source2::resource::IKeyValueCollection*>("m_vertexBuffers");
			uint32_t vertexBufferVbibIndex = std::numeric_limits<uint32_t>::max();
			for(auto *vbuf : vertexBuffers)
			{
				auto bindOffsetBytes = vbuf->FindValue<int64_t>("m_nBindOffsetBytes");
				auto bufferIndex = vbuf->FindValue<int64_t>("m_hBuffer");
				if(bufferIndex.has_value() && *bufferIndex < vbufs.size())
				{
					vertexBufferVbibIndex = *bufferIndex;
					auto &vbuf = vbufs.at(*bufferIndex);
					// TODO: bindOffsetBytes?
					meshData = std::move(source2::impl::initialize_vertices(vbuf,optSkeleton,meshIdx));
				}
				break; // TODO: How to handle multiple vertex buffers?
			}

			auto *indexBuffer = drawCall->FindSubCollection("m_indexBuffer");
			if(indexBuffer)
			{
				auto bindOffsetBytes = indexBuffer->FindValue<int64_t>("m_nBindOffsetBytes");
				auto bufferIndex = indexBuffer->FindValue<int64_t>("m_hBuffer");
				if(bufferIndex.has_value() && *bufferIndex < ibufs.size())
				{
					auto &ibuf = ibufs.at(*bufferIndex);
					auto indexElementSize = ibuf.size;

					// Note: Pragma only supports indices in a range that fit into a uint16, but Souce 2
					// also supports uint32, so we have to use a container with a larger type to store
					// the indices before assigning them to the mesh
					std::vector<uint64_t> tris {};
					tris.reserve(indexCount);
					if(indexElementSize == 2)
					{
						for(auto i=startIndex;i<(startIndex +indexCount);++i)
						{
							auto idx = *(reinterpret_cast<const uint16_t*>(ibuf.buffer.data()) +i);
							tris.push_back(idx);
						}
					}
					else if(indexElementSize == 4)
					{
						for(auto i=startIndex;i<(startIndex +indexCount);++i)
						{
							auto idx = *(reinterpret_cast<const uint32_t*>(ibuf.buffer.data()) +i);
							tris.push_back(idx);
						}
					}
					else
						Con::cwar<<"WARNING: Unsupported index buffer element size of "<<indexElementSize<<" bytes!"<<Con::endl;

					// We don't need all vertices, so we'll just remove the unused ones
					std::vector<bool> verticesUsed {};
					auto &verts = meshData.verts;
					auto &vertWeights = meshData.vertWeights;
					auto &lightmapUvs = meshData.lightmapUvs;
					auto numVerts = verts.size();
					verticesUsed.resize(numVerts,false);
					for(auto idx : tris)
						verticesUsed.at(idx) = true;

					if(numVerts > 0u)
					{
						// We have to iterate backwards, otherwise erasing one of the vertices
						// would invalidate the indices
						for(int64_t i=(numVerts -1);i>=0;--i)
						{
							if(verticesUsed.at(i))
								continue;
							verts.erase(meshData.verts.begin() +i);
							if(i < vertWeights.size())
								vertWeights.erase(vertWeights.begin() +i);
							if(i < lightmapUvs.size())
								lightmapUvs.erase(lightmapUvs.begin() +i);
						}
					}

					std::vector<uint64_t> indexTranslationTableToPragma {};
					indexTranslationTableToPragma.resize(numVerts);
					int64_t indexOffset = 0;
					for(auto i=decltype(numVerts){0u};i<numVerts;++i)
					{
						if(verticesUsed.at(i) == false)
							--indexOffset;
						indexTranslationTableToPragma.at(i) = i +indexOffset;
					}

					for(auto &idx : tris)
						idx = indexTranslationTableToPragma[idx];

					auto meshes = generate_split_meshes(nw,meshData,tris);
					if(optVbibVertexIndexToPragmaMeshId && optVbibVertexIndexToPragmaMeshId->empty() == false)
					{
						auto &subMesh = meshes.front();
						auto subMeshIdx = prMesh->GetSubMeshes().size();
						for(auto idx=decltype(subMesh->GetVertices().size()){0u};idx<subMesh->GetVertices().size();++idx)
						{
							if(idx == std::numeric_limits<uint64_t>::max())
								continue;
							optVbibVertexIndexToPragmaMeshId->at(optVbibVertexBufferOffsets->at(vertexBufferVbibIndex) +idx).push_back({
								meshGroupIdx,
								static_cast<uint32_t>(prMeshIdx),
								static_cast<uint32_t>(subMeshIdx),
								static_cast<uint32_t>(idx)
							});
						}
					}
					for(auto &subMesh : meshes)
					{
						prMesh->AddSubMesh(subMesh);
						subMesh->SetSkinTextureIndex(skinTexIdx);
					}

					if(meshGroupIdx == 0)
					{
						auto &subMesh = meshes.front();
						for(auto &va : mdl.GetVertexAnimations())
						{
							auto name = va->GetName();
							name = name.substr(5);
							auto it = prMorphData.find(name);
							if(it == prMorphData.end())
								continue;
							auto *flex = mdl.GetFlex(name);
							auto &flexData = it->second;
							//auto actualVertCount = subMesh->GetVertexCount();
							uint32_t bundleIndex = 0;
							auto &bundleData = flexData.at(bundleIndex);

							auto numFlexVerts = 0;
							for(uint64_t vertId=0;vertId<*vertexCount;++vertId)
							{
								if(verticesUsed.at(vertId) == false)
									continue;
								auto it = bundleData.find(globalVertexOffset +vertId);
								if(it == bundleData.end())
									continue;
								++numFlexVerts;
							}

							if(numFlexVerts > 0)
							{
								//if(name == "jawDrop") {
								auto meshFrame = va->AddMeshFrame(*prMesh,*subMesh);
								meshFrame->SetVertexCount(*vertexCount);
								for(uint64_t vertId=0;vertId<*vertexCount;++vertId)
								{
									if(verticesUsed.at(vertId) == false)
										continue;
									auto it = bundleData.find(globalVertexOffset +vertId);
									if(it == bundleData.end())
										continue;
									auto offset = it->second;

									auto pragmaVertId = indexTranslationTableToPragma[vertId];
									auto &v = subMesh->GetVertices().at(pragmaVertId);
									//v.position += Vector3{offset.x,offset.y,offset.z};
									meshFrame->SetVertexPosition(pragmaVertId,Vector3{offset.x,offset.z,-offset.y});

								}
								//std::cout<<"NUM FLEX VERTS: "<<numFlexVerts<<std::endl;
								if(flex)
								{
									if(flex->GetVertexAnimation())
										;//std::cout<<"FLEX ALREADY HAS VA: "<<name<<std::endl;
									flex->SetVertexAnimation(*va);
								}
							//}
							}
						}

					}

#if 0
					auto &meshTris = prSubMesh->GetTriangles();
					meshTris.reserve(tris.size());
					auto hasIndicesThatExceedSupportedRange = false;
					for(auto idx : tris)
					{
						if(idx > std::numeric_limits<uint16_t>::max())
						{
							hasIndicesThatExceedSupportedRange = true;
							idx = std::numeric_limits<uint16_t>::max();
						}
						prSubMesh->GetTriangles().push_back(idx);
					}
					if(hasIndicesThatExceedSupportedRange)
						Con::cwar<<"WARNING: Mesh vertex count exceeds maximum supported number of vertices per mesh! Mesh may be rendered incorrectly..."<<Con::endl;
#endif
				}
			}
		}
#if 0
		for(auto &mp : prMorphData)
		{
			for(auto &fd : mp.second)
			{
				for(auto &bd : fd.second)
				{
					if(bd.first >= globalVertexOffset)
						std::cout<<"Index "<<bd.first<<" exceeds "<<globalVertexOffset<<"!"<<std::endl;
				}
			}
		}
#endif
	}
}

static std::shared_ptr<source2::resource::Resource> load_resource(NetworkState &nw,const std::string &fileName)
{
	auto f = FileManager::OpenFile(fileName.c_str(),"rb");
	if(f == nullptr)
		f = uarch::load(fileName);
	if(f == nullptr)
		return nullptr;
	return source2::load_resource(f,[&nw](const std::string &path) -> VFilePtr {
		if(::util::port_file(&nw,path) == false)
			return nullptr;
		return FileManager::OpenFile(path.c_str(),"rb");
	});
}

static std::unordered_map<std::string,Flex::Operation::Type> g_flexOpTable {
	{"FLEX_OP_FETCH1",Flex::Operation::Type::Fetch},
	{"FLEX_OP_CONST",Flex::Operation::Type::Const},
	{"FLEX_OP_MAX",Flex::Operation::Type::Max},
	{"FLEX_OP_MIN",Flex::Operation::Type::Min},
	{"FLEX_OP_SUB",Flex::Operation::Type::Sub},
	{"FLEX_OP_DIV",Flex::Operation::Type::Div},
	{"FLEX_OP_MUL",Flex::Operation::Type::Mul},
	{"FLEX_OP_NWAY",Flex::Operation::Type::NWay},

	// These are unconfirmed, the names may be incorrect
	{"FLEX_OP_FETCH2",Flex::Operation::Type::Fetch2},
	{"FLEX_OP_ADD",Flex::Operation::Type::Add},
	{"FLEX_OP_CLOSE",Flex::Operation::Type::Close},
	{"FLEX_OP_COMMA",Flex::Operation::Type::Comma},
	{"FLEX_OP_TWOWAY0",Flex::Operation::Type::TwoWay0},
	{"FLEX_OP_TWOWAY1",Flex::Operation::Type::TwoWay1},
	{"FLEX_OP_COMBO",Flex::Operation::Type::Combo},
	{"FLEX_OP_DOMINATE",Flex::Operation::Type::Dominate}
};

enum class S2FlexOp : uint32_t
{
	Const = 1,
	Fetch1 = 2,
	Fetch2 = 3,
	Add = 4,
	Sub = 5,
	Mul = 6,
	Div = 7,
	Neg = 8,
	Exp = 9,
	Open = 10,
	Close = 11,
	Comma = 12,
	Max = 13,
	Min = 14,
	e2Way0 = 15,
	e2Way1 = 16,
	NWay = 17,
	Combo = 18,
	Dominate = 19,
	DMELowerEyelid = 20,
	DMEUpperEyelid = 21
};

// TODO: These are taken from Source 1, I have no idea if they actually match the values from Source 2
std::unordered_map<S2FlexOp,Flex::Operation::Type> g_s2FlexToPragma = {
	{S2FlexOp::Const,Flex::Operation::Type::Const},
	{S2FlexOp::Fetch1,Flex::Operation::Type::Fetch},
	{S2FlexOp::Fetch2,Flex::Operation::Type::Fetch2},
	{S2FlexOp::Add,Flex::Operation::Type::Add},
	{S2FlexOp::Sub,Flex::Operation::Type::Sub},
	{S2FlexOp::Mul,Flex::Operation::Type::Mul},
	{S2FlexOp::Div,Flex::Operation::Type::Div},
	{S2FlexOp::Neg,Flex::Operation::Type::Neg},
	{S2FlexOp::Exp,Flex::Operation::Type::Exp},
	{S2FlexOp::Open,Flex::Operation::Type::Open},
	{S2FlexOp::Close,Flex::Operation::Type::Close},
	{S2FlexOp::Comma,Flex::Operation::Type::Comma},
	{S2FlexOp::Max,Flex::Operation::Type::Max},
	{S2FlexOp::Min,Flex::Operation::Type::Min},
	{S2FlexOp::e2Way0,Flex::Operation::Type::TwoWay0},
	{S2FlexOp::e2Way1,Flex::Operation::Type::TwoWay1},
	{S2FlexOp::NWay,Flex::Operation::Type::NWay},
	{S2FlexOp::Combo,Flex::Operation::Type::Combo},
	{S2FlexOp::Dominate,Flex::Operation::Type::Dominate},
	{S2FlexOp::DMELowerEyelid,Flex::Operation::Type::DMELowerEyelid},
	{S2FlexOp::DMEUpperEyelid,Flex::Operation::Type::DMEUpperEyelid}
};

static void load_morph_targets(NetworkState &nw,source2::resource::IKeyValueCollection &morphData,Model &mdl,std::unordered_map<uint32_t,uint32_t> &s2FlexDescToPragma)
{
	auto textureAtlasPath = morphData.FindValue<std::string>("m_pTextureAtlas");
	auto texAtlasResource = textureAtlasPath.has_value() ? ::load_resource(nw,*textureAtlasPath +"_c") : nullptr;
	auto *texAtlas = texAtlasResource ? dynamic_cast<source2::resource::Texture*>(texAtlasResource->FindBlock(source2::BlockType::DATA)) : nullptr;
	if(texAtlas == nullptr)
	{
		if(textureAtlasPath.has_value())
			Con::cwar<<"WARNING: Unable to load texture atlas '"<<*textureAtlasPath<<"'!"<<Con::endl;
	}
	else
	{
		assert(texAtlas->GetFormat() == source2::VTexFormat::RGBA8888);
		if(texAtlas->GetFormat() != source2::VTexFormat::RGBA8888)
			throw std::logic_error{"Unexpected format for FACS morph atlas: '" +std::to_string(umath::to_integral(texAtlas->GetFormat())) +"'!"};
		auto atlasWidth = texAtlas->GetWidth();
		auto atlasHeight = texAtlas->GetHeight();
		//uint32_t maxX = 0;
		//uint32_t maxY = 0;
		std::vector<uint8_t> atlasData {};
		texAtlas->ReadTextureData(0,atlasData);

		auto wRectDst = morphData.FindValue<int64_t>("m_nWidth",0);
		auto hRectDst = morphData.FindValue<int64_t>("m_nHeight",0);

		morphDstWidth = wRectDst;
		morphDstHeight = hRectDst;


		imgBuf = uimg::ImageBuffer::Create(atlasData.data(),atlasWidth,atlasHeight,uimg::Format::RGBA8);
#if 0
		auto imgBuf = uimg::ImageBuffer::Create(width,height,uimg::Format::RGBA8);
		imgBuf->Clear(Color::White.ToVector4());
		std::vector<bool> dstImageData;
		dstImageData.resize(width *height,false);
		for(auto *morphData : morphData.FindArrayValues<source2::resource::IKeyValueCollection*>("m_morphDatas"))
		{
			for(auto *morphRectData : morphData.FindArrayValues<source2::resource::IKeyValueCollection*>("m_morphRectDatas"))
			{
				auto xLeftDst = morphRectData->FindValue<int64_t>("m_nXLeftDst",0);
				auto yTopDst = morphRectData->FindValue<int64_t>("m_nYTopDst",0);
				auto idx = yTopDst *width +xLeftDst;
				dstImageData.at(idx) = true;
			}
		}
#endif

		auto fGetPixelData = [&atlasData,atlasWidth,atlasHeight](int32_t x,int32_t y) -> uint32_t {
			auto pxOffset = y *atlasWidth +x;
			return *(reinterpret_cast<uint32_t*>(atlasData.data()) +pxOffset);
		};

		auto lookupType = morphData.FindValue<std::string>("m_nLookupType");
		auto bundleTypes = morphData.FindArrayValues<std::string>("m_bundleTypes");

		struct PositionSpeed
		{
			Vector3 position;
			float speed;
		};
		std::vector<std::optional<PositionSpeed>> positionSpeedData {};
		std::unordered_map<std::string,std::vector<size_t>> morphVertexIndices {};

		uint32_t morphIdx = 0;
		//auto col0 = Color::Red;
		//auto col1 = Color::Lime;
		//std::vector<float> transformedData {};

		for(auto *morphData : morphData.FindArrayValues<source2::resource::IKeyValueCollection*>("m_morphDatas"))
		{
			util::ScopeGuard sg {[&morphIdx]() {++morphIdx;}};
			//auto col = col0.Lerp(col1,morphIdx++ /235.f);

			auto name = morphData->FindValue<std::string>("m_name","");
			morphVertexIndices[name] = {};
			auto &prBundleData = prMorphData[name];
			std::vector<std::shared_ptr<uimg::ImageBuffer>> rects {};

			auto dstImage = uimg::ImageBuffer::Create(morphDstWidth,morphDstHeight,uimg::Format::RGBA32);
			dstImage->Clear(Vector4{0.f,0.f,0.f,0.f});
			for(auto *morphRectData : morphData->FindArrayValues<source2::resource::IKeyValueCollection*>("m_morphRectDatas"))
			{
				auto xRectDst = morphRectData->FindValue<int64_t>("m_nXLeftDst",0);
				auto yRectDst = morphRectData->FindValue<int64_t>("m_nYTopDst",0);

#if 0
				auto numAvailable = 1;
				auto idxTest = yTopDst *width +xLeftDst;
				++idxTest;
				while(idxTest < dstImageData.size() && dstImageData.at(idxTest) == false)
				{
					++numAvailable;
					++idxTest;
				}
#endif

				//imgBuf->SetPixelColor(xLeftDst,yTopDst,col.ToVector4());
				//maxX = umath::max(maxX,static_cast<uint32_t>(xLeftDst));
				//maxY = umath::max(maxY,static_cast<uint32_t>(yTopDst));

				auto uWidthSrc = morphRectData->FindValue<double>("m_flUWidthSrc",0.0);
				auto uHeightSrc = morphRectData->FindValue<double>("m_flVHeightSrc",0.0);

				auto wRectSrc = umath::round(uWidthSrc *atlasWidth);
				auto hRectSrc = umath::round(uHeightSrc *atlasHeight);

				std::vector<Vector3> offsetsPerVertex {};
				std::vector<uint64_t> vertexIndices {};
				uint32_t bundleIdx = 0;
				for(auto *bundleData : morphRectData->FindArrayValues<source2::resource::IKeyValueCollection*>("m_bundleDatas"))
				{
					//auto &bundleVertexData = prBundleData[bundleIdx];
					auto &bundleType = bundleTypes.at(bundleIdx++);
					auto uLeftSrc = bundleData->FindValue<double>("m_flULeftSrc",0.0);
					auto vTopSrc = bundleData->FindValue<double>("m_flVTopSrc",0.0);
					auto offsets = bundleData->FindArrayValues<double>("m_offsets");
					auto ranges = bundleData->FindArrayValues<double>("m_ranges");
					assert(offsets.size() == 4 && ranges.size() == 4);

					auto xRectSrc = umath::round(uLeftSrc *atlasWidth);
					auto yRectSrc = umath::round(vTopSrc *atlasHeight);

					//auto atlasRect = uimg::ImageBuffer::Create(*imgBuf,yRectSrc,xRectSrc,hRectSrc,wRectSrc);

					std::vector<std::array<uint8_t,4>> atlasRectData {};
					atlasRectData.reserve(wRectSrc *hRectSrc);
					for(uint32_t x=0;x<wRectSrc;++x)
					{
						for(uint32_t y=0;y<hRectSrc;++y)
						{
							auto pxView = imgBuf->GetPixelView(xRectSrc +x,yRectSrc +y);
							atlasRectData.push_back({
								pxView.GetLDRValue(uimg::Channel::Red),
								pxView.GetLDRValue(uimg::Channel::Green),
								pxView.GetLDRValue(uimg::Channel::Blue),
								pxView.GetLDRValue(uimg::Channel::Alpha)
							});
						}
					}

					auto *rawData = reinterpret_cast<uint8_t*>(atlasRectData.data());//static_cast<uint8_t*>(atlasRect->GetData());
					auto rawSize = atlasRectData.size() *sizeof(uint8_t) *4;//atlasRect->GetSize();

					std::vector<Vector4> transformedData {};
					transformedData.resize(rawSize /4);
					for(auto i=decltype(rawSize){0u};i<rawSize;i+=4)
					{
						for(uint8_t j=0;j<4;++j)
							transformedData.at(i /4)[j] = (rawData[i +j] /static_cast<float>(std::numeric_limits<uint8_t>::max())) *ranges.at(j) +offsets.at(j);
					}

					auto startVertexIndex = yRectDst *wRectDst +xRectDst;
					if(bundleType == "MORPH_BUNDLE_TYPE_POSITION_SPEED")
					{
						uint32_t i = 0;
						auto dst_y = yRectDst;
						auto dst_x = xRectDst;
						auto rect_width = wRectSrc;
						auto rect_height = hRectSrc;
						for(uint32_t x=dst_x;x<(dst_x +rect_width);++x)
						{
							for(uint32_t y=dst_y;y<(dst_y +rect_height);++y)
							{
								auto &v = transformedData[i];
								//bundleVertexData[x *rect_height +(y -dst_y)] = v;
								dstImage->SetPixelColor(x,y,v);
								++i;
							}
						}
					}
				}
			}

			auto &bd = prBundleData[0];
			auto *data = dstImage->GetData();
			std::vector<Vector4> vData {};
			vData.resize(morphDstWidth *morphDstHeight);
			memcpy(vData.data(),data,vData.size() *sizeof(vData.front()));
			for(uint32_t i=0;i<vData.size();++i)
				bd[i] = vData[i];
		}
	}

	uint32_t s2FlexIdx = 0;
	for(auto *flexDesc : morphData.FindArrayValues<source2::resource::IKeyValueCollection*>("m_FlexDesc"))
	{
		auto szFacs = flexDesc->FindValue<std::string>("m_szFacs");
		if(szFacs.has_value())
		{
			auto flexIdx = mdl.GetFlexCount();
			auto &flex = mdl.AddFlex(*szFacs);
			s2FlexDescToPragma[s2FlexIdx] = flexIdx;
			auto vertAnim = mdl.AddVertexAnimation("flex_" +*szFacs);

		}
		++s2FlexIdx;
	}
}

std::shared_ptr<Model> source2::convert::convert_model(
	Game &game,source2::resource::Model &s2Mdl,source2::resource::Resource *optResource
)
{
	auto &nw = *game.GetNetworkState();
	auto ptrMdl = game.CreateModel();
	auto &mdl = *ptrMdl;
	auto skins = s2Mdl.GetSkins();
	for(auto iSkin=decltype(skins.size()){0u};iSkin<skins.size();++iSkin)
	{
		auto &skin = skins.at(iSkin);
		auto &name = skin.GetName();
		// TODO: Pragma has no support for named skins (yet)

		auto &materials = skin.GetMaterials();
		for(auto &mat : materials)
			add_material(nw,mdl,mat,iSkin);
	}

#if 0
	auto matGroups = s2Mdl.GetData()->FindArrayValues<source2::resource::IKeyValueCollection>("m_materialGroups");
	for(auto &matGroup : matGroups)
	{
		auto name = matGroup.FindValue<std::string>("m_name");
		for(auto &mat : matGroup.FindArrayValues<source2::resource::IKeyValueCollection>("m_materials"))
		{
			std::cout<<"";
		}
	}
#endif

	auto fLookupMeshGroup = [](Model &mdl,const std::string &name) -> uint32_t {
		auto &meshGroups = mdl.GetMeshGroups();
		auto it = std::find_if(meshGroups.begin(),meshGroups.end(),[&name](const std::shared_ptr<ModelMeshGroup> &meshGroup) {
			return meshGroup->GetName() == name;
		});
		return it -meshGroups.begin();
	};

	auto &meshGroup = *mdl.AddMeshGroup("reference");
	auto refMeshGroupIdx = fLookupMeshGroup(mdl,meshGroup.GetName());
	mdl.GetBaseMeshes() = {0u};

	auto s2Skeleton = s2Mdl.GetSkeleton();

	auto &skeleton = mdl.GetSkeleton();
	skeleton.GetBones().clear();
	skeleton.GetRootBones().clear();

	if(s2Skeleton)
	{
		auto reference = pragma::animation::Animation::Create();
		mdl.AddAnimation("reference",reference); // Reference always has to be the first animation!

		auto &s2Bones = s2Skeleton->GetBoneList();
		auto numBones = s2Bones.size();
		auto refFrame = Frame::Create(numBones);
		reference->AddFrame(refFrame);

		reference->ReserveBoneIds(numBones);
		for(auto i=decltype(numBones){0u};i<numBones;++i)
			reference->AddBoneId(i);

		auto refPose = Frame::Create(s2Bones.size());
		std::unordered_map<source2::resource::Bone*,Bone*> s2BoneToPragma {};
		std::unordered_map<source2::resource::Bone*,source2::resource::Bone*> s2BoneToS2Parent {};
		for(auto i=decltype(s2Bones.size()){0u};i<s2Bones.size();++i)
		{
			auto &s2Bone = s2Bones.at(i);
			auto *bone = new Bone{};
			bone->name = s2Bone->GetName();
			auto boneId = skeleton.AddBone(bone);

			refPose->SetBonePosition(i,s2Bone->GetPosition());
			refPose->SetBoneOrientation(i,s2Bone->GetRotation());
			refFrame->SetBonePosition(i,s2Bone->GetPosition());
			refFrame->SetBoneOrientation(i,s2Bone->GetRotation());
			s2BoneToPragma[s2Bone.get()] = bone;

			for(auto &child : s2Bone->GetChildren())
				s2BoneToS2Parent[child.get()] = s2Bone.get();
		}
		for(auto &s2RootBone : s2Skeleton->GetRootBones())
		{
			auto *bone = s2BoneToPragma[s2RootBone.get()];
			skeleton.GetRootBones()[bone->ID] = bone->shared_from_this();
		}
		for(auto &s2Bone : s2Skeleton->GetBoneList())
		{
			auto itParent = s2BoneToS2Parent.find(s2Bone.get());
			if(itParent == s2BoneToS2Parent.end())
				continue;
			auto *s2Parent = itParent->second;
			auto *bone = s2BoneToPragma[s2Bone.get()];
			auto *parent = s2BoneToPragma[s2Parent];
			bone->parent = parent->shared_from_this();
			parent->children[bone->ID] = bone->shared_from_this();
		}
		mdl.SetReference(refPose);
	}

	// TODO: Add reference meshes

	// TODO: Grab materials

	// We'll have to rotate the vertices to convert them to Pragma's coordinate system
	//auto rotS2ToPragma = uquat::create(EulerAngles{-90.f,-90.f,0.f});

	auto defaultMeshGroupMask = s2Mdl.GetData()->FindValue<int64_t>("m_nDefaultMeshGroupMask",0);
	// TODO: defaultMeshGroupMask can be -1
	// -> props_vehicles/car_hatchback_a01_glass.wmd
	auto meshGroups = s2Mdl.GetData()->FindArrayValues<std::string>("m_meshGroups");
	auto refLodGroupMasks = s2Mdl.GetData()->FindArrayValues<int64_t>("m_refLODGroupMasks");

	std::vector<uint32_t> lodMeshGroups {};
	lodMeshGroups.push_back(refMeshGroupIdx);

	std::vector<uint64_t> vbibVertexBufferOffsets {};

	std::vector<std::vector<MeshVertexInfo>> vbibVertexIndexToPragmaMeshId {};

	std::unordered_map<uint32_t,uint32_t> s2FlexDescToPragma {};
	std::vector<std::shared_ptr<source2::resource::IKeyValueCollection>> morphBlocks {};

	auto *morph = optResource ? dynamic_cast<source2::resource::KeyValuesOrNTRO*>(optResource->FindBlock(source2::BlockType::MRPH)) : nullptr;
	auto morphData = morph ? morph->GetData() : nullptr;
	if(morphData)
	{
		load_morph_targets(nw,*morphData,mdl,s2FlexDescToPragma);
		morphBlocks.push_back(morphData);
	}

	auto s2Meshes = s2Mdl.GetEmbeddedMeshes();
	for(auto &s2Mesh : s2Meshes)
	{
		auto meshIdx = s2Mesh->GetMeshIndex();

		auto useForMorphs = true;
#if 0
		auto useForMorphs = (meshIdx == 0); // ???
		if(useForMorphs)
		{
			auto vbib = s2Mesh->GetVBIB();
			if(vbib)
			{
				auto &vbufs = vbib->GetVertexBuffers();
				vbibVertexBufferOffsets.reserve(vbufs.size());
				uint64_t offset = 0;
				for(auto &vbuf : vbufs)
				{
					vbibVertexBufferOffsets.push_back(offset);
					offset += vbuf.count;
				}
				vbibVertexIndexToPragmaMeshId.resize(offset);
			}
		}
#endif

		auto lodMask = (meshIdx < refLodGroupMasks.size()) ? refLodGroupMasks.at(meshIdx) : 0;
		// Determine lowest lod that this mesh belongs to
		uint32_t lodIndex = std::numeric_limits<uint32_t>::max();
		for(uint32_t i=0;i<32;++i) // Maximum number is arbitrary, but 32 LODs should never be reached (?)
		{
			if(lodMask &(1<<i))
			{
				lodIndex = i;
				break;
			}
		}
		if(lodIndex == std::numeric_limits<uint32_t>::max())
			lodIndex = 0; // Unknown LOD, default to 0

		if(lodIndex >= lodMeshGroups.size())
			lodMeshGroups.resize(lodIndex +1,std::numeric_limits<uint32_t>::max());
		auto &lodGroupId = lodMeshGroups.at(lodIndex);
		if(lodGroupId == std::numeric_limits<uint32_t>::max())
		{
			auto lodGroup = ModelMeshGroup::Create("lod" +std::to_string(lodIndex));
			lodGroupId = fLookupMeshGroup(mdl,lodGroup->GetName());
			mdl.AddMeshGroup(lodGroup);
		}
		auto *data = s2Mesh->GetResourceData()->GetData();
		if(data)
			initialize_scene_objects(nw,mdl,lodGroupId,*s2Mesh->GetVBIB(),*data,s2Skeleton.get(),meshIdx,useForMorphs ? &vbibVertexBufferOffsets : nullptr,useForMorphs ? &vbibVertexIndexToPragmaMeshId : nullptr);
	}

	auto refMeshGroupMasks = s2Mdl.GetData()->FindArrayValues<int64_t>("m_refMeshGroupMasks");
	uint32_t meshIdx = 0;
	for(auto &meshName : s2Mdl.GetReferencedMeshNames())
	{
		util::ScopeGuard sg {[&meshIdx]() {++meshIdx;}};
		//auto lodMask = (refMeshIdx < refLodGroupMasks.size()) ? refLodGroupMasks.at(refMeshIdx) : 0;
		//if(!(lodMask &1)) // TODO: Load ALL lod meshes!
		//	continue;
		auto fileName = meshName +"_c";
		auto resource = ::load_resource(nw,fileName);
		if(resource == nullptr)
		{
			Con::cwar<<"WARNING: Unable to load referenced mesh '"<<meshName<<"'!"<<Con::endl;
			continue;
		}
		auto *vbib = dynamic_cast<source2::resource::VBIB*>(resource->FindBlock(source2::BlockType::VBIB));
		auto *dataBlock = dynamic_cast<source2::resource::BinaryKV3*>(resource->FindBlock(source2::BlockType::DATA));
		if(vbib == nullptr || dataBlock == nullptr)
			continue;
		auto morphSet = dataBlock->GetData()->FindValue<std::string>("m_morphSet");
		if(morphSet.has_value())
		{
			auto fileName = *morphSet +"_c";
			auto resource = ::load_resource(nw,fileName);
			if(resource != nullptr)
			{
				auto *morph = resource ? dynamic_cast<source2::resource::KeyValuesOrNTRO*>(resource->FindBlock(source2::BlockType::MRPH)) : nullptr;
				auto *data = dynamic_cast<source2::resource::ResourceData*>(resource->FindBlock(source2::BlockType::DATA));
				if(data)
				{
					load_morph_targets(nw,*data->GetData(),mdl,s2FlexDescToPragma);
					morphBlocks.push_back(data->GetData()->shared_from_this());
				}
			}
		}

		initialize_scene_objects(nw,mdl,refMeshGroupIdx,*vbib,*dataBlock->GetData(),s2Skeleton.get(),meshIdx,&vbibVertexBufferOffsets,&vbibVertexIndexToPragmaMeshId);
	}

	std::vector<std::shared_ptr<source2::resource::Animation>> animations {};
	if(optResource)
		animations = s2Mdl.GetEmbeddedAnimations(*optResource);
	for(auto &refAnimGroup : s2Mdl.GetData()->FindArrayValues<std::string>("m_refAnimGroups"))
	{
		auto fileName = refAnimGroup +"_c";
		auto resource = ::load_resource(nw,fileName);
		if(resource == nullptr)
		{
			Con::cwar<<"WARNING: Unable to load referenced animation group '"<<refAnimGroup<<"'!"<<Con::endl;
			continue;
		}
		auto *dataBlock = dynamic_cast<source2::resource::ResourceData*>(resource->FindBlock(source2::BlockType::DATA));
		auto data = dataBlock ? dataBlock->GetData() : nullptr;
		if(data == nullptr)
			continue;
		auto animArray = data->FindArrayValues<std::string>("m_localHAnimArray");
		auto *decodeKey = data->FindSubCollection("m_decodeKey");
		if(decodeKey == nullptr)
			continue;
		for(auto &animFile : animArray)
		{
			auto fileName = animFile +"_c";
			auto resource = ::load_resource(nw,fileName);
			if(resource == nullptr)
			{
				Con::cwar<<"WARNING: Unable to load referenced animation '"<<animFile<<"'!"<<Con::endl;
				continue;
			}
			auto *dataBlock = dynamic_cast<source2::resource::ResourceData*>(resource->FindBlock(source2::BlockType::DATA));
			auto data = dataBlock ? dataBlock->GetData() : nullptr;
			if(data == nullptr)
				continue;
			auto anims = source2::resource::Animation::CreateAnimations(*data,*decodeKey);
			animations.reserve(animations.size() +anims.size());
			for(auto &anim : anims)
				animations.push_back(anim);
		}
	}

	for(auto &s2Anim : animations)
	{
		auto anim = pragma::animation::Animation::Create();
		anim->SetFPS(s2Anim->GetFPS());

		// Find all of the bones used by this animation
		std::unordered_map<std::string,uint32_t> boneNameToAnimBoneListIndex {};
		for(auto &s2Frame : s2Anim->GetFrames())
		{
			for(auto &pair : s2Frame->GetBones())
			{
				auto it = boneNameToAnimBoneListIndex.find(pair.first);
				if(it != boneNameToAnimBoneListIndex.end())
					continue;
				auto boneId = skeleton.LookupBone(pair.first);
				if(boneId == -1)
					continue;
				auto localIdx = anim->AddBoneId(boneId);
				boneNameToAnimBoneListIndex.insert(std::make_pair(pair.first,localIdx));
			}
		}
		//

		auto numBonesAnim = anim->GetBoneList().size();
		for(auto &s2Frame : s2Anim->GetFrames())
		{
			auto frame = Frame::Create(numBonesAnim);
			for(auto &pair : s2Frame->GetBones())
			{
				auto it = boneNameToAnimBoneListIndex.find(pair.first);
				if(it == boneNameToAnimBoneListIndex.end())
					continue;
				auto boneListIdx = it->second;

				auto &s2FrameBone = pair.second;
				umath::Transform pose {s2FrameBone.position,s2FrameBone.rotation};
				frame->SetBonePose(boneListIdx,pose);
			}
			anim->AddFrame(frame);
		}

		mdl.AddAnimation(s2Anim->GetName(),anim);
	}

	for(auto &morphData : morphBlocks)
	{
		auto textureAtlasPath = morphData->FindValue<std::string>("m_pTextureAtlas");
		auto texAtlasResource = textureAtlasPath.has_value() ? ::load_resource(nw,*textureAtlasPath +"_c") : nullptr;
		auto *texAtlas = texAtlasResource ? dynamic_cast<source2::resource::Texture*>(texAtlasResource->FindBlock(source2::BlockType::DATA)) : nullptr;
		if(texAtlas == nullptr)
		{
			if(textureAtlasPath.has_value())
				Con::cwar<<"WARNING: Unable to load texture atlas '"<<*textureAtlasPath<<"'!"<<Con::endl;
		}
		else
		{
#if 0
			assert(texAtlas->GetFormat() == source2::VTexFormat::RGBA8888);
			if(texAtlas->GetFormat() != source2::VTexFormat::RGBA8888)
				throw std::logic_error{"Unexpected format for FACS morph atlas: '" +std::to_string(umath::to_integral(texAtlas->GetFormat())) +"'!"};
			auto atlasWidth = texAtlas->GetWidth();
			auto atlasHeight = texAtlas->GetHeight();
			//uint32_t maxX = 0;
			//uint32_t maxY = 0;
			std::vector<uint8_t> atlasData {};
			texAtlas->ReadTextureData(0,atlasData);

			auto width = morphData->FindValue<int64_t>("m_nWidth",0);
			auto height = morphData->FindValue<int64_t>("m_nHeight",0);

			auto imgBuf = uimg::ImageBuffer::Create(atlasData.data(),atlasWidth,atlasHeight,uimg::Format::RGBA8);
#if 0
			auto imgBuf = uimg::ImageBuffer::Create(width,height,uimg::Format::RGBA8);
			imgBuf->Clear(Color::White.ToVector4());
			std::vector<bool> dstImageData;
			dstImageData.resize(width *height,false);
			for(auto *morphData : morphData->FindArrayValues<source2::resource::IKeyValueCollection*>("m_morphDatas"))
			{
				for(auto *morphRectData : morphData->FindArrayValues<source2::resource::IKeyValueCollection*>("m_morphRectDatas"))
				{
					auto xLeftDst = morphRectData->FindValue<int64_t>("m_nXLeftDst",0);
					auto yTopDst = morphRectData->FindValue<int64_t>("m_nYTopDst",0);
					auto idx = yTopDst *width +xLeftDst;
					dstImageData.at(idx) = true;
				}
			}
#endif

			auto fGetPixelData = [&atlasData,atlasWidth,atlasHeight](int32_t x,int32_t y) -> uint32_t {
				auto pxOffset = y *atlasWidth +x;
				return *(reinterpret_cast<uint32_t*>(atlasData.data()) +pxOffset);
			};

			auto lookupType = morphData->FindValue<std::string>("m_nLookupType");
			auto bundleTypes = morphData->FindArrayValues<std::string>("m_bundleTypes");

			struct PositionSpeed
			{
				Vector3 position;
				float speed;
			};
			std::vector<std::optional<PositionSpeed>> positionSpeedData {};
			std::unordered_map<std::string,std::vector<size_t>> morphVertexIndices {};

			uint32_t morphIdx = 0;
			//auto col0 = Color::Red;
			//auto col1 = Color::Lime;
			//std::vector<float> transformedData {};

			for(auto *morphData : morphData->FindArrayValues<source2::resource::IKeyValueCollection*>("m_morphDatas"))
			{
				ScopeGuard sg {[&morphIdx]() {++morphIdx;}};
				//auto col = col0.Lerp(col1,morphIdx++ /235.f);

				auto name = morphData->FindValue<std::string>("m_name","");
				morphVertexIndices[name] = {};
				auto &prBundleData = prMorphData[name];
				std::vector<std::shared_ptr<uimg::ImageBuffer>> rects {};
				for(auto *morphRectData : morphData->FindArrayValues<source2::resource::IKeyValueCollection*>("m_morphRectDatas"))
				{
					auto xLeftDst = morphRectData->FindValue<int64_t>("m_nXLeftDst",0);
					auto yTopDst = morphRectData->FindValue<int64_t>("m_nYTopDst",0);

#if 0
					auto numAvailable = 1;
					auto idxTest = yTopDst *width +xLeftDst;
					++idxTest;
					while(idxTest < dstImageData.size() && dstImageData.at(idxTest) == false)
					{
						++numAvailable;
						++idxTest;
					}
#endif

					//imgBuf->SetPixelColor(xLeftDst,yTopDst,col.ToVector4());
					//maxX = umath::max(maxX,static_cast<uint32_t>(xLeftDst));
					//maxY = umath::max(maxY,static_cast<uint32_t>(yTopDst));

					auto uWidthSrc = morphRectData->FindValue<double>("m_flUWidthSrc",0.0);
					auto uHeightSrc = morphRectData->FindValue<double>("m_flVHeightSrc",0.0);

					auto wRect = umath::round(uWidthSrc *atlasWidth);
					auto hRect = umath::round(uHeightSrc *atlasHeight);

					std::vector<Vector3> offsetsPerVertex {};
					std::vector<uint64_t> vertexIndices {};
					uint32_t bundleIdx = 0;
					for(auto *bundleData : morphRectData->FindArrayValues<source2::resource::IKeyValueCollection*>("m_bundleDatas"))
					{
						auto &bundleVertexData = prBundleData[bundleIdx];
						auto &bundleType = bundleTypes.at(bundleIdx++);
						auto uLeftSrc = bundleData->FindValue<double>("m_flULeftSrc",0.0);
						auto vTopSrc = bundleData->FindValue<double>("m_flVTopSrc",0.0);
						auto offsets = bundleData->FindArrayValues<double>("m_offsets");
						auto ranges = bundleData->FindArrayValues<double>("m_ranges");
						assert(offsets.size() == 4 && ranges.size() == 4);

						auto xRect = umath::round(uLeftSrc *atlasWidth);
						auto yRect = umath::round(vTopSrc *atlasHeight);

						auto atlasRect = uimg::ImageBuffer::Create(*imgBuf,xRect,yRect,wRect,hRect);
						auto *rawData = static_cast<uint8_t*>(atlasRect->GetData());
						auto rawSize = atlasRect->GetSize();

						std::vector<Vector4> transformedData {};
						transformedData.resize(atlasRect->GetSize() /4);
						for(auto i=decltype(rawSize){0u};i<rawSize;i+=4)
						{
							for(uint8_t j=0;j<4;++j)
								transformedData.at(i /4)[j] = (rawData[i] /static_cast<float>(std::numeric_limits<uint8_t>::max())) *ranges.at(j) +offsets.at(j);
						}

						auto startVertexIndex = yTopDst *width +xLeftDst;
						if(bundleType == "MORPH_BUNDLE_TYPE_POSITION_SPEED")
						{
							uint32_t i = 0;
							for(auto y=decltype(yTopDst){0u};y<(yTopDst +height);++y)
							{
								auto &yData = bundleVertexData[y];
								for(auto x=decltype(xLeftDst){0u};x<(xLeftDst +width);++x)
								{
									yData[x] = transformedData.at(i);
									++i;
								}
							}

							auto *psData = reinterpret_cast<PositionSpeed*>(transformedData.data());
							auto numEls = transformedData.size() *sizeof(transformedData.front()) /sizeof(PositionSpeed);
							morphVertexIndices[name].reserve(numEls);
							for(auto i=decltype(numEls){0u};i<numEls;++i)
							{
								auto &ps = psData[i];
								auto vertIdx = startVertexIndex +i;
								if(vertIdx >= positionSpeedData.size())
								{
									positionSpeedData.reserve(positionSpeedData.size() *1.5 +1'000);
									positionSpeedData.resize(vertIdx +1);
								}
								positionSpeedData.at(vertIdx) = ps;
								morphVertexIndices[name].push_back(vertIdx);
								//if(vertIdx >= indicesTest.size())
								//{
								//	indicesTest.reserve(indicesTest.size() +1000);
								////	indicesTest.resize(vertIdx +1);
								//}
								//if(indicesTest.at(vertIdx))
								//	std::cout<<"WARNING: VERTEX ALREADY DEFINED FOR THIS MORPH!"<<std::endl;
								//indicesTest.at(vertIdx) = true;
								//std::cout<<"Vertex id: "<<vertIdx<<std::endl;
							}
						}
					}
				}
				//std::cout<<"Done: "<<indicesTest.size()<<std::endl;
			}
#endif
#if 0
			{
			auto f = FileManager::OpenFile<VFilePtrReal>("morph_image_atlas.png","wb");
			if(f)
			uimg::save_image(f,*imgBuf,uimg::ImageFormat::PNG);
			}
#endif


			for(auto *flexController : morphData->FindArrayValues<source2::resource::IKeyValueCollection*>("m_FlexControllers"))
			{
				auto szName = flexController->FindValue<std::string>("m_szName");
				if(szName.has_value() == false)
					continue;
				auto szType = flexController->FindValue<std::string>("m_szType");
				auto min = flexController->FindValue<double>("min",0.0);
				auto max = flexController->FindValue<double>("max",0.0);
				auto &fc = mdl.AddFlexController(*szName);
				fc.min = min;
				fc.max = max;
			}

			for(auto *flexRule : morphData->FindArrayValues<source2::resource::IKeyValueCollection*>("m_FlexRules"))
			{
				auto nFlex = flexRule->FindValue<int64_t>("m_nFlex",-1);
				auto it = s2FlexDescToPragma.find(nFlex);
				if(it == s2FlexDescToPragma.end())
					continue;
				auto &flex = *mdl.GetFlex(nFlex);
				auto &ops = flex.GetOperations();
				if(ops.empty() == false)
					continue; // Flex already has operations? For some reason Source 2 models have multiple sets of flex rules per flex
				for(auto *flexOp : flexRule->FindArrayValues<source2::resource::IKeyValueCollection*>("m_FlexOps"))
				{
					auto opCode = flexOp->FindValue<std::string>("m_OpCode","");
					auto data = flexOp->FindValue<int32_t>("m_Data",0);

					std::optional<Flex::Operation::Type> opType {};
					if(::ustring::is_integer(opCode))
					{
						auto it = g_s2FlexToPragma.find(static_cast<S2FlexOp>(ustring::to_int(opCode)));
						if(it != g_s2FlexToPragma.end())
							opType = it->second;
					}
					else
					{
						auto it = g_flexOpTable.find(opCode);
						if(it != g_flexOpTable.end())
							opType = it->second;
					}
					if(opType.has_value() == false)
						Con::cwar<<"WARNING: Unsupported flex operator code '"<<opCode<<"'! Skipping..."<<Con::endl;
					else
					{
						ops.push_back({});
						auto &op = ops.back();
						op.d.index = data;
						op.type = *opType;
					}
				}
			}
		}
	}

	// Convert coordinate system to Pragma
	for(auto &meshGroup : mdl.GetMeshGroups())
	{
		for(auto &mesh : meshGroup->GetMeshes())
		{
			for(auto &subMesh : mesh->GetSubMeshes())
			{
				for(auto &v : subMesh->GetVertices())
				{
					v.position = source2::impl::convert_source2_vector_to_pragma(v.position);
					v.normal = source2::impl::convert_source2_vector_to_pragma(v.normal);
				}
			}
		}
	}
	for(auto &colMesh : mdl.GetCollisionMeshes())
	{
		for(auto &v : colMesh->GetVertices())
			v = source2::impl::convert_source2_vector_to_pragma(v);
	}
	//Vector3 source2::impl::convert_source2_vector_to_pragma(const Vector3 &v)
	auto fTranslateBonePoseToPragma = [](umath::Transform &pose) {
		auto &pos = pose.GetOrigin();
		auto &rot = pose.GetRotation();
		// These were determined through testing.
		// I'm unsure why the translation looks like this.
		pos = {pos.y,pos.x,-pos.z};
		rot = {-rot.w,-rot.y,-rot.x,rot.z};
	};
	for(auto &anim : mdl.GetAnimations())
	{
		for(auto &frame : anim->GetFrames())
		{
			for(auto &t : frame->GetBoneTransforms())
				fTranslateBonePoseToPragma(t);
		}
	}
	auto &refPose = mdl.GetReference();
	for(auto &t : refPose.GetBoneTransforms())
		fTranslateBonePoseToPragma(t);

	// Reference pose bones are in parent space, we need them in global space
	std::function<void(Bone&,const umath::Transform&)> fRelativeToGlobal = nullptr;
	fRelativeToGlobal = [&fRelativeToGlobal,&refPose](Bone &bone,const umath::Transform &parentPose) {
		auto *pPose = refPose.GetBoneTransform(bone.ID);
		auto pose = pPose ? *pPose : umath::Transform{};
		pose = parentPose *pose;

		refPose.SetBonePose(bone.ID,pose);
		for(auto &pair : bone.children)
			fRelativeToGlobal(*pair.second,pose);
	};
	umath::Transform rootBoneTransform {Vector3{},uquat::create(EulerAngles{90.f,90.f,0.f})};
	for(auto &rootBone : skeleton.GetRootBones())
		fRelativeToGlobal(*rootBone.second,rootBoneTransform);
	for(auto &anim : mdl.GetAnimations())
	{
		auto &boneList = anim->GetBoneList();
		for(auto &frame : anim->GetFrames())
		{
			for(auto i=decltype(boneList.size()){0u};i<boneList.size();++i)
			{
				auto boneId = boneList.at(i);
				if(skeleton.IsRootBone(boneId) == false)
					continue;
				// Root bones need to be rotated, unsure why
				auto &pose = *frame->GetBoneTransform(i);
				pose = rootBoneTransform *pose;
			}
		}
	}

	// Finalize model
	mdl.Update(ModelUpdateFlags::All);
	import::util::port_model_texture_assets(nw,mdl);
	return ptrMdl;
}
