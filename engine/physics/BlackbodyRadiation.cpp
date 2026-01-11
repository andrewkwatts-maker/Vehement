#include "BlackbodyRadiation.hpp"
#include <cmath>
#include <algorithm>
#include <glm/gtc/constants.hpp>

namespace Engine {

// Planck's law implementation
float BlackbodyRadiation::PlanckLaw(float wavelength_nm, float temperature_K) {
    if (temperature_K < 100.0f) {
        return 0.0f;
    }

    float lambda = wavelength_nm * 1e-9f;  // Convert to meters

    float c1 = 2.0f * PLANCK_CONSTANT * SPEED_OF_LIGHT * SPEED_OF_LIGHT;
    float c2 = PLANCK_CONSTANT * SPEED_OF_LIGHT / BOLTZMANN_CONSTANT;

    float numerator = c1 / std::pow(lambda, 5.0f);
    float exponent = c2 / (lambda * temperature_K);
    float denominator = std::exp(exponent) - 1.0f;

    return numerator / denominator * 1e-9f;  // Convert to per nm
}

std::vector<float> BlackbodyRadiation::GetSpectralDistribution(float temperature_K, int samples) {
    std::vector<float> distribution;
    distribution.reserve(samples);

    float step = (WAVELENGTH_MAX - WAVELENGTH_MIN) / static_cast<float>(samples - 1);

    for (int i = 0; i < samples; ++i) {
        float wavelength = WAVELENGTH_MIN + i * step;
        distribution.push_back(PlanckLaw(wavelength, temperature_K));
    }

    return distribution;
}

// Mitchell's blackbody approximation (fast)
glm::vec3 BlackbodyRadiation::TemperatureToRGB(float temperature_K) {
    temperature_K = glm::clamp(temperature_K, 1000.0f, 40000.0f);

    float temp = temperature_K / 100.0f;
    float r, g, b;

    // Red
    if (temp <= 66.0f) {
        r = 1.0f;
    } else {
        r = temp - 60.0f;
        r = 1.29293618606f * std::pow(r, -0.1332047592f);
        r = glm::clamp(r, 0.0f, 1.0f);
    }

    // Green
    if (temp <= 66.0f) {
        g = temp;
        g = 0.39008157876f * std::log(g) - 0.63184144378f;
    } else {
        g = temp - 60.0f;
        g = 1.12989086089f * std::pow(g, -0.0755148492f);
    }
    g = glm::clamp(g, 0.0f, 1.0f);

    // Blue
    if (temp >= 66.0f) {
        b = 1.0f;
    } else if (temp <= 19.0f) {
        b = 0.0f;
    } else {
        b = temp - 10.0f;
        b = 0.54320678911f * std::log(b) - 1.19625408134f;
        b = glm::clamp(b, 0.0f, 1.0f);
    }

    return glm::vec3(r, g, b);
}

// CIE color matching function (simplified table)
const std::vector<BlackbodyRadiation::CIE_ColorMatchingFunction> BlackbodyRadiation::s_CIE_CMF_Table = {
    {380.0f, 0.0014f, 0.0000f, 0.0065f},
    {390.0f, 0.0042f, 0.0001f, 0.0201f},
    {400.0f, 0.0143f, 0.0004f, 0.0679f},
    {410.0f, 0.0435f, 0.0012f, 0.2074f},
    {420.0f, 0.1344f, 0.0040f, 0.6456f},
    {430.0f, 0.2839f, 0.0116f, 1.3856f},
    {440.0f, 0.3483f, 0.0230f, 1.7471f},
    {450.0f, 0.3362f, 0.0380f, 1.7721f},
    {460.0f, 0.2908f, 0.0600f, 1.6692f},
    {470.0f, 0.1954f, 0.0910f, 1.2876f},
    {480.0f, 0.0956f, 0.1390f, 0.8130f},
    {490.0f, 0.0320f, 0.2080f, 0.4652f},
    {500.0f, 0.0049f, 0.3230f, 0.2720f},
    {510.0f, 0.0093f, 0.5030f, 0.1582f},
    {520.0f, 0.0633f, 0.7100f, 0.0782f},
    {530.0f, 0.1655f, 0.8620f, 0.0422f},
    {540.0f, 0.2904f, 0.9540f, 0.0203f},
    {550.0f, 0.4334f, 0.9950f, 0.0087f},
    {560.0f, 0.5945f, 0.9950f, 0.0039f},
    {570.0f, 0.7621f, 0.9520f, 0.0021f},
    {580.0f, 0.9163f, 0.8700f, 0.0017f},
    {590.0f, 1.0263f, 0.7570f, 0.0011f},
    {600.0f, 1.0622f, 0.6310f, 0.0008f},
    {610.0f, 1.0026f, 0.5030f, 0.0003f},
    {620.0f, 0.8544f, 0.3810f, 0.0002f},
    {630.0f, 0.6424f, 0.2650f, 0.0000f},
    {640.0f, 0.4479f, 0.1750f, 0.0000f},
    {650.0f, 0.2835f, 0.1070f, 0.0000f},
    {660.0f, 0.1649f, 0.0610f, 0.0000f},
    {670.0f, 0.0874f, 0.0320f, 0.0000f},
    {680.0f, 0.0468f, 0.0170f, 0.0000f},
    {690.0f, 0.0227f, 0.0082f, 0.0000f},
    {700.0f, 0.0114f, 0.0041f, 0.0000f},
    {710.0f, 0.0058f, 0.0021f, 0.0000f},
    {720.0f, 0.0029f, 0.0010f, 0.0000f},
    {730.0f, 0.0014f, 0.0005f, 0.0000f},
    {740.0f, 0.0007f, 0.0003f, 0.0000f},
    {750.0f, 0.0003f, 0.0001f, 0.0000f},
    {760.0f, 0.0002f, 0.0001f, 0.0000f},
    {770.0f, 0.0001f, 0.0000f, 0.0000f},
    {780.0f, 0.0000f, 0.0000f, 0.0000f},
};

BlackbodyRadiation::CIE_ColorMatchingFunction BlackbodyRadiation::GetCIE_CMF(float wavelength_nm) {
    // Find nearest entry in table
    for (size_t i = 0; i < s_CIE_CMF_Table.size() - 1; ++i) {
        if (wavelength_nm >= s_CIE_CMF_Table[i].wavelength &&
            wavelength_nm <= s_CIE_CMF_Table[i + 1].wavelength) {

            // Linear interpolation
            float t = (wavelength_nm - s_CIE_CMF_Table[i].wavelength) /
                      (s_CIE_CMF_Table[i + 1].wavelength - s_CIE_CMF_Table[i].wavelength);

            CIE_ColorMatchingFunction cmf;
            cmf.wavelength = wavelength_nm;
            cmf.x = Interpolate(t, 0.0f, 1.0f, s_CIE_CMF_Table[i].x, s_CIE_CMF_Table[i + 1].x);
            cmf.y = Interpolate(t, 0.0f, 1.0f, s_CIE_CMF_Table[i].y, s_CIE_CMF_Table[i + 1].y);
            cmf.z = Interpolate(t, 0.0f, 1.0f, s_CIE_CMF_Table[i].z, s_CIE_CMF_Table[i + 1].z);

            return cmf;
        }
    }

    return {wavelength_nm, 0.0f, 0.0f, 0.0f};
}

glm::vec3 BlackbodyRadiation::IntegrateSpectralToXYZ(float temperature_K) {
    glm::vec3 xyz(0.0f);

    const int samples = 100;
    const float step = (WAVELENGTH_MAX - WAVELENGTH_MIN) / static_cast<float>(samples);

    for (int i = 0; i < samples; ++i) {
        float wavelength = WAVELENGTH_MIN + i * step;
        float radiance = PlanckLaw(wavelength, temperature_K);
        CIE_ColorMatchingFunction cmf = GetCIE_CMF(wavelength);

        xyz.x += radiance * cmf.x * step;
        xyz.y += radiance * cmf.y * step;
        xyz.z += radiance * cmf.z * step;
    }

    // Normalize
    float sum = xyz.x + xyz.y + xyz.z;
    if (sum > 0.0f) {
        xyz /= sum;
    }

    return xyz;
}

glm::vec3 BlackbodyRadiation::TemperatureToXYZ(float temperature_K) {
    return IntegrateSpectralToXYZ(temperature_K);
}

glm::vec3 BlackbodyRadiation::XYZ_to_sRGB(const glm::vec3& xyz) {
    // XYZ to linear RGB conversion matrix (sRGB D65)
    glm::mat3 M(
        3.2404542f, -1.5371385f, -0.4985314f,
        -0.9692660f,  1.8760108f,  0.0415560f,
        0.0556434f, -0.2040259f,  1.0572252f
    );

    glm::vec3 rgb = M * xyz;

    // Apply gamma correction
    rgb.r = sRGB_Gamma(rgb.r);
    rgb.g = sRGB_Gamma(rgb.g);
    rgb.b = sRGB_Gamma(rgb.b);

    return glm::clamp(rgb, 0.0f, 1.0f);
}

glm::vec3 BlackbodyRadiation::TemperatureToRGB_Accurate(float temperature_K) {
    glm::vec3 xyz = TemperatureToXYZ(temperature_K);
    return XYZ_to_sRGB(xyz);
}

float BlackbodyRadiation::LuminousEfficacy(float temperature_K) {
    // Luminous efficacy peaks at ~6500K (daylight)
    // Maximum theoretical: 683 lm/W at 555nm (photopic vision peak)

    // Simplified approximation
    float peak_temp = 6500.0f;
    float max_efficacy = 683.0f;

    // Gaussian-like falloff from peak
    float temp_ratio = temperature_K / peak_temp;
    float efficacy = max_efficacy * std::exp(-std::pow(std::log(temp_ratio), 2.0f) / 0.5f);

    // Lower bound for very low/high temperatures
    return glm::max(efficacy, 1.0f);
}

float BlackbodyRadiation::StefanBoltzmannLaw(float temperature_K, float area_m2) {
    // P = σ * A * T^4
    return STEFAN_BOLTZMANN * area_m2 * std::pow(temperature_K, 4.0f);
}

float BlackbodyRadiation::WiensDisplacementLaw(float temperature_K) {
    // λ_peak = b / T
    // Wien's displacement constant: b = 2.897771955e-3 m·K
    const float b = 2.897771955e-3f;
    float lambda_m = b / temperature_K;
    return lambda_m * 1e9f;  // Convert to nm
}

float BlackbodyRadiation::RGB_to_Temperature(const glm::vec3& rgb) {
    // Approximate inverse (not exact)
    // Based on red/blue ratio

    float r = rgb.r;
    float b = rgb.b;

    if (b < 0.01f) {
        return 2000.0f;  // Very warm
    }

    float ratio = r / b;

    // Empirical fit
    if (ratio > 1.0f) {
        return 2000.0f + 4500.0f / ratio;
    } else {
        return 6500.0f + 8500.0f * (1.0f - ratio);
    }
}

float BlackbodyRadiation::GetRadianceAtWavelength(float temperature_K, float wavelength_nm) {
    return PlanckLaw(wavelength_nm, temperature_K);
}

float BlackbodyRadiation::GetLuminance(float temperature_K, float area_m2) {
    // Luminance (cd/m²) = Luminous intensity / Area
    float efficacy = LuminousEfficacy(temperature_K);
    float radiantPower = StefanBoltzmannLaw(temperature_K, area_m2);
    float luminousFlux = radiantPower * efficacy;

    // Assuming isotropic emission over hemisphere
    return luminousFlux / (glm::pi<float>() * area_m2);
}

float BlackbodyRadiation::GetPresetTemperature(ColorTemperaturePreset preset) {
    return static_cast<float>(preset);
}

// Helper functions
float BlackbodyRadiation::Interpolate(float x, float x0, float x1, float y0, float y1) {
    float t = (x - x0) / (x1 - x0);
    return y0 + t * (y1 - y0);
}

float BlackbodyRadiation::sRGB_Gamma(float linear) {
    if (linear <= 0.0031308f) {
        return 12.92f * linear;
    } else {
        return 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
    }
}

float BlackbodyRadiation::sRGB_InverseGamma(float srgb) {
    if (srgb <= 0.04045f) {
        return srgb / 12.92f;
    } else {
        return std::pow((srgb + 0.055f) / 1.055f, 2.4f);
    }
}

} // namespace Engine
