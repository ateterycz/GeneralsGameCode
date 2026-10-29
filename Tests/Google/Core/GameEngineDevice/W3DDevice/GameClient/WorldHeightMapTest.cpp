/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <gtest/gtest.h>
#include <string>

#include "Common/GlobalData.h"
#include "Common/MapReaderWriterInfo.h"
#include "Common/NameKeyGenerator.h"
#include "Common/TerrainTypes.h"
#include "GameLogic/SidesList.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"

#include "../../../GameEngine/Common/DataChunkTestFixture.h"

// The terrain of a map comes from the map file. Its counts index fixed arrays of the height map.
class WorldHeightMapTest : public DataChunkTestFixture
{
protected:
	// The chunks that the height map reads, with their ids in the table of contents.
	enum { HEIGHT_MAP_DATA = 1, BLEND_TILE_DATA, GLOBAL_LIGHTING };

	// The texture classes, blended tiles and cliff infos of the blend tile data.
	struct BlendTiles
	{
		Int numBitmapTiles = 1;
		Int numBlendedTiles = 1;
		Int numCliffInfo = 1;
		Int numTextureClasses = 1;
		std::string textureClasses = textureClass(0, 1, 1);
		Int numEdgeTextureClasses = 0;
		std::string edgeTextureClasses;
		std::string blendedTilesAndCliffInfo;
	};

	virtual void SetUp() override
	{
		DataChunkTestFixture::SetUp();
		TheWritableGlobalData = new GlobalData;
		TheNameKeyGenerator = new NameKeyGenerator;
		TheNameKeyGenerator->init();
		TheTerrainTypes = new TerrainTypeCollection;
		TheSidesList = new SidesList;
	}

	virtual void TearDown() override
	{
		delete TheSidesList;
		TheSidesList = nullptr;
		delete TheTerrainTypes;
		TheTerrainTypes = nullptr;
		delete TheNameKeyGenerator;
		TheNameKeyGenerator = nullptr;
		delete TheWritableGlobalData;
		TheWritableGlobalData = nullptr;
		DataChunkTestFixture::TearDown();
	}

	static std::string tableOfContents()
	{
		return DataChunkTestFixture::tableOfContents({"HeightMapData", "BlendTileData", "GlobalLighting"});
	}

	// Returns a height map chunk of width by width cells, which states the given number of boundaries and has one.
	static std::string heightMap(Int width = 2, Int numBoundaries = 1)
	{
		std::string data;
		append<Int>(data, width);
		append<Int>(data, width);
		append<Int>(data, 0); // border size
		append<Int>(data, numBoundaries);
		append<Int>(data, width);
		append<Int>(data, width);
		append<Int>(data, width * width);
		data += std::string(width * width, '\0');
		return chunk(HEIGHT_MAP_DATA, data, K_HEIGHT_MAP_VERSION_4);
	}

	// Returns a texture class with the tiles of a 64 by 64 bitmap per width and height.
	static std::string textureClass(Int firstTile, Int numTiles, Int width, Bool isEdge = false)
	{
		std::string out;
		append<Int>(out, firstTile);
		append<Int>(out, numTiles);
		append<Int>(out, width);
		if (!isEdge)
		{
			append<Int>(out, 0); // legacy
		}
		appendAsciiString(out, "Texture");
		return out;
	}

	static std::string blendedTile()
	{
		std::string out;
		append<Int>(out, 0); // blend index
		out += std::string(6, '\0'); // horizontal, vertical, right diagonal, left diagonal, inverted and long diagonal
		append<Int>(out, -1); // custom blend edge class
		append<Int>(out, FLAG_VAL);
		return out;
	}

	static std::string cliffInfo()
	{
		std::string out;
		append<Int>(out, 0); // tile index
		out += std::string(8 * sizeof(Real) + 2, '\0'); // texture coordinates, flip and mutant
		return out;
	}

	// Returns a blend tile data chunk for a height map of width by width cells.
	static std::string blendTileData(const BlendTiles& tiles, Int width = 2)
	{
		std::string data;
		append<Int>(data, width * width);
		data += std::string(4 * width * width * sizeof(Short), '\0'); // tile, blend tile, extra blend tile and cliff info indices
		data += std::string(width * ((width + 7) / 8), '\0'); // cliff state
		append<Int>(data, tiles.numBitmapTiles);
		append<Int>(data, tiles.numBlendedTiles);
		append<Int>(data, tiles.numCliffInfo);
		append<Int>(data, tiles.numTextureClasses);
		data += tiles.textureClasses;
		append<Int>(data, 0); // edge tiles
		append<Int>(data, tiles.numEdgeTextureClasses);
		data += tiles.edgeTextureClasses;
		data += tiles.blendedTilesAndCliffInfo;
		return chunk(BLEND_TILE_DATA, data, K_BLEND_TILE_VERSION_8);
	}

	static std::string globalLighting(Int timeOfDay)
	{
		std::string data;
		append<Int>(data, timeOfDay);
		// 4 times of day with 3 terrain lights and 3 object lights of 9 values each.
		data += std::string(4 * 6 * 9 * sizeof(Real), '\0');
		return chunk(GLOBAL_LIGHTING, data, K_LIGHTING_VERSION_3);
	}

	// Reads the map as the terrain visual does, or only its logical data as the terrain logic does.
	void load(const std::string& chunks, Bool logicalDataOnly = false)
	{
		open(tableOfContents() + chunks);
		WorldHeightMap *map = new WorldHeightMap(&m_stream, logicalDataOnly);
		map->Release_Ref();
	}
};

TEST_F(WorldHeightMapTest, LoadsValidMap)
{
	open(tableOfContents() + heightMap() + blendTileData(BlendTiles()) + globalLighting(TIME_OF_DAY_NIGHT));
	WorldHeightMap *map = new WorldHeightMap(&m_stream);
	EXPECT_EQ(map->getXExtent(), 2);
	EXPECT_EQ(map->getYExtent(), 2);
	ASSERT_EQ(map->getAllBoundaries().size(), 1u);
	EXPECT_EQ(map->getAllBoundaries()[0].x, 2);
	EXPECT_EQ(TheGlobalData->m_timeOfDay, TIME_OF_DAY_NIGHT);
	map->Release_Ref();
}

TEST_F(WorldHeightMapTest, LoadsValidLogicalData)
{
	open(tableOfContents() + heightMap());
	WorldHeightMap *map = new WorldHeightMap(&m_stream, true);
	EXPECT_EQ(map->getXExtent(), 2);
	ASSERT_EQ(map->getAllBoundaries().size(), 1u);
	map->Release_Ref();
}

TEST_F(WorldHeightMapTest, ThrowsOnNegativeBoundaryCount)
{
	expectCorruptFileFormat([&] { load(heightMap(2, -1) + blendTileData(BlendTiles())); });
	expectCorruptFileFormat([&] { load(heightMap(2, -1), true); });
}

TEST_F(WorldHeightMapTest, ThrowsOnMoreBoundariesThanChunkHasBeforeAllocatingThem)
{
	// Allocating the boundaries of this count fails in a 32 bit process.
	expectCorruptFileFormat([&] { load(heightMap(2, 0x10000000) + blendTileData(BlendTiles())); });
	expectCorruptFileFormat([&] { load(heightMap(2, 0x10000000), true); });
}
