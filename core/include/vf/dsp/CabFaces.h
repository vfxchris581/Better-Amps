// Viridian Forge — DSP core (JUCE-independent)
//
// Geometry for the six-face cabinet view: the cabinet is a unit cube (x right, y up,
// z towards the front). Each face has a 2D (u, v) space in [-0.5, 0.5]^2, a 3D point
// mapping, a yaw/pitch orbit with perspective projection, and exact hit-testing that
// finds which visible face is under the mouse and where on it.
#pragma once

#include <array>
#include <cmath>

namespace vf::dsp::cabfaces
{

struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct Pt2 { float x = 0.0f, y = 0.0f; };

/** Face order matches MicFace: Front, Back, Top, Bottom, Left, Right. */
enum class Face { Front = 0, Back, Top, Bottom, Left, Right, count };

/** Outward normal and the two in-face axes (u along the first, v along the second). */
struct FaceFrame { Vec3 normal, uAxis, vAxis; };

inline FaceFrame frameOf (Face f) noexcept
{
    switch (f)
    {
        case Face::Front:  return { { 0, 0, 1 },  { 1, 0, 0 }, { 0, 1, 0 } };
        case Face::Back:   return { { 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 } };
        case Face::Top:    return { { 0, 1, 0 },  { 1, 0, 0 }, { 0, 0, -1 } };
        case Face::Bottom: return { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } };
        case Face::Left:   return { { -1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } };
        case Face::Right:  return { { 1, 0, 0 },  { 0, 0, 1 }, { 0, 1, 0 } };
        default:           return { { 0, 0, 1 },  { 1, 0, 0 }, { 0, 1, 0 } };
    }
}

/** 3D point on a face for face coordinates u, v in [-0.5, 0.5]. */
inline Vec3 pointOnFace (Face f, float u, float v) noexcept
{
    const auto fr = frameOf (f);
    return { 0.5f * fr.normal.x + u * fr.uAxis.x + v * fr.vAxis.x,
             0.5f * fr.normal.y + u * fr.uAxis.y + v * fr.vAxis.y,
             0.5f * fr.normal.z + u * fr.uAxis.z + v * fr.vAxis.z };
}

/** Orbit: yaw about the vertical axis, then pitch about the horizontal axis (radians). */
struct View { float yaw = 0.0f, pitch = 0.0f, scale = 200.0f, distance = 4.0f; };

inline Vec3 rotate (Vec3 p, const View& v) noexcept
{
    const float cy = std::cos (v.yaw), sy = std::sin (v.yaw);
    Vec3 a { cy * p.x + sy * p.z, p.y, -sy * p.x + cy * p.z };
    const float cp = std::cos (v.pitch), sp = std::sin (v.pitch);
    return { a.x, cp * a.y - sp * a.z, sp * a.y + cp * a.z };
}

/** Screen position (origin at the cube centre, y down) and depth (larger = nearer the viewer). */
inline Pt2 project (Vec3 p, const View& v, float& depth) noexcept
{
    const Vec3 r = rotate (p, v);
    const float k = v.distance / (v.distance - r.z);
    depth = r.z;
    return { r.x * v.scale * k, -r.y * v.scale * k };
}

/** A face points at the viewer when its rotated normal has a positive z component. */
inline bool faceVisible (Face f, const View& v) noexcept
{
    return rotate (frameOf (f).normal, v).z > 0.0f;
}

/** The four screen corners of a face: (-,-), (+,-), (+,+), (-,+) in face coordinates. */
inline std::array<Pt2, 4> faceQuad (Face f, const View& v) noexcept
{
    const float c[4][2] { { -0.5f, -0.5f }, { 0.5f, -0.5f }, { 0.5f, 0.5f }, { -0.5f, 0.5f } };
    std::array<Pt2, 4> out {};
    float depth = 0.0f;
    for (int i = 0; i < 4; ++i)
        out[static_cast<size_t> (i)] = project (pointOnFace (f, c[i][0], c[i][1]), v, depth);
    return out;
}

/** Inverse of rotate(): from the rotated frame back to cube (world) coordinates. */
inline Vec3 unrotate (Vec3 r, const View& v) noexcept
{
    const float cp = std::cos (v.pitch), sp = std::sin (v.pitch);
    const Vec3 a { r.x, cp * r.y + sp * r.z, -sp * r.y + cp * r.z };
    const float cy = std::cos (v.yaw), sy = std::sin (v.yaw);
    return { cy * a.x - sy * a.z, a.y, sy * a.x + cy * a.z };
}

/** Hit test: the visible face under a screen point (origin at the cube centre), with its
    face coordinates u, v in [-0.5, 0.5]. Exact for the perspective projection: the view ray
    through the point is intersected with each face plane. The nearest face wins. */
inline bool pickFace (Pt2 p, const View& view, Face& face, float& u, float& v) noexcept
{
    // Screen -> rotated frame: X = a (D - Z), Y = b (D - Z) with the ray parameter Z.
    const float a = p.x / (view.scale * view.distance);
    const float b = -p.y / (view.scale * view.distance);
    const float D = view.distance;
    bool found = false;
    float bestZ = -1e9f;
    for (int i = 0; i < static_cast<int> (Face::count); ++i)
    {
        const auto f = static_cast<Face> (i);
        if (! faceVisible (f, view)) continue;
        const auto fr = frameOf (f);
        // World point on the ray as an affine function of Z: P(Z) = P0 + Z * Q.
        const Vec3 P0 = unrotate ({ a * D, b * D, 0.0f }, view);
        const Vec3 Q = unrotate ({ -a, -b, 1.0f }, view);
        const float qn = Q.x * fr.normal.x + Q.y * fr.normal.y + Q.z * fr.normal.z;
        if (std::fabs (qn) < 1e-9f) continue;
        const float p0n = P0.x * fr.normal.x + P0.y * fr.normal.y + P0.z * fr.normal.z;
        const float Z = (0.5f - p0n) / qn;
        if (D - Z <= 0.0f) continue;   // behind the camera
        const Vec3 w { P0.x + Z * Q.x, P0.y + Z * Q.y, P0.z + Z * Q.z };
        const Vec3 d { w.x - 0.5f * fr.normal.x, w.y - 0.5f * fr.normal.y, w.z - 0.5f * fr.normal.z };
        const float fu = d.x * fr.uAxis.x + d.y * fr.uAxis.y + d.z * fr.uAxis.z;
        const float fv = d.x * fr.vAxis.x + d.y * fr.vAxis.y + d.z * fr.vAxis.z;
        if (std::fabs (fu) > 0.5f || std::fabs (fv) > 0.5f) continue;
        if (! found || Z > bestZ)
        {
            found = true;
            bestZ = Z;
            face = f;
            u = fu;
            v = fv;
        }
    }
    return found;
}

} // namespace vf::dsp::cabfaces
