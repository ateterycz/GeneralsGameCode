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
#include <climits>
#include <string>
#include <vector>

#include "Common/DataChunk.h"

#include "DataChunkTestFixture.h"

// Map files come from other players, so a chunk must not be read past its end, nor past the end of the file.
class DataChunkInputTest : public DataChunkTestFixture
{
protected:
	// Returns a top level chunk of 64 bytes, so that a read past the end of the chunk before it finds data.
	static std::string trailingChunk()
	{
		return chunk(2, std::string(64, 'x'));
	}
};

TEST_F(DataChunkInputTest, ReadsValidChunkData)
{
	std::string data;
	append<Int>(data, -7);
	append<Real>(data, 1.5f);
	append<Byte>(data, 3);
	appendAsciiString(data, "abc");
	append<UnsignedShort>(data, 2);
	append<WideChar>(data, L'd');
	append<WideChar>(data, L'e');
	append<UnsignedShort>(data, 0);
	data += "xyz";
	open(tableOfContents({"Data", "Trailing"}) + chunk(1, data, 2) + trailingChunk());

	DataChunkInput file(&m_stream);
	ASSERT_TRUE(file.isValidFileType());
	DataChunkVersionType version = 0;
	EXPECT_EQ(file.openDataChunk(&version), "Data");
	EXPECT_EQ(version, 2);
	EXPECT_EQ(file.getChunkDataSize(), data.size());
	EXPECT_EQ(file.readInt(), -7);
	EXPECT_EQ(file.readReal(), 1.5f);
	EXPECT_EQ(file.readByte(), 3);
	EXPECT_EQ(file.readAsciiString(), "abc");
	EXPECT_EQ(file.readUnicodeString(), UnicodeString(L"de"));
	EXPECT_TRUE(file.readUnicodeString().isEmpty());
	char bytes[3];
	file.readArrayOfBytes(bytes, 3);
	EXPECT_EQ(std::string(bytes, 3), "xyz");
	EXPECT_TRUE(file.atEndOfChunk());
	file.closeDataChunk();

	EXPECT_EQ(file.openDataChunk(&version), "Trailing");
	EXPECT_EQ(file.getChunkDataSizeLeft(), 64u);
	file.closeDataChunk();
	EXPECT_TRUE(file.atEndOfFile());
}

static Bool parseChild(DataChunkInput& file, DataChunkInfo *info, void *userData)
{
	static_cast<std::vector<Int> *>(userData)->push_back(file.readInt());
	return true;
}

static Bool parseParent(DataChunkInput& file, DataChunkInfo *info, void *userData)
{
	static_cast<std::vector<Int> *>(userData)->push_back(file.readInt());
	file.registerParser("Child", info->label, parseChild);
	return file.parse(userData);
}

TEST_F(DataChunkInputTest, ParsesNestedChunks)
{
	std::string first;
	append<Int>(first, 10);
	std::string second;
	append<Int>(second, 20);
	std::string parent;
	append<Int>(parent, 1);
	parent += chunk(2, first) + chunk(2, second);
	open(tableOfContents({"Parent", "Child"}) + chunk(1, parent));

	DataChunkInput file(&m_stream);
	file.registerParser("Parent", AsciiString::TheEmptyString, parseParent);
	std::vector<Int> values;
	EXPECT_TRUE(file.parse(&values));
	EXPECT_EQ(values, (std::vector<Int>{1, 10, 20}));
}

TEST_F(DataChunkInputTest, ThrowsOnReadPastEndOfChunk)
{
	open(tableOfContents({"Data", "Trailing"}) + chunk(1, "ab") + trailingChunk());

	DataChunkInput file(&m_stream);
	DataChunkVersionType version;
	file.openDataChunk(&version);
	expectCorruptFileFormat([&] { file.readInt(); });
	expectCorruptFileFormat([&] { file.readReal(); });
	EXPECT_EQ(file.readByte(), 'a');
	EXPECT_EQ(file.readByte(), 'b');
	expectCorruptFileFormat([&] { file.readByte(); });
}

TEST_F(DataChunkInputTest, ThrowsOnReadPastEndOfFile)
{
	// The chunk states more data than the file has.
	open(tableOfContents({"Data"}) + chunkWithSize(1, 100, "ab"));

	DataChunkInput file(&m_stream);
	DataChunkVersionType version;
	file.openDataChunk(&version);
	expectCorruptFileFormat([&] { file.readInt(); });
}

