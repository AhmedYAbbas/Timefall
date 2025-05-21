#pragma once

#include <string>

namespace Timefall
{
	class Base64
	{
	public:
		static std::string Encode(const std::vector<uint8_t>& data);
		static std::vector<uint8_t> Decode(const std::string& dataStr);

	private:
		static std::array<char, 4> EncodeTripplet(uint8_t a, uint8_t b, uint8_t c);
		static std::array<uint8_t, 3> DecodeQuad(char a, char b, char c, char d);
	};
}
