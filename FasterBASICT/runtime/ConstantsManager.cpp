//
// ConstantsManager.cpp
// FasterBASIC Runtime - Constants Manager Implementation
//
// Manages compile-time constants with efficient integer-indexed storage.
//

#include "ConstantsManager.h"
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <memory>

namespace FasterBASIC {

// Normalize constant name to lowercase for case-insensitive lookup
std::string ConstantsManager::normalizeName(const std::string& name) {
    std::string normalized = name;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return normalized;
}

ConstantsManager::ConstantsManager() {
    // Reserve space for typical number of constants
    // This prevents reallocation during initialization
    // We have ~200 predefined constants (colors, audio, display modes, etc.)
    // After removing duplicates (lowercase normalization), we need less space
    // Reserve 256 for constants and 200 for name lookup
    m_constants.reserve(256);
    
    // Reserve hash table capacity to prevent rehashing during addPredefinedConstants()
    // Reserve 200 since we now store only lowercase versions (no duplicates)
    m_nameToIndex.reserve(200);
}

int ConstantsManager::addConstant(const std::string& name, int64_t value) {
    // Normalize name to lowercase for case-insensitive storage
    std::string normalizedName = normalizeName(name);
    
    // Check if constant already exists
    auto it = m_nameToIndex.find(normalizedName);
    if (it != m_nameToIndex.end()) {
        // Update existing constant
        m_constants[it->second] = value;
        return it->second;
    }

    // Add new constant
    int index = static_cast<int>(m_constants.size());
    m_constants.push_back(value);
    m_nameToIndex[normalizedName] = index;
    return index;
}

int ConstantsManager::addConstant(const std::string& name, double value) {
    // Normalize name to lowercase for case-insensitive storage
    std::string normalizedName = normalizeName(name);
    
    // Check if constant already exists
    auto it = m_nameToIndex.find(normalizedName);
    if (it != m_nameToIndex.end()) {
        // Update existing constant
        m_constants[it->second] = value;
        return it->second;
    }

    // Add new constant
    int index = static_cast<int>(m_constants.size());
    m_constants.push_back(value);
    m_nameToIndex[normalizedName] = index;
    return index;
}

int ConstantsManager::addConstant(const std::string& name, const std::string& value) {
    // Normalize name to lowercase for case-insensitive storage
    std::string normalizedName = normalizeName(name);
    
    // Check if constant already exists
    auto it = m_nameToIndex.find(normalizedName);
    if (it != m_nameToIndex.end()) {
        // Update existing constant
        m_constants[it->second] = value;
        return it->second;
    }

    // Add new constant
    int index = static_cast<int>(m_constants.size());
    m_constants.push_back(value);
    m_nameToIndex[normalizedName] = index;
    return index;
}

ConstantValue ConstantsManager::getConstant(int index) const {
    if (index < 0 || index >= static_cast<int>(m_constants.size())) {
        throw std::out_of_range("Constant index out of range");
    }
    return m_constants[index];
}

int64_t ConstantsManager::getConstantAsInt(int index) const {
    auto value = getConstant(index);

    if (std::holds_alternative<int64_t>(value)) {
        return std::get<int64_t>(value);
    } else if (std::holds_alternative<double>(value)) {
        return static_cast<int64_t>(std::get<double>(value));
    } else {
        // String to int conversion
        try {
            return std::stoll(std::get<std::string>(value));
        } catch (...) {
            return 0;
        }
    }
}

double ConstantsManager::getConstantAsDouble(int index) const {
    auto value = getConstant(index);

    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value);
    } else if (std::holds_alternative<int64_t>(value)) {
        return static_cast<double>(std::get<int64_t>(value));
    } else {
        // String to double conversion
        try {
            return std::stod(std::get<std::string>(value));
        } catch (...) {
            return 0.0;
        }
    }
}

std::string ConstantsManager::getConstantAsString(int index) const {
    auto value = getConstant(index);

    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    } else if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    } else {
        return std::to_string(std::get<double>(value));
    }
}

