#include "../KeyScanner/Secp256k1/secp256k1.h"
#include <vector>
#include <thrust/host_vector.h>
#include <thrust/generate.h>
#include <thrust/random.h>
//#include <thrust/sort.h>
//#include <thrust/functional.h>

namespace Random {

#pragma once
	class RandomHelper
	{

	private:

		//static RandomThrustDefault rndDefault;

	public:

		static secp256k1::uint256 getDefaultRandomRange(secp256k1::uint256 min, secp256k1::uint256 max);

		static std::vector<unsigned int> RandomHelper::getStaticAssemblyBuffer(unsigned int max);
		static std::vector<std::vector<unsigned int>> RandomHelper::getRandomizers(uint64_t seed, unsigned int max, uint64_t len, uint64_t width);

		static std::vector<std::vector<unsigned int>> RandomHelper::getRndBuffer(uint64_t seed, unsigned int min, unsigned int max, uint64_t len, uint64_t width, bool singlePool);
		static std::vector<std::vector<unsigned int>> RandomHelper::getRndBuffer(uint64_t seed, unsigned int min, unsigned int max, uint64_t len, uint64_t width, bool singlePool, bool allowDupes);

		static std::vector<unsigned int> getRndBuffer16(uint64_t buffer_len);
		static std::vector<unsigned int> getRndBuffer32(uint64_t buffer_len);

		static std::vector<uint64_t> RandomHelper::randRange(uint64_t buffer_len, uint64_t min, uint64_t max);
		static std::vector<uint64_t> RandomHelper::sequentialRange(uint64_t buffer_len, uint64_t min, uint64_t max, bool reversed);

		static std::vector<uint64_t> getRndRange(uint64_t seed, uint64_t min_value, uint64_t max_value, uint64_t size);
		static std::vector<uint64_t> getRndRange(uint64_t seed, std::vector<uint64_t> values, uint64_t size);

		static thrust::host_vector<uint64_t> getRndBatch(uint64_t seed, uint64_t min_value, uint64_t max_value, uint64_t size);

		static std::vector<uint64_t> getCPURndBatch(uint64_t seed, uint64_t min_value, uint64_t max_value, uint64_t size);
		static std::vector<uint64_t> getCPURndRange(uint64_t seed, std::vector<uint64_t> values, uint64_t size);

		static std::vector<secp256k1::uint256> sortKeys(std::vector<secp256k1::uint256> keys);

		static std::vector<secp256k1::uint256> getDistances(std::vector<secp256k1::uint256> keys);

	};


	class GPURandom
	{
		std::vector<std::vector<unsigned int>> randomizers;
		std::vector<unsigned int> rand_buffer;
		uint64_t buffer_max;
		uint64_t buffer_index;

	public:

		GPURandom()
		{

		}

		void loadRandomizers(uint64_t seed, uint64_t len) {
			rand_buffer = Random::RandomHelper::getStaticAssemblyBuffer(0xffff);
			randomizers = Random::RandomHelper::getRandomizers(seed, 0xffff, len, 16);
		}

		void setRandomizers(std::vector<std::vector<unsigned int>> rnd) {
			rand_buffer = Random::RandomHelper::getStaticAssemblyBuffer(0xffff);
			randomizers = rnd;
		}

		std::vector<std::vector<unsigned int>> getRandomizers() {
			return randomizers;
		}

		unsigned int getChunk(uint64_t col, uint64_t row) {
			//treating them as 2x16 instead of 1x32 for perf reasons.
			//bufferpool is a sequential vector of unsigned ints from 0 to 0xffff of length[len]; 
			//randomizers are a [col][row] vector of random values from 0 to 0xffff;

			uint64_t bufferIndex16a = randomizers[col][row];
			uint64_t bufferIndex16b = randomizers[col + 8][row];

			unsigned int buffer16a = rand_buffer[bufferIndex16a];
			unsigned int buffer16b = rand_buffer[bufferIndex16b];

			if (buffer16a == 0) buffer16a = 1;
			if (buffer16b == 0) buffer16b = 1;

			//pad the parts, if needed
			while (buffer16a < 0x1000) {
				buffer16a = buffer16a * 0x10;
			}

			while (buffer16b < 0x1000) {
				buffer16b = buffer16b * 0x10;
			}

			unsigned int buffer32 = (buffer16a * 0x1000) + buffer16b;

			return buffer32;
		}

		std::vector<secp256k1::uint256> getRandomPool(secp256k1::uint256 min, secp256k1::uint256 max, uint64_t length) {

			//bufferpool is a sequential vector of unsigned ints from 0 to 0xffff; 
			//randomizers are a [col][row] vector of random values from 0 to 0xffff;
			//Constructs a random uint256 by assembling uint.v[column] from bufferpool[randomizers[col][row]
			//- so that randomizers doesn't necessarily have to match len() - handy for experimentation

			uint64_t REPORTING_SIZE = length / 8;
			uint64_t generatedKeys = 0;

			secp256k1::uint256 range = max.sub(min);
			std::vector<secp256k1::uint256> results;

			unsigned int block_size = 0x10000;
			unsigned char targetByteSize = (range.getBitRange() + 31) / 32;

			while (generatedKeys < length) {

				secp256k1::uint256 result;

				for (int i = 0; i < 8; i++) {
					if (targetByteSize > i) {

						/* for testing - don't judge :)
						uint64_t bufferIndex16a = randomizers[i][generatedKeys];
						uint64_t bufferIndex16b = randomizers[i + 8][generatedKeys];

						unsigned int buffer16a = rand_buffer[bufferIndex16a];
						unsigned int buffer16b = rand_buffer[bufferIndex16b];

						if (buffer16a == 0) buffer16a = 1;
						if (buffer16b == 0) buffer16b = 1;
						//pad the parts, if needed
						while (buffer16a < 0x1000) {
							buffer16a = buffer16a * 0x10;
						}

						while (buffer16b < 0x1000) {
							buffer16b = buffer16b * 0x10;
						}

						unsigned int buffer32 = (buffer16a * 0x1000) + buffer16b;

						result.v[i] = buffer32;

						*/
						result.v[i] = getChunk(i, generatedKeys);

						if (targetByteSize > i && targetByteSize <= i + 1 && range.v[i] != 0 && result.v[i] > range.v[i]) {
							result.v[i] %= range.v[i];
						}
					}
				}
				results.push_back(result);
				generatedKeys++;
			}

			return results;
		}

		~GPURandom() {
			//delete rand_buffer;
		}
	};


}
