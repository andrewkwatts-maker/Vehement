#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

/**
 * @file SphericalCoordinates.hpp
 * @brief Coordinate system utilities for spherical worlds
 *
 * Provides conversion between Cartesian (XYZ) and spherical (lat/long) coordinates
 * for representing worlds as spheres (e.g., planets).
 */

/**
 * @brief Represents a spherical world with a radius and center point
 */
struct SphericalWorld {
    float radius = 6371.0f;           // Default to Earth radius in km
    glm::vec3 center = glm::vec3(0.0f); // World center in 3D space

    SphericalWorld() = default;
    SphericalWorld(float r, const glm::vec3& c = glm::vec3(0.0f))
        : radius(r), center(c) {}
};

/**
 * @brief Convert latitude/longitude to 3D Cartesian coordinates
 *
 * @param lat Latitude in degrees (-90 to +90, where +90 is north pole)
 * @param lon Longitude in degrees (-180 to +180, where 0 is prime meridian)
 * @param altitude Altitude above surface in world units (same as radius)
 * @param radius Radius of the spherical world
 * @return 3D position in Cartesian coordinates (Y-up convention)
 *
 * @note Uses Y-up coordinate system:
 *       - Y axis points to north pole (lat = +90)
 *       - X axis points to (lat=0, lon=+90)
 *       - Z axis points to (lat=0, lon=0)
 */
inline glm::vec3 LatLongToXYZ(float lat, float lon, float altitude, float radius) {
    // Convert degrees to radians
    float latRad = glm::radians(lat);
    float lonRad = glm::radians(lon);

    // Calculate distance from center (radius + altitude)
    float r = radius + altitude;

    // Convert spherical to Cartesian (Y-up)
    // x = r * cos(lat) * sin(lon)
    // y = r * sin(lat)
    // z = r * cos(lat) * cos(lon)
    float cosLat = std::cos(latRad);
    float sinLat = std::sin(latRad);
    float cosLon = std::cos(lonRad);
    float sinLon = std::sin(lonRad);

    return glm::vec3(
        r * cosLat * sinLon,  // X
        r * sinLat,           // Y (up)
        r * cosLat * cosLon   // Z
    );
}

/**
 * @brief Convert latitude/longitude to 3D Cartesian coordinates (SphericalWorld version)
 *
 * @param lat Latitude in degrees (-90 to +90)
 * @param lon Longitude in degrees (-180 to +180)
 * @param altitude Altitude above surface
 * @param world SphericalWorld defining the sphere
 * @return 3D position in world space
 */
inline glm::vec3 LatLongToXYZ(float lat, float lon, float altitude, const SphericalWorld& world) {
    return world.center + LatLongToXYZ(lat, lon, altitude, world.radius);
}

/**
 * @brief Convert 3D Cartesian coordinates to latitude/longitude
 *
 * @param xyz 3D position in Cartesian coordinates
 * @param radius Radius of the spherical world
 * @return vec2 containing (latitude, longitude) in degrees
 *
 * @note Returns:
 *       - x component: latitude in degrees (-90 to +90)
 *       - y component: longitude in degrees (-180 to +180)
 */
inline glm::vec2 XYZToLatLong(const glm::vec3& xyz, float radius) {
    // Calculate distance from origin
    float distance = glm::length(xyz);

    // Handle degenerate case (at center)
    if (distance < 0.0001f) {
        return glm::vec2(0.0f, 0.0f);
    }

    // Normalize to unit sphere
    glm::vec3 norm = xyz / distance;

    // Calculate latitude from Y component
    // lat = asin(y)
    float lat = std::asin(glm::clamp(norm.y, -1.0f, 1.0f));

    // Calculate longitude from X and Z components
    // lon = atan2(x, z)
    float lon = std::atan2(norm.x, norm.z);

    // Convert to degrees
    return glm::vec2(
        glm::degrees(lat),
        glm::degrees(lon)
    );
}

/**
 * @brief Convert 3D Cartesian coordinates to latitude/longitude (SphericalWorld version)
 *
 * @param xyz 3D position in world space
 * @param world SphericalWorld defining the sphere
 * @return vec2 containing (latitude, longitude) in degrees
 */
inline glm::vec2 XYZToLatLong(const glm::vec3& xyz, const SphericalWorld& world) {
    return XYZToLatLong(xyz - world.center, world.radius);
}

/**
 * @brief Get altitude (height above surface) from 3D position
 *
 * @param xyz 3D position in Cartesian coordinates
 * @param center Center of the spherical world
 * @param radius Radius of the spherical world (at surface level)
 * @return Altitude above surface (negative if below surface)
 */
inline float GetAltitude(const glm::vec3& xyz, const glm::vec3& center, float radius) {
    float distance = glm::length(xyz - center);
    return distance - radius;
}

/**
 * @brief Get altitude from 3D position (SphericalWorld version)
 *
 * @param xyz 3D position in world space
 * @param world SphericalWorld defining the sphere
 * @return Altitude above surface
 */
