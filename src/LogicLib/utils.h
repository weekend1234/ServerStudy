#pragma once

namespace NLogicLib
{
	template <typename T>
	bool IsInBounds(const T& value, const T& low, const T& high) {
		return (value >= low) && (value < high);
	}
}
