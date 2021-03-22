#include <iostream>
#include "source_engine/vbsp/bsp_converter.hpp"

pragma::asset::vbsp::BSPTree::BSPTree()
	: util::BSPTree{}
{}
void pragma::asset::vbsp::BSPTree::InitializeNode(Node &node,bsp::File &bsp,const std::array<int16_t,3u> &minSrc,const std::array<int16_t,3u> &maxSrc,uint16_t firstFaceSrc,uint16_t faceCountSrc)
{
	node.firstFace = firstFaceSrc;
	node.numFaces = faceCountSrc;
	node.min = Vector3{static_cast<float>(minSrc.at(0u)),static_cast<float>(minSrc.at(1u)),static_cast<float>(minSrc.at(2u))};
	umath::swap(node.min.y,node.min.z);
	umath::negate(node.min.z);

	node.max = Vector3{static_cast<float>(maxSrc.at(0u)),static_cast<float>(maxSrc.at(1u)),static_cast<float>(maxSrc.at(2u))};
	umath::swap(node.max.y,node.max.z);
	umath::negate(node.max.z);
}
pragma::asset::vbsp::BSPTree::Node &pragma::asset::vbsp::BSPTree::CreateLeaf(bsp::File &bsp,int32_t nodeIndex)
{
	auto &leaf = bsp.GetLeaves().at(nodeIndex);
	auto &pNode = ::util::BSPTree::CreateNode();
	pNode.leaf = true;
	pNode.cluster = leaf.cluster;
	pNode.originalNodeIndex = nodeIndex;
	InitializeNode(pNode,bsp,leaf.mins,leaf.maxs,leaf.firstleafface,leaf.numleaffaces);
	return pNode;
}

pragma::asset::vbsp::BSPTree::Node &pragma::asset::vbsp::BSPTree::CreateNode(bsp::File &bsp,int32_t nodeIndex)
{
	auto &node = bsp.GetNodes().at(nodeIndex);
	auto &pNode = ::util::BSPTree::CreateNode();
	pNode.leaf = false;
	pNode.originalNodeIndex = nodeIndex;
	InitializeNode(pNode,bsp,node.mins,node.maxs,node.firstface,node.numfaces);

	auto &plane = bsp.GetPlanes().at(node.planenum);
	auto planeNormal = plane.normal;
	umath::swap(planeNormal.y,planeNormal.z);
	umath::negate(planeNormal.z);
	pNode.plane = umath::Plane{planeNormal,plane.dist};

	auto i = 0u;
	for(auto childIdx : node.children)
	{
		if(childIdx >= 0)
		{
			auto nodeIdx = childIdx;
			pNode.children.at(i) = CreateNode(bsp,nodeIdx).index;
		}
		else
		{
			auto leafIdx = -1 -childIdx;
			pNode.children.at(i) = CreateLeaf(bsp,leafIdx).index;
		}
		++i;
	}
	return pNode;
}
std::shared_ptr<pragma::asset::vbsp::BSPTree> pragma::asset::vbsp::BSPTree::Create(bsp::File &bsp)
{
	auto tree = std::shared_ptr<pragma::asset::vbsp::BSPTree>{new pragma::asset::vbsp::BSPTree{}};
	auto &nodes = bsp.GetNodes();
	if(nodes.empty())
		return tree;
	auto &clusterVisibility = bsp.GetVisibilityData();
	if(clusterVisibility.empty())
		return tree; // Some maps don't seem to have cluster visibility information? I'm not sure what's causing it (Maybe vvis disabled during compilation?)
	tree->m_nodes.reserve(bsp.GetLeaves().size() +bsp.GetNodes().size());
	tree->m_rootNode = tree->CreateNode(bsp,0).index;

	auto numClusters = tree->m_clusterCount = clusterVisibility.size();
	auto numCompressedClusters = umath::pow2(numClusters);
	numCompressedClusters = numCompressedClusters /8u +((numCompressedClusters %8u) > 0u ? 1u : 0u);
	tree->m_clusterVisibility.resize(numCompressedClusters,0u);
	auto offset = 0ull; // Offset in bits(!)
	for(auto &cluster : clusterVisibility)
	{
		for(auto vis : cluster)
		{
			auto &visCompressed = tree->m_clusterVisibility.at(offset /8u);
			if(vis)
				visCompressed |= 1<<(offset %8u);
			++offset;
		}
	}

	// Convert BSP nodes to Pragma's coordinate system
	std::function<void(util::BSPTree::Node &node)> fConvertNodes = nullptr;
	fConvertNodes = [&fConvertNodes,&tree](util::BSPTree::Node &node) {
		node.min = pragma::asset::vbsp::BSPConverter::BSPVertexToPragma(node.min);
		node.max = pragma::asset::vbsp::BSPConverter::BSPVertexToPragma(node.max);
		if(node.leaf)
			return;
		node.plane.SetNormal(pragma::asset::vbsp::BSPConverter::BSPVertexToPragma(node.plane.GetNormal()));
		fConvertNodes(tree->m_nodes[node.children.at(0)]);
		fConvertNodes(tree->m_nodes[node.children.at(1)]);
	};
	fConvertNodes(tree->GetRootNode());
	return tree;
}
