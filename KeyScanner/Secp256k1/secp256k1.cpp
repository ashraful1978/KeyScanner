#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include <regex>
#include <set>
#include"../CryptoUtil/CryptoUtil.h"

#include "secp256k1.h"


using namespace secp256k1;

static uint256 _ONE(1);
static uint256 _ZERO;
static crypto::Rng _rng;

static const uint32_t MASK32 = 0xFFFFFFFFUL;

static inline void addc(unsigned int a, unsigned int b, unsigned int carryIn, unsigned int& sum, int& carryOut)
{
	uint64_t sum64 = (uint64_t)a + b + carryIn;

	sum = (unsigned int)sum64;
	carryOut = (int)(sum64 >> 32) & 1;
}


static inline void subc(unsigned int a, unsigned int b, unsigned int borrowIn, unsigned int& diff, int& borrowOut)
{
	uint64_t diff64 = (uint64_t)a - b - borrowIn;

	diff = (unsigned int)diff64;
	borrowOut = (int)((diff64 >> 32) & 1);
}



static bool lessThanEqualTo(const unsigned int* a, const unsigned int* b, int len)
{
	for (int i = len - 1; i >= 0; i--) {
		if (a[i] < b[i]) {
			// is greater than
			return true;
		}
		else if (a[i] > b[i]) {
			// is less than
			return false;
		}
	}

	// is equal
	return true;
}

static bool greaterThanEqualTo(const unsigned int* a, const unsigned int* b, int len)
{
	for (int i = len - 1; i >= 0; i--) {
		if (a[i] > b[i]) {
			// is greater than
			return true;
		}
		else if (a[i] < b[i]) {
			// is less than
			return false;
		}
	}

	// is equal
	return true;
}

static int add(const unsigned int* a, const unsigned int* b, unsigned int* c, int len)
{
	int carry = 0;

	for (int i = 0; i < len; i++) {
		addc(a[i], b[i], carry, c[i], carry);
	}

	return carry;
}

static int sub(const unsigned int* a, const unsigned int* b, unsigned int* c, int len)
{
	int borrow = 0;

	for (int i = 0; i < len; i++) {
		subc(a[i], b[i], borrow, c[i], borrow);
	}

	return borrow & 1;
}

static void multiply(const unsigned int* x, int xLen, const unsigned int* y, int yLen, unsigned int* z)
{
	// Zero out the first yLen words of the z. We only need to clear the first yLen words
	// because after each iteration the most significant word is overwritten anyway
	//memset(z, 0, (yLen + xLen) * sizeof(unsigned int));

	for (int i = 0; i < xLen + yLen; i++) {
		z[i] = 0;
	}

	int i, j;
	for (i = 0; i < xLen; i++) {

		unsigned int high = 0;

		for (j = 0; j < yLen; j++) {

			uint64_t product = (uint64_t)x[i] * y[j];

			// Take the existing sum and add it to the product plus the high word from the
			// previous multiplication. Since we are adding to a larger datatype, the compiler
			// will take care of performing any carries resulting from the addition
			product = product + z[i + j] + high;
			// update the sum
			z[i + j] = (unsigned int)product;

			// Keep the high word for the next iteration
			high = product >> 32;
		}

		z[i + yLen] = high;
	}
}

static uint256 rightShift(const uint256& x, int count)
{
	uint256 r;

	count &= 0x1f;

	for (int i = 0; i < 7; i++) {
		r.v[i] = (x.v[i] >> count) | (x.v[i + 1] << (32 - count));
	}
	r.v[7] = x.v[7] >> count;

	return r;
}

uint256 uint256::mul(const uint256& x) const
{
	unsigned int product[16] = { 0 };

	multiply(this->v, 8, x.v, 8, product);

	return uint256(product);
}

uint256 uint256::mul(uint64_t i) const
{
	unsigned int product[16] = { 0 };
	unsigned int x[2];

	x[0] = (unsigned int)i;
	x[1] = (unsigned int)(i >> 32);

	multiply(x, 2, this->v, 8, product);

	return uint256(product);
}

uint256 uint256::mul(int i) const
{
	unsigned int product[16] = { 0 };

	multiply((unsigned int*)&i, 1, this->v, 8, product);

	return uint256(product);
}

uint256 uint256::mul(uint32_t i) const
{
	unsigned int product[16] = { 0 };

	multiply((unsigned int*)&i, 1, this->v, 8, product);

	return uint256(product);
}

