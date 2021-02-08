#include <iostream>
#include "source_engine/vbsp/bsp_converter.hpp"
#include <pragma/asset_types/world.hpp>
#include <pragma/game/game_resources.hpp>
#include <util_image.hpp>
#include <util_texture_info.hpp>
#include <util_image_buffer.hpp>
#include <GuillotineBinPack.h>
#pragma optimize("",off)
bool pragma::asset::vbsp::BSPConverter::GenerateLightMapAtlas(LightmapData &lightmapInfo,const std::string &mapName)
{
	auto widthLightmapAtlas = lightmapInfo.atlasSize.x;
	auto heightLightmapAtlas = lightmapInfo.atlasSize.y;
	auto borderSize = lightmapInfo.borderSize;
	auto &lightMapData = lightmapInfo.luxelData;
	if(lightMapData.empty())
		return false;
	auto lightMapAtlas = uimg::ImageBuffer::Create(widthLightmapAtlas,heightLightmapAtlas,uimg::ImageBuffer::Format::RGBA16);
	auto &rects = lightmapInfo.lightmapAtlas;
	if(rects.size() != lightmapInfo.faceInfos.size())
		; // TODO: Print Warning: LIGHT MAP ATLAS TO SMALL TO ENCOMPASS ALL LIGHTMAPS
	auto rectIdx = 0u;
	for(auto lmIdx=decltype(lightmapInfo.faceInfos.size()){0u};lmIdx<lightmapInfo.faceInfos.size();++lmIdx)
	{
		auto &info = lightmapInfo.faceInfos.at(lmIdx);
		if(info.valid() == false)
			continue;
		auto &rect = rects.at(rectIdx++);
		info.x = rect.x;
		info.y = rect.y;
		if(info.lightMapSize.at(0) +borderSize *2u != rect.w)
		{
			// This shouldn't happen (The bounds should already have been put in the proper rotation before the map-file was created)
			throw std::runtime_error("Illegal light map bounds!");
			info.flags |= FaceLightMapInfo::Flags::Rotated;
		}
		info.lightMapSize.at(0) = rect.w -borderSize *2u;
		info.lightMapSize.at(1) = rect.h -borderSize *2u;
	}

	for(auto faceIndex=decltype(lightmapInfo.faceInfos.size()){0u};faceIndex<lightmapInfo.faceInfos.size();++faceIndex)
	{
		auto &lmInfo = lightmapInfo.faceInfos.at(faceIndex);
		if(lmInfo.valid() == false)
			continue;
		auto &face = lightmapInfo.faceInfos.at(faceIndex);
		if(face.luxelDataOffset == std::numeric_limits<uint32_t>::max())
			continue; // No light map available for this face (e.g. nodraw or skybox)
		auto widthLightmap = face.lightMapSize.at(0);
		auto heightLightmap = face.lightMapSize.at(1);
		auto bRotated = (lmInfo.flags &FaceLightMapInfo::Flags::Rotated) != FaceLightMapInfo::Flags::None;
		const auto fCalcPixelIndex = [&lmInfo,widthLightmapAtlas,heightLightmap,bRotated,borderSize](int x,int y,int offsetX,int offsetY) {
			x += lmInfo.x +borderSize +offsetX;
			y += lmInfo.y +borderSize +offsetY;
			return y *widthLightmapAtlas +x;
		};
		auto *lightmapData = lightMapData.data() +face.luxelDataOffset;
		for(auto y=decltype(heightLightmap){0u};y<heightLightmap;++y)
		{
			for(auto x=decltype(widthLightmap){0u};x<widthLightmap;++x)
			{
				auto xLuxel = x;
				auto yLuxel = y;
				if(bRotated)
				{
					auto tmp = yLuxel;
					yLuxel = widthLightmap -xLuxel -1;
					xLuxel = tmp;
				}
				auto &luxel = *reinterpret_cast<const bsp::ColorRGBExp32*>(lightmapData +(yLuxel *(bRotated ? heightLightmap : widthLightmap) +xLuxel) *sizeof(bsp::ColorRGBExp32));

				auto exp = umath::pow(2.0,static_cast<double>(luxel.exponent));
				auto rgbR = luxel.r *exp /255.f;
				auto rgbG = luxel.g *exp /255.f;
				auto rgbB = luxel.b *exp /255.f;

				if(lmInfo.lightMapSize.at(1) != 0)
				{
					auto pxIdx = fCalcPixelIndex(x,y,0,0);

					std::array<uint16_t,4> hdrPxCol = {
						umath::float32_to_float16_glm(rgbR),
						umath::float32_to_float16_glm(rgbG),
						umath::float32_to_float16_glm(rgbB),
						umath::float32_to_float16_glm(1.f)
					};
					lightMapAtlas->SetPixelColor(pxIdx,hdrPxCol);

					auto borderCol = hdrPxCol;
					//borderCol = bRotated ? Vector4{255.f,0.f,255.f,255.f} : Vector4{0.f,255.f,255.f,255.f};
					// Vertical border
					if(x == 0u)
					{
						for(auto i=decltype(borderSize){1u};i<=borderSize;++i)
							lightMapAtlas->SetPixelColor(fCalcPixelIndex(x,y,-i,0),borderCol);
						if(y == 0u)
						{
							// Fill out top left corner
							for(auto xOff=decltype(borderSize){1u};xOff<=borderSize;++xOff)
							{
								for(auto yOff=decltype(borderSize){1u};yOff<=borderSize;++yOff)
									lightMapAtlas->SetPixelColor(fCalcPixelIndex(x,y,-xOff,-yOff),borderCol);
							}
						}
					}
					else if(x == (widthLightmap -1u))
					{
						for(auto i=decltype(borderSize){1u};i<=borderSize;++i)
							lightMapAtlas->SetPixelColor(fCalcPixelIndex(x,y,i,0),borderCol);
						if(y == (heightLightmap -1u))
						{
							// Fill out bottom right corner
							for(auto xOff=decltype(borderSize){1u};xOff<=borderSize;++xOff)
							{
								for(auto yOff=decltype(borderSize){1u};yOff<=borderSize;++yOff)
									lightMapAtlas->SetPixelColor(fCalcPixelIndex(x,y,xOff,yOff),borderCol);
							}
						}
					}

					// Horizontal border
					if(y == 0u)
					{
						for(auto i=decltype(borderSize){1u};i<=borderSize;++i)
							lightMapAtlas->SetPixelColor(fCalcPixelIndex(x,y,0,-i),borderCol);
						if(x == (widthLightmap -1u))
						{
							// Fill out top right corner
							for(auto xOff=decltype(borderSize){1u};xOff<=borderSize;++xOff)
							{
								for(auto yOff=decltype(borderSize){1u};yOff<=borderSize;++yOff)
									lightMapAtlas->SetPixelColor(fCalcPixelIndex(x,y,xOff,-yOff),borderCol);
							}
						}
					}
					else if(y == (heightLightmap -1u))
					{
						for(auto i=decltype(borderSize){1u};i<=borderSize;++i)
							lightMapAtlas->SetPixelColor(fCalcPixelIndex(x,y,0,i),borderCol);
						if(x == 0u)
						{
							// Fill out bottom left corner
							for(auto xOff=decltype(borderSize){1u};xOff<=borderSize;++xOff)
							{
								for(auto yOff=decltype(borderSize){1u};yOff<=borderSize;++yOff)
									lightMapAtlas->SetPixelColor(fCalcPixelIndex(x,y,-xOff,yOff),borderCol);
							}
						}
					}
				}
			}
		}
	}
	m_outputWorldData->SetLightMapAtlas(*lightMapAtlas);
	return true;
}

