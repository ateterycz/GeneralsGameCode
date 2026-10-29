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

#include <gtest/gtest.h>
#include <windows.h>

#include "FileSystemTestFixture.h"
#include "Common/Recorder.h"

namespace
{

// Exposes the string readers of the replay header.
class RecorderStrings : public RecorderClass
{
public:
	using RecorderClass::readAsciiString;
	using RecorderClass::readUnicodeString;
};

// The replay header ends each string with a terminator. Wide strings have two bytes per character.
std::string asciiField(const std::string &text)
{
	return text + '\0';
}

std::string wideText(const std::string &text)
{
	std::string bytes;
	for (size_t i = 0; i < text.size(); ++i)
	{
		bytes += text[i];
		bytes += '\0';
	}
	return bytes;
}

std::string wideField(const std::string &text)
{
	return wideText(text) + std::string(2, '\0');
}

std::wstring widen(const std::string &text)
{
	return std::wstring(text.begin(), text.end());
}

} // namespace

class RecorderStringTest : public FileSystemTestFixture
{
protected:
	virtual void TearDown() override
	{
		if (m_file != nullptr)
		{
			m_file->close();
			m_file = nullptr;
		}
		FileSystemTestFixture::TearDown();
	}

	File *openReplay(const std::string &content)
	{
		writeLooseFile("Test.rep", content);
		m_file = TheFileSystem->openFile("Test.rep", File::READ | File::BINARY);
		return m_file;
	}

	File *m_file = nullptr;
};

TEST_F(RecorderStringTest, ReadsAsciiStringsUpTo1023Characters)
{
	const std::string longest(1023, 'x');
	File *file = openReplay(asciiField("abc") + asciiField("") + asciiField(longest) + asciiField("next"));
	ASSERT_NE(file, nullptr);

	EXPECT_STREQ(RecorderStrings::readAsciiString(file).str(), "abc");
	EXPECT_STREQ(RecorderStrings::readAsciiString(file).str(), "");
	EXPECT_EQ(std::string(RecorderStrings::readAsciiString(file).str()), longest);
	EXPECT_STREQ(RecorderStrings::readAsciiString(file).str(), "next");
}

// Longer strings keep their first 1023 characters, and the next string reads from its own start.
TEST_F(RecorderStringTest, TruncatesLongerAsciiStrings)
{
	const std::string longer(1024, 'y');
	const std::string longest(5000, 'z');
	File *file = openReplay(asciiField(longer) + asciiField("one") + asciiField(longest) + asciiField("two"));
	ASSERT_NE(file, nullptr);

	EXPECT_EQ(std::string(RecorderStrings::readAsciiString(file).str()), longer.substr(0, 1023));
	EXPECT_STREQ(RecorderStrings::readAsciiString(file).str(), "one");
	EXPECT_EQ(std::string(RecorderStrings::readAsciiString(file).str()), longest.substr(0, 1023));
	EXPECT_STREQ(RecorderStrings::readAsciiString(file).str(), "two");
}

TEST_F(RecorderStringTest, ReadsUnicodeStringsUpTo1023Characters)
{
	const std::string longest(1023, 'x');
	File *file = openReplay(wideField("abc") + wideField("") + wideField(longest) + wideField("next"));
	ASSERT_NE(file, nullptr);

	EXPECT_EQ(std::wstring(RecorderStrings::readUnicodeString(file).str()), L"abc");
	EXPECT_EQ(std::wstring(RecorderStrings::readUnicodeString(file).str()), L"");
	EXPECT_EQ(std::wstring(RecorderStrings::readUnicodeString(file).str()), widen(longest));
	EXPECT_EQ(std::wstring(RecorderStrings::readUnicodeString(file).str()), L"next");
}

TEST_F(RecorderStringTest, TruncatesLongerUnicodeStrings)
{
	const std::string longer(1024, 'y');
	const std::string longest(5000, 'z');
	File *file = openReplay(wideField(longer) + wideField("one") + wideField(longest) + wideField("two"));
	ASSERT_NE(file, nullptr);

	EXPECT_EQ(std::wstring(RecorderStrings::readUnicodeString(file).str()), widen(longer.substr(0, 1023)));
	EXPECT_EQ(std::wstring(RecorderStrings::readUnicodeString(file).str()), L"one");
	EXPECT_EQ(std::wstring(RecorderStrings::readUnicodeString(file).str()), widen(longest.substr(0, 1023)));
	EXPECT_EQ(std::wstring(RecorderStrings::readUnicodeString(file).str()), L"two");
}

// The wide reader gets WEOF at the end of the file, not EOF, so skipping the rest of a string must stop there.
TEST_F(RecorderStringTest, StopsAtTheEndOfAnUnterminatedUnicodeString)
{
	const std::string longest(2000, 'z');
	File *file = openReplay(wideText(longest));
	ASSERT_NE(file, nullptr);

	EXPECT_EQ(std::wstring(RecorderStrings::readUnicodeString(file).str()), widen(longest.substr(0, 1023)));
}
