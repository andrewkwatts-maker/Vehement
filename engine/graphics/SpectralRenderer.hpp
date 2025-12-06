#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Engine {

/**
 * @brief Spectral renderer for wavelength-dependent rendering
 *
 * Implements hero wavelength sampling for chromatic dispersion,
 * accurate color reproduction, and spectral effects.
 */
class SpectralRenderer {
public:
    SpectralRenderer();
    ~SpectralRenderer();

    /**
     * @brief Rendering modes
     */
    enum class Mode {
        RGB,            // Standard RGB rendering
        Spectral,       // Full spectral rendering
        HeroWavelength  // Hero wavelength sampling (efficient)
    };

    Mode mode = Mode::HeroWavelength;

    /**
     * @brief Number of wavelength samples for full spectral rendering
     */
    int spectralSamples = 16;

    /**
     * @brief Wavelength range (nm)
     */
    float wavelengthMin = 380.0f;
    float wavelengthMax = 780.0f;

    /**
     * @brief Sample wavelength for hero wavelength rendering
     *
     * @param u Random value [0,1]
     * @return Wavelength in nm
     */
    float SampleWavelength(float u) const;

    /**
     * @brief Get wavelength PDF
     *
     * @param wavelength Wavelength in nm
     * @return Probability density
     */
    float GetWavelengthPDF(float wavelength) const;

    /**
     * @brief Convert wavelength to RGB color
     *
     * @param wavelength Wavelength in nm
     * @return RGB color
     */
    static glm::vec3 WavelengthToRGB(float wavelength);

    /**
     * @brief Convert spectral distribution to XYZ
     *
     * @param spectralDistribution Power at each wavelength
     * @param wavelengths Corresponding wavelengths
     * @return XYZ color
     */
    static glm::vec3 SpectralToXYZ(const std::vector<float>& spectralDistribution,
                                    const std::vector<float>& wavelengths);

    /**
     * @brief Convert XYZ to RGB
     *
     * @param xyz XYZ color
     * @return RGB color
     */
    static glm::vec3 XYZ_to_RGB(const glm::vec3& xyz);

    /**
     * @brief Apply chromatic dispersion to ray direction
     *
     * @param incident Incident ray direction
     * @param normal Surface normal
     * @param ior IOR at wavelength
     * @param wavelength Wavelength in nm
     * @return Refracted direction
     */
    static glm::vec3 RefractSpectral(const glm::vec3& incident,
                                      const glm::vec3& normal,
                                      float ior,
                                      float wavelength);

    /**
     * @brief Calculate wavelength-dependent IOR
     *
     * @param baseIOR Base IOR
     * @param abbeNumber Abbe number (dispersion)
     * @param wavelength Wavelength in nm
     * @return IOR at wavelength
     */
    static float GetDispersedIOR(float baseIOR, float abbeNumber, float wavelength);

    /**
     * @brief Fresnel reflectance at wavelength
     *
     * @param cosTheta Cosine of incident angle
     * @param ior IOR at wavelength
     * @return Reflectance [0,1]
     */
    static float FresnelSpectral(float cosTheta, float ior);

    /**
     * @brief CIE 1931 color matching functions
     */
    struct CIE_CMF {
        float x, y, z;
    };

    /**
     * @brief Get CIE color matching function at wavelength
     *
     * @param wavelength Wavelength in nm
     * @return Color matching values
     */
    static CIE_CMF GetCIE_CMF(float wavelength);

    /**
     * @brief Spectral upsampling (RGB to spectrum)
     *
     * @param rgb RGB color
     * @return Approximate spectral distribution
     */
    static std::vector<float> RGB_to_Spectrum(const glm::vec3& rgb);

private:
    /**
     * @brief CIE 1931 standard observer data
     */
    static const std::vector<CIE_CMF> s_CIE_Table;

    static float Interpolate(float x, float x0, float x1, float y0, float y1);
};

/**
 * @brief Chromatic dispersion calculator
 */
class ChromaticDispersion {
public:
    /**
     * @brief Calculate dispersion for RGB wavelengths
     *
     * @param incident Incident direction
     * @param normal Surface normal
     * @param baseIOR Base IOR
     * @param abbeNumber Abbe number
     * @param outRed Refracted red ray
     * @param outGreen Refracted green ray
     * @param outBlue Refracted blue ray
     */
    static void CalculateRGB(const glm::vec3& incident,
                            const glm::vec3& normal,
                            float baseIOR,
                            float abbeNumber,
                            glm::vec3& outRed,
                            glm::vec3& outGreen,
                            glm::vec3& outBlue);

    /**
     * @brief Calculate chromatic aberration offset
     *
     * @param position Screen position
     * @param baseIOR Base IOR
     * @param abbeNumber Abbe number
     * @return Offset for RGB channels
     */
    static glm::vec3 GetChromaticAberration(const glm::vec2& position,
                                            float baseIOR,
                                            float abbeNumber);

    /**
     * @brief Rainbow effect (spectral dispersion)
     *
     * @param incident Incident direction
     * @param normal Surface normal
     * @param baseIOR Base IOR
     * @param wavelength Wavelength in nm
     * @return Refracted direction
     */
    static glm::vec3 Rainbow(const glm::vec3& incident,
                            const glm::vec3& normal,
                            float baseIOR,
                            float wavelength);
};

} // namespace Engine