uint256 uint256::div(uint32_t val) const
{
	uint256 t = *this;
	uint256 quotient;

	// Shift divisor left until MSB is 1
	uint32_t kWords[8] = { 0 };
	kWords[7] = val;

	int shiftCount = 7 * 32;

	while ((kWords[7] & 0x80000000) == 0) {
		kWords[7] <<= 1;
		shiftCount++;
	}

	uint256 k(kWords);
	// while t >= divisor
	while (t.cmp(uint256(val)) >= 0) {

		// while t < k
		while (t.cmp(k) < 0) {
			// k = k / 2
			k = rightShift(k, 1);
			shiftCount--;
		}
		// t = t - k
		::sub(t.v, k.v, t.v, 8);

		quotient = quotient.add(uint256(2).pow(shiftCount));
	}

	return quotient;
}


uint256 uint256::div(const uint256& val) const
{
	uint256 t = *this;
	uint256 quotient;

	// Shift divisor left until MSB is 1
	//uint32_t kWords[8] = { 0 };
	//kWords[7] = val;
	uint256 k0(val.v);
	printf("\n%s ", k0.toString().c_str());

	int shiftCount = 0 * 32;

	for (int j = 0; j < 8; j++) {
		if (k0.v[j] == 0) {
			shiftCount += 32;
		}
		else {

			while ((k0.v[j] & 0x80000000) == 0) {
				k0.v[j] <<= 1;
				shiftCount++;
			}
		}
		//printf("%d: %d\n", j, shiftCount);
	}
	printf("%s \n", k0.toString().c_str());
	//printf("completed: %d, %s %s\n", shiftCount, t.toString().c_str(), k0.toString().c_str());
	uint256 k(k0.v);
	//int kanha = 0;
	// while t >= divisor
	while (t.cmp(val) >= 0) {

		// while t < k
		while (t.cmp(k) < 0) {
			// k = k / 2
			k = rightShift(k, 1);
			shiftCount--;
		}
		// t = t - k
		::sub(t.v, k.v, t.v, 8);

		quotient = quotient.add(uint256(2).pow(shiftCount));
		//printf("hi: %d\n", kanha++);
	}

	return quotient;
}


uint256 uint256::mod(uint32_t val) const
{
	uint256 quotient = this->div(val);

	uint256 product = quotient.mul(val);

	uint256 result;

	::sub(this->v, product.v, result.v, 8);

	return result;
}

uint256 uint256::add(int val) const
{
	uint256 result(val);

	::add(this->v, result.v, result.v, 8);

	return result;
}

uint256 uint256::add(unsigned int val) const
{
	uint256 result(val);

	::add(this->v, result.v, result.v, 8);

	return result;
}

uint256 uint256::add(uint64_t val) const
{
	uint256 result(val);

	::add(this->v, result.v, result.v, 8);

	return result;
}

uint256 uint256::sub(int val) const
{
	uint256 result(val);

	::sub(this->v, result.v, result.v, 8);

	return result;
}

uint256 uint256::add(const uint256& val) const
{
	uint256 result;

	::add(this->v, val.v, result.v, 8);

	return result;
}

uint256 uint256::sub(const uint256& val) const
{
	uint256 result;

	::sub(this->v, val.v, result.v, 8);

	return result;
}

static bool isOne(const uint256& x)
{
	if (x.v[0] != 1) {
		return false;
	}

	for (int i = 1; i < 8; i++) {
		if (x.v[i] != 0) {
			return false;
		}
	}

	return true;
}

static uint256 divBy2(const uint256& x)
{
	uint256 r;

	for (int i = 0; i < 7; i++) {
		r.v[i] = (x.v[i] >> 1) | (x.v[i + 1] << 31);
	}
	r.v[7] = x.v[7] >> 1;

	return r;
}


static bool isEven(const uint256& x)
{
	return (x.v[0] & 1) == 0;
}

ecpoint secp256k1::pointAtInfinity()
{
	uint256 x(_POINT_AT_INFINITY_WORDS);

	return ecpoint(x, x);
}

ecpoint secp256k1::G()
{
	uint256 x(_GX_WORDS);
	uint256 y(_GY_WORDS);

	return ecpoint(x, y);
}

