#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Engine {

/**
 * @brief Blackbody radiation calculator based on Planck's law
 *
 * Implements physically-based blackbody radiation calculations for
 * temperature-based emission and color conversion.
 */
class BlackbodyRadiation {
public:
    // Physical constants
    static constexpr float PLANCK_CONSTANT = 6.62607015e-34f;      // h (J·s)
    static constexpr float SPEED_OF_LIGHT = 299792458.0f;          // c (m/s)
    static constexpr float BOLTZMANN_CONSTANT = 1.380649e-23f;     // k (J/K)
    static constexpr float STEFAN_BOLTZMANN = 5.670374419e-8f;     // σ (W·m⁻²·K⁻⁴)

    // Wavelength constants (nm)
    static constexpr float WAVELENGTH_MIN = 380.0f;   // Visible spectrum start
    static constexpr float WAVELENGTH_MAX = 780.0f;   // Visible spectrum end
    static constexpr float WAVELENGTH_D = 587.6f;     // D-line (yellow)
    static constexpr float WAVELENGTH_F = 486.1f;     // F-line (blue)
    static constexpr float WAVELENGTH_C = 656.3f;     // C-line (red)

    /**
     * @brief Calculate spectral radiance using Planck's law
     *
     * @param wavelength_nm Wavelength in nanometers
     * @param temperature_K Temperature in Kelvin
     * @return Spectral radiance (W·sr⁻¹·m⁻²·nm⁻¹)
     */
    static float PlanckLaw(float wavelength_nm, float temperature_K);

    /**
     * @brief Calculate spectral radiance for entire visible spectrum
     *
     * @param temperature_K Temperature in Kelvin
     * @param samples Number of wavelength samples
     * @return Vector of spectral radiance values
     */
    static std::vector<float> GetSpectralDistribution(float temperature_K, int samples = 100);

    /**
     * @brief Convert temperature to RGB color (CIE 1931 color space)
     *
     * Uses Mitchell's approximation for fast conversion.
     *
     * @param temperature_K Temperature in Kelvin (1000-40000)
     * @return RGB color (0-1 range)
     */
    static glm::vec3 TemperatureToRGB(float temperature_K);

    /**
     * @brief Convert temperature to RGB using accurate CIE color matching
     *
     * More accurate but slower than TemperatureToRGB.
     *
     * @param temperature_K Temperature in Kelvin
     * @return RGB color (0-1 range)
     */
    static glm::vec3 TemperatureToRGB_Accurate(float temperature_K);

    /**
     * @brief Convert temperature to XYZ color space
     *
     * @param temperature_K Temperature in Kelvin
     * @return XYZ color
     */
    static glm::vec3 TemperatureToXYZ(float temperature_K);

    /**
     * @brief Convert XYZ to sRGB color space
     *
     * @param xyz XYZ color
     * @return sRGB color (0-1 range)
     */
    static glm::vec3 XYZ_to_sRGB(const glm::vec3& xyz);

    /**
     * @brief Calculate luminous efficacy (lumens per watt)
     *
     * @param temperature_K Temperature in Kelvin
     * @return Luminous efficacy (lm/W)
     */
    static float LuminousEfficacy(float temperature_K);

    /**
     * @brief Calculate total radiated power using Stefan-Boltzmann law
     *
     * @param temperature_K Temperature in Kelvin
     * @param area_m2 Surface area in square meters
     * @return Total power (Watts)
     */
    static float StefanBoltzmannLaw(float temperature_K, float area_m2 = 1.0f);

    /**
     * @brief Calculate peak wavelength using Wien's displacement law
     *
     * @param temperature_K Temperature in Kelvin
     * @return Peak wavelength (nm)
     */
    static float WiensDisplacementLaw(float temperature_K);

    /**
     * @brief Get color temperature from RGB
     *
     * Inverse of TemperatureToRGB (approximate)
     *
     * @param rgb RGB color
     * @return Approximate color temperature (K)
     */
    static float RGB_to_Temperature(const glm::vec3& rgb);

    /**
     * @brief Calculate radiance at given wavelength
     *
     * @param temperature_K Temperature in Kelvin
     * @param wavelength_nm Wavelength in nanometers
     * @return Radiance value
     */
    static float GetRadianceAtWavelength(float temperature_K, float wavelength_nm);

    /**
     * @brief Get luminance (cd/m²) from temperature and area
     *
     * @param temperature_K Temperature in Kelvin
     * @param area_m2 Emitting area
     * @return Luminance (cd/m²)
     */
    static float GetLuminance(float temperature_K, float area_m2);

    /**
     * @brief CIE color matching functions
     */
    struct CIE_ColorMatchingFunction {
        float wavelength;  // nm
        float x;           // X tristimulus
        float y;           // Y tristimulus (luminance)
        float z;           // Z tristimulus
    };

    /**
     * @brief Get CIE 1931 color matching function at wavelength
     *
     * @param wavelength_nm Wavelength in nanometers
     * @return Color matching values
     */
    static CIE_ColorMatchingFunction GetCIE_CMF(float wavelength_nm);

    /**
     * @brief Integrate spectral distribution with CIE color matching
     *
     * @param temperature_K Temperature in Kelvin
     * @return XYZ tristimulus values
     */
    static glm::vec3 IntegrateSpectralToXYZ(float temperature_K);

    /**
     * @brief Common color temperature presets
     */
    enum class ColorTemperaturePreset {
        Candle = 1800,
        Tungsten40W = 2600,
        Tungsten100W = 2850,
        Halogen = 3200,
        CarbonArc = 5200,
        Daylight = 6500,
        Overcast = 7000,
        ClearSky = 10000,
        BlueSky = 15000,
    };

    /**
     * @brief Get preset color temperature
     *
     * @param preset Temperature preset
     * @return Temperature in Kelvin
     */
    static float GetPresetTemperature(ColorTemperaturePreset preset);

private:
    // CIE color matching function lookup table
    static const std::vector<CIE_ColorMatchingFunction> s_CIE_CMF_Table;

    // Helper functions
    static float Interpolate(float x, float x0, float x1, float y0, float y1);
    static float sRGB_Gamma(float linear);
    static float sRGB_InverseGamma(float srgb);
};

} // namespace Engine