bool ConstantsManager::hasConstant(const std::string& name) const {
    std::string normalizedName = normalizeName(name);
    return m_nameToIndex.find(normalizedName) != m_nameToIndex.end();
}

int ConstantsManager::getConstantIndex(const std::string& name) const {
    std::string normalizedName = normalizeName(name);
    auto it = m_nameToIndex.find(normalizedName);
    if (it != m_nameToIndex.end()) {
        return it->second;
    }
    return -1;
}

void ConstantsManager::clear() {
    m_constants.clear();
    m_nameToIndex.clear();
}

void ConstantsManager::copyFrom(const ConstantsManager& other) {
    // Clear existing constants
    m_constants.clear();
    m_nameToIndex.clear();
    
    // Copy the entire vector (preserves indices)
    m_constants = other.m_constants;
    
    // Copy the name-to-index map
    m_nameToIndex = other.m_nameToIndex;
}

void ConstantsManager::addPredefinedConstants() {
    // NOTE: Graphics dimensions (GRAPHICS_WIDTH, GRAPHICS_HEIGHT) should be
    // queried from the runtime/window system, not hardcoded as constants.
    // Use runtime functions like WIDTH() or SCREEN_WIDTH() instead.
    
    // NOTE: All constants are now stored in lowercase due to case-insensitive normalization.
    // Users can reference them in any case (PI, pi, Pi, etc.) and they will all resolve correctly.
    
    // Mathematical constants
    addConstant("pi", 3.14159265358979323846);
    addConstant("e", 2.71828182845904523536);
    addConstant("sqrt2", 1.41421356237309504880);
    addConstant("sqrt3", 1.73205080756887729353);
    addConstant("golden_ratio", 1.61803398874989484820);

    // Boolean constants
    addConstant("true", static_cast<int64_t>(1));
    addConstant("false", static_cast<int64_t>(0));

    // Display mode constants
    addConstant("text", static_cast<int64_t>(0));    // TEXT mode (standard text grid)
    addConstant("lores", static_cast<int64_t>(1));   // LORES mode (160×75 pixel buffer)
    addConstant("midres", static_cast<int64_t>(2));  // MIDRES mode (320×150 pixel buffer)
    addConstant("hires", static_cast<int64_t>(3));   // HIRES mode (640×300 pixel buffer)
    addConstant("ultrares", static_cast<int64_t>(4)); // ULTRARES mode (1280×720 direct color ARGB4444)

    // Color constants (24-bit RGB values for compatibility)
    addConstant("black", static_cast<int64_t>(0x000000));
    addConstant("white", static_cast<int64_t>(0xFFFFFF));
    addConstant("red", static_cast<int64_t>(0xFF0000));
    addConstant("green", static_cast<int64_t>(0x00FF00));
    addConstant("blue", static_cast<int64_t>(0x0000FF));
    addConstant("yellow", static_cast<int64_t>(0xFFFF00));
    addConstant("cyan", static_cast<int64_t>(0x00FFFF));
    addConstant("magenta", static_cast<int64_t>(0xFF00FF));

    // RGBA color constants (32-bit with alpha channel - 0xRRGGBBAA)
    // SOLID_* variants are fully opaque (alpha = 0xFF)
    addConstant("solid_black", static_cast<int64_t>(0x000000FF));
    addConstant("solid_white", static_cast<int64_t>(0xFFFFFFFF));
    addConstant("solid_red", static_cast<int64_t>(0xFF0000FF));
    addConstant("solid_green", static_cast<int64_t>(0x00FF00FF));
    addConstant("solid_blue", static_cast<int64_t>(0x0000FFFF));
    addConstant("solid_yellow", static_cast<int64_t>(0xFFFF00FF));
    addConstant("solid_cyan", static_cast<int64_t>(0x00FFFFFF));
    addConstant("solid_magenta", static_cast<int64_t>(0xFF00FFFF));
    addConstant("solid_purple", static_cast<int64_t>(0x800080FF));
    addConstant("solid_orange", static_cast<int64_t>(0xFF8000FF));
    
    // Browns/Earth Tones
    addConstant("solid_brown", static_cast<int64_t>(0x964B00FF));
    addConstant("solid_maroon", static_cast<int64_t>(0x800000FF));
    
    // Grays (both spellings)
    addConstant("solid_gray", static_cast<int64_t>(0x808080FF));
    addConstant("solid_grey", static_cast<int64_t>(0x808080FF));
    addConstant("solid_dark_gray", static_cast<int64_t>(0x404040FF));
    addConstant("solid_dark_grey", static_cast<int64_t>(0x404040FF));
    addConstant("solid_light_gray", static_cast<int64_t>(0xC0C0C0FF));
    addConstant("solid_light_grey", static_cast<int64_t>(0xC0C0C0FF));
    
    // Pinks
    addConstant("solid_pink", static_cast<int64_t>(0xFFC0CBFF));
    addConstant("solid_hot_pink", static_cast<int64_t>(0xFF69B4FF));
    
    // Purples/Violets
    addConstant("solid_violet", static_cast<int64_t>(0xEE82EEFF));
    addConstant("solid_indigo", static_cast<int64_t>(0x4B0082FF));
    
    // Greens
    addConstant("solid_lime", static_cast<int64_t>(0x00FF00FF));
    addConstant("solid_dark_green", static_cast<int64_t>(0x006400FF));
    addConstant("solid_olive", static_cast<int64_t>(0x808000FF));
    
    // Blues
    addConstant("solid_navy", static_cast<int64_t>(0x000080FF));
    addConstant("solid_teal", static_cast<int64_t>(0x008080FF));
    addConstant("solid_sky_blue", static_cast<int64_t>(0x87CEEBFF));
    
    // Metallics/Others
    addConstant("solid_gold", static_cast<int64_t>(0xFFD700FF));
    addConstant("solid_silver", static_cast<int64_t>(0xC0C0C0FF));
    
    // CLEAR_BLACK is fully transparent (alpha = 0x00)
    addConstant("clear_black", static_cast<int64_t>(0x00000000));

    // C64 Color Palette (ARGB format: 0xAARRGGBB)
    // These are the classic Commodore 64 colors, perfect for retro graphics
    // and 16-color features like chunky pixels
    addConstant("colour_0", static_cast<int64_t>(0xFF000000));  // Black
    addConstant("colour_1", static_cast<int64_t>(0xFFFFFFFF));  // White
    addConstant("colour_2", static_cast<int64_t>(0xFF880000));  // Red
    addConstant("colour_3", static_cast<int64_t>(0xFFAAFFEE));  // Cyan
    addConstant("colour_4", static_cast<int64_t>(0xFFCC44CC));  // Purple
    addConstant("colour_5", static_cast<int64_t>(0xFF00CC55));  // Green
    addConstant("colour_6", static_cast<int64_t>(0xFF0000AA));  // Blue
    addConstant("colour_7", static_cast<int64_t>(0xFFEEEE77));  // Yellow
    addConstant("colour_8", static_cast<int64_t>(0xFFDD8855));  // Orange
    addConstant("colour_9", static_cast<int64_t>(0xFF664400));  // Brown
    addConstant("colour_10", static_cast<int64_t>(0xFFFF7777)); // Light Red
    addConstant("colour_11", static_cast<int64_t>(0xFF333333)); // Dark Grey
    addConstant("colour_12", static_cast<int64_t>(0xFF777777)); // Grey
    addConstant("colour_13", static_cast<int64_t>(0xFFAAFF66)); // Light Green
    addConstant("colour_14", static_cast<int64_t>(0xFF0088FF)); // Light Blue
    addConstant("colour_15", static_cast<int64_t>(0xFFBBBBBB)); // Light Grey

    // Voice/Audio Waveform Types
    addConstant("wave_silence", static_cast<int64_t>(0));
    addConstant("wave_sine", static_cast<int64_t>(1));
    addConstant("wave_square", static_cast<int64_t>(2));
    addConstant("wave_sawtooth", static_cast<int64_t>(3));
    addConstant("wave_triangle", static_cast<int64_t>(4));
    addConstant("wave_noise", static_cast<int64_t>(5));
    addConstant("wave_pulse", static_cast<int64_t>(6));
    addConstant("wave_physical", static_cast<int64_t>(7));

    // Physical Model Types
    addConstant("model_plucked_string", static_cast<int64_t>(0));
    addConstant("model_struck_bar", static_cast<int64_t>(1));
    addConstant("model_blown_tube", static_cast<int64_t>(2));
    addConstant("model_drumhead", static_cast<int64_t>(3));
    addConstant("model_glass", static_cast<int64_t>(4));

    // Filter Types
    addConstant("filter_none", static_cast<int64_t>(0));
    addConstant("filter_lowpass", static_cast<int64_t>(1));
    addConstant("filter_highpass", static_cast<int64_t>(2));
    addConstant("filter_bandpass", static_cast<int64_t>(3));
    addConstant("filter_notch", static_cast<int64_t>(4));

    // LFO Waveform Types
    addConstant("lfo_sine", static_cast<int64_t>(0));
    addConstant("lfo_triangle", static_cast<int64_t>(1));
    addConstant("lfo_square", static_cast<int64_t>(2));
    addConstant("lfo_sawtooth", static_cast<int64_t>(3));
    addConstant("lfo_random", static_cast<int64_t>(4));

    // Rectangle Gradient Modes
    addConstant("st_gradient_solid", static_cast<int64_t>(0));
    addConstant("st_gradient_horizontal", static_cast<int64_t>(1));
    addConstant("st_gradient_vertical", static_cast<int64_t>(2));
    addConstant("st_gradient_diagonal_tl_br", static_cast<int64_t>(3));
    addConstant("st_gradient_diagonal_tr_bl", static_cast<int64_t>(4));
    addConstant("st_gradient_radial", static_cast<int64_t>(5));
    addConstant("st_gradient_four_corner", static_cast<int64_t>(6));
    addConstant("st_gradient_three_point", static_cast<int64_t>(7));

    // Rectangle Procedural Pattern Modes
    addConstant("st_pattern_outline", static_cast<int64_t>(100));
    addConstant("st_pattern_dashed_outline", static_cast<int64_t>(101));
    addConstant("st_pattern_horizontal_stripes", static_cast<int64_t>(102));
    addConstant("st_pattern_vertical_stripes", static_cast<int64_t>(103));
    addConstant("st_pattern_diagonal_stripes", static_cast<int64_t>(104));
    addConstant("st_pattern_checkerboard", static_cast<int64_t>(105));
    addConstant("st_pattern_dots", static_cast<int64_t>(106));
    addConstant("st_pattern_crosshatch", static_cast<int64_t>(107));
    addConstant("st_pattern_rounded_corners", static_cast<int64_t>(108));
    addConstant("st_pattern_grid", static_cast<int64_t>(109));
}

std::vector<std::string> ConstantsManager::getAllConstantNames() const {
    std::vector<std::string> names;
    names.reserve(m_nameToIndex.size());
    
    for (const auto& pair : m_nameToIndex) {
        names.push_back(pair.first);
    }
    
    return names;
}

// Global predefined constants manager (singleton)
static std::unique_ptr<ConstantsManager> g_globalPredefinedConstants;

ConstantsManager* getGlobalPredefinedConstants() {
    if (!g_globalPredefinedConstants) {
        g_globalPredefinedConstants = std::make_unique<ConstantsManager>();
        g_globalPredefinedConstants->addPredefinedConstants();
    }
    return g_globalPredefinedConstants.get();
}

} // namespace FasterBASIC
