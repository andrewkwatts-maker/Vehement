/**
 * @file test_spectral_renderer.cpp
 * @brief Comprehensive unit tests for SpectralRenderer
 *
 * Test categories:
 * 1. Wavelength sampling and PDF
 * 2. Wavelength to RGB conversion
 * 3. Spectral to XYZ/RGB conversion
 * 4. IOR dispersion calculations
 * 5. Fresnel calculations
 * 6. CIE color matching functions
 * 7. Chromatic dispersion
 * 8. Edge cases and numerical stability
 * 9. Performance benchmarks
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "graphics/SpectralRenderer.hpp"
#include "utils/TestHelpers.hpp"
#include "utils/Generators.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <cmath>
#include <limits>
#include <vector>

using namespace Engine;
using namespace Nova::Test;

// =============================================================================
// Test Fixture
// =============================================================================

class SpectralRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        renderer = std::make_unique<SpectralRenderer>();
    }

    void TearDown() override {
        renderer.reset();
    }

    std::unique_ptr<SpectralRenderer> renderer;

    // Common wavelengths
    static constexpr float WAVELENGTH_VIOLET = 400.0f;
    static constexpr float WAVELENGTH_BLUE = 470.0f;
    static constexpr float WAVELENGTH_CYAN = 500.0f;
    static constexpr float WAVELENGTH_GREEN = 550.0f;
    static constexpr float WAVELENGTH_YELLOW = 580.0f;
    static constexpr float WAVELENGTH_ORANGE = 600.0f;
    static constexpr float WAVELENGTH_RED = 650.0f;

    // Fraunhofer lines
    static constexpr float FRAUNHOFER_C = 656.3f; // Red
    static constexpr float FRAUNHOFER_D = 587.6f; // Yellow
    static constexpr float FRAUNHOFER_F = 486.1f; // Blue
};

// =============================================================================
// Construction and Default Values Tests
// =============================================================================

TEST_F(SpectralRendererTest, DefaultConstruction) {
    EXPECT_EQ(SpectralRenderer::Mode::HeroWavelength, renderer->mode);
    EXPECT_EQ(16, renderer->spectralSamples);
    EXPECT_FLOAT_EQ(380.0f, renderer->wavelengthMin);
    EXPECT_FLOAT_EQ(780.0f, renderer->wavelengthMax);
}

TEST_F(SpectralRendererTest, ModeChange) {
    renderer->mode = SpectralRenderer::Mode::RGB;
    EXPECT_EQ(SpectralRenderer::Mode::RGB, renderer->mode);

    renderer->mode = SpectralRenderer::Mode::Spectral;
    EXPECT_EQ(SpectralRenderer::Mode::Spectral, renderer->mode);
}

TEST_F(SpectralRendererTest, WavelengthRangeChange) {
    renderer->wavelengthMin = 400.0f;
    renderer->wavelengthMax = 700.0f;

    EXPECT_FLOAT_EQ(400.0f, renderer->wavelengthMin);
    EXPECT_FLOAT_EQ(700.0f, renderer->wavelengthMax);
}

// =============================================================================
// Wavelength Sampling Tests
// =============================================================================

TEST_F(SpectralRendererTest, SampleWavelengthAtZero) {
    float wavelength = renderer->SampleWavelength(0.0f);
    EXPECT_FLOAT_EQ(renderer->wavelengthMin, wavelength);
}

TEST_F(SpectralRendererTest, SampleWavelengthAtOne) {
    float wavelength = renderer->SampleWavelength(1.0f);
    EXPECT_FLOAT_EQ(renderer->wavelengthMax, wavelength);
}

TEST_F(SpectralRendererTest, SampleWavelengthAtMiddle) {
    float wavelength = renderer->SampleWavelength(0.5f);
    float expected = (renderer->wavelengthMin + renderer->wavelengthMax) / 2.0f;
    EXPECT_FLOAT_EQ(expected, wavelength);
}

TEST_F(SpectralRendererTest, SampleWavelengthRange) {
    for (float u = 0.0f; u <= 1.0f; u += 0.1f) {
        float wavelength = renderer->SampleWavelength(u);
        EXPECT_GE(wavelength, renderer->wavelengthMin);
        EXPECT_LE(wavelength, renderer->wavelengthMax);
    }
}

TEST_F(SpectralRendererTest, SampleWavelengthMonotonic) {
    float prev = renderer->SampleWavelength(0.0f);
    for (float u = 0.1f; u <= 1.0f; u += 0.1f) {
        float wavelength = renderer->SampleWavelength(u);
        EXPECT_GE(wavelength, prev);
        prev = wavelength;
    }
}

TEST_F(SpectralRendererTest, SampleWavelengthWithCustomRange) {
    renderer->wavelengthMin = 400.0f;
    renderer->wavelengthMax = 700.0f;

    float wavelength = renderer->SampleWavelength(0.5f);
    EXPECT_FLOAT_EQ(550.0f, wavelength);
}

// =============================================================================
// Wavelength PDF Tests
// =============================================================================

TEST_F(SpectralRendererTest, GetWavelengthPDFWithinRange) {
    float pdf = renderer->GetWavelengthPDF(550.0f);
    float expected = 1.0f / (renderer->wavelengthMax - renderer->wavelengthMin);
    EXPECT_FLOAT_EQ(expected, pdf);
}

TEST_F(SpectralRendererTest, GetWavelengthPDFAtBoundaries) {
    float pdfMin = renderer->GetWavelengthPDF(renderer->wavelengthMin);
    float pdfMax = renderer->GetWavelengthPDF(renderer->wavelengthMax);

    EXPECT_GT(pdfMin, 0.0f);
    EXPECT_GT(pdfMax, 0.0f);
}

TEST_F(SpectralRendererTest, GetWavelengthPDFOutsideRange) {
    float pdfBelow = renderer->GetWavelengthPDF(300.0f);
    float pdfAbove = renderer->GetWavelengthPDF(900.0f);

    EXPECT_FLOAT_EQ(0.0f, pdfBelow);
    EXPECT_FLOAT_EQ(0.0f, pdfAbove);
}

TEST_F(SpectralRendererTest, PDFIntegratesToOne) {
    // Numerical integration of PDF should equal 1
    float integral = 0.0f;
    float step = 1.0f;
    for (float w = renderer->wavelengthMin; w <= renderer->wavelengthMax; w += step) {
        integral += renderer->GetWavelengthPDF(w) * step;
    }

    EXPECT_NEAR(1.0f, integral, 0.01f);
}

// =============================================================================
// Wavelength to RGB Conversion Tests
// =============================================================================

TEST_F(SpectralRendererTest, WavelengthToRGBViolet) {
    glm::vec3 rgb = SpectralRenderer::WavelengthToRGB(WAVELENGTH_VIOLET);

    // Violet should have high blue, some red (purple appearance)
    EXPECT_GT(rgb.b, 0.0f);
    EXPECT_GT(rgb.r, 0.0f);
    EXPECT_LT(rgb.g, rgb.b);
}

TEST_F(SpectralRendererTest, WavelengthToRGBBlue) {
    glm::vec3 rgb = SpectralRenderer::WavelengthToRGB(WAVELENGTH_BLUE);

    // Blue should have dominant blue component
    EXPECT_GT(rgb.b, rgb.r);
    EXPECT_GT(rgb.b, rgb.g);
}

TEST_F(SpectralRendererTest, WavelengthToRGBGreen) {
    glm::vec3 rgb = SpectralRenderer::WavelengthToRGB(WAVELENGTH_GREEN);

    // Green should have dominant green component
    EXPECT_GT(rgb.g, 0.5f);
}

TEST_F(SpectralRendererTest, WavelengthToRGBYellow) {
    glm::vec3 rgb = SpectralRenderer::WavelengthToRGB(WAVELENGTH_YELLOW);

    // Yellow = red + green
    EXPECT_GT(rgb.r, 0.5f);
    EXPECT_GT(rgb.g, 0.5f);
    EXPECT_LT(rgb.b, 0.1f);
}

TEST_F(SpectralRendererTest, WavelengthToRGBRed) {
    glm::vec3 rgb = SpectralRenderer::WavelengthToRGB(WAVELENGTH_RED);

    // Red should have dominant red component
    EXPECT_GT(rgb.r, rgb.g);
    EXPECT_GT(rgb.r, rgb.b);
}

TEST_F(SpectralRendererTest, WavelengthToRGBBoundaries) {
    glm::vec3 rgbMin = SpectralRenderer::WavelengthToRGB(380.0f);
    glm::vec3 rgbMax = SpectralRenderer::WavelengthToRGB(780.0f);

    // At boundaries, intensity should be reduced
    EXPECT_GE(rgbMin.r, 0.0f);
    EXPECT_GE(rgbMin.g, 0.0f);
    EXPECT_GE(rgbMin.b, 0.0f);
    EXPECT_GE(rgbMax.r, 0.0f);
    EXPECT_GE(rgbMax.g, 0.0f);
    EXPECT_GE(rgbMax.b, 0.0f);
}

TEST_F(SpectralRendererTest, WavelengthToRGBOutOfRange) {
    glm::vec3 rgbBelow = SpectralRenderer::WavelengthToRGB(300.0f);
    glm::vec3 rgbAbove = SpectralRenderer::WavelengthToRGB(900.0f);

    // Out of range should return black or near-black
    EXPECT_NEAR(0.0f, rgbBelow.r + rgbBelow.g + rgbBelow.b, 0.1f);
    EXPECT_NEAR(0.0f, rgbAbove.r + rgbAbove.g + rgbAbove.b, 0.1f);
}

TEST_F(SpectralRendererTest, WavelengthToRGBContinuous) {
    // RGB should change smoothly across spectrum
    glm::vec3 prev = SpectralRenderer::WavelengthToRGB(380.0f);

    for (float w = 390.0f; w <= 780.0f; w += 10.0f) {
        glm::vec3 current = SpectralRenderer::WavelengthToRGB(w);

        // Maximum change between adjacent samples should be bounded
        glm::vec3 diff = glm::abs(current - prev);
        float maxDiff = glm::max(diff.r, glm::max(diff.g, diff.b));
        EXPECT_LT(maxDiff, 0.5f);

        prev = current;
    }
}

TEST_F(SpectralRendererTest, WavelengthToRGBValidRange) {
    // All RGB values should be in [0, 1]
    for (float w = 380.0f; w <= 780.0f; w += 5.0f) {
        glm::vec3 rgb = SpectralRenderer::WavelengthToRGB(w);

        EXPECT_GE(rgb.r, 0.0f);
        EXPECT_GE(rgb.g, 0.0f);
        EXPECT_GE(rgb.b, 0.0f);
        EXPECT_LE(rgb.r, 1.0f);
        EXPECT_LE(rgb.g, 1.0f);
        EXPECT_LE(rgb.b, 1.0f);
    }
}

// =============================================================================
// CIE Color Matching Function Tests
// =============================================================================

TEST_F(SpectralRendererTest, GetCIE_CMFAtPeaks) {
    // Y should peak around 555nm (green)
    SpectralRenderer::CIE_CMF cmf555 = SpectralRenderer::GetCIE_CMF(555.0f);
    EXPECT_GT(cmf555.y, 0.9f);

    // X peaks around 600nm
    SpectralRenderer::CIE_CMF cmf600 = SpectralRenderer::GetCIE_CMF(600.0f);
    EXPECT_GT(cmf600.x, 0.8f);
}

TEST_F(SpectralRendererTest, GetCIE_CMFAtBluePeak) {
    // Z peaks around 445nm
    SpectralRenderer::CIE_CMF cmf445 = SpectralRenderer::GetCIE_CMF(445.0f);
    EXPECT_GT(cmf445.z, 1.0f);
}

TEST_F(SpectralRendererTest, GetCIE_CMFNonNegative) {
    for (float w = 380.0f; w <= 780.0f; w += 10.0f) {
        SpectralRenderer::CIE_CMF cmf = SpectralRenderer::GetCIE_CMF(w);
        EXPECT_GE(cmf.x, 0.0f);
        EXPECT_GE(cmf.y, 0.0f);
        EXPECT_GE(cmf.z, 0.0f);
    }
}

TEST_F(SpectralRendererTest, GetCIE_CMFBoundaryHandling) {
    // Should not crash at boundaries
    SpectralRenderer::CIE_CMF cmfLow = SpectralRenderer::GetCIE_CMF(300.0f);
    SpectralRenderer::CIE_CMF cmfHigh = SpectralRenderer::GetCIE_CMF(900.0f);

    EXPECT_FALSE(std::isnan(cmfLow.x));
    EXPECT_FALSE(std::isnan(cmfHigh.x));
}

// =============================================================================
// Spectral to XYZ/RGB Conversion Tests
// =============================================================================

TEST_F(SpectralRendererTest, SpectralToXYZEmpty) {
    std::vector<float> spectrum;
    std::vector<float> wavelengths;

    glm::vec3 xyz = SpectralRenderer::SpectralToXYZ(spectrum, wavelengths);

    EXPECT_VEC3_EQ(glm::vec3(0.0f), xyz);
}

TEST_F(SpectralRendererTest, SpectralToXYZSingleWavelength) {
    std::vector<float> spectrum = {1.0f};
    std::vector<float> wavelengths = {550.0f};

    glm::vec3 xyz = SpectralRenderer::SpectralToXYZ(spectrum, wavelengths);

    // Should produce a valid XYZ
    EXPECT_GE(xyz.x + xyz.y + xyz.z, 0.0f);
}

TEST_F(SpectralRendererTest, SpectralToXYZWhiteSpectrum) {
    std::vector<float> spectrum;
    std::vector<float> wavelengths;

    // Flat spectrum
    for (float w = 380.0f; w <= 780.0f; w += 10.0f) {
        wavelengths.push_back(w);
        spectrum.push_back(1.0f);
    }

    glm::vec3 xyz = SpectralRenderer::SpectralToXYZ(spectrum, wavelengths);

    // White-ish should have roughly equal x, y, z (normalized)
    float sum = xyz.x + xyz.y + xyz.z;
    if (sum > 0.0f) {
        glm::vec3 chromaticity = xyz / sum;
        // White point chromaticity is around (0.33, 0.33, 0.33)
        EXPECT_NEAR(chromaticity.x, chromaticity.y, 0.15f);
    }
}

TEST_F(SpectralRendererTest, XYZ_to_RGBWhitePoint) {
    // D65 white point XYZ
    glm::vec3 xyz(0.95047f, 1.0f, 1.08883f);

    glm::vec3 rgb = SpectralRenderer::XYZ_to_RGB(xyz);

    // Should produce near-white
    EXPECT_NEAR(rgb.r, rgb.g, 0.05f);
    EXPECT_NEAR(rgb.g, rgb.b, 0.05f);
}

TEST_F(SpectralRendererTest, XYZ_to_RGBPrimaries) {
    // Red primary XYZ
    glm::vec3 xyzRed(0.4124f, 0.2126f, 0.0193f);
    glm::vec3 rgbRed = SpectralRenderer::XYZ_to_RGB(xyzRed);
    EXPECT_GT(rgbRed.r, rgbRed.g);
    EXPECT_GT(rgbRed.r, rgbRed.b);

    // Green primary XYZ
    glm::vec3 xyzGreen(0.3576f, 0.7152f, 0.1192f);
    glm::vec3 rgbGreen = SpectralRenderer::XYZ_to_RGB(xyzGreen);
    EXPECT_GT(rgbGreen.g, rgbGreen.r);
    EXPECT_GT(rgbGreen.g, rgbGreen.b);
}

TEST_F(SpectralRendererTest, XYZ_to_RGBClipping) {
    // Out-of-gamut XYZ should be clipped
    glm::vec3 xyzBright(2.0f, 2.0f, 2.0f);
    glm::vec3 rgb = SpectralRenderer::XYZ_to_RGB(xyzBright);

    EXPECT_LE(rgb.r, 1.0f);
    EXPECT_LE(rgb.g, 1.0f);
    EXPECT_LE(rgb.b, 1.0f);
}

TEST_F(SpectralRendererTest, XYZ_to_RGBNegativeClipping) {
    glm::vec3 xyzNegative(-0.5f, 0.5f, 0.5f);
    glm::vec3 rgb = SpectralRenderer::XYZ_to_RGB(xyzNegative);

    EXPECT_GE(rgb.r, 0.0f);
    EXPECT_GE(rgb.g, 0.0f);
    EXPECT_GE(rgb.b, 0.0f);
}

// =============================================================================
// RGB to Spectrum Tests
// =============================================================================

TEST_F(SpectralRendererTest, RGB_to_SpectrumWhite) {
    std::vector<float> spectrum = SpectralRenderer::RGB_to_Spectrum(glm::vec3(1.0f));

    EXPECT_EQ(41u, spectrum.size()); // 380-780nm in 10nm steps
    EXPECT_GT(spectrum[0], 0.0f);
}

TEST_F(SpectralRendererTest, RGB_to_SpectrumRed) {
    std::vector<float> spectrum = SpectralRenderer::RGB_to_Spectrum(glm::vec3(1.0f, 0.0f, 0.0f));

    EXPECT_EQ(41u, spectrum.size());

    // Red wavelengths should have higher values
    float redSum = 0.0f;
    float blueSum = 0.0f;
    for (size_t i = 0; i < spectrum.size(); ++i) {
        float wavelength = 380.0f + i * 10.0f;
        if (wavelength >= 600.0f && wavelength <= 700.0f) {
            redSum += spectrum[i];
        } else if (wavelength >= 400.0f && wavelength <= 500.0f) {
            blueSum += spectrum[i];
        }
    }

    EXPECT_GT(redSum, blueSum);
}

TEST_F(SpectralRendererTest, RGB_to_SpectrumBlack) {
    std::vector<float> spectrum = SpectralRenderer::RGB_to_Spectrum(glm::vec3(0.0f));

    for (float val : spectrum) {
        EXPECT_NEAR(0.0f, val, 0.01f);
    }
}

// =============================================================================
// IOR Dispersion Tests
// =============================================================================

TEST_F(SpectralRendererTest, GetDispersedIORAtDLine) {
    float baseIOR = 1.5f;
    float abbeNumber = 60.0f;

    float ior = SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, FRAUNHOFER_D);

    // At D-line, should be close to base IOR
    EXPECT_NEAR(baseIOR, ior, 0.1f);
}

TEST_F(SpectralRendererTest, GetDispersedIORDispersion) {
    float baseIOR = 1.5f;
    float abbeNumber = 30.0f; // Lower Abbe = more dispersion

    float iorRed = SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, WAVELENGTH_RED);
    float iorBlue = SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, WAVELENGTH_BLUE);

    // Blue light should refract more (higher IOR)
    EXPECT_GT(iorBlue, iorRed);
}

TEST_F(SpectralRendererTest, GetDispersedIORHighAbbe) {
    // High Abbe number = low dispersion
    float baseIOR = 1.5f;
    float abbeNumber = 100.0f;

    float iorRed = SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, WAVELENGTH_RED);
    float iorBlue = SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, WAVELENGTH_BLUE);

    // Very small difference for high Abbe
    EXPECT_NEAR(iorRed, iorBlue, 0.05f);
}

TEST_F(SpectralRendererTest, GetDispersedIORMonotonic) {
    float baseIOR = 1.5f;
    float abbeNumber = 30.0f;

    float prevIOR = SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, 380.0f);

    for (float w = 400.0f; w <= 780.0f; w += 20.0f) {
        float ior = SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, w);
        // IOR should decrease as wavelength increases
        EXPECT_LE(ior, prevIOR + 0.001f); // Small tolerance for numerical errors
        prevIOR = ior;
    }
}

// =============================================================================
// Fresnel Tests
// =============================================================================

TEST_F(SpectralRendererTest, FresnelSpectralAtNormal) {
    // At normal incidence (cos = 1)
    float reflectance = SpectralRenderer::FresnelSpectral(1.0f, 1.5f);

    // Schlick: F0 = ((n-1)/(n+1))^2 = ((0.5)/(2.5))^2 = 0.04
    EXPECT_NEAR(0.04f, reflectance, 0.01f);
}

TEST_F(SpectralRendererTest, FresnelSpectralAtGrazing) {
    // At grazing incidence (cos = 0)
    float reflectance = SpectralRenderer::FresnelSpectral(0.0f, 1.5f);

    // Should approach 1.0 at grazing angle
    EXPECT_NEAR(1.0f, reflectance, 0.01f);
}

TEST_F(SpectralRendererTest, FresnelSpectralRange) {
    for (float cosTheta = 0.0f; cosTheta <= 1.0f; cosTheta += 0.1f) {
        float reflectance = SpectralRenderer::FresnelSpectral(cosTheta, 1.5f);

        EXPECT_GE(reflectance, 0.0f);
        EXPECT_LE(reflectance, 1.0f);
    }
}

TEST_F(SpectralRendererTest, FresnelSpectralMonotonic) {
    // Reflectance should increase as angle increases (cos decreases)
    float prevReflectance = SpectralRenderer::FresnelSpectral(1.0f, 1.5f);

    for (float cosTheta = 0.9f; cosTheta >= 0.0f; cosTheta -= 0.1f) {
        float reflectance = SpectralRenderer::FresnelSpectral(cosTheta, 1.5f);
        EXPECT_GE(reflectance, prevReflectance - 0.001f);
        prevReflectance = reflectance;
    }
}

TEST_F(SpectralRendererTest, FresnelSpectralDifferentIOR) {
    float cos45 = std::cos(M_PI / 4.0f);

    float reflectance15 = SpectralRenderer::FresnelSpectral(cos45, 1.5f);
    float reflectance20 = SpectralRenderer::FresnelSpectral(cos45, 2.0f);

    // Higher IOR should have higher reflectance
    EXPECT_GT(reflectance20, reflectance15);
}

// =============================================================================
// Refraction Tests
// =============================================================================

TEST_F(SpectralRendererTest, RefractSpectralNormalIncidence) {
    glm::vec3 incident(0.0f, -1.0f, 0.0f);
    glm::vec3 normal(0.0f, 1.0f, 0.0f);

    glm::vec3 refracted = SpectralRenderer::RefractSpectral(incident, normal, 1.5f, WAVELENGTH_GREEN);

    // At normal incidence, refracted should be same as incident
    EXPECT_VEC3_NEAR(incident, refracted, 0.01f);
}

TEST_F(SpectralRendererTest, RefractSpectralAngle) {
    glm::vec3 incident = glm::normalize(glm::vec3(0.5f, -1.0f, 0.0f));
    glm::vec3 normal(0.0f, 1.0f, 0.0f);

    glm::vec3 refracted = SpectralRenderer::RefractSpectral(incident, normal, 1.5f, WAVELENGTH_GREEN);

    // Refracted ray should bend toward normal
    float incidentAngle = std::acos(glm::dot(-incident, normal));
    float refractedAngle = std::acos(glm::dot(-refracted, normal));

    // Snell's law: n1*sin(theta1) = n2*sin(theta2)
    // sin(refracted) should be smaller for n2 > n1
    EXPECT_LT(refractedAngle, incidentAngle);
}

TEST_F(SpectralRendererTest, RefractSpectralTIR) {
    // Total internal reflection case
    glm::vec3 incident = glm::normalize(glm::vec3(0.9f, -0.4f, 0.0f));
    glm::vec3 normal(0.0f, 1.0f, 0.0f);

    // High IOR should cause TIR at shallow angles
    glm::vec3 result = SpectralRenderer::RefractSpectral(incident, normal, 2.5f, WAVELENGTH_GREEN);

    // Result should be reflection or handle TIR somehow
    EXPECT_FALSE(std::isnan(result.x));
    EXPECT_FALSE(std::isnan(result.y));
    EXPECT_FALSE(std::isnan(result.z));
}

TEST_F(SpectralRendererTest, RefractSpectralUnitVector) {
    glm::vec3 incident = glm::normalize(glm::vec3(0.3f, -1.0f, 0.2f));
    glm::vec3 normal(0.0f, 1.0f, 0.0f);

    glm::vec3 refracted = SpectralRenderer::RefractSpectral(incident, normal, 1.5f, WAVELENGTH_GREEN);

    // Result should be unit vector
    float length = glm::length(refracted);
    EXPECT_NEAR(1.0f, length, 0.01f);
}

// =============================================================================
// ChromaticDispersion Tests
// =============================================================================

TEST_F(SpectralRendererTest, ChromaticDispersion_CalculateRGB) {
    glm::vec3 incident = glm::normalize(glm::vec3(0.3f, -1.0f, 0.0f));
    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    float baseIOR = 1.5f;
    float abbeNumber = 30.0f;

    glm::vec3 outRed, outGreen, outBlue;
    ChromaticDispersion::CalculateRGB(incident, normal, baseIOR, abbeNumber, outRed, outGreen, outBlue);

    // All outputs should be valid unit vectors
    EXPECT_NEAR(1.0f, glm::length(outRed), 0.01f);
    EXPECT_NEAR(1.0f, glm::length(outGreen), 0.01f);
    EXPECT_NEAR(1.0f, glm::length(outBlue), 0.01f);

    // Red should bend less than blue
    float redAngle = std::acos(glm::dot(-outRed, normal));
    float blueAngle = std::acos(glm::dot(-outBlue, normal));
    EXPECT_LE(redAngle, blueAngle + 0.01f);
}

TEST_F(SpectralRendererTest, ChromaticDispersion_NormalIncidence) {
    glm::vec3 incident(0.0f, -1.0f, 0.0f);
    glm::vec3 normal(0.0f, 1.0f, 0.0f);

    glm::vec3 outRed, outGreen, outBlue;
    ChromaticDispersion::CalculateRGB(incident, normal, 1.5f, 30.0f, outRed, outGreen, outBlue);

    // At normal incidence, all should be same as incident
    EXPECT_VEC3_NEAR(incident, outRed, 0.01f);
    EXPECT_VEC3_NEAR(incident, outGreen, 0.01f);
    EXPECT_VEC3_NEAR(incident, outBlue, 0.01f);
}

TEST_F(SpectralRendererTest, ChromaticAberrationCenter) {
    glm::vec3 offset = ChromaticDispersion::GetChromaticAberration(glm::vec2(0.0f), 1.5f, 30.0f);

    // At center, offset should be zero or near-zero
    EXPECT_NEAR(0.0f, offset.r, 0.01f);
    EXPECT_NEAR(0.0f, offset.g, 0.01f);
    EXPECT_NEAR(0.0f, offset.b, 0.01f);
}

TEST_F(SpectralRendererTest, ChromaticAberrationRadial) {
    glm::vec2 centerPos(0.0f, 0.0f);
    glm::vec2 edgePos(1.0f, 0.0f);

    glm::vec3 centerOffset = ChromaticDispersion::GetChromaticAberration(centerPos, 1.5f, 30.0f);
    glm::vec3 edgeOffset = ChromaticDispersion::GetChromaticAberration(edgePos, 1.5f, 30.0f);

    // Edge should have more aberration than center
    EXPECT_GT(glm::length(glm::vec3(edgeOffset)), glm::length(glm::vec3(centerOffset)));
}

TEST_F(SpectralRendererTest, ChromaticAberrationRGBOrder) {
    glm::vec3 offset = ChromaticDispersion::GetChromaticAberration(glm::vec2(1.0f, 0.0f), 1.5f, 30.0f);

    // Red bends less, blue bends more
    // So: redOffset < greenOffset < blueOffset
    EXPECT_LE(offset.r, offset.g);
    EXPECT_LE(offset.g, offset.b);
}

TEST_F(SpectralRendererTest, Rainbow) {
    glm::vec3 incident = glm::normalize(glm::vec3(0.5f, -1.0f, 0.0f));
    glm::vec3 normal(0.0f, 1.0f, 0.0f);

    glm::vec3 refractedRed = ChromaticDispersion::Rainbow(incident, normal, 1.33f, WAVELENGTH_RED);
    glm::vec3 refractedBlue = ChromaticDispersion::Rainbow(incident, normal, 1.33f, WAVELENGTH_BLUE);

    // Different wavelengths should produce different refraction
    EXPECT_FALSE(Vec3Equal(refractedRed, refractedBlue, 0.01f));
}

// =============================================================================
// Edge Cases and Numerical Stability Tests
// =============================================================================

TEST_F(SpectralRendererTest, WavelengthToRGBNaN) {
    glm::vec3 rgb = SpectralRenderer::WavelengthToRGB(std::numeric_limits<float>::quiet_NaN());

    // Should handle NaN gracefully
    // Implementation may return black or clamp
}

TEST_F(SpectralRendererTest, WavelengthToRGBInfinity) {
    glm::vec3 rgbPosInf = SpectralRenderer::WavelengthToRGB(std::numeric_limits<float>::infinity());
    glm::vec3 rgbNegInf = SpectralRenderer::WavelengthToRGB(-std::numeric_limits<float>::infinity());

    // Should handle infinity gracefully (return black or clamp)
}

TEST_F(SpectralRendererTest, FresnelEdgeCases) {
    // cos > 1 (numerically impossible but should handle)
    float reflectance = SpectralRenderer::FresnelSpectral(1.1f, 1.5f);
    EXPECT_GE(reflectance, 0.0f);
    EXPECT_LE(reflectance, 1.0f);

    // cos < 0
    reflectance = SpectralRenderer::FresnelSpectral(-0.5f, 1.5f);
    EXPECT_GE(reflectance, 0.0f);
    EXPECT_LE(reflectance, 1.0f);
}

TEST_F(SpectralRendererTest, DispersionZeroAbbe) {
    // Abbe = 0 would cause division by zero
    float ior = SpectralRenderer::GetDispersedIOR(1.5f, 0.001f, WAVELENGTH_GREEN);

    // Should handle gracefully (may return extreme value)
    EXPECT_FALSE(std::isnan(ior));
    EXPECT_FALSE(std::isinf(ior));
}

TEST_F(SpectralRendererTest, RefractZeroLengthNormal) {
    glm::vec3 incident = glm::normalize(glm::vec3(0.5f, -1.0f, 0.0f));
    glm::vec3 zeroNormal(0.0f, 0.0f, 0.0f);

    // Should handle gracefully
    glm::vec3 result = SpectralRenderer::RefractSpectral(incident, zeroNormal, 1.5f, WAVELENGTH_GREEN);

    // Implementation-dependent but should not crash
}

// =============================================================================
// Performance Benchmarks
// =============================================================================

class SpectralRendererBenchmark : public SpectralRendererTest {
protected:
    static constexpr int BENCHMARK_ITERATIONS = 100000;
};

TEST_F(SpectralRendererBenchmark, WavelengthToRGBPerformance) {
    ScopedTimer timer("WavelengthToRGB");

    volatile glm::vec3 result;
    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        float wavelength = 380.0f + (i % 400);
        result = SpectralRenderer::WavelengthToRGB(wavelength);
    }

    double avgTimeNs = (timer.ElapsedMicroseconds() * 1000.0) / BENCHMARK_ITERATIONS;
    std::cout << "Average WavelengthToRGB time: " << avgTimeNs << " ns" << std::endl;

    EXPECT_LT(avgTimeNs, 1000.0); // Less than 1 microsecond
}

TEST_F(SpectralRendererBenchmark, FresnelPerformance) {
    ScopedTimer timer("FresnelSpectral");

    volatile float result;
    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        float cosTheta = (i % 100) / 100.0f;
        result = SpectralRenderer::FresnelSpectral(cosTheta, 1.5f);
    }

    double avgTimeNs = (timer.ElapsedMicroseconds() * 1000.0) / BENCHMARK_ITERATIONS;
    std::cout << "Average FresnelSpectral time: " << avgTimeNs << " ns" << std::endl;

    EXPECT_LT(avgTimeNs, 500.0);
}

TEST_F(SpectralRendererBenchmark, DispersionPerformance) {
    ScopedTimer timer("GetDispersedIOR");

    volatile float result;
    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        float wavelength = 380.0f + (i % 400);
        result = SpectralRenderer::GetDispersedIOR(1.5f, 30.0f, wavelength);
    }

    double avgTimeNs = (timer.ElapsedMicroseconds() * 1000.0) / BENCHMARK_ITERATIONS;
    std::cout << "Average GetDispersedIOR time: " << avgTimeNs << " ns" << std::endl;

    EXPECT_LT(avgTimeNs, 500.0);
}

TEST_F(SpectralRendererBenchmark, RefractPerformance) {
    ScopedTimer timer("RefractSpectral");

    glm::vec3 incident = glm::normalize(glm::vec3(0.3f, -1.0f, 0.0f));
    glm::vec3 normal(0.0f, 1.0f, 0.0f);

    volatile glm::vec3 result;
    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        float wavelength = 380.0f + (i % 400);
        result = SpectralRenderer::RefractSpectral(incident, normal, 1.5f, wavelength);
    }

    double avgTimeNs = (timer.ElapsedMicroseconds() * 1000.0) / BENCHMARK_ITERATIONS;
    std::cout << "Average RefractSpectral time: " << avgTimeNs << " ns" << std::endl;

    EXPECT_LT(avgTimeNs, 2000.0);
}

TEST_F(SpectralRendererBenchmark, ChromaticDispersionRGBPerformance) {
    ScopedTimer timer("ChromaticDispersion::CalculateRGB");

    glm::vec3 incident = glm::normalize(glm::vec3(0.3f, -1.0f, 0.0f));
    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    glm::vec3 outRed, outGreen, outBlue;

    for (int i = 0; i < BENCHMARK_ITERATIONS / 100; ++i) {
        ChromaticDispersion::CalculateRGB(incident, normal, 1.5f, 30.0f, outRed, outGreen, outBlue);
    }

    double avgTimeNs = (timer.ElapsedMicroseconds() * 1000.0) / (BENCHMARK_ITERATIONS / 100);
    std::cout << "Average CalculateRGB time: " << avgTimeNs << " ns" << std::endl;

    EXPECT_LT(avgTimeNs, 5000.0);
}

// =============================================================================
// Property-Based Tests
// =============================================================================

TEST_F(SpectralRendererTest, FresnelSymmetry) {
    // Fresnel should give same result for equivalent angles
    RandomGenerator rng(42);
    FloatGenerator cosGen(0.0f, 1.0f);
    FloatGenerator iorGen(1.0f, 3.0f);

    for (int i = 0; i < 100; ++i) {
        float cosTheta = cosGen.Generate(rng);
        float ior = iorGen.Generate(rng);

        float reflectance = SpectralRenderer::FresnelSpectral(cosTheta, ior);

        // Reflectance should be deterministic
        float reflectance2 = SpectralRenderer::FresnelSpectral(cosTheta, ior);
        EXPECT_FLOAT_EQ(reflectance, reflectance2);
    }
}

TEST_F(SpectralRendererTest, DispersionOrdering) {
    RandomGenerator rng(42);
    FloatGenerator iorGen(1.3f, 2.0f);
    FloatGenerator abbeGen(20.0f, 80.0f);

    for (int i = 0; i < 100; ++i) {
        float baseIOR = iorGen.Generate(rng);
        float abbeNumber = abbeGen.Generate(rng);

        float iorBlue = SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, WAVELENGTH_BLUE);
        float iorRed = SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, WAVELENGTH_RED);

        // Blue should always have higher or equal IOR than red
        EXPECT_GE(iorBlue, iorRed - 0.001f);
    }
}

TEST_F(SpectralRendererTest, WavelengthRoundTrip) {
    // Sample wavelength and convert to RGB should give a non-zero color
    for (float u = 0.1f; u <= 0.9f; u += 0.1f) {
        float wavelength = renderer->SampleWavelength(u);
        glm::vec3 rgb = SpectralRenderer::WavelengthToRGB(wavelength);

        float luminance = rgb.r * 0.2126f + rgb.g * 0.7152f + rgb.b * 0.0722f;
        EXPECT_GT(luminance, 0.0f);
    }
}