struct SBPRect
{
	uint32_t index = 0u;
	uint32_t w = 0u;
	uint32_t h = 0u;
};

struct SBPSpace
{
	uint32_t x = 0u;
	uint32_t y = 0u;
	uint32_t w = 0u;
	uint32_t h = 0u;
};

struct SBPPacked
{
	uint32_t rectIdx = 0u;
	SBPRect packedRect = {};
};

static std::vector<rbp::Rect> simple_binpacking(std::vector<SBPRect> &rects,uint32_t &outAtlasWidth,uint32_t &outAtlasHeight)
{
	// Source: https://observablehq.com/@mourner/simple-rectangle-packing
	uint32_t area = 0u;
	uint32_t maxWidth = 0u;
	for(auto &rect : rects)
	{
		area += rect.w *rect.h;
		maxWidth = umath::max(maxWidth,rect.w);
	}

	std::sort(rects.begin(),rects.end(),[](const SBPRect &a,const SBPRect &b) {
		return (static_cast<int32_t>(b.h) -static_cast<int32_t>(a.h)) < 0;
		});

	auto startWidth = umath::max(static_cast<uint32_t>(umath::ceil(umath::sqrt(area /0.95f))),maxWidth);
	std::vector<SBPSpace> spaces {
		{0u,0u,static_cast<uint32_t>(startWidth),std::numeric_limits<uint32_t>::max()}
	};
	spaces.reserve(100);
	std::vector<rbp::Rect> packed {};
	packed.resize(rects.size());
	for(auto &rect : rects)
	{
		for(auto i=static_cast<int32_t>(spaces.size()) -1;i>=0;--i)
		{
			auto &space = spaces.at(i);
			if((rect.w > space.w || rect.h > space.h))
				continue;
			packed.at(rect.index) = {
				static_cast<int32_t>(space.x),static_cast<int32_t>(space.y),
				static_cast<int32_t>(rect.w),static_cast<int32_t>(rect.h)
			};
			if(rect.w == space.w && rect.h == space.h)
			{
				auto last = spaces.back();
				spaces.erase(spaces.end() -1);
				if(i < spaces.size())
					spaces.at(i) = last;
			}
			else if(rect.h == space.h)
			{
				space.x += rect.w;
				space.w -= rect.w;
			}
			else if(rect.w == space.w)
			{
				space.y += rect.h;
				space.h -= rect.h;
			}
			else
			{
				if(spaces.size() == spaces.capacity())
					spaces.reserve(spaces.size() *1.5f); // Increase by 50%
				auto &space = spaces.at(i); // We need a new reference, because the reserve call above may have invalidated the old one
				spaces.push_back({
					space.x +rect.w,
					space.y,
					space.w -rect.w,
					rect.h
					});
				space.y += rect.h;
				space.h -= rect.h;
			}
			break;
		}
	}

	outAtlasWidth = 0u;
	outAtlasHeight = 0u;
	for(auto &rect : packed)
	{
		outAtlasWidth = umath::max(outAtlasWidth,static_cast<uint32_t>(rect.x +rect.width));
		outAtlasHeight = umath::max(outAtlasHeight,static_cast<uint32_t>(rect.y +rect.height));
	}

	constexpr auto validateResult = false;
	if constexpr(validateResult)
	{
		// Validate (Make sure no boxes are overlapping or going out of bounds)
		std::vector<uint32_t> pixels {};
		pixels.resize(outAtlasWidth *outAtlasHeight,std::numeric_limits<uint32_t>::max());
		for(auto i=decltype(packed.size()){0u};i<packed.size();++i)
		{
			auto &rect = packed.at(i);
			if((rect.x +rect.width) > outAtlasWidth || (rect.y +rect.height) > outAtlasHeight)
				throw std::logic_error{"Lightmap resolutions out of bounds!"};
			for(auto x=rect.x;x<(rect.x +rect.width);++x)
			{
				for(auto y=rect.y;y<(rect.y +rect.height);++y)
				{
					auto pxOffset = (y *outAtlasWidth) +x;
					if(pixels.at(pxOffset) != std::numeric_limits<uint32_t>::max())
						throw std::logic_error{"Rectangle " +std::to_string(i) +" and " +std::to_string(pixels.at(pxOffset)) +" are overlapping!"};
					pixels.at(pxOffset) = i;
				}
			}
		}
	}
	return packed;
}