uint256 secp256k1::invModP(const uint256& x)
{
	uint256 u = x;
	uint256 v = P;
	uint256 x1 = _ONE;
	uint256 x2 = _ZERO;

	// Signed part of the 256-bit words
	int x1Signed = 0;
	int x2Signed = 0;

	while (!isOne(u) && !isOne(v)) {

		while (isEven(u)) {

			u = divBy2(u);

			if (isEven(x1)) {
				x1 = divBy2(x1);

				// Shift right (signed bit is preserved)
				x1.v[7] |= ((unsigned int)x1Signed & 0x01) << 31;

				x1Signed >>= 1;
			}
			else {
				int carry = add(x1.v, P.v, x1.v, 8);

				x1 = divBy2(x1);

				x1Signed += carry;

				x1.v[7] |= ((unsigned int)x1Signed & 0x01) << 31;

				x1Signed >>= 1;
			}

		}

		while (isEven(v)) {

			v = divBy2(v);

			if (isEven(x2)) {

				x2 = divBy2(x2);

				x2.v[7] |= ((unsigned int)x2Signed & 0x01) << 31;

				x2Signed >>= 1;
			}
			else {
				int carry = add(x2.v, P.v, x2.v, 8);

				x2 = divBy2(x2);

				x2Signed += carry;

				x2.v[7] |= ((unsigned int)x2Signed & 0x01) << 31;

				x2Signed >>= 1;
			}
		}

		if (lessThanEqualTo(v.v, u.v, 8)) {
			sub(u.v, v.v, u.v, 8);

			// x1 = x1 - x2
			int borrow = sub(x1.v, x2.v, x1.v, 8);
			x1Signed -= x2Signed;
			x1Signed -= borrow;
		}
		else {
			sub(v.v, u.v, v.v, 8);
			int borrow = sub(x2.v, x1.v, x2.v, 8);
			x2Signed -= x1Signed;
			x2Signed -= borrow;
		}
	}

	uint256 output;

	if (isOne(u)) {

		while (x1Signed < 0) {
			x1Signed += add(x1.v, P.v, x1.v, 8);
		}

		while (x1Signed > 0) {
			x1Signed -= sub(x1.v, P.v, x1.v, 8);
		}

		for (int i = 0; i < 8; i++) {
			output.v[i] = x1.v[i];
		}

	}
	else {

		while (x2Signed < 0) {
			x2Signed += add(x2.v, P.v, x2.v, 8);
		}

		while (x2Signed > 0) {
			x2Signed -= sub(x2.v, P.v, x2.v, 8);
		}

		for (int i = 0; i < 8; i++) {
			output.v[i] = x2.v[i];
		}
	}

	return output;
}



uint256 secp256k1::addModP(const uint256& a, const uint256& b)
{
	uint256 sum;

	int overflow = add(a.v, b.v, sum.v, 8);

	// mod P
	if (overflow || greaterThanEqualTo(sum.v, P.v, 8)) {
		sub(sum.v, P.v, sum.v, 8);
	}

	return sum;
}

uint256 secp256k1::addModN(const uint256& a, const uint256& b)
{
	uint256 sum;

	int overflow = add(a.v, b.v, sum.v, 8);

	// mod P
	if (overflow || greaterThanEqualTo(sum.v, N.v, 8)) {
		sub(sum.v, N.v, sum.v, 8);
	}

	return sum;
}

uint256 secp256k1::subModN(const uint256& a, const uint256& b)
{
	uint256 diff;

	if (sub(a.v, b.v, diff.v, 8)) {
		add(diff.v, N.v, diff.v, 8);
	}

	return diff;
}

uint256 secp256k1::subModP(const uint256& a, const uint256& b)
{
	uint256 diff;

	if (sub(a.v, b.v, diff.v, 8)) {
		add(diff.v, P.v, diff.v, 8);
	}

	return diff;
}



uint256 secp256k1::negModP(const uint256& x)
{
	return subModP(P, x);
}

uint256 secp256k1::negModN(const uint256& x)
{
	return subModN(N, x);
}

uint256 secp256k1::multiplyModP(const uint256& a, const uint256& b)
{
	unsigned int product[16];

	multiply(a.v, 8, b.v, 8, product);

	unsigned int tmp[10] = { 0 };
	unsigned int tmp2[10] = { 0 };
	unsigned int s = 977;

	//multiply by high 8 words by 2^32 + 977
	for (int i = 0; i < 8; i++) {
		tmp2[1 + i] = product[8 + i];
	}

	multiply(&s, 1, &product[8], 8, &tmp[0]);
	add(tmp, tmp2, tmp, 10);

	// clear top 8 words of product
	for (int i = 8; i < 16; i++) {
		product[i] = 0;
	}

	//add to product
	add(&product[0], tmp, &product[0], 10);


	//multiply high 2 words by 2^32 + 977
	for (int i = 0; i < 8; i++) {
		tmp2[1 + i] = product[8 + i];
	}
	//multiply(&s, 1, &product[8], 2, &tmp[1]);
	multiply(&s, 1, &product[8], 8, &tmp[0]);
	add(tmp, tmp2, tmp, 10);



	// add to low 8 words
	int overflow = add(&product[0], &tmp[0], &product[0], 8);

	if (overflow || greaterThanEqualTo(&product[0], P.v, 8)) {
		sub(&product[0], P.v, &product[0], 8);
	}

	uint256 result;

	for (int i = 0; i < 8; i++) {
		result.v[i] = product[i];
	}

	return result;
}


