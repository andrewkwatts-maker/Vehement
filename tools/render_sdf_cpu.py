#!/usr/bin/env python3
"""
CPU-based SDF Renderer for Asset Icons

Renders SDF models from JSON assets using Python numpy CPU raymarching.
This is slower than GPU but works immediately without compilation issues.
"""

import json
import numpy as np
from PIL import Image
import sys
from pathlib import Path
import argparse

def load_asset(json_path):
    """Load asset JSON file"""
    with open(json_path, 'r', encoding='utf-8') as f:
        return json.load(f)

def normalize(v):
    """Normalize a vector"""
    length = np.linalg.norm(v)
    if length < 1e-10:
        return v
    return v / length

def sdf_sphere(p, radius):
    """SDF for sphere"""
    return np.linalg.norm(p) - radius

def sdf_box(p, size):
    """SDF for box"""
    q = np.abs(p) - np.array(size)
    return np.linalg.norm(np.maximum(q, 0.0)) + min(max(q[0], max(q[1], q[2])), 0.0)

def sdf_rounded_box(p, size, radius):
    """SDF for rounded box"""
    q = np.abs(p) - np.array(size)
    return np.linalg.norm(np.maximum(q, 0.0)) + min(max(q[0], max(q[1], q[2])), 0.0) - radius

def sdf_ellipsoid(p, radii):
    """SDF for ellipsoid"""
    k0 = np.linalg.norm(p / radii)
    k1 = np.linalg.norm(p / (radii * radii))
    return k0 * (k0 - 1.0) / k1 if k1 > 1e-10 else -1.0

def sdf_capsule(p, radius, height):
    """SDF for capsule (cylinder with rounded ends)"""
    p_y = p[1]
    p_xz = np.array([p[0], p[2]])
    h = height / 2.0
    p_y = p_y - np.clip(p_y, -h, h)
    return np.sqrt(p_y * p_y + np.dot(p_xz, p_xz)) - radius

def sdf_torus(p, major_radius, minor_radius):
    """SDF for torus"""
    q = np.array([np.linalg.norm([p[0], p[2]]) - major_radius, p[1]])
    return np.linalg.norm(q) - minor_radius

def sdf_cylinder(p, radius, height):
    """SDF for cylinder"""
    d = np.array([np.linalg.norm([p[0], p[2]]) - radius, abs(p[1]) - height / 2.0])
    return min(max(d[0], d[1]), 0.0) + np.linalg.norm(np.maximum(d, 0.0))

def evaluate_primitive(p, prim, primitives):
    """Evaluate SDF for a single primitive"""
    # Transform point to local space
    pos = np.array(prim['transform']['position'])
    p_local = p - pos

    # Get parameters
    params = prim.get('params', {})
    prim_type = prim['type']

    # Evaluate based on type
    if prim_type == 'Sphere':
        d = sdf_sphere(p_local, params.get('radius', 0.5))
    elif prim_type == 'Box':
        size = params.get('size', [0.5, 0.5, 0.5])
        d = sdf_box(p_local, size)
    elif prim_type == 'RoundedBox':
        size = params.get('size', [0.5, 0.5, 0.5])
        radius = params.get('radius', 0.1)
        d = sdf_rounded_box(p_local, size, radius)
    elif prim_type == 'Ellipsoid':
        radii = np.array(params.get('radii', [0.5, 0.5, 0.5]))
        d = sdf_ellipsoid(p_local, radii)
    elif prim_type == 'Capsule':
        radius = params.get('radius', 0.2)
        height = params.get('height', 1.0)
        d = sdf_capsule(p_local, radius, height)
    elif prim_type == 'Torus':
        major_r = params.get('majorRadius', 0.5)
        minor_r = params.get('minorRadius', 0.1)
        d = sdf_torus(p_local, major_r, minor_r)
    elif prim_type == 'Cylinder':
        radius = params.get('radius', 0.3)
        height = params.get('height', 1.0)
        d = sdf_cylinder(p_local, radius, height)
    else:
        d = float('inf')

    return d, prim.get('material', {})

def scene_sdf(p, primitives):
    """Evaluate SDF for entire scene (union of all primitives)"""
    min_dist = float('inf')
    closest_material = None

    for prim in primitives:
        d, material = evaluate_primitive(p, prim, primitives)
        if d < min_dist:
            min_dist = d
            closest_material = material

    return min_dist, closest_material

def raymarch(ray_origin, ray_dir, primitives, max_steps=64, max_dist=20.0):
    """Raymarch to find intersection"""
    t = 0.0
    for _ in range(max_steps):
        p = ray_origin + t * ray_dir
        d, material = scene_sdf(p, primitives)

        if d < 0.001:
            return t, material, p

        t += d
        if t > max_dist:
            break

    return None, None, None