bool pragma::asset::vbsp::FaceLightMapInfo::valid() const {return (flags &Flags::Valid) != Flags::None;}

std::shared_ptr<pragma::asset::vbsp::LightmapData> pragma::asset::vbsp::BSPConverter::LoadLightmapData(NetworkState &nw,::bsp::File &bsp)
{
	auto &faces = bsp.GetHDRFaces().empty() ? bsp.GetFaces() : bsp.GetHDRFaces();
	auto &texInfo = bsp.GetTexInfo();
	{
		/*auto &dispInfo = bsp.GetDispInfo().front();
		auto &face = faces.at(dispInfo.MapFace);

		auto &dispLightmapSamplePositions = bsp.GetDispLightmapSamplePositions();
		struct SamplePosition
		{
		uint32_t triangleIndex;
		Vector3 barycentricCoordinates;
		};
		std::vector<SamplePosition> decompressedSamplePositions {};
		decompressedSamplePositions.reserve(dispLightmapSamplePositions.size());

		for(auto i=decltype(dispLightmapSamplePositions.size()){0u};i<dispLightmapSamplePositions.size();)
		{
		decompressedSamplePositions.push_back({});
		auto &samplePosition = decompressedSamplePositions.back();
		auto v = dispLightmapSamplePositions.at(i++);
		if(v < 255)
		samplePosition.triangleIndex = v;
		else
		samplePosition.triangleIndex = dispLightmapSamplePositions.at(i++) +255;
		auto &flBarycentric = samplePosition.barycentricCoordinates;
		flBarycentric.x = dispLightmapSamplePositions.at(i++) /255.9f;
		flBarycentric.y = dispLightmapSamplePositions.at(i++) /255.9f;
		flBarycentric.z = dispLightmapSamplePositions.at(i++) /255.9f;

		auto it = test.find(samplePosition.triangleIndex);
		if(it != test.end())
		throw std::runtime_error("!!");
		test[samplePosition.triangleIndex] = samplePosition;
		}
		for(auto i=0;i<512;++i)
		{
		auto it = test.find(i);
		if(it == test.end())
		throw std::runtime_error("@@");
		}*/
	}

	auto lightMapInfo = std::make_shared<LightmapData>();
	lightMapInfo->faceInfos.reserve(faces.size());
	lightMapInfo->luxelData = bsp.GetHDRLightMapData();
	if(lightMapInfo->luxelData.empty())
		lightMapInfo->luxelData = bsp.GetLightMapData(); // Use LDR light map data if HDR is not available
	auto borderSize = lightMapInfo->borderSize;

	// Load lightmaps
	auto &planes = bsp.GetPlanes();
	auto numFacesWithLightMapInfo = 0u;
	for(auto &face : faces)
	{
		auto faceIndex = lightMapInfo->faceInfos.size();
		//if(face.styles.front() == 0 && face.lightofs >= -1)
		{
			auto widthLightmap = face.LightmapTextureSizeInLuxels.at(0) +1u;
			auto heightLightmap = face.LightmapTextureSizeInLuxels.at(1) +1u;

			auto &plane = planes.at(face.planenum);
			auto n = plane.normal;
			if(face.side)
				n = -n;
			lightMapInfo->faceInfos.push_back({
				0,0,
				{static_cast<int32_t>(widthLightmap),static_cast<int32_t>(heightLightmap)},
				{static_cast<int32_t>(face.LightmapTextureMinsInLuxels.at(0)),static_cast<int32_t>(face.LightmapTextureMinsInLuxels.at(1))},
				FaceLightMapInfo::Flags::Valid,
				faceIndex,
				static_cast<uint32_t>(face.lightofs),
				face.texinfo,
				face.dispinfo,
				static_cast<uint32_t>(face.firstedge),
				static_cast<uint16_t>(face.numedges),
				n
				});
			++numFacesWithLightMapInfo;
		}
		//else
		//	lightMapInfo.faceInfos.push_back({0,0,{0,0},{0,0},util::bsp::FaceLightMapInfo::Flags::None,faceIndex,0u,-1,-1,0u,0u,Vector3{}});
	}

	enum class BinPackAlgorithm : uint8_t
	{
		Guillotine = 0u,
		Simple
	};
	constexpr auto algorithm = BinPackAlgorithm::Simple;
	std::vector<rbp::Rect> packedRects {};
	Vector2i atlasResolution {};
	switch(algorithm)
	{
	case BinPackAlgorithm::Guillotine:
	{
		// Attempt to find smallest texture required to fit all of the
		// lightmaps. This may take several attempts.
		const auto maxSizeLightMapAtlas = 16'384;
		auto szLightMapAtlas = 8u;
		rbp::GuillotineBinPack binPack {};
		do
		{
			szLightMapAtlas <<= 1u;
			binPack.Init(szLightMapAtlas,szLightMapAtlas);

			for(auto &info : lightMapInfo->faceInfos)
			{
				if(info.valid() == false)
					continue;
				binPack.Insert(info.lightMapSize.at(0) +borderSize *2u,info.lightMapSize.at(1) +borderSize *2u,true,rbp::GuillotineBinPack::FreeRectChoiceHeuristic::RectBestAreaFit,rbp::GuillotineBinPack::GuillotineSplitHeuristic::SplitShorterLeftoverAxis);
			}

			// Note: Passing the entire container to the Insert-call
			// like below would be more efficient, but causes
			// discrepancies between the width and height of the original
			// rect and the inserted one in some cases.
			/*
			// This vector will be cleared by the Insert-call, but we may need it for
			// another iteration if the size doesn't fit all rects,
			// so we need to make a copy.
			auto tmpLightMapRects = lightMapRects;
			binPack.Insert(tmpLightMapRects,true,rbp::GuillotineBinPack::FreeRectChoiceHeuristic::RectBestAreaFit,rbp::GuillotineBinPack::GuillotineSplitHeuristic::SplitShorterLeftoverAxis);*/
		}
		while(binPack.GetUsedRectangles().size() < numFacesWithLightMapInfo && szLightMapAtlas < maxSizeLightMapAtlas);
		packedRects = std::move(binPack.GetUsedRectangles());
		atlasResolution = {szLightMapAtlas,szLightMapAtlas};
		break;
	}
	case BinPackAlgorithm::Simple:
	{
		// Very simple and inaccurate algorithm, but much faster than the one above and
		// it's sufficient for our purposes.
		std::vector<SBPRect> rects {};
		rects.reserve(lightMapInfo->faceInfos.size());
		for(auto i=decltype(lightMapInfo->faceInfos.size()){0u};i<lightMapInfo->faceInfos.size();++i)
		{
			auto &info = lightMapInfo->faceInfos.at(i);
			if(info.valid() == false)
				continue;
			rects.push_back({});
			auto &rect = rects.back();
			rect.index = i;
			rect.w = info.lightMapSize.at(0) +borderSize *2u;
			rect.h = info.lightMapSize.at(1) +borderSize *2u;
		}
		uint32_t w,h;
		packedRects = simple_binpacking(rects,w,h);
		if((w & (w - 1)) != 0) // Check if not power of 2. TODO: Use std::ispow2 once Visual Studio has C++-20 support
			w = umath::next_power_of_2(w);
		if((h & (h - 1)) != 0)
			h = umath::next_power_of_2(h);
		atlasResolution = {w,h};
		break;
	}
	};

	lightMapInfo->atlasSize = atlasResolution;
	auto &atlasRects = lightMapInfo->lightmapAtlas;
	atlasRects.reserve(packedRects.size());
	for(auto &rect : packedRects)
		atlasRects.push_back({static_cast<uint16_t>(rect.x),static_cast<uint16_t>(rect.y),static_cast<uint16_t>(rect.width),static_cast<uint16_t>(rect.height)});

	auto rectIdx = 0u;
	for(auto lmIdx=decltype(lightMapInfo->faceInfos.size()){0u};lmIdx<lightMapInfo->faceInfos.size();++lmIdx)
	{
		auto &info = lightMapInfo->faceInfos.at(lmIdx);
		if(info.valid() == false)
			continue;
		auto &rect = packedRects.at(rectIdx++);
		info.x = rect.x;
		info.y = rect.y;
		if(info.lightMapSize.at(0) +borderSize *2u != rect.width)
			info.flags |= FaceLightMapInfo::Flags::Rotated;
		info.lightMapSize.at(0) = rect.width -borderSize *2u;
		info.lightMapSize.at(1) = rect.height -borderSize *2u;
	}
	return lightMapInfo;
}

