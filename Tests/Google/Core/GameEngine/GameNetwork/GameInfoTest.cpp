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

#include <climits>

#include <gtest/gtest.h>

#include "Common/INI.h"
#include "GameNetwork/GameInfo.h"

TEST(GameInfo, ClampsCRCIntervalToAtLeastOneFrame)
{
	EXPECT_EQ(GameInfo::clampCRCInterval(INT_MIN), 1);
	EXPECT_EQ(GameInfo::clampCRCInterval(-1), 1);
	EXPECT_EQ(GameInfo::clampCRCInterval(0), 1);
}

TEST(GameInfo, KeepsCRCIntervalOfAtMost100Frames)
{
	EXPECT_EQ(GameInfo::clampCRCInterval(1), 1);
	EXPECT_EQ(GameInfo::clampCRCInterval(50), 50);
	EXPECT_EQ(GameInfo::clampCRCInterval(100), 100);
	EXPECT_EQ(GameInfo::clampCRCInterval(101), 100);
	EXPECT_EQ(GameInfo::clampCRCInterval(INT_MAX), 100);
}
