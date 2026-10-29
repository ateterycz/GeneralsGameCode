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

#include "GameNetwork/GameSpy/MainMenuUtils.h"

TEST(MainMenuUtils, CopiesAllTextOfSuccessfulRequest)
{
	// The response of an HTTP request is not terminated.
	const char response[] = { 'M', 'O', 'T', 'D' };
	char *text = NewHTTPTextBuffer(TRUE, response, sizeof(response));
	ASSERT_NE(text, nullptr);
	EXPECT_STREQ(text, "MOTD");
	delete[] text;
}

TEST(MainMenuUtils, CopiesEmptyTextOfSuccessfulRequest)
{
	const char response[] = { 'M' };
	char *text = NewHTTPTextBuffer(TRUE, response, 0);
	ASSERT_NE(text, nullptr);
	EXPECT_STREQ(text, "");
	delete[] text;
}

TEST(MainMenuUtils, CopiesNoTextOfFailedRequest)
{
	const char response[] = { 'M', 'O', 'T', 'D' };
	EXPECT_EQ(NewHTTPTextBuffer(FALSE, response, sizeof(response)), nullptr);
	EXPECT_EQ(NewHTTPTextBuffer(FALSE, response, 0), nullptr);
	EXPECT_EQ(NewHTTPTextBuffer(FALSE, nullptr, 0), nullptr);
}