static void reduceModN(const unsigned int* x, unsigned int* r)
{
	unsigned int barrettN[] = { 0x2fc9bec0, 0x402da173, 0x50b75fc4, 0x45512319, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 00000001 };
	unsigned int product[25] = { 0 };

	// Multiply by barrett constant
	multiply(barrettN, 9, x, 16, product);

	// divide by 2^512
	for (int i = 0; i < 9; i++) {
		product[i] = product[16 + i];
	}

	unsigned int product2[16] = { 0 };

	// Multiply by N
	multiply(product, 8, N.v, 8, product2);

	// Take the difference
	unsigned int diff[16] = { 0 };
	sub(x, product2, diff, 16);

	if ((diff[8] & 1) || greaterThanEqualTo(diff, N.v, 8)) {
		sub(diff, N.v, diff, 8);
	}

	for (int i = 0; i < 8; i++) {
		r[i] = diff[i];
	}
}

uint256 secp256k1::multiplyModN(const uint256& a, const uint256& b)
{
	unsigned int product[16];

	multiply(a.v, 8, b.v, 8, product);

	uint256 r;

	bool gt = false;
	for (int i = 0; i < 8; i++) {
		if (product[8 + i] != 0) {
			gt = true;
			break;
		}
	}

	if (gt) {
		reduceModN(product, r.v);
	}
	else if (greaterThanEqualTo(product, N.v, 8)) {
		sub(product, N.v, r.v, 8);
	}
	else {
		for (int i = 0; i < 8; i++) {
			r.v[i] = product[i];
		}
	}

	return r;
}


static std::string removeLeadingZeros(std::string str)
{
	// Regex to remove leading
	// zeros from a string
	const std::regex pattern("^0+(?!$)");

	// Replaces the matched
	// value with given string
	str = std::regex_replace(str, pattern, "");

	return str;
}


std::string secp256k1::uint256::toString(int base)
{
	std::string s = "";

	for (int i = 7; i >= 0; i--) {
		char hex[9] = { 0 };

		sprintf(hex, "%.8X", this->v[i]);
		s += std::string(hex);
	}

	return removeLeadingZeros(s);
}


uint256 secp256k1::generatePrivateKey()
{
	uint256 k;

	_rng.get((unsigned char*)k.v, 32);

	return k;
}

bool secp256k1::isPointAtInfinity(const ecpoint& p)
{

	for (int i = 0; i < 8; i++) {
		if (p.x.v[i] != 0xffffffff) {
			return false;
		}
	}

	for (int i = 0; i < 8; i++) {
		if (p.y.v[i] != 0xffffffff) {
			return false;
		}
	}

	return true;
}

ecpoint secp256k1::doublePoint(const ecpoint& p)
{
	// 1 / 2y
	uint256 yInv = invModP(addModP(p.y, p.y));

	// s = 3x^2 / 2y
	uint256 x3 = multiplyModP(p.x, p.x);
	uint256 s = multiplyModP(addModP(addModP(x3, x3), x3), yInv);

	//rx = s^2 - 2x
	uint256 rx = subModP(subModP(multiplyModP(s, s), p.x), p.x);

	//ry = s * (px - rx) - py
	uint256 ry = subModP(multiplyModP(s, subModP(p.x, rx)), p.y);

	ecpoint result;
	result.x = rx;
	result.y = ry;

	return result;
}

ecpoint secp256k1::addPoints(const ecpoint& p1, const ecpoint& p2)
{
	if (p1 == p2) {
		return doublePoint(p1);
	}

	if (p1.x == p2.x) {
		return pointAtInfinity();
	}

	if (isPointAtInfinity(p1)) {
		return p2;
	}

	if (isPointAtInfinity(p2)) {
		return p1;
	}

	uint256 rise = subModP(p1.y, p2.y);
	uint256 run = subModP(p1.x, p2.x);

	uint256 s = multiplyModP(rise, invModP(run));

	//rx = (s*s - px - qx) % _p;
	uint256 rx = subModP(subModP(multiplyModP(s, s), p1.x), p2.x);

	//ry = (s * (px - rx) - py) % _p;
	uint256 ry = subModP(multiplyModP(s, subModP(p1.x, rx)), p1.y);

	ecpoint sum;
	sum.x = rx;
	sum.y = ry;

	return sum;
}

ecpoint secp256k1::multiplyPoint(const uint256& k, const ecpoint& p)
{
	ecpoint sum = pointAtInfinity();
	ecpoint d = p;

	for (int i = 0; i < 256; i++) {
		unsigned int mask = 1 << (i % 32);

		if (k.v[i / 32] & mask) {
			sum = addPoints(sum, d);
		}

		d = doublePoint(d);
	}

	return sum;
}

uint256 generatePrivateKey()
{
	uint256 k;

	for (int i = 0; i < 8; i++) {
		k.v[i] = ((unsigned int)rand() | ((unsigned int)rand()) << 17);
	}

	return k;
}

