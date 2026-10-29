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
#include <vector>

#include "Compression.h"

// The game compresses map and save data chunks and transferred map previews with the preferred compression.
TEST(CompressionManager, PreferredCompressionRoundTrips)
{
	std::vector<unsigned char> source(4096);
	for (size_t i = 0; i < source.size(); ++i)
	{
		source[i] = static_cast<unsigned char>(i % 61);
	}
	const Int sourceSize = static_cast<Int>(source.size());

	const CompressionType type = CompressionManager::getPreferredCompression();
	std::vector<unsigned char> compressed(CompressionManager::getMaxCompressedSize(sourceSize, type));
	const Int compressedSize = CompressionManager::compressData(type, source.data(), sourceSize, compressed.data(), static_cast<Int>(compressed.size()));
	ASSERT_GT(compressedSize, 0);
	EXPECT_LT(compressedSize, sourceSize);
	EXPECT_TRUE(CompressionManager::isDataCompressed(compressed.data(), compressedSize));
	EXPECT_EQ(CompressionManager::getUncompressedSize(compressed.data(), compressedSize), sourceSize);

	std::vector<unsigned char> decompressed(source.size());
	EXPECT_EQ(CompressionManager::decompressData(compressed.data(), compressedSize, decompressed.data(), sourceSize), sourceSize);
	EXPECT_EQ(decompressed, source);
}

namespace
{

// The codecs of the EA compression library, which decode map data chunks of other players.
const CompressionType EacCompressionTypes[] = { COMPRESSION_REFPACK, COMPRESSION_BTREE, COMPRESSION_HUFF };

const size_t GuardSize = 64;
const unsigned char GuardByte = 0xCD;

std::vector<unsigned char> makePatternedData(size_t size)
{
	std::vector<unsigned char> data(size);
	for (size_t i = 0; i < size; ++i)
	{
		data[i] = static_cast<unsigned char>((i * 7) % 61 + (i / 509) % 13);
	}
	return data;
}

// A one byte input, a short text, a long run of one byte and a larger pattern.
std::vector<std::vector<unsigned char> > makeInputs()
{
	const char text[] = "The quick brown fox jumps over the lazy dog, the lazy dog sleeps.";
	std::vector<std::vector<unsigned char> > inputs;
	inputs.push_back(std::vector<unsigned char>(1, 'A'));
	inputs.push_back(std::vector<unsigned char>(text, text + sizeof(text) - 1));
	inputs.push_back(std::vector<unsigned char>(20000, 0));
	inputs.push_back(makePatternedData(70000));
	return inputs;
}

// Compresses the data with the encoder of the codec. Returns the compressed data without spare bytes.
std::vector<unsigned char> compress(CompressionType type, std::vector<unsigned char>& data)
{
	// The EA encoders do not take the size of the destination, so leave plenty of room.
	std::vector<unsigned char> compressed(data.size() * 2 + 1024);
	const Int size = CompressionManager::compressData(type, data.data(), static_cast<Int>(data.size()), compressed.data(), static_cast<Int>(compressed.size()));
	compressed.resize(size > 0 ? size : 0);
	return compressed;
}

// Decompresses the first compressedSize bytes into a destination of destSize bytes that a guard region follows.
// The compressed data keeps all of its bytes in memory, so that a decoder that reads past compressedSize finds them.
Int decompressGuarded(std::vector<unsigned char>& compressed, Int compressedSize, Int destSize, std::vector<unsigned char>& dest)
{
	dest.assign(destSize + GuardSize, GuardByte);
	return CompressionManager::decompressData(compressed.data(), compressedSize, dest.data(), destSize);
}

bool isGuardIntact(const std::vector<unsigned char>& dest)
{
	for (size_t i = dest.size() - GuardSize; i < dest.size(); ++i)
	{
		if (dest[i] != GuardByte)
		{
			return false;
		}
	}
	return true;
}

} // namespace

TEST(CompressionManager, EacCodecsRoundTrip)
{
	std::vector<std::vector<unsigned char> > inputs = makeInputs();
	for (size_t size = 2; size <= 300; ++size)
	{
		inputs.push_back(makePatternedData(size));
	}

	for (size_t t = 0; t < ARRAY_SIZE(EacCompressionTypes); ++t)
	{
		const CompressionType type = EacCompressionTypes[t];
		for (size_t i = 0; i < inputs.size(); ++i)
		{
			SCOPED_TRACE(::testing::Message() << CompressionManager::getCompressionNameByType(type) << ", " << inputs[i].size() << " bytes");
			std::vector<unsigned char> compressed = compress(type, inputs[i]);
			ASSERT_FALSE(compressed.empty());
			EXPECT_EQ(CompressionManager::getCompressionType(compressed.data(), static_cast<Int>(compressed.size())), type);

			const Int size = static_cast<Int>(inputs[i].size());
			std::vector<unsigned char> decompressed;
			EXPECT_EQ(decompressGuarded(compressed, static_cast<Int>(compressed.size()), size, decompressed), size);
			EXPECT_TRUE(isGuardIntact(decompressed));
			decompressed.resize(size);
			EXPECT_EQ(decompressed, inputs[i]);
		}
	}
}

// Data whose header states a smaller size than it decodes to must fail without writing past the destination.
TEST(CompressionManager, EacCodecsFailOnTooSmallDestination)
{
	std::vector<std::vector<unsigned char> > inputs = makeInputs();
	for (size_t t = 0; t < ARRAY_SIZE(EacCompressionTypes); ++t)
	{
		const CompressionType type = EacCompressionTypes[t];
		for (size_t i = 0; i < inputs.size(); ++i)
		{
			SCOPED_TRACE(::testing::Message() << CompressionManager::getCompressionNameByType(type) << ", " << inputs[i].size() << " bytes");
			std::vector<unsigned char> compressed = compress(type, inputs[i]);
			ASSERT_FALSE(compressed.empty());

			const Int destSize = static_cast<Int>(inputs[i].size()) - 1;
			std::vector<unsigned char> decompressed;
			EXPECT_EQ(decompressGuarded(compressed, static_cast<Int>(compressed.size()), destSize, decompressed), 0);
			EXPECT_TRUE(isGuardIntact(decompressed));
		}
	}
}

// Data that ends early must fail without writing past the destination.
TEST(CompressionManager, EacCodecsFailOnTruncatedInput)
{
	std::vector<std::vector<unsigned char> > inputs = makeInputs();
	for (size_t t = 0; t < ARRAY_SIZE(EacCompressionTypes); ++t)
	{
		const CompressionType type = EacCompressionTypes[t];
		for (size_t i = 0; i < inputs.size(); ++i)
		{
			std::vector<unsigned char> compressed = compress(type, inputs[i]);
			ASSERT_FALSE(compressed.empty());

			const Int fullSize = static_cast<Int>(compressed.size());
			const Int truncatedSizes[] = { fullSize - 1, 8 + (fullSize - 8) / 2 };
			for (size_t s = 0; s < ARRAY_SIZE(truncatedSizes); ++s)
			{
				SCOPED_TRACE(::testing::Message() << CompressionManager::getCompressionNameByType(type) << ", " << inputs[i].size()
					<< " bytes, " << truncatedSizes[s] << " of " << fullSize << " compressed bytes");
				const Int size = static_cast<Int>(inputs[i].size());
				std::vector<unsigned char> decompressed;
				EXPECT_EQ(decompressGuarded(compressed, truncatedSizes[s], size, decompressed), 0);
				EXPECT_TRUE(isGuardIntact(decompressed));
			}
		}
	}
}
