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

#pragma once

#include <gtest/gtest.h>
#include <cstring>
#include <initializer_list>
#include <string>

#include "Common/DataChunk.h"
#include "Common/Errors.h"

#include "FileSystemTestFixture.h"

// Builds chunk files in the layout that DataChunkOutput writes. The test opens them with the stream that the
// game reads map files with.
class DataChunkTestFixture : public FileSystemTestFixture
{
protected:
	template <typename T>
	static void append(std::string& out, T value)
	{
		out.append(reinterpret_cast<const char *>(&value), sizeof(value));
	}

	static void appendAsciiString(std::string& out, const std::string& value)
	{
		append<UnsignedShort>(out, static_cast<UnsignedShort>(value.size()));
		out += value;
	}

	// Returns a table of contents in which each name gets its position plus one as its id.
	static std::string tableOfContents(std::initializer_list<const char *> names)
	{
		std::string out("CkMp", 4);
		append<Int>(out, static_cast<Int>(names.size()));
		UnsignedInt id = 1;
		for (const char *name : names)
		{
			append<unsigned char>(out, static_cast<unsigned char>(strlen(name)));
			out += name;
			append<UnsignedInt>(out, id++);
		}
		return out;
	}

	// Returns a chunk header that states the given data size, followed by the data.
	static std::string chunkWithSize(UnsignedInt id, Int dataSize, const std::string& data, DataChunkVersionType version = 1)
	{
		std::string out;
		append<UnsignedInt>(out, id);
		append<DataChunkVersionType>(out, version);
		append<Int>(out, dataSize);
		return out + data;
	}

	static std::string chunk(UnsignedInt id, const std::string& data, DataChunkVersionType version = 1)
	{
		return chunkWithSize(id, static_cast<Int>(data.size()), data, version);
	}

	// Writes the content to a map file and opens it.
	void open(const std::string& content)
	{
		writeLooseFile("test.map", content);
		ASSERT_TRUE(m_stream.open("test.map"));
	}

	CachedFileInputStream m_stream;
};

// Expects the function to throw the error that the map readers throw for corrupt files.
template <typename Function>
void expectCorruptFileFormat(Function function)
{
	try
	{
		function();
		ADD_FAILURE() << "Expected ERROR_CORRUPT_FILE_FORMAT, but nothing was thrown";
	}
	catch (ErrorCode code)
	{
		EXPECT_EQ(code, ERROR_CORRUPT_FILE_FORMAT);
	}
}
