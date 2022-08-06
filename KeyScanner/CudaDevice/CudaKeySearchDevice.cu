#include <cuda.h>
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include "../KeyFinder/KeySearchTypes.h"
#include "CudaKeySearchDevice.h"
#include "../CudaMath/ptx.cuh"
#include "../CudaMath/secp256k1.cuh"

#include "../CudaMath/sha256.cuh"
#include "../CudaMath/ripemd160.cuh"

#include "../Secp256k1/secp256k1.h"
//#include "../Logger/Logger.h"

#include "CudaHashLookup.cuh"
#include "CudaAtomicList.cuh"
#include "CudaDeviceKeys.cuh"

__constant__ unsigned int _INC_X[8];

__constant__ unsigned int _INC_Y[8];

__constant__ unsigned int* _CHAIN[1];

static unsigned int* _chainBufferPtr = NULL;


__device__ void doRMD160FinalRound(const unsigned int hIn[5], unsigned int hOut[5])
{
	const unsigned int iv[5] = {
		0x67452301,
		0xefcdab89,
		0x98badcfe,
		0x10325476,
		0xc3d2e1f0
	};

	for (int i = 0; i < 5; i++) {
		hOut[i] = endian(hIn[i] + iv[(i + 1) % 5]);
	}
}


/**
 * Allocates device memory for storing the multiplication chain used in
 the batch inversion operation
 */
cudaError_t allocateChainBuf(unsigned int count)
{
	//allocate chainBuffer
	cudaError_t err = cudaMalloc(&_chainBufferPtr, count * sizeof(unsigned int) * 8);

	if (err) {
		return err;
	}

	err = cudaMemcpyToSymbol(_CHAIN, &_chainBufferPtr, sizeof(unsigned int*));
	if (err) {
		cudaFree(_chainBufferPtr);
	}

	return err;
}


cudaError_t resetDevice()
{
	cudaError_t err;
	
	if (_chainBufferPtr != NULL) {
		err = cudaFree(_chainBufferPtr);
		_chainBufferPtr = NULL;
	}

	return err;
	/*
	cudaFree(_INC_X);
	cudaFree(_INC_Y);*/
}

/**
 *Sets the EC point which all points will be incremented by
 */
cudaError_t setIncrementorPoint(const secp256k1::uint256& x, const secp256k1::uint256& y)
{
	cudaError_t err;

	//clear out what is there, if anything
	//err = cudaMemcpyToSymbol(_INC_X, 0, sizeof(unsigned int) * 8);
	//err = cudaMemcpyToSymbol(_INC_Y, 0, sizeof(unsigned int) * 8);

	unsigned int xWords[8];
	unsigned int yWords[8];

	x.exportWords(xWords, 8, secp256k1::uint256::BigEndian);
	y.exportWords(yWords, 8, secp256k1::uint256::BigEndian);

	 err = cudaMemcpyToSymbol(_INC_X, xWords, sizeof(unsigned int) * 8);
	if (err) {
		return err;
	}

	return cudaMemcpyToSymbol(_INC_Y, yWords, sizeof(unsigned int) * 8);
}


__device__ void hashPublicKey(const unsigned int* x, const unsigned int* y, unsigned int* digestOut)
{
	unsigned int hash[8];

	sha256PublicKey(x, y, hash);

	// Swap to little-endian
	for (int i = 0; i < 8; i++) {
		hash[i] = endian(hash[i]);
	}

	ripemd160sha256NoFinal(hash, digestOut);
}

__device__ void hashPublicKeyCompressed(const unsigned int* x, unsigned int yParity, unsigned int* digestOut)
{
	unsigned int hash[8];

	sha256PublicKeyCompressed(x, yParity, hash);

	// Swap to little-endian
	for (int i = 0; i < 8; i++) {
		hash[i] = endian(hash[i]);
	}

	ripemd160sha256NoFinal(hash, digestOut);
}


__device__ void setResultFound(int idx, bool compressed, unsigned int x[8], unsigned int y[8], unsigned int digest[5])
{
	CudaDeviceResult r;

	r.block = blockIdx.x;
	r.thread = threadIdx.x;
	r.idx = idx;
	r.compressed = compressed;

	for (int i = 0; i < 8; i++) {
		r.x[i] = x[i];
		r.y[i] = y[i];
	}

	doRMD160FinalRound(digest, r.digest);

	atomicListAdd(&r, sizeof(r));
}