bool secp256k1::pointExists(const ecpoint& p)
{
	uint256 y = multiplyModP(p.y, p.y);

	uint256 x = addModP(multiplyModP(multiplyModP(p.x, p.x), p.x), uint256(7));

	return y == x;
}

static void bulkInversionModP(std::vector<uint256>& in)
{

	std::vector<uint256> products;
	uint256 total(1);

	for (unsigned int i = 0; i < in.size(); i++) {
		total = secp256k1::multiplyModP(total, in[i]);

		products.push_back(total);
	}

	// Do the inversion

	uint256 inverse = secp256k1::invModP(total);

	for (int i = (int)in.size() - 1; i >= 0; i--) {

		if (i > 0) {
			uint256 newValue = secp256k1::multiplyModP(products[i - 1], inverse);
			inverse = multiplyModP(inverse, in[i]);
			in[i] = newValue;
		}
		else {
			in[i] = inverse;
		}
	}
}

void secp256k1::generateKeyPairsBulk(unsigned int count, const ecpoint& basePoint, std::vector<uint256>& privKeysOut, std::vector<ecpoint>& pubKeysOut)
{
	privKeysOut.clear();

	for (unsigned int i = 0; i < count; i++) {
		privKeysOut.push_back(generatePrivateKey());
	}

	generateKeyPairsBulk(basePoint, privKeysOut, pubKeysOut);
}

void secp256k1::generateKeyPairsBulk(const ecpoint& basePoint, std::vector<uint256>& privKeys, std::vector<ecpoint>& pubKeysOut)
{
	unsigned int count = (unsigned int)privKeys.size();

	//privKeysOut.clear();
	pubKeysOut.clear();

	// generate a table of points G, 2G, 4G, 8G...(2^255)G
	std::vector<ecpoint> table;

	table.push_back(basePoint);
	for (int i = 1; i < 256; i++) {

		ecpoint p = doublePoint(table[i - 1]);
		if (!pointExists(p)) {
			throw std::string("Point does not exist!");
		}
		table.push_back(p);
	}

	for (unsigned int i = 0; i < count; i++) {
		pubKeysOut.push_back(ecpoint());
	}

	for (int i = 0; i < 256; i++) {

		std::vector<uint256> runList;

		// calculate (Px - Qx)
		for (unsigned int j = 0; j < count; j++) {
			uint256 run;
			uint256 k = privKeys[j];

			if (k.bit(i)) {
				if (isPointAtInfinity(pubKeysOut[j])) {
					run = uint256(2);
				}
				else {
					run = subModP(pubKeysOut[j].x, table[i].x);
				}
			}
			else {
				run = uint256(2);
			}

			runList.push_back(run);
		}

		// calculate 1/(Px - Qx)
		bulkInversionModP(runList);

		// complete the addition
		for (unsigned int j = 0; j < count; j++) {
			uint256 rise;
			uint256 k = privKeys[j];

			if (k.bit(i)) {
				if (isPointAtInfinity(pubKeysOut[j])) {
					pubKeysOut[j] = table[i];
				}
				else {
					rise = subModP(pubKeysOut[j].y, table[i].y);

					// s = (Py - Qy)/(Px - Qx)
					uint256 s = multiplyModP(rise, runList[j]);

					//rx = (s*s - px - qx) % _p;
					uint256 rx = subModP(subModP(multiplyModP(s, s), pubKeysOut[j].x), table[i].x);

					//ry = (s * (px - rx) - py) % _p;
					uint256 ry = subModP(multiplyModP(s, subModP(pubKeysOut[j].x, rx)), pubKeysOut[j].y);

					ecpoint r(rx, ry);
					if (!pointExists(r)) {
						throw std::string("Point does not exist");
					}
					pubKeysOut[j] = r;
				}
			}
		}
	}
}

/**
 * Parses a public key. Expected format is 04<64 hex digits for X><64 hex digits for Y>
 */
secp256k1::ecpoint secp256k1::parsePublicKey(const std::string& pubKeyString)
{
	if (pubKeyString.length() != 130) {
		throw std::string("Invalid public key");
	}

	if (pubKeyString[0] != '0' || pubKeyString[1] != '4') {
		throw std::string("Invalid public key");
	}

	std::string xString = pubKeyString.substr(2, 64);
	std::string yString = pubKeyString.substr(66, 64);

	uint256 x(xString);
	uint256 y(yString);

	ecpoint p(x, y);

	if (!pointExists(p)) {
		throw std::string("Invalid public key");
	}

	return p;
}


