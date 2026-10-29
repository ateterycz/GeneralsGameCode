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

#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <windows.h>

#include "WW3D2/assetmgr.h"

namespace
{

const size_t GuardSize = 64;
const char GuardByte = '\x5A';

// Returns the file name for the render object name, from a buffer of MAX_PATH characters that a guard region follows.
std::string makeFilename(const std::string &name, const char *extension, bool *guardIntact)
{
	std::vector<char> buffer(MAX_PATH + GuardSize, GuardByte);
	WW3DAssetManager::Make_Render_Obj_Filename(buffer.data(), MAX_PATH, name.c_str(), extension);

	*guardIntact = true;
	for (size_t i = MAX_PATH; i < buffer.size(); ++i)
	{
		if (buffer[i] != GuardByte)
		{
			*guardIntact = false;
		}
	}
	return std::string(buffer.data());
}

} // namespace

// The file name is the render object name up to its first '.', followed by the extension.
TEST(AssetManager, MakesRenderObjFilename)
{
	bool guardIntact = false;
	EXPECT_EQ(makeFilename("AVTank", ".w3d", &guardIntact), "AVTank.w3d");
	EXPECT_EQ(makeFilename("AVTank.Turret", ".w3d", &guardIntact), "AVTank.w3d");
	EXPECT_EQ(makeFilename("AVTank.Turret.Barrel", ".gr2", &guardIntact), "AVTank.gr2");
	EXPECT_EQ(makeFilename(".Turret", ".w3d", &guardIntact), ".w3d");
	EXPECT_EQ(makeFilename("", ".w3d", &guardIntact), ".w3d");
	EXPECT_TRUE(guardIntact);
}

// The longest name whose file name fits the buffer keeps all of its characters.
TEST(AssetManager, KeepsRenderObjNamesThatFit)
{
	const std::string name(MAX_PATH - 5, 'a');
	bool guardIntact = false;
	EXPECT_EQ(makeFilename(name, ".w3d", &guardIntact), name + ".w3d");
	EXPECT_TRUE(guardIntact);
	EXPECT_EQ(makeFilename(name + ".Turret", ".w3d", &guardIntact), name + ".w3d");
	EXPECT_TRUE(guardIntact);
}

// A longer name, which map.ini can define, is shortened so that the extension fits and nothing is written past the buffer.
TEST(AssetManager, ShortensLongerRenderObjNames)
{
	const std::string fitting(MAX_PATH - 5, 'b');
	const std::string longer = fitting + std::string(300, 'c');
	bool guardIntact = false;
	EXPECT_EQ(makeFilename(fitting + "c", ".w3d", &guardIntact), fitting + ".w3d");
	EXPECT_TRUE(guardIntact);
	EXPECT_EQ(makeFilename(longer, ".w3d", &guardIntact), fitting + ".w3d");
	EXPECT_TRUE(guardIntact);
	EXPECT_EQ(makeFilename(longer + ".Turret", ".gr2", &guardIntact), fitting + ".gr2");
	EXPECT_TRUE(guardIntact);
}