__device__ void doIteration(int pointsPerThread, int compression, int searchMode)
{
	unsigned int* chain = _CHAIN[0];
	unsigned int* xPtr = ec::getXPtr();
	unsigned int* yPtr = ec::getYPtr();

	// Multiply together all (_Gx - x) and then invert
	unsigned int inverse[8] = { 0,0,0,0,0,0,0,1 };
	for (int i = 0; i < pointsPerThread; i++) {
		unsigned int x[8];

		unsigned int digest[5];

		readInt(xPtr, i, x);


		if (searchMode == SearchMode::ADDRESS) {

			if (compression == PointCompressionType::UNCOMPRESSED || compression == PointCompressionType::BOTH) {
				unsigned int y[8];
				readInt(yPtr, i, y);
				

				hashPublicKey(x, y, digest);

				if (checkHash(digest)) {
					setResultFound(i, false, x, y, digest);
				}
			}

			if (compression == PointCompressionType::COMPRESSED || compression == PointCompressionType::BOTH) {
				hashPublicKeyCompressed(x, readIntLSW(yPtr, i), digest);
				
				/*
				std::string debugVal;
				for (int i = 0; i < 8; i++) {
					debugVal += std::to_string(x[i]);
				}

				Logger::log(LogLevel::Debug, "doIteration() thread: " + std::to_string(i) + " x: " + debugVal);
				*/

				if (checkHash(digest)) {
					unsigned int y[8];
					readInt(yPtr, i, y);
					setResultFound(i, true, x, y, digest);
				}
			}
		}

		beginBatchAdd(_INC_X, x, chain, i, i, inverse);
	}

	doBatchInverse(inverse);

	for (int i = pointsPerThread - 1; i >= 0; i--) {

		unsigned int newX[8];
		unsigned int newY[8];

		completeBatchAdd(_INC_X, _INC_Y, xPtr, yPtr, i, i, chain, inverse, newX, newY);

		writeInt(xPtr, i, newX);
		writeInt(yPtr, i, newY);
	}
	__syncthreads();
}

__device__ void doIterationWithDouble(int pointsPerThread, int compression, int searchMode)
{
	unsigned int* chain = _CHAIN[0];
	unsigned int* xPtr = ec::getXPtr();
	unsigned int* yPtr = ec::getYPtr();

	// Multiply together all (_Gx - x) and then invert
	unsigned int inverse[8] = { 0,0,0,0,0,0,0,1 };
	for (int i = 0; i < pointsPerThread; i++) {
		unsigned int x[8];

		unsigned int digest[5];

		readInt(xPtr, i, x);

		if (searchMode == SearchMode::ADDRESS) {

			// uncompressed
			if (compression == PointCompressionType::UNCOMPRESSED || compression == PointCompressionType::BOTH) {
				unsigned int y[8];
				readInt(yPtr, i, y);
				hashPublicKey(x, y, digest);

				if (checkHash(digest)) {
					setResultFound(i, false, x, y, digest);
				}
			}

			// compressed
			if (compression == PointCompressionType::COMPRESSED || compression == PointCompressionType::BOTH) {

				hashPublicKeyCompressed(x, readIntLSW(yPtr, i), digest);

				if (checkHash(digest)) {

					unsigned int y[8];
					readInt(yPtr, i, y);

					setResultFound(i, true, x, y, digest);
				}
			}
		}

		beginBatchAddWithDouble(_INC_X, _INC_Y, xPtr, chain, i, i, inverse);
	}

	doBatchInverse(inverse);

	for (int i = pointsPerThread - 1; i >= 0; i--) {

		unsigned int newX[8];
		unsigned int newY[8];

		completeBatchAddWithDouble(_INC_X, _INC_Y, xPtr, yPtr, i, i, chain, inverse, newX, newY);

		writeInt(xPtr, i, newX);
		writeInt(yPtr, i, newY);
	}
	__syncthreads();
}

/**
* Performs a single iteration
*/
__global__ void keyFinderKernel(int points, int compression, int searchMode)
{
	doIteration(points, compression, searchMode);
}

__global__ void keyFinderKernelWithDouble(int points, int compression, int searchMode)
{
	doIterationWithDouble(points, compression, searchMode);
}


