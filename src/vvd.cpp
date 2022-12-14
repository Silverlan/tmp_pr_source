#include "mdl.h"
#include "vvd.h"
#include "mdl_optimize.h"

bool import::mdl::load_vvd(const VFilePtr &f,std::vector<umath::Vertex> &verts,std::vector<umath::VertexWeight> &vertWeights,std::vector<std::vector<uint32_t>> &fixedLodVertexIndices)
{
	auto offset = f->Tell();
	auto header = f->Read<vvd::vertexFileHeader_t>();

	f->Seek(offset +header.fixupTableStart);
	auto fixup = f->Read<vvd::vertexFileFixup_t>();

	f->Seek(offset +header.vertexDataStart);

	if(header.numLODs > 0)
	{
		auto numVerts = header.numLODVertexes.front();
		verts.reserve(numVerts);
		vertWeights.reserve(numVerts);
		for(auto i=decltype(numVerts){0};i<numVerts;++i)
		{
			auto stdVert = f->Read<vvd::mstudiovertex_t>();
			auto n = stdVert.m_vecNormal;
			auto l = uvec::length(n);
			if(l < 0.1) // Some models have normals with a length of zero (e.g. "models/custom/train_bumper.mdl"), which is illegal.
				n = uvec::FORWARD;
			else
				n /= l;
			verts.push_back(umath::Vertex{stdVert.m_vecPosition,stdVert.m_vecTexCoord,n});
			if(uvec::length(stdVert.m_vecNormal) < 0.8)
				std::cout<<"";
			vertWeights.push_back(umath::VertexWeight{{-1,-1,-1,-1},{}});
			auto &weight = vertWeights.back();
			auto weightSum = 0.f;
			for(auto j=decltype(stdVert.m_BoneWeights.numbones){0};j<umath::min(stdVert.m_BoneWeights.numbones,static_cast<byte>(4));++j)
			{
				weight.boneIds[j] = stdVert.m_BoneWeights.bone[j];
				weight.weights[j] = stdVert.m_BoneWeights.weight[j];

				weightSum += stdVert.m_BoneWeights.weight[j];
			}
		}
	}

	// Read fixups
	if(header.numFixups > 0)
	{
		f->Seek(header.fixupTableStart);
		std::vector<vvd::vertexFileFixup_t> fixups(header.numFixups);
		f->Read(fixups.data(),fixups.size() *sizeof(fixups.front()));
		
		f->Seek(header.vertexDataStart);
		fixedLodVertexIndices.reserve(header.numLODs);
		for(auto lod=decltype(header.numLODs){0};lod<header.numLODs;++lod)
		{
			fixedLodVertexIndices.push_back({});
			auto &lodIndices = fixedLodVertexIndices.back();
			for(auto &fixup : fixups)
			{
				if(fixup.lod >= lod)
				{
					lodIndices.reserve(lodIndices.size() +fixup.numVertexes);
					for(auto i=decltype(fixup.numVertexes){0};i<fixup.numVertexes;++i)
						lodIndices.push_back(fixup.sourceVertexID +i);
				}
			}
		}
	}
	return true;
}