uint256 secp256k1::getDefaultCPURandomRange(uint256 min, uint256 max)
{
	//uint256's are really just wrapper structures for an array[8] of 32bit ints
	//this essentially constructs a new uint256 from a source array of ints

	uint256 result;
	uint256 range = max.sub(min);

	unsigned char targetByteSize = (range.getBitRange() + 31) / 32;

	for (int i = 0; i < 8; i++) {
		if (targetByteSize > i) {
			result.v[i] = rndCPUDefault.getChunk();
			if (targetByteSize > i && targetByteSize <= i + 1 && range.v[i] != 0 && result.v[i] > range.v[i]) {
				result.v[i] %= range.v[i];
			}
		}
	}

	return result.add(min);
}

uint256 secp256k1::getDefaultCPURandomRangeExt(uint256 min, uint256 max, uint64_t max_repeats) {

	bool complete = false;
	uint256 finalResult;

	while (!complete){
	uint256 result;
	uint256 range = max.sub(min);

	unsigned char targetByteSize = (range.getBitRange() + 31) / 32;

	for (int i = 0; i < 8; i++) {
		if (targetByteSize > i) {
			result.v[i] = rndCPUDefault.getChunk();
			if (targetByteSize > i && targetByteSize <= i + 1 && range.v[i] != 0 && result.v[i] > range.v[i]) {
				result.v[i] %= range.v[i];
			}
		}
	}

	 finalResult = result.add(min);

	//check for dupes
	std::set<uint32_t> deduplicator;
	for (uint32_t b : finalResult.v) {
		deduplicator.insert(b);
	}

	if ((finalResult.toString().size() - deduplicator.size())<= max_repeats) complete=true;

	}
	
	return finalResult;
}

uint256 secp256k1::getMTCPURandomRange(uint256 min, uint256 max)
{
	//uint256's are really just wrapper structures for an array[8] of 32bit ints
	//this essentially constructs a new uint256 from a source array of ints

	uint256 result;
	uint256 range = max.sub(min);

	unsigned char targetByteSize = (range.getBitRange() + 31) / 32;

	for (int i = 0; i < 8; i++) {
		if (targetByteSize > i) {
			result.v[i] = rndCPUMt.getChunk();
			if (targetByteSize > i && targetByteSize <= i + 1 && range.v[i] != 0 && result.v[i] > range.v[i]) {
				result.v[i] %= range.v[i];
			}
		}
	}

	return result.add(min);
}

void secp256k1::setDefaultGPURandomBuffer(std::vector<unsigned int> random_buff) {
	rndGPUDefault.setBuffer(random_buff);
}

uint256 secp256k1::getDefaultGPURandomRange16(uint256 min, uint256 max)
{
	//uint256's are really just wrapper structures for an array[8] of 32bit ints
	//this essentially constructs a new uint256 from a source array of ints
	//this variation assumes the buffer is filled with 16bit instead of 32 bit ints

	uint256 result;
	uint256 range = max.sub(min);

	uint64_t block_size = 65535;

	unsigned char targetByteSize = (range.getBitRange() + 31) / 32;

	for (int i = 0; i < 8; i++) {
		if (targetByteSize > i) {
			
			result.v[i] = (rndGPUDefault.getChunk() * block_size) + rndGPUDefault.getChunk();

			if (targetByteSize > i && targetByteSize <= i + 1 && range.v[i] != 0 && result.v[i] > range.v[i]) {
				result.v[i] %= range.v[i];
			}
		}
	}

	return result.add(min);
}

uint256 secp256k1::getDefaultGPURandomRange32(uint256 min, uint256 max)
{
	//uint256's are really just wrapper structures for an array[8] of 32bit ints
	//this essentially constructs a new uint256 from a source array of ints
	// 
	//for this to work the buffer beneath that getChunk() call needs to be populated first

	uint256 result;
	uint256 range = max.sub(min);

	unsigned char targetByteSize = (range.getBitRange() + 31) / 32;

	for (int i = 0; i < 8; i++) {
		if (targetByteSize > i) {

			result.v[i] = rndGPUDefault.getChunk();

			if (targetByteSize > i && targetByteSize <= i + 1 && range.v[i] != 0 && result.v[i] > range.v[i]) {
				result.v[i] %= range.v[i];
			}
		}
	}

	return result.add(min);
}