inline float GetAltitude(const glm::vec3& xyz, const SphericalWorld& world) {
    return GetAltitude(xyz, world.center, world.radius);
}

/**
 * @brief Get the surface normal at a given 3D position on a sphere
 *
 * @param xyz 3D position in world space
 * @param center Center of the spherical world
 * @return Normalized surface normal pointing outward
 */
inline glm::vec3 GetSurfaceNormal(const glm::vec3& xyz, const glm::vec3& center) {
    glm::vec3 offset = xyz - center;
    float len = glm::length(offset);
    if (len < 0.0001f) {
        return glm::vec3(0.0f, 1.0f, 0.0f); // Default to up
    }
    return offset / len;
}

/**
 * @brief Get the surface normal at a given 3D position (SphericalWorld version)
 *
 * @param xyz 3D position in world space
 * @param world SphericalWorld defining the sphere
 * @return Normalized surface normal pointing outward
 */
inline glm::vec3 GetSurfaceNormal(const glm::vec3& xyz, const SphericalWorld& world) {
    return GetSurfaceNormal(xyz, world.center);
}

/**
 * @brief Calculate great circle distance between two lat/long positions
 *
 * Uses the Haversine formula to calculate the shortest distance between two
 * points on a sphere.
 *
 * @param lat1 Latitude of first point in degrees
 * @param lon1 Longitude of first point in degrees
 * @param lat2 Latitude of second point in degrees
 * @param lon2 Longitude of second point in degrees
 * @param radius Radius of the spherical world
 * @return Distance along the surface in world units
 */
inline float GreatCircleDistance(float lat1, float lon1, float lat2, float lon2, float radius) {
    // Convert to radians
    float lat1Rad = glm::radians(lat1);
    float lon1Rad = glm::radians(lon1);
    float lat2Rad = glm::radians(lat2);
    float lon2Rad = glm::radians(lon2);

    // Haversine formula
    float dLat = lat2Rad - lat1Rad;
    float dLon = lon2Rad - lon1Rad;

    float a = std::sin(dLat / 2.0f) * std::sin(dLat / 2.0f) +
              std::cos(lat1Rad) * std::cos(lat2Rad) *
              std::sin(dLon / 2.0f) * std::sin(dLon / 2.0f);

    float c = 2.0f * std::atan2(std::sqrt(a), std::sqrt(1.0f - a));

    return radius * c;
}

/**
 * @brief Calculate bearing from one lat/long position to another
 *
 * @param lat1 Latitude of first point in degrees
 * @param lon1 Longitude of first point in degrees
 * @param lat2 Latitude of second point in degrees
 * @param lon2 Longitude of second point in degrees
 * @return Bearing in degrees (0 = north, 90 = east, 180 = south, 270 = west)
 */
inline float CalculateBearing(float lat1, float lon1, float lat2, float lon2) {
    // Convert to radians
    float lat1Rad = glm::radians(lat1);
    float lon1Rad = glm::radians(lon1);
    float lat2Rad = glm::radians(lat2);
    float lon2Rad = glm::radians(lon2);

    float dLon = lon2Rad - lon1Rad;

    float y = std::sin(dLon) * std::cos(lat2Rad);
    float x = std::cos(lat1Rad) * std::sin(lat2Rad) -
              std::sin(lat1Rad) * std::cos(lat2Rad) * std::cos(dLon);

    float bearing = std::atan2(y, x);

    // Convert to degrees and normalize to 0-360
    float bearingDeg = glm::degrees(bearing);
    return std::fmod(bearingDeg + 360.0f, 360.0f);
}

/**
 * @brief Move a lat/long position by a distance and bearing
 *
 * @param lat Starting latitude in degrees
 * @param lon Starting longitude in degrees
 * @param distance Distance to move in world units
 * @param bearing Direction to move in degrees (0 = north, 90 = east)
 * @param radius Radius of the spherical world
 * @return New lat/long position as vec2
 */
inline glm::vec2 MoveLatLong(float lat, float lon, float distance, float bearing, float radius) {
    // Convert to radians
    float latRad = glm::radians(lat);
    float lonRad = glm::radians(lon);
    float bearingRad = glm::radians(bearing);

    // Angular distance
    float angularDist = distance / radius;

    // Calculate new position
    float newLatRad = std::asin(
        std::sin(latRad) * std::cos(angularDist) +
        std::cos(latRad) * std::sin(angularDist) * std::cos(bearingRad)
    );

    float newLonRad = lonRad + std::atan2(
        std::sin(bearingRad) * std::sin(angularDist) * std::cos(latRad),
        std::cos(angularDist) - std::sin(latRad) * std::sin(newLatRad)
    );

    // Convert back to degrees
    return glm::vec2(
        glm::degrees(newLatRad),
        glm::degrees(newLonRad)
    );
}