def calculate_normal(p, primitives, eps=0.001):
    """Calculate surface normal at point p"""
    d, _ = scene_sdf(p, primitives)
    dx, _ = scene_sdf(p + np.array([eps, 0, 0]), primitives)
    dy, _ = scene_sdf(p + np.array([0, eps, 0]), primitives)
    dz, _ = scene_sdf(p + np.array([0, 0, eps]), primitives)

    return normalize(np.array([dx - d, dy - d, dz - d]))

def render_pixel(x, y, width, height, camera_pos, camera_target, primitives, light_dir):
    """Render a single pixel"""
    # Convert to NDC
    ndc_x = (2.0 * x / width - 1.0) * (width / height)
    ndc_y = -(2.0 * y / height - 1.0)

    # Camera setup
    forward = normalize(camera_target - camera_pos)
    right = normalize(np.cross(forward, np.array([0, 1, 0])))
    up = normalize(np.cross(right, forward))

    # Ray direction
    fov = 35.0 * np.pi / 180.0
    ray_dir = normalize(
        forward +
        ndc_x * right * np.tan(fov / 2.0) +
        ndc_y * up * np.tan(fov / 2.0)
    )

    # Raymarch
    t, material, hit_point = raymarch(camera_pos, ray_dir, primitives)

    if t is None:
        # Background - transparent
        return [0, 0, 0, 0]

    # Calculate lighting
    normal = calculate_normal(hit_point, primitives)
    light_intensity = max(0.0, np.dot(normal, -light_dir))

    # Get material color
    albedo = material.get('albedo', [0.7, 0.7, 0.7])
    emissive = material.get('emissive', [0, 0, 0])
    emissive_strength = material.get('emissiveStrength', 0.0)

    # Combine lighting
    ambient = 0.3
    diffuse = 0.7 * light_intensity

    color = np.array(albedo) * (ambient + diffuse) + np.array(emissive) * emissive_strength
    color = np.clip(color, 0.0, 1.0)

    return [int(color[0] * 255), int(color[1] * 255), int(color[2] * 255), 255]

def render_asset(asset_data, width=512, height=512):
    """Render asset to image"""
    sdf_model = asset_data.get('sdfModel', {})
    primitives = sdf_model.get('primitives', [])

    if not primitives:
        print("No primitives found in asset")
        return None

    # Camera setup from asset or defaults
    camera_data = sdf_model.get('camera', {})
    camera_pos = np.array(camera_data.get('position', [3.0, 1.8, 4.0]))
    camera_target = np.array(camera_data.get('lookAt', [0.0, 1.2, 0.0]))

    # Lighting setup
    lighting_data = sdf_model.get('lighting', {})
    directional = lighting_data.get('directional', {})
    light_dir = normalize(np.array(directional.get('direction', [-0.45, -1.2, -0.65])))

    # Create image
    img = Image.new('RGBA', (width, height))
    pixels = img.load()

    print(f"Rendering {width}x{height} image with {len(primitives)} primitives...")

    # Render each pixel
    for y in range(height):
        if y % 50 == 0:
            print(f"  Progress: {y}/{height} rows ({100*y//height}%)")
        for x in range(width):
            color = render_pixel(x, y, width, height, camera_pos, camera_target, primitives, light_dir)
            pixels[x, y] = tuple(color)

    print("  Progress: 100%")
    return img

def main():
    parser = argparse.ArgumentParser(description='Render SDF asset to PNG')
    parser.add_argument('asset', help='Path to asset JSON file')
    parser.add_argument('output', help='Output PNG file path')
    parser.add_argument('--width', type=int, default=512, help='Image width (default: 512)')
    parser.add_argument('--height', type=int, default=512, help='Image height (default: 512)')

    args = parser.parse_args()

    print("=" * 70)
    print("SDF CPU Renderer")
    print("=" * 70)
    print(f"Asset:  {args.asset}")
    print(f"Output: {args.output}")
    print(f"Size:   {args.width}x{args.height}")
    print("=" * 70)

    # Load asset
    print("\nLoading asset...")
    try:
        asset_data = load_asset(args.asset)
        print(f"Loaded: {asset_data.get('name', 'Unknown')}")
    except Exception as e:
        print(f"ERROR: Failed to load asset: {e}")
        return 1

    # Render
    print("\nRendering...")
    try:
        img = render_asset(asset_data, args.width, args.height)
        if img is None:
            return 1
    except Exception as e:
        print(f"ERROR: Rendering failed: {e}")
        import traceback
        traceback.print_exc()
        return 1

    # Save
    print("\nSaving image...")
    try:
        output_path = Path(args.output)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        img.save(output_path)
        print(f"SUCCESS: Saved to {args.output}")
    except Exception as e:
        print(f"ERROR: Failed to save image: {e}")
        return 1

    print("=" * 70)
    return 0

if __name__ == '__main__':
    sys.exit(main())
