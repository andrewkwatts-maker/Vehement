#include "SpectralRenderer.hpp"
#include <cmath>
#include <algorithm>

namespace Engine {

SpectralRenderer::SpectralRenderer() {
}

SpectralRenderer::~SpectralRenderer() {
}

float SpectralRenderer::SampleWavelength(float u) const {
    // Uniform sampling in visible spectrum
    return wavelengthMin + u * (wavelengthMax - wavelengthMin);
}

float SpectralRenderer::GetWavelengthPDF(float wavelength) const {
    if (wavelength >= wavelengthMin && wavelength <= wavelengthMax) {
        return 1.0f / (wavelengthMax - wavelengthMin);
    }
    return 0.0f;
}

glm::vec3 SpectralRenderer::WavelengthToRGB(float wavelength) {
    // Approximate wavelength to RGB conversion
    float r = 0.0f, g = 0.0f, b = 0.0f;

    if (wavelength >= 380.0f && wavelength < 440.0f) {
        r = -(wavelength - 440.0f) / (440.0f - 380.0f);
        b = 1.0f;
    } else if (wavelength >= 440.0f && wavelength < 490.0f) {
        g = (wavelength - 440.0f) / (490.0f - 440.0f);
        b = 1.0f;
    } else if (wavelength >= 490.0f && wavelength < 510.0f) {
        g = 1.0f;
        b = -(wavelength - 510.0f) / (510.0f - 490.0f);
    } else if (wavelength >= 510.0f && wavelength < 580.0f) {
        r = (wavelength - 510.0f) / (580.0f - 510.0f);
        g = 1.0f;
    } else if (wavelength >= 580.0f && wavelength < 645.0f) {
        r = 1.0f;
        g = -(wavelength - 645.0f) / (645.0f - 580.0f);
    } else if (wavelength >= 645.0f && wavelength <= 780.0f) {
        r = 1.0f;
    }

    // Intensity correction for edges of visible spectrum
    float factor = 1.0f;
    if (wavelength >= 380.0f && wavelength < 420.0f) {
        factor = 0.3f + 0.7f * (wavelength - 380.0f) / (420.0f - 380.0f);
    } else if (wavelength >= 700.0f && wavelength <= 780.0f) {
        factor = 0.3f + 0.7f * (780.0f - wavelength) / (780.0f - 700.0f);
    }

    return glm::vec3(r, g, b) * factor;
}

const std::vector<SpectralRenderer::CIE_CMF> SpectralRenderer::s_CIE_Table = {
    {0.0014f, 0.0000f, 0.0065f},   // 380nm
    {0.0042f, 0.0001f, 0.0201f},   // 390nm
    {0.0143f, 0.0004f, 0.0679f},   // 400nm
    {0.0435f, 0.0012f, 0.2074f},   // 410nm
    {0.1344f, 0.0040f, 0.6456f},   // 420nm
    {0.2839f, 0.0116f, 1.3856f},   // 430nm
    {0.3483f, 0.0230f, 1.7471f},   // 440nm
    {0.3362f, 0.0380f, 1.7721f},   // 450nm
    {0.2908f, 0.0600f, 1.6692f},   // 460nm
    {0.1954f, 0.0910f, 1.2876f},   // 470nm
    {0.0956f, 0.1390f, 0.8130f},   // 480nm
    {0.0320f, 0.2080f, 0.4652f},   // 490nm
    {0.0049f, 0.3230f, 0.2720f},   // 500nm
    {0.0093f, 0.5030f, 0.1582f},   // 510nm
    {0.0633f, 0.7100f, 0.0782f},   // 520nm
    {0.1655f, 0.8620f, 0.0422f},   // 530nm
    {0.2904f, 0.9540f, 0.0203f},   // 540nm
    {0.4334f, 0.9950f, 0.0087f},   // 550nm
    {0.5945f, 0.9950f, 0.0039f},   // 560nm
    {0.7621f, 0.9520f, 0.0021f},   // 570nm
    {0.9163f, 0.8700f, 0.0017f},   // 580nm
    {1.0263f, 0.7570f, 0.0011f},   // 590nm
    {1.0622f, 0.6310f, 0.0008f},   // 600nm
    {1.0026f, 0.5030f, 0.0003f},   // 610nm
    {0.8544f, 0.3810f, 0.0002f},   // 620nm
    {0.6424f, 0.2650f, 0.0000f},   // 630nm
    {0.4479f, 0.1750f, 0.0000f},   // 640nm
    {0.2835f, 0.1070f, 0.0000f},   // 650nm
};

SpectralRenderer::CIE_CMF SpectralRenderer::GetCIE_CMF(float wavelength) {
    // Table starts at 380nm with 10nm intervals
    int index = static_cast<int>((wavelength - 380.0f) / 10.0f);
    index = glm::clamp(index, 0, static_cast<int>(s_CIE_Table.size()) - 1);

    return s_CIE_Table[index];
}

glm::vec3 SpectralRenderer::SpectralToXYZ(const std::vector<float>& spectralDistribution,
                                          const std::vector<float>& wavelengths) {
    glm::vec3 xyz(0.0f);

    for (size_t i = 0; i < spectralDistribution.size(); ++i) {
        CIE_CMF cmf = GetCIE_CMF(wavelengths[i]);
        xyz.x += spectralDistribution[i] * cmf.x;
        xyz.y += spectralDistribution[i] * cmf.y;
        xyz.z += spectralDistribution[i] * cmf.z;
    }

    // Normalize
    float sum = xyz.x + xyz.y + xyz.z;
    if (sum > 0.0f) {
        xyz /= sum;
    }

    return xyz;
}

glm::vec3 SpectralRenderer::XYZ_to_RGB(const glm::vec3& xyz) {
    // XYZ to linear RGB (sRGB D65)
    glm::mat3 M(
        3.2404542f, -1.5371385f, -0.4985314f,
        -0.9692660f,  1.8760108f,  0.0415560f,
        0.0556434f, -0.2040259f,  1.0572252f
    );

    glm::vec3 rgb = M * xyz;
    return glm::clamp(rgb, 0.0f, 1.0f);
}

glm::vec3 SpectralRenderer::RefractSpectral(const glm::vec3& incident,
                                            const glm::vec3& normal,
                                            float ior,
                                            float wavelength) {
    // Get wavelength-dependent IOR would be applied here
    // For now, use standard refraction
    float eta = 1.0f / ior;
    float cosI = glm::dot(-incident, normal);
    float sinT2 = eta * eta * (1.0f - cosI * cosI);

    if (sinT2 > 1.0f) {
        // Total internal reflection
        return glm::reflect(incident, normal);
    }

    float cosT = std::sqrt(1.0f - sinT2);
    return eta * incident + (eta * cosI - cosT) * normal;
}

float SpectralRenderer::GetDispersedIOR(float baseIOR, float abbeNumber, float wavelength) {
    // Abbe number dispersion formula
    const float lambda_d = 587.6f;  // D-line
    const float lambda_F = 486.1f;  // F-line
    const float lambda_C = 656.3f;  // C-line

    float V_d = abbeNumber;
    float n_d = baseIOR;
    float delta_n = (n_d - 1.0f) / V_d;

    float wavelength_factor = (wavelength - lambda_d) / (lambda_F - lambda_C);
    return n_d + delta_n * wavelength_factor;
}

float SpectralRenderer::FresnelSpectral(float cosTheta, float ior) {
    // Schlick's approximation
    float F0 = std::pow((ior - 1.0f) / (ior + 1.0f), 2.0f);
    return F0 + (1.0f - F0) * std::pow(1.0f - cosTheta, 5.0f);
}

std::vector<float> SpectralRenderer::RGB_to_Spectrum(const glm::vec3& rgb) {
    // Simplified RGB to spectrum upsampling
    std::vector<float> spectrum;
    spectrum.reserve(41);  // 380-780nm in 10nm steps

    for (int i = 0; i < 41; ++i) {
        float wavelength = 380.0f + i * 10.0f;
        glm::vec3 wavelengthRGB = WavelengthToRGB(wavelength);

        // Find closest match
        float match = glm::dot(rgb, wavelengthRGB);
        spectrum.push_back(glm::max(match, 0.0f));
    }

    return spectrum;
}

float SpectralRenderer::Interpolate(float x, float x0, float x1, float y0, float y1) {
    float t = (x - x0) / (x1 - x0);
    return y0 + t * (y1 - y0);
}

// ChromaticDispersion implementation
void ChromaticDispersion::CalculateRGB(const glm::vec3& incident,
                                       const glm::vec3& normal,
                                       float baseIOR,
                                       float abbeNumber,
                                       glm::vec3& outRed,
                                       glm::vec3& outGreen,
                                       glm::vec3& outBlue) {
    // RGB wavelengths
    const float redWavelength = 630.0f;
    const float greenWavelength = 530.0f;
    const float blueWavelength = 470.0f;

    // Calculate IOR for each wavelength
    float iorRed = SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, redWavelength);
    float iorGreen = SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, greenWavelength);
    float iorBlue = SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, blueWavelength);

    // Refract each wavelength
    outRed = SpectralRenderer::RefractSpectral(incident, normal, iorRed, redWavelength);
    outGreen = SpectralRenderer::RefractSpectral(incident, normal, iorGreen, greenWavelength);
    outBlue = SpectralRenderer::RefractSpectral(incident, normal, iorBlue, blueWavelength);
}

glm::vec3 ChromaticDispersion::GetChromaticAberration(const glm::vec2& position,
                                                      float baseIOR,
                                                      float abbeNumber) {
    // Radial distortion from center
    float distance = glm::length(position);
    float dispersionStrength = (baseIOR - 1.0f) / abbeNumber;

    // RGB offsets (red bends less, blue bends more)
    float redOffset = distance * dispersionStrength * 0.95f;
    float greenOffset = distance * dispersionStrength;
    float blueOffset = distance * dispersionStrength * 1.05f;

    return glm::vec3(redOffset, greenOffset, blueOffset);
}

glm::vec3 ChromaticDispersion::Rainbow(const glm::vec3& incident,
                                       const glm::vec3& normal,
                                       float baseIOR,
                                       float wavelength) {
    float ior = SpectralRenderer::GetDispersedIOR(baseIOR, 30.0f, wavelength);  // Low Abbe number for strong dispersion
    return SpectralRenderer::RefractSpectral(incident, normal, ior, wavelength);
}

} // namespace Engine
