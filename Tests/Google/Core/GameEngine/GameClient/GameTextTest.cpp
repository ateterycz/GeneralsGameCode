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

#include "Common/SubsystemInterface.h"
#include "GameClient/GameText.h"

#include "../Common/FileSystemTestFixture.h"

class GameTextTest : public FileSystemTestFixture
{
};

// Mods such as Contra have a label without quoted text in their Generals.str, which the retail game reads as an empty
// text. The other labels of the file must still be found.
TEST_F(GameTextTest, ReadsStringFileWithLabelWithoutText)
{
	writeLooseFile("Data\\Generals.str",
		"GUI:Before\n\"Before\"\nEND\n\n"
		"TOOLTIP:InvalidGameVersion\n\nEND\n\n"
		"GUI:After\n\"After\"\nEND\n");

	GameTextInterface *gameText = CreateGameTextInterface();
	gameText->init();

	Bool exists = FALSE;
	EXPECT_STREQ(gameText->fetch("GUI:Before", &exists).str(), L"Before");
	EXPECT_TRUE(exists);
	EXPECT_STREQ(gameText->fetch("GUI:After", &exists).str(), L"After");
	EXPECT_TRUE(exists);
	EXPECT_STREQ(gameText->fetch("TOOLTIP:InvalidGameVersion", &exists).str(), L"");
	EXPECT_TRUE(exists);

	delete gameText;
}
