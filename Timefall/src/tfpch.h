#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <expected>

#include <string>
#include <sstream>
#include <fstream>
#include <format>
#include <filesystem>

#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <atomic>
#include <cmath>

#include "Timefall/Core/Log.h"
#include "Timefall/Core/Buffer.h"
#include "Timefall/Debug/Profiler.h"

#ifdef TF_PLATFORM_WINDOWS
#include <Windows.h>
#endif