TEST_F(DataChunkInputTest, ThrowsOnReadOutsideOfChunk)
{
	open(tableOfContents({"Data"}) + chunk(1, "abcd"));

	DataChunkInput file(&m_stream);
	expectCorruptFileFormat([&] { file.readInt(); });
}

TEST_F(DataChunkInputTest, ThrowsOnNegativeArrayLength)
{
	open(tableOfContents({"Data"}) + chunk(1, "abcd"));

	DataChunkInput file(&m_stream);
	DataChunkVersionType version;
	file.openDataChunk(&version);
	char bytes[4];
	expectCorruptFileFormat([&] { file.readArrayOfBytes(bytes, -1); });
}

TEST_F(DataChunkInputTest, ThrowsOnOversizedAsciiStringLength)
{
	std::string data;
	appendAsciiString(data, "abc");
	data[0] = 100; // The length states more characters than the chunk has.
	open(tableOfContents({"Data", "Trailing"}) + chunk(1, data) + trailingChunk());

	DataChunkInput file(&m_stream);
	DataChunkVersionType version;
	file.openDataChunk(&version);
	expectCorruptFileFormat([&] { file.readAsciiString(); });
}

TEST_F(DataChunkInputTest, ThrowsOnOversizedUnicodeStringLength)
{
	// 3 characters take 6 bytes, but the chunk has 4 bytes after the length.
	std::string data;
	append<UnsignedShort>(data, 3);
	append<WideChar>(data, L'd');
	append<WideChar>(data, L'e');
	open(tableOfContents({"Data", "Trailing"}) + chunk(1, data) + trailingChunk());

	DataChunkInput file(&m_stream);
	DataChunkVersionType version;
	file.openDataChunk(&version);
	expectCorruptFileFormat([&] { file.readUnicodeString(); });
}

TEST_F(DataChunkInputTest, ThrowsOnChildChunkLargerThanParent)
{
	const std::string parent = chunkWithSize(2, 32, "abcd");
	open(tableOfContents({"Parent", "Child", "Trailing"}) + chunk(1, parent) + chunk(3, std::string(64, 'x')));

	DataChunkInput file(&m_stream);
	DataChunkVersionType version;
	EXPECT_EQ(file.openDataChunk(&version), "Parent");
	expectCorruptFileFormat([&] { file.openDataChunk(&version); });
}

TEST_F(DataChunkInputTest, ThrowsOnNegativeChildChunkSize)
{
	const std::string parent = chunkWithSize(2, -1, "abcd");
	open(tableOfContents({"Parent", "Child"}) + chunk(1, parent));

	DataChunkInput file(&m_stream);
	DataChunkVersionType version;
	file.openDataChunk(&version);
	expectCorruptFileFormat([&] { file.openDataChunk(&version); });
}

TEST_F(DataChunkInputTest, ThrowsOnChildChunkHeaderPastEndOfParent)
{
	// The parent has 6 bytes left, but a chunk header takes 10 bytes.
	std::string parent;
	append<Int>(parent, 0);
	parent += chunk(2, "");
	parent.resize(sizeof(Int) + 6);
	open(tableOfContents({"Parent", "Child", "Trailing"}) + chunk(1, parent) + trailingChunk());

	DataChunkInput file(&m_stream);
	DataChunkVersionType version;
	file.openDataChunk(&version);
	file.readInt();
	expectCorruptFileFormat([&] { file.openDataChunk(&version); });
}

TEST_F(DataChunkInputTest, StopsTableOfContentsAtEndOfFile)
{
	// The table of contents states more symbols than the file has.
	std::string content = tableOfContents({"Data"});
	const Int count = INT_MAX;
	content.replace(4, sizeof(count), reinterpret_cast<const char *>(&count), sizeof(count));
	open(content);

	DataChunkInput file(&m_stream);
	EXPECT_FALSE(file.isValidFileType());
}

TEST_F(DataChunkInputTest, ThrowsWhenParserReadsPastEndOfChunk)
{
	// The parser of the parent reads an Int, but the parent has 2 bytes.
	open(tableOfContents({"Parent", "Child", "Trailing"}) + chunk(1, "ab") + trailingChunk());

	DataChunkInput file(&m_stream);
	file.registerParser("Parent", AsciiString::TheEmptyString, parseParent);
	std::vector<Int> values;
	expectCorruptFileFormat([&] { file.parse(&values); });
	EXPECT_TRUE(values.empty());
}