std::vector<uint256> secp256k1::getGPUAssistedRandoms(std::vector<std::vector<unsigned int>> bufferpool, uint256 min, uint256 max, uint64_t length) {

	secp256k1::uint256 range = max.sub(min);
	std::vector<secp256k1::uint256> results;


	std::vector<uint64_t> bufferHeads;  //bufferheads provides iteration indices for each col
	for (int i = 0; i < bufferpool.size(); i++) {
		bufferHeads.push_back(0);
	}

	uint64_t block_size = 65536;
	uint64_t bufferIndex = 0;
	unsigned char targetByteSize = (range.getBitRange() + 31) / 32;

	while (results.size() < length) {

		secp256k1::uint256 resultA;
		secp256k1::uint256 resultB;

		for (int i = 0; i < 8; i++) {

			if (targetByteSize > i) {

				//A/B sides was meant to end up with a result where you might have two values, say like 2b6a and 9c41 and return one key based on each 
				//such like: 2b6a9c41  9c412b6a, effectively   
				//however - I don't think its actually working quite like that.   possibly the block_size math?   need to investigate or hell, maybe this is a dumb idea anyway
				//it would be probably more efficient to construct it out of the 8 way approach and then truncate the extra 16 bits but this is already sub-second @ 10million

				unsigned int partA = bufferpool[i][bufferHeads[i]];   // buffer[col][row]
				unsigned int partB = bufferpool[i + 8][bufferHeads[i]];

				bufferHeads[i]++;

				resultA.v[i] = (partA * block_size) + partB;

				if (targetByteSize > i && targetByteSize <= i + 1 && range.v[i] != 0 && resultA.v[i] > range.v[i]) {
					resultA.v[i] %= range.v[i];
				}

				resultB.v[i] = (partB * block_size) + partA;
				if (targetByteSize > i && targetByteSize <= i + 1 && range.v[i] != 0 && resultB.v[i] > range.v[i]) {
					resultB.v[i] %= range.v[i];
				}
			}

			if (bufferHeads[i] >= bufferHeads.size()) bufferHeads[i] = 1; //rollover if needed.
		}

		bufferIndex++;
		//if (results.size() % 260000 == 0) Logger::log(LogLevel::Info, "A:" + resultA.toString() + " B:" + resultB.toString());
		results.push_back(resultA.add(min));
		//results.push_back(resultB.add(min));

	}

	return results;
}

std::vector<StrideMapEntry> secp256k1::makeStrideQueue(uint64_t cycles, uint256 baseStride, uint256 maxStride, uint64_t count) {
	uint64_t sixteen = 16;
	uint64_t incrementor = 1;
	uint64_t baseLen = baseStride.length();

	std::vector<uint256> interim_results;
	std::vector<secp256k1::StrideMapEntry> results;
	std::set<uint256> distinctResults;

	distinctResults.insert(baseStride);

	uint256 derivedStride = baseStride;
	uint256 rolloverStride = baseStride;

	//first pass;
	while (derivedStride < maxStride) {
		derivedStride = derivedStride.mul(sixteen);
		if (derivedStride < maxStride && distinctResults.size() < count) 	distinctResults.insert(derivedStride);
	}

	//populate the set (prevents dupes)
	while (distinctResults.size() < count) {
		derivedStride = incrementor;
		//next pass
		while (derivedStride < maxStride) {
			derivedStride = derivedStride.mul(sixteen); // add a zero
			derivedStride = derivedStride.mul(sixteen);  //add a placeholder to receive the single-digt incrementor
			derivedStride = derivedStride + incrementor;

			uint256 expandedDerivedStride = derivedStride;
			for (int j = 0; j < baseLen - 1; j++) {
				expandedDerivedStride = expandedDerivedStride.mul(sixteen);  //pad to the right to match base
			}
			if (expandedDerivedStride < maxStride && distinctResults.size() < count) 	distinctResults.insert(expandedDerivedStride);
		}

		//1, 3, 5, 7, 9,... (for modulo rollover / chaos reasons)
		incrementor+=2;
		
		if (incrementor > 3) {  //this, stepping through and adding to itself, should create some mixed values 
			//fill a working vector.
			std::copy(distinctResults.begin(), distinctResults.end(), std::inserter(interim_results, interim_results.end()));

			for (int j = 0; j < interim_results.size(); j++) {
				
				for (int k = interim_results.size() - 1; k > 0; k--) {
					derivedStride = interim_results[k].add(interim_results[j]);
					if (derivedStride < maxStride && distinctResults.size() < count) 	distinctResults.insert(derivedStride);
				}
			}

		}
		
	}

	interim_results.clear();
	std::copy(distinctResults.begin(), distinctResults.end(), std::inserter(interim_results, interim_results.end()));

	//fill and return the vector.
	for (int r = 0; r < interim_results.size(); r++) {
		secp256k1::StrideMapEntry sme;
		sme.cycles = cycles;
		sme.stride = interim_results[r];
		results.push_back(sme);
	}

	return results;
}