#define BSP_ASSERT(condition,msg) \
	if(!(condition)) \
	{ \
		throw std::logic_error{#msg}; \
	}

// These are hardcoded limits in the source engine which mustn't be exceeded when we're writing luxel data to the BSP
constexpr uint32_t BSP_MAX_BRUSH_LIGHTMAP_DIM_WITHOUT_BORDER = 32;
constexpr uint32_t BSP_MAX_BRUSH_LIGHTMAP_DIM_INCLUDING_BORDER = 35;
constexpr uint32_t BSP_MAX_DISP_LIGHTMAP_DIM_WITHOUT_BORDER = 128;
constexpr uint32_t BSP_MAX_DISP_LIGHTMAP_DIM_INCLUDING_BORDER = 131;

bool pragma::asset::vbsp::BSPConverter::ConvertLightmapToBSPLuxelData(NetworkState &nw,const std::string &mapPath,const uimg::ImageBuffer &imgBuf,uint32_t atlasWidth,uint32_t atlasHeight,std::string &outErrMsg)
{
	auto fbsp = FileManager::OpenFile(mapPath.c_str(),"rb");
	if(fbsp == nullptr)
	{
		outErrMsg = "File '" +mapPath +"' not found!";
		return false;
	}
	bsp::ResultCode resultCode;
	auto bspFile = bsp::File::Open(fbsp,resultCode);
	if(bspFile == nullptr)
	{
		outErrMsg = "Unable to load BSP data from map '" +mapPath +"'!";
		return false;
	}

	// TODO: Pragma's map format does not contain all of the lightmap information we need anymore.
	// Instead, we need to read the information from the original Source Engine BSP-file.
	// TODO
	auto lightMapInfo = pragma::asset::vbsp::BSPConverter::LoadLightmapData(nw,*bspFile);
	if(lightMapInfo == nullptr)
	{
		outErrMsg = "Unable to load lightmap data from map '" +mapPath +"'!";
		return false;
	}
	bsp::ColorRGBExp32 *luxelData = nullptr;
	uint32_t luxelDataSize = 0u;
	if(lightMapInfo->luxelData.empty())
	{
		outErrMsg = "Unable to load luxel data from map '" +mapPath +"'!";
		return false;
	}
	// If the lightmap resolution has changed, we will have to make some additional changes to the BSP file (i.e. updating offsets)
	auto widthLightmapAtlas = lightMapInfo->atlasSize.x;
	auto heightLightmapAtlas = lightMapInfo->atlasSize.y;
	auto scaleFactorW = atlasWidth /static_cast<float>(widthLightmapAtlas);
	auto scaleFactorH = atlasHeight /static_cast<float>(heightLightmapAtlas);
	auto newResolution = (atlasWidth != widthLightmapAtlas || atlasHeight != heightLightmapAtlas);

	std::vector<bsp::ColorRGBExp32> newLuxels; // Only used if resolution has changed
	auto borderSize = lightMapInfo->borderSize;
	if(newResolution == false)
	{
		luxelData = reinterpret_cast<bsp::ColorRGBExp32*>(lightMapInfo->luxelData.data());
		luxelDataSize = lightMapInfo->luxelData.size();
	}
	else
	{
		uint32_t maxLuxelCount = 0;
		for(auto faceIndex=decltype(lightMapInfo->faceInfos.size()){0u};faceIndex<lightMapInfo->faceInfos.size();++faceIndex)
		{
			auto &lmInfo = lightMapInfo->faceInfos.at(faceIndex);
			if(lmInfo.valid() == false)
				continue;
			auto &face = lightMapInfo->faceInfos.at(faceIndex);
			if(face.luxelDataOffset == std::numeric_limits<uint32_t>::max())
				continue; // No light map available for this face (e.g. nodraw or skybox)
			auto offset = face.luxelDataOffset;
#ifdef TEST_SCALE_LIGHTMAP_ATLAS
			offset *= 2 *2;
#endif
			BSP_ASSERT((offset %sizeof(bsp::ColorRGBExp32)) == 0,"Unexpected offset alignment for luxel data!");
			auto index = offset /sizeof(bsp::ColorRGBExp32);
			auto widthLightmap = umath::ceil(face.lightMapSize.at(0) *scaleFactorW);
			auto heightLightmap = umath::ceil(face.lightMapSize.at(1) *scaleFactorH);
			maxLuxelCount = umath::max<uint32_t>(index +widthLightmap *heightLightmap,maxLuxelCount);
		}
		newLuxels.resize(maxLuxelCount);
		luxelData = newLuxels.data();
		luxelDataSize = newLuxels.size() *sizeof(newLuxels.front());
	}

	// Convert lightmap image data to BSP luxels
	for(auto faceIndex=decltype(lightMapInfo->faceInfos.size()){0u};faceIndex<lightMapInfo->faceInfos.size();++faceIndex)
	{
		auto &lmInfo = lightMapInfo->faceInfos.at(faceIndex);
		if(lmInfo.valid() == false)
			continue;
		auto &face = lightMapInfo->faceInfos.at(faceIndex);
		if(face.luxelDataOffset == std::numeric_limits<uint32_t>::max())
			continue; // No light map available for this face (e.g. nodraw or skybox)
		auto widthLightmap = umath::ceil(face.lightMapSize.at(0) *scaleFactorW);
		auto heightLightmap = umath::ceil(face.lightMapSize.at(1) *scaleFactorH);
		auto bRotated = (lmInfo.flags &pragma::asset::vbsp::FaceLightMapInfo::Flags::Rotated) != pragma::asset::vbsp::FaceLightMapInfo::Flags::None;
		const auto fCalcPixelIndex = [&lmInfo,atlasWidth,bRotated,borderSize,scaleFactorW,scaleFactorH](int x,int y,int offsetX,int offsetY) {
			x += lmInfo.x *scaleFactorW +borderSize +offsetX; // TODO
			y += lmInfo.y *scaleFactorH +borderSize +offsetY;
			return y *atlasWidth +x;
		};
		auto luxelDataOffset = face.luxelDataOffset;
#ifdef TEST_SCALE_LIGHTMAP_ATLAS
		luxelDataOffset *= 2 *2;
#endif
		auto *lightmapData = reinterpret_cast<bsp::ColorRGBExp32*>(reinterpret_cast<uint8_t*>(luxelData) +luxelDataOffset);
		for(auto y=decltype(heightLightmap){0u};y<heightLightmap;++y)
		{
			for(auto x=decltype(widthLightmap){0u};x<widthLightmap;++x)
			{
				auto xLuxel = x;
				auto yLuxel = y;
				if(bRotated)
				{
					auto tmp = yLuxel;
					yLuxel = widthLightmap -xLuxel -1;
					xLuxel = tmp;
				}
				auto &luxel = *(lightmapData +(yLuxel *(bRotated ? heightLightmap : widthLightmap) +xLuxel));

				if(lmInfo.lightMapSize.at(1) != 0)
				{
					auto pxIdx = fCalcPixelIndex(x,y,0,0);

					auto *pxCol = static_cast<const uint16_t*>(imgBuf.GetData()) +(pxIdx *4);

					// TODO: How to calculate luxel exponent from arbitrary pixel color data?
					auto exp = umath::pow(2.0,static_cast<double>(luxel.exponent));
					const auto fToCompressedLuxel = [exp](uint16_t pxVal) {
						auto floatVal = umath::float16_to_float32_glm(pxVal) *255.f;
						return static_cast<uint8_t>(umath::clamp<double>(static_cast<double>(floatVal) /exp,0,std::numeric_limits<uint8_t>::max()));
					};
					luxel.r = fToCompressedLuxel(pxCol[0]);
					luxel.g = fToCompressedLuxel(pxCol[1]);
					luxel.b = fToCompressedLuxel(pxCol[2]);
				}
			}
		}
	}

	// Write new luxel data back into BSP
	auto f = FileManager::OpenFile<VFilePtrReal>(mapPath.c_str(),"r+b");
	if(f == nullptr)
	{
		outErrMsg = "Unable to open map file '" +mapPath +"' for writing!";
		return false;
	}
	bsp::ResultCode code;
	auto bsp = bsp::File::Open(VFilePtr{f},code);
	//auto *pLumpInfoLDR = bsp ? bsp->GetLumpHeaderInfo(bsp::LUMP_ID_LIGHTING) : nullptr;
	auto *pLumpInfoHDR = bsp ? bsp->GetLumpHeaderInfo(bsp::LUMP_ID_LIGHTING_HDR) : nullptr;
	if(pLumpInfoHDR == nullptr)
	{
		outErrMsg = "BSP contains no HDR lighting lump!";
		return false;
	}
	if(newResolution)
	{
		std::vector<uint8_t> originalData {};
		originalData.resize(f->GetSize());
		f->Seek(0);
		f->Read(originalData.data(),f->GetSize());

		// If the lightmap atlas resolution has changed, we will need to move all data after the lightmap lump back,
		// as well as update all of the offsets in the header and the game lumps.
		auto oldLumpSize = pLumpInfoHDR->filelen;
		auto newLumpSize = luxelDataSize;
		auto deltaSize = static_cast<int32_t>(newLumpSize) -static_cast<int32_t>(oldLumpSize);

		auto lightingLumpOffset = pLumpInfoHDR->fileofs;
		auto *pGameLump = bsp ? bsp->GetLumpHeaderInfo(bsp::LUMP_ID_GAME) : nullptr;
		if(pGameLump)
		{
			// Offsets in game lump are relative to the beginning of the file instead of relative to the lump, so we have to update them
			// (exception being the BSP-files of the console version of Portal 2, but we'll ignore that here)
			auto gameLumps = bsp->GetGameLumps();
			for(auto &lump : gameLumps)
			{
				if(lump.fileofs <= lightingLumpOffset)
					continue;
				lump.fileofs += deltaSize;
			}
			f->Seek(pGameLump->fileofs +sizeof(int32_t)); // We can skip the number of lumps, since it has remained the same

														  // BSP_ASSERT(gameLumps.size() *sizeof(gameLumps.front()) == pGameLump->filelen,"Game lump size mismatch!");
			f->Write(gameLumps.data(),gameLumps.size() *sizeof(gameLumps.front()));
		}

		// Update face infos
		auto faces = bsp->GetHDRFaces();
		for(auto &face : faces)
		{
			if(face.lightofs == -1)
				continue;
			face.LightmapTextureSizeInLuxels.at(0) = (face.LightmapTextureSizeInLuxels.at(0) +1) *scaleFactorW -1;
			face.LightmapTextureSizeInLuxels.at(1) = (face.LightmapTextureSizeInLuxels.at(1) +1) *scaleFactorH -1;

			//if(face.LightmapTextureSizeInLuxels.at(0) >= BSP_MAX_BRUSH_LIGHTMAP_DIM_INCLUDING_BORDER ||
			//	face.LightmapTextureSizeInLuxels.at(1) >= BSP_MAX_BRUSH_LIGHTMAP_DIM_INCLUDING_BORDER)
			//	Con::cwar<<"WARNING!"<<Con::endl;
			BSP_ASSERT(face.LightmapTextureSizeInLuxels.at(0) < BSP_MAX_BRUSH_LIGHTMAP_DIM_INCLUDING_BORDER &&
				face.LightmapTextureSizeInLuxels.at(1) < BSP_MAX_BRUSH_LIGHTMAP_DIM_INCLUDING_BORDER,"Lightmap bounds exceed max bounds supported by source engine");
		}
		auto &hdrFaceLump = *bsp->GetLumpHeaderInfo(bsp::LUMP_ID_FACES_HDR);
		f->Seek(hdrFaceLump.fileofs);
		f->Write(faces.data(),faces.size() *sizeof(faces.front()));


		// Update header lumps
		auto headerData = bsp->GetHeaderData();
		for(auto &lumpData : headerData.lumps)
		{
			if(lumpData.fileofs <= lightingLumpOffset)
				continue;
			lumpData.fileofs += deltaSize;
		}

		f->Seek(0);
		f->Write(&headerData,sizeof(headerData));

		// Temporarily store remaining data after luxel data
		f->Seek(lightingLumpOffset +pLumpInfoHDR->filelen);
		auto offset = f->Tell();
		auto bspSize = f->GetSize();
		auto sizeRemaining = bspSize -offset;
		std::vector<uint8_t> remainingData {};
		remainingData.resize(sizeRemaining);
		f->Read(remainingData.data(),remainingData.size() *sizeof(remainingData.front()));

		// Write new luxel data
		f->Seek(lightingLumpOffset);
		f->Write(luxelData,luxelDataSize);

		// Write remaining data back to file
		f->Write(remainingData.data(),remainingData.size() *sizeof(remainingData.front()));
	}
	else
	{
		f->Seek(pLumpInfoHDR->fileofs);
		f->Write(luxelData,luxelDataSize);
	}




	//fOut->Seek(lumpInfoLDR.fileofs);
	//fOut->Write(luxelData.data(),luxelData.size() *sizeof(luxelData.front()));
	f = nullptr;
	return true;
}
#pragma optimize("",on)