BetaKeyManipulator secp256k1::parseBetaKey(uint256 sampleKey, char placeHolder) {
	BetaKeyManipulator result;
	
	std::string keyStr = sampleKey.toString(16);

	//determine the maximum concurrent count of [placeholder]
	int n = keyStr.length();
	int count = 0;
	int res = 0;
	int lastidx = 0;
	int cur_count = 1;
	for (int i = 0; i < n; i++)
	{
		// If current character matches with next
		if (i < n - 1 && (keyStr[i] == placeHolder) && (keyStr[i] == keyStr[i + 1]))
			cur_count++;

		// If doesn't match, update result
		// (if required) and reset count
		else
		{
			if (cur_count > count)
			{
				count = cur_count;
				res = cur_count;
				lastidx = i;
			}
			cur_count = 1;
		}
	}

	result.keyA = getRangeStart(res, "1");
	result.keyB = getRangeEnd(res, "F");
	result.expander = getRangeStart(keyStr.size() - lastidx, "1");

	return result;
}

uint256 secp256k1::getRandom128(int32_t bits, std::vector<uint32_t>& rStrideHistory)
{
	uint256 result;

	if (bits > 128 || bits < 1) {
		throw std::string("secp256k1::getRandom128: wrong random bits");
	}

	if (bits <= 32) {

		uint32_t r = 0; // rnd.getChunk() | ((uint32_t)0x1UL << (bits - 1));

		if (((uint64_t)0x1UL << bits) == rStrideHistory.size()) {
			throw std::string("Tried all possible value in " + std::to_string(bits) + " bits for random stride");
		}

		do {
			r = rndCPUDefault.getChunk() | ((uint32_t)0x1UL << (bits - 1));
		} while (std::count(rStrideHistory.begin(), rStrideHistory.end(), r) > 0);

		//printf("r = %lu, bits = %d\n", r, bits);

		result.v[0] = (uint32_t)((uint32_t)MASK32 >> (32 - bits)) & r;
		result.v[1] = 0UL;
		result.v[2] = 0UL;
		result.v[3] = 0UL;
		result.v[4] = 0UL;
		result.v[5] = 0UL;
		result.v[6] = 0UL;
		result.v[7] = 0UL;

		rStrideHistory.push_back(r);
	}
	else if (bits <= 64 && bits > 32) {

		result.v[0] = rndCPUDefault.getChunk();
		result.v[1] = (uint32_t)((uint32_t)MASK32 >> (32 - (bits - 32))) & (rndCPUDefault.getChunk() | ((uint32_t)0x1UL << (bits - 32 - 1)));
		result.v[2] = 0UL;
		result.v[3] = 0UL;
		result.v[4] = 0UL;
		result.v[5] = 0UL;
		result.v[6] = 0UL;
		result.v[7] = 0UL;
	}
	else if (bits <= 96 && bits > 64) {

		result.v[0] = rndCPUDefault.getChunk();
		result.v[1] = rndCPUDefault.getChunk();
		result.v[2] = (uint32_t)((uint32_t)MASK32 >> (64 - (bits - 64))) & (rndCPUDefault.getChunk() | ((uint32_t)0x1UL << (bits - 64 - 1)));
		result.v[3] = 0UL;
		result.v[4] = 0UL;
		result.v[5] = 0UL;
		result.v[6] = 0UL;
		result.v[7] = 0UL;
	}
	else if (bits <= 128 && bits > 96) {

		result.v[0] = rndCPUDefault.getChunk();
		result.v[1] = rndCPUDefault.getChunk();
		result.v[2] = rndCPUDefault.getChunk();
		result.v[3] = (uint32_t)((uint32_t)MASK32 >> (96 - (bits - 96))) & (rndCPUDefault.getChunk() | ((uint32_t)0x1UL << (bits - 96 - 1)));;
		result.v[4] = 0UL;
		result.v[5] = 0UL;
		result.v[6] = 0UL;
		result.v[7] = 0UL;
	}
	return result;
}

uint256 secp256k1::getRangeStart(int32_t len, std::string start) {
	//todo - make non-string-version of this
	std::string sRangeStart = start;

	while (sRangeStart.size() < len) {
		sRangeStart += "0";
	}


	uint256 startRange = secp256k1::uint256(sRangeStart); 
	return startRange;
}

uint256 secp256k1::getRangeEnd(int32_t len, std::string start) {
	std::string sRangeEnd = start;

	while (sRangeEnd.size() < len) {
		sRangeEnd += "F";
	}

	uint256 endRange = secp256k1::uint256(sRangeEnd);
	return endRange;
}

std::vector<uint256> secp256k1::getSequentialRange(uint256 start, uint256 end, bool thinProvisioning) {
	std::vector<uint256> results;

	secp256k1::uint256 lastKey = 0;
	if (thinProvisioning) lastKey = getRangeStart(end.toString(16).size(), "1");
	while (lastKey < end) {
		secp256k1::uint256 sequential = lastKey + 1;
		results.push_back(sequential);
		lastKey = sequential;
	}
	return results;
}











