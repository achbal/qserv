/*
 * LSST Data Management System
 * Copyright 2013 LSST Corporation.
 *
 * This product includes software developed by the
 * LSST Project (http://www.lsst.org/).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the LSST License Statement and
 * the GNU General Public License along with this program.  If not,
 * see <http://www.lsstcorp.org/LegalNotices/>.
 */

#include "Geometry.h"

#include <stdexcept>

#include "boost/math/constants/constants.hpp"

#include "Constants.h"

using std::fabs;
using std::sin;
using std::cos;
using std::atan2;
using std::sqrt;
using std::min;
using std::max;
using std::pair;
using std::runtime_error;
using std::swap;
using std::vector;

using boost::math::constants::pi;


namespace lsst { namespace qserv { namespace admin { namespace dupr {

namespace {

//  HTM triangles are subdivided into 4 sub-triangles as follows :
//
//             v2
//              *
//             / \
//            /   \
//       sv1 *-----* sv0
//          / \   / \
//         /   \ /   \
//     v0 *-----*-----* v1
//             sv2
//
//   -  vertices are unit magnitude 3-vectors
//   -  edges are great circles on the unit sphere
//   -  vertices are stored in counter-clockwise order
//     (when viewed from outside the unit sphere in a
//     right handed coordinate system)
//   -  sv0 = (v1 + v2) / ||v1 + v2||, and likewise for sv1, sv2
//
//  Note that if the HTM triangle given by (v0,v1,v2) has index I, then:
//   -  sub triangle T0 = (v0,sv2,sv1) has index I*4
//   -  sub triangle T1 = (v1,sv0,sv2) has index I*4 + 1
//   -  sub triangle T2 = (v2,sv1,sv0) has index I*4 + 2
//   -  sub triangle T3 = (sv0,sv1,sv2) has index I*4 + 3
//
//  All HTM triangles are obtained via subdivision of 8 initial
//  triangles, defined from the following set of 6 vertices :
//   -  V0 = ( 0,  0,  1) north pole
//   -  V1 = ( 1,  0,  0)
//   -  V2 = ( 0,  1,  0)
//   -  V3 = (-1,  0,  0)
//   -  V4 = ( 0, -1,  0)
//   -  V5 = ( 0,  0, -1) south pole
//
//  The root triangles (corresponding to subdivision level 0) are :
//   -  S0 = (V1, V5, V2), HTM index = 8
//   -  S1 = (V2, V5, V3), HTM index = 9
//   -  S2 = (V3, V5, V4), HTM index = 10
//   -  S3 = (V4, V5, V1), HTM index = 11
//   -  N0 = (V1, V0, V4), HTM index = 12
//   -  N1 = (V4, V0, V3), HTM index = 13
//   -  N2 = (V3, V0, V2), HTM index = 14
//   -  N3 = (V2, V0, V1), HTM index = 15
//
//  'S' denotes a triangle in the southern hemisphere,
//  'N' denotes a triangle in the northern hemisphere.

// HTM root triangle numbers. Add 8 to obtain a level 0 HTM ID.
uint32_t const S0 = 0;
uint32_t const S1 = 1;
uint32_t const S2 = 2;
uint32_t const S3 = 3;
uint32_t const N0 = 4;
uint32_t const N1 = 5;
uint32_t const N2 = 6;
uint32_t const N3 = 7;

Vector3d const X(1.0, 0.0, 0.0);
Vector3d const Y(0.0, 1.0, 0.0);
Vector3d const Z(0.0, 0.0, 1.0);

Vector3d const NX(-1.0,  0.0,  0.0);
Vector3d const NY( 0.0, -1.0,  0.0);
Vector3d const NZ( 0.0,  0.0, -1.0);

// Vertex triplet for each HTM root triangle.
Vector3d const * const htmRootVert[24] = {
    &X,  &NZ, &Y,  // S0
    &Y,  &NZ, &NX, // S1
    &NX, &NZ, &NY, // S2
    &NY, &NZ, &X,  // S3
    &X,  &Z,  &NY, // N0
    &NY, &Z,  &NX, // N1
    &NX, &Z,  &Y,  // N2
    &Y,  &Z,  &X   // N3
};

// Edge normal triplet for each HTM root triangle.
Vector3d const * const htmRootEdge[24] = {
    &Y,  &X,  &NZ, // S0
    &NX, &Y,  &NZ, // S1
    &NY, &NX, &NZ, // S2
    &X,  &NY, &NZ, // S3
    &NY, &X,  &Z,  // N0
    &NX, &NY, &Z,  // N1
    &Y,  &NX, &Z,  // N2
    &X,  &Y,  &Z   // N3
};

// Return the number of the HTM root triangle containing v.
inline uint32_t rootNumFor(Vector3d const &v)
{
    if (v(2) < 0.0) {
        // S0, S1, S2, S3
        if (v(1) > 0.0) {
            return (v(0) > 0.0) ? S0 : S1;
        } else if (v(1) == 0.0) {
            return (v(0) >= 0.0) ? S0 : S2;
        } else {
            return (v(0) < 0.0) ? S2 : S3;
        }
    } else {
        // N0, N1, N2, N3
        if (v(1) > 0.0) {
            return (v(0) > 0.0) ? N3 : N2;
        } else if (v(1) == 0.0) {
            return (v(0) >= 0.0) ? N3 : N1;
        } else {
            return (v(0) < 0.0) ? N1 : N0;
        }
    }
}

} // unnamed namespace


double reduceRa(double ra) {
    ra = fmod(ra, 360.0);
    if (ra < 0.0) {
        ra += 360.0;
        if (ra == 360.0) {
            ra = 0.0;
        }
    }
    return ra;
}

double maxAlpha(double r, double centerDec) {
    if (r < 0.0 || r > 90.0) {
        throw runtime_error("Radius must lie in range [0, 90] deg.");
    }
    if (r == 0.0) {
        return 0.0;
    }
    double d = clampDec(centerDec);
    if (fabs(d) + r > 90.0 - 1/3600.0) {
        return 180.0;
    }
    r *= RAD_PER_DEG;
    d *= RAD_PER_DEG;
    double y = sin(r);
    double x = sqrt(fabs(cos(d - r) * cos(d + r)));
    return DEG_PER_RAD * fabs(atan(y / x));
}

uint32_t htmId(Vector3d const &v, int level) {
    if (level < 0 || level > HTM_MAX_LEVEL) {
        throw runtime_error("Invalid HTM subdivision level.");
    }
    uint32_t id = rootNumFor(v);
    if (level == 0) {
        return id + 8;
    }
    Vector3d v0(*htmRootVert[id*3]);
    Vector3d v1(*htmRootVert[id*3 + 1]);
    Vector3d v2(*htmRootVert[id*3 + 2]);
    id += 8;
    for (; level != 0; --level) {
        Vector3d const sv1 = (v2 + v0).normalized();
        Vector3d const sv2 = (v0 + v1).normalized();
        if (v.dot((sv1 + sv2).cross(sv1 - sv2)) >= 0) {
            v1 = sv2;
            v2 = sv1;
            id = id << 2;
            continue;
        }
        Vector3d const sv0 = (v1 + v2).normalized();
        if (v.dot((sv2 + sv0).cross(sv2 - sv0)) >= 0) {
            v0 = v1;
            v1 = sv0;
            v2 = sv2;
            id = (id << 2) + 1;
            continue;
        }
        if (v.dot((sv0 + sv1).cross(sv0 - sv1)) >= 0) {
            v0 = v2;
            v1 = sv1;
            v2 = sv0;
            id = (id << 2) + 2;
        } else {
            v0 = sv0;
            v1 = sv1;
            v2 = sv2;
            id = (id << 2) + 3;
        }
    }
    return id;
}

int htmLevel(uint32_t id) {
    if (id < 8) {
        return -1; // invalid ID
    }
    // Set x = 2**(i + 1) - 1, where i is the index of the MSB of id.
    uint32_t x = id;
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    // Compute the bit population count p of x - the HTM level is (p - 4)/2.
    // See http://en.wikipedia.org/wiki/Hamming_weight for details.
    uint32_t const m1 = 0x55555555;
    uint32_t const m2 = 0x33333333;
    uint32_t const m4 = 0x0f0f0f0f;
    uint32_t const h01 = 0x01010101;
    x -= (x >> 1) & m1;
    x = (x & m2) + ((x >> 2) & m2);
    x = (x + (x >> 4)) & m4;
    uint32_t level = ((x * h01) >> 24) - 4;
    // Check that level is even, in range and that the 4 MSBs
    // of id are between 8 and 15.
    if ((level & 1) != 0 || ((id >> level) & 0x8) == 0 || level > HTM_MAX_LEVEL*2) {
        return -1;
    }
    return static_cast<int>(level >> 1);
}

Vector3d const cartesian(pair<double, double> const &radec) {
    double ra = radec.first * RAD_PER_DEG;
    double dec = radec.second * RAD_PER_DEG;
    double sinRa = sin(ra);
    double cosRa = cos(ra);
    double sinDec = sin(dec);
    double cosDec = cos(dec);
    return Vector3d(cosRa * cosDec, sinRa * cosDec, sinDec);
}

pair<double, double> const spherical(Vector3d const &v) {
    pair<double, double> sc(0.0, 0.0);
    double d2 = v(0)*v(0) + v(1)*v(1);
    if (d2 != 0.0) {
        double ra = atan2(v(1), v(0)) * DEG_PER_RAD;
        if (ra < 0.0) {
            ra += 360.0;
            if (ra == 360.0) {
                ra = 0.0;
            }
        }
        sc.first = ra;
    }
    if (v(2) != 0.0) {
        sc.second = clampDec(atan2(v(2), sqrt(d2)) * DEG_PER_RAD);
    }
    return sc;
}

double angSep(Vector3d const & v0, Vector3d const & v1) {
    double cs = v0.dot(v1);
    Vector3d n = v0.cross(v1);
    double ss = n.norm();
    if (cs == 0.0 && ss == 0.0) {
        return 0.0;
    }
    return atan2(ss, cs);
}


// -- SphericalTriangle implementation ----

SphericalTriangle::SphericalTriangle(uint32_t id) : _m(), _mi() {
    int level = htmLevel(id);
    if (level < 0) {
        throw runtime_error("Invalid HTM ID.");
    }
    uint32_t r = (id >> (level * 2)) - 8;
    Vector3d v0(*htmRootVert[r*3]);
    Vector3d v1(*htmRootVert[r*3 + 1]);
    Vector3d v2(*htmRootVert[r*3 + 2]);
    for (--level; level >= 0; --level) {
        uint32_t child = (id >> (level * 2)) & 0x3;
        Vector3d const sv0 = (v1 + v2).normalized();
        Vector3d const sv1 = (v2 + v0).normalized();
        Vector3d const sv2 = (v0 + v1).normalized();
        switch (child) {
        case 0:
            v1 = sv2;
            v2 = sv1;
            break;
        case 1:
            v0 = v1;
            v1 = sv0;
            v2 = sv2;
            break;
        case 2:
            v0 = v2;
            v1 = sv1;
            v2 = sv0;
            break;
        case 3:
            v0 = sv0;
            v1 = sv1;
            v2 = sv2;
            break;
        }
    }
    // Set column vectors of _m to triangle vertices.
    _m.col(0) = v0;
    _m.col(1) = v1;
    _m.col(2) = v2;
    _mi = _m.inverse();
}

SphericalTriangle::SphericalTriangle(Vector3d const & v0,
                                     Vector3d const & v1,
                                     Vector3d const & v2) :
   _m(), _mi()
{
   _m.col(0) = v0;
   _m.col(1) = v1;
   _m.col(2) = v2;
   _mi = _m.inverse();
}

double SphericalTriangle::area() const {
    // The area of a spherical triangle is it's spherical excess (Girard's
    // theorem), i.e. the sum of the triangle's interior angles minus π.
    //
    // The interior angle at vertex v is π minus the turning angle of the
    // triangle boundary at v, and the turning angle is the angle between the
    // normals for the half planes that define the edges meeting at v.
    //
    // Numerically, this is not very accurate for small triangles.
    // An alternate formula is:
    //
    // tan(A/4) = √(tan(S/2) tan((S-a)/2) tan((S-b)/2) tan((S-c)/2))
    //
    // where a, b, c are the arc-lengths of the edges and S = a + b + c.
    Vector3d p01 = (vertex(1) + vertex(0)).cross(vertex(1) - vertex(0));
    Vector3d p12 = (vertex(2) + vertex(1)).cross(vertex(2) - vertex(1));
    Vector3d p20 = (vertex(0) + vertex(2)).cross(vertex(0) - vertex(2));
    return (2.0*pi<double>() -
            angSep(p20, p01) - angSep(p01, p12) - angSep(p12, p20));
}

// The area of intersection between a spherical box and a spherical triangle
// is computed as follows:
//
// 1. Intersect the triangle with the ra >= box.getRaMin() and
//    ra <= box.getRaMax() half-spaces. These half-spaces correspond to great
//    circles on the sphere, and the result is a spherical convex polygon so
//    long as the RA extent of the box is <= 180 deg.
//
// 2. Intersect the triangle with the z >= sin(box.getDecMin()) and
//    z <= sin(box.getDecMax()) half-spaces, which correspond to small circles.
//
// The resulting surface M has constant Gaussian curvature of 1 (because the
// unit sphere has curvature of 1 everywhere). For the cases under consideration
// (HTM triangles), the intersections from step 2.) cannot punch holes into the
// polygon from step 1.), so the Euler characteristic of M is 1.
//
// The Gauss-Bonnet theorem (http://en.wikipedia.org/wiki/Gauss-Bonnet_theorem)
// states that:
//
// ∫K dA + ∫k ds = 2πχ(M)
//
// Here K is the Gaussian curvature of M, dA is the area element of the
// surface, and the first integral is over M. The second integral is over
// ∂M, the boundary of M, where k is the geodesic curvature of ∂M and ds is
// the line element along ∂M. Since K = 1 and χ(M) = 1, ∫K dA is just the
// desired area A, and:
//
// A = 2π - ∫k ds
//
// ∂M is piecewise-smooth, so ∫k ds is the sum of the corresponding
// integrals along the smooth parts of the boundary (the edges), plus the sum
// of the angles αᵥ by which the smooth portions turn at the vertices (see
// below).
//
// Great circles have zero geodesic curvature, so the corresponding integrals
// vanish. A small circle u of radius sin(φᵤ) (where φᵤ is the opening angle
// of the small circle) has geodesic curvature k = cot(φᵤ). Parameterizing
// the small circle by the winding angle θ about it's center vector, one obtains
// ds = sin(φᵤ) dθ, so that ∫k ds is:
//
// cos(φᵤ) ∆θᵤ
//
// where ∆θᵤ is the change in winding angle necessary to take the first vertex
// of u to the second. This justifies the evaluation of ∫k ds at the vertices
// of ∂M: if one smooths a vertex v of ∂M by replacing it with a small
// circle tangent to the 2 incident edges and takes the limit as φ → 0, then
// cos(φ) ∆θ → αᵥ and the change in area induced by smoothing tends to 0.
//
// Therefore:
//
// A = 2π - Σ αᵥ - Σ cos(φᵤ) ∆θᵤ
//
// Girard's theorem is a trivial special case - a spherical triangle has no
// small circle edges, and there are 3 vertices with interior angles
// βᵥ = π - αᵥ.
//
// The formula is also easily generalized to surfaces with holes (each hole
// decreases χ(M) by 1) and does not require the surface to be convex.
//
// The last piece of the puzzle is the computation of αᵥ - for great circle
// edges meeting at v, this is simply the angle between the normal vectors
// of the corresponding half-space bounding planes. More generally, it is the
// angle between the vectors n₀ * v and n₁ * v, where * is the vector product,
// and n₀, n₁ are the plane normals of the half-spaces involved.

namespace {

// Intersect the input spherical convex polygon with the given plane.
size_t intersect(Vector3d const * inVe,  // input (vertex, edge) pair array
                 size_t numVerts,        // # of input vertices
                 Vector3d const & plane, // half-space to intersect with
                 Vector3d * outVe)       // output (vertex, edge) pair array
{
    assert(numVerts > 1 && inVe != 0 && outVe != 0);
    size_t i = 0, j = numVerts - 1, n = 0;
    bool inside = plane.dot(inVe[2*j]) >= 0.0;
    for (; i < numVerts; j = i, ++i) {
        if (plane.dot(inVe[2*i]) >= 0.0) {
            if (!inside) {
                // Edge crosses plane (outside to inside) - copy
                // intersection to output polygon.
                Vector3d edge = inVe[2*j + 1].normalized();
                outVe[2*n] = (edge + plane).cross(edge - plane).normalized();
                outVe[2*n + 1] = inVe[2*j + 1];
                ++n;
                inside = true;
            }
            // Copy vertex i to output polygon.
            outVe[2*n] = inVe[2*i];
            outVe[2*n + 1] = inVe[2*i + 1];
            ++n;
        } else if (inside) {
            // Edge crosses plane (inside to outside) -
            // copy intersection to output polygon.
            Vector3d edge = inVe[2*j + 1].normalized();
            outVe[2*n] = (plane + edge).cross(plane - edge).normalized();
            outVe[2*n + 1] = plane;
            ++n;
            inside = false;
        }
    }
    return n;
}

// A list of non-overlapping RA ranges (in radians).
class RaRangeList {
public:
    // A new RaRangeList object contains a single range [-π,π].
    RaRangeList() : _numRanges(1) {
        _ranges[0].first = -pi<double>();
        _ranges[0].second = pi<double>();
    }

    // Is this range list empty? If so, extent() == 0.0.
    bool empty() const { return _numRanges == 0; }
    // Is this range list full? If so, extent() == 2π.
    bool full() const {
        return _numRanges == 1 &&
               _ranges[0].first == -pi<double>() &&
               _ranges[0].second == pi<double>();
    }
    void clear() { _numRanges = 0; }
    // Intersect this range list with the given RA range.
    void clip(double ra0, double ra1);
    // Return the sum of the RA extents of the ranges in this list.
    double extent() const;

private:
    pair<double, double> _ranges[3];
    int _numRanges;
};

void RaRangeList::clip(double ra0, double ra1) {
    assert(ra0 != ra1);
    pair<double, double> out[3];
    int j = 0;
    for (int i = 0; i < _numRanges; ++i) {
        double cra0 = _ranges[i].first;
        double cra1 = _ranges[i].second;
        assert(cra0 != cra1);
        if (ra0 < ra1) {
            if (cra0 < cra1) {
                if (ra0 < cra1 && ra1 > cra0) {
                    out[j].first = max(ra0, cra0);
                    out[j].second = min(ra1, cra1);
                    if (out[j].first != out[j].second) {
                        ++j;
                    }
                }
            } else {
                if (ra0 < cra1) {
                    out[j].first = ra0;
                    out[j].second = min(ra1, cra1);
                    ++j;
                    if (ra1 > cra0) {
                        out[j].first = cra0;
                        out[j].second = ra1;
                        ++j;
                    }
                } else if (ra1 > cra0) {
                    out[j].first = min(ra0, cra0);
                    out[j].second = ra1;
                    ++j;
                }
            }
        } else {
            if (cra0 < cra1) {
                if (cra0 < ra1) {
                    out[j].first = cra0;
                    out[j].second = min(cra1, ra1);
                    ++j;
                    if (cra1 > ra0) {
                        out[j].first = ra0;
                        out[j].second = cra1;
                        ++j;
                    }
                } else if (cra1 > ra0) {
                    out[j].first = min(cra0, ra0);
                    out[j].second = cra1;
                    ++j;
                }
            } else {
                out[j].first = max(ra0, cra0);
                out[j].second = min(ra1, cra1);
                ++j;
                if (ra1 > cra0) {
                    out[j].first = cra0;
                    out[j].second = ra1;
                    ++j;
                } else if (ra0 < cra1) {
                    out[j].first = ra0;
                    out[j].second = cra1;
                    ++j;
                }
            }
        }
    }
    for (int i = 0; i < j; ++i) {
        _ranges[i] = out[i];
    }
    _numRanges = j;
}

double RaRangeList::extent() const {
    double e = 0.0;
    for (int i = 0; i < _numRanges; ++i) {
        double delta = _ranges[i].second - _ranges[i].first;
        if (delta < 0.0) {
            delta += 2.0*pi<double>();
        }
        e += delta;
    }
    return e;
}

// Compute the area of the input polygon intersected with zmin <= z <= zmax.
double zArea(Vector3d const * inVe,
             size_t numVerts,
             double zmin,
             double zmax)
{
    double angle = 0.0;
    RaRangeList bot, top;
    for (size_t i = 0, j = numVerts - 1; i < numVerts; j = i, ++i) {
        double z = inVe[2*i](2);
        Vector3d const & n = inVe[2*j + 1];
        if (z >= zmin && z <= zmax) {
            // Vertex is in z-range; accumulate vertex turning angle.
            angle += angSep(n, inVe[2*i + 1]);
        }
        double u = n(0)*n(0) + n(1)*n(1);
        double n2 = u + n(2)*n(2);
        if (u == 0.0) {
            assert(n(2) != 0.0);
            // Edge lies on x-y plane.
            if (n(2)*zmin <= 0.0) {
                bot.clear();
            }
            if (n(2)*zmax <= 0.0) {
                top.clear();
            }
            continue;
        }
        // Compare z = zmin and edge j,i.
        double z2 = zmin*zmin;
        double v = u - n2*z2;
        if (v > 0.0 && !bot.empty()) {
            Vector3d p(-n(0)*n(2)*zmin, -n(1)*n(2)*zmin, u*zmin);
            Vector3d nc(n(1), -n(0), 0.0);
            double lambda = sqrt(v);
            // Compute unnormalized intersection points v0, v1.
            Vector3d v0 = p + lambda * nc;
            Vector3d v1 = p - lambda * nc;
            if (angSep(v0, v1) <= RAD_PER_DEG/36000.0) {
                // Angle between intersections is less than 100 milliarcsec;
                // treat this as a single degenerate intersection point.
                if (n(2)*zmin < 0.0) {
                    bot.clear();
                }
            } else {
                Vector3d ncv0 = n.cross(v0);
                Vector3d ncv1 = n.cross(v1);
                if (ncv0.dot(inVe[2*j]) < 0.0 && ncv0.dot(inVe[2*i]) > 0.0) {
                    // v0 lies on edge j,i - accumulate turning angle.
                    angle += angSep(ncv0, Vector3d(-v0(1), v0(0), 0.0));
                }
                if (ncv1.dot(inVe[2*j]) < 0.0 && ncv1.dot(inVe[2*i]) > 0.0) {
                    // v1 lies on edge j,i - accumulate turning angle.
                    angle += angSep(ncv1, Vector3d(-v1(1), v1(0), 0.0));
                }
                bot.clip(atan2(v0(1), v0(0)), atan2(v1(1), v1(0)));
            }
        } else if (n(2)*zmin < 0.0) {
            bot.clear();
        }
        // Compare z = zmax and edge j,i.
        z2 = zmax*zmax;
        v = u - n2*z2;
        if (v > 0.0 && !top.empty()) {
            Vector3d p(n(0)*n(2)*zmax, n(1)*n(2)*zmax, u*zmax);
            Vector3d nc(-n(1), n(0), 0.0);
            double lambda = sqrt(v);
            // Compute unnormalized intersection points v0, v1.
            Vector3d v0 = p + lambda * nc;
            Vector3d v1 = p - lambda * nc;
            if (angSep(v0, v1) <= RAD_PER_DEG/36000.0) {
                // Angle between intersections is less than 100 milliarcsec;
                // treat this as a single degenerate intersection point.
                if (n(2)*zmax < 0.0) {
                    top.clear();
                }
            } else {
                Vector3d ncv0 = n.cross(v0);
                Vector3d ncv1 = n.cross(v1);
                if (ncv0.dot(inVe[2*j]) < 0.0 && ncv0.dot(inVe[2*i]) > 0.0) {
                    // v0 lies on edge j,i - accumulate turning angle.
                    angle += angSep(ncv0, Vector3d(v0(1), -v0(0), 0.0));
                }
                if (ncv1.dot(inVe[2*j]) < 0.0 && ncv1.dot(inVe[2*i]) > 0.0) {
                    // v1 lies on edge j,i - accumulate turning angle.
                    angle += angSep(ncv1, Vector3d(v1(1), -v1(0), 0.0));
                }
                top.clip(atan2(v1(1), v1(0)), atan2(v0(1), v0(0)));
            }
        } else if (n(2)*zmax < 0.0) {
            top.clear();
        }
    }
    double chi = 1.0; // Euler characteristic
    if (bot.full() && top.full()) {
        assert(angle == 0.0);
        chi = 0.0;
    } else if (angle != 0.0 && (bot.full() || top.full())) {
        chi = 0.0;
    }
    return 2.0*pi<double>()*chi - angle + top.extent()*zmax - bot.extent()*zmin;
}

} // unnamed namespace

double SphericalTriangle::intersectionArea(SphericalBox const & box) const {
    if (box.getRaMin() == box.getRaMax() ||
        box.getDecMin() >= 90.0 - EPSILON_DEG ||
        box.getDecMax() <= -90.0- + EPSILON_DEG) {
        // box is degenerate or very small.
        return 0.0;
    } else if (box.isFull()) {
        // box completely contains this triangle.
        return area();
    }
    double const zmin = sin(box.getDecMin()*RAD_PER_DEG);
    double const zmax = sin(box.getDecMax()*RAD_PER_DEG);
    if (zmin >= zmax) {
        return 0.0;
    }
    Vector3d veBuf0[(3 + 2)*2];
    Vector3d veBuf1[(3 + 2)*2];
    Vector3d * in = veBuf0;
    Vector3d * out = veBuf1;
    size_t numVerts = 3;
    in[0] = vertex(0);
    in[1] = (vertex(1) + vertex(0)).cross(vertex(1) - vertex(0));
    in[2] = vertex(1);
    in[3] = (vertex(2) + vertex(1)).cross(vertex(2) - vertex(1));
    in[4] = vertex(2);
    in[5] = (vertex(0) + vertex(2)).cross(vertex(0) - vertex(2));
    if (box.getRaMin() != 0.0 || box.getRaMax() != 360.0) {
        // Box is not an annulus.
        double raExtent = box.getRaExtent();
        if (raExtent > 180.0 + EPSILON_DEG) {
            // Punt for now, because the intersection can be non-convex.
            throw runtime_error("Cannot compute triangle-box intersection area: "
                                "spherical box has RA extent > 180 deg.");
        }
        // Intersect with the half-space ra >= box.getRaMin().
        double ra = RAD_PER_DEG * box.getRaMin();
        numVerts = intersect(
            in, numVerts, Vector3d(-sin(ra), cos(ra), 0.0), out);
        if (numVerts == 0) {
            return 0.0;
        }
        swap(in, out);
        // If the RA extent is close to 180 deg, avoid intersecting
        // with (nearly) the same half-space twice.
        if (raExtent < 180.0 - EPSILON_DEG) {
            // Intersect with the half-space ra <= box.getRaMax().
            ra = RAD_PER_DEG * box.getRaMax();
            numVerts = intersect(
                in, numVerts, Vector3d(sin(ra), -cos(ra), 0.0), out);
            if (numVerts == 0) {
                return 0.0;
            }
            swap(in, out);
        }
    }
    return zArea(in, numVerts, zmin, zmax);
}


// -- SphericalBox implementation ----

SphericalBox::SphericalBox(double raMin,
                           double raMax,
                           double decMin,
                           double decMax)
{
    if (decMin > decMax) {
        throw runtime_error("Spherical box declination max < min.");
    } else if (raMax < raMin && (raMax < 0.0 || raMin > 360.0)) {
        throw runtime_error("Spherical box right ascension max < min.");
    }
    if (raMax - raMin >= 360.0) {
        _raMin = 0.0;
        _raMax = 360.0;
    } else {
        _raMin = reduceRa(raMin);
        _raMax = reduceRa(raMax);
    }
    _decMin = clampDec(decMin);
    _decMax = clampDec(decMax);
}

SphericalBox::SphericalBox(Vector3d const & v0,
                           Vector3d const & v1,
                           Vector3d const & v2)
{
    // Find the bounding circle of the triangle.
    Vector3d cv = v0 + v1 + v2;
    double r = angSep(cv, v0);
    r = max(r, angSep(cv, v1));
    r = max(r, angSep(cv, v2));
    r = r*DEG_PER_RAD + 1/3600.0;
    // Construct the bounding box for the bounding circle. This is inexact,
    // but involves less code than a more accurate computation.
    pair<double, double> c = spherical(cv);
    double alpha = maxAlpha(r, c.second);
    _decMin = clampDec(c.second - r);
    _decMax = clampDec(c.second + r);
    if (alpha > 180.0 - 1/3600.0) {
        _raMin = 0.0;
        _raMax = 360.0;
    } else {
        double raMin = c.first - alpha;
        double raMax = c.first + alpha;
        if (raMin < 0.0) {
            raMin += 360.0;
            if (raMin == 360.0) {
                raMin = 0.0;
            }
        }
        _raMin = raMin;
        if (raMax > 360.0) {
            raMax -= 360.0;
        }
        _raMax = raMax;
    }
}

void SphericalBox::expand(double radius) {
    if (radius < 0.0) {
        throw runtime_error(
            "Cannot expand spherical box by a negative angle.");
    } else if (radius == 0.0) {
        return;
    }
    double const extent = getRaExtent();
    double const alpha = maxAlpha(radius, max(fabs(_decMin), fabs(_decMax)));
    if (extent + 2.0 * alpha >= 360.0 - 1/3600.0) {
        _raMin = 0.0;
        _raMax = 360.0;
    } else {
        _raMin -= alpha;
        if (_raMin < 0.0) {
            _raMin += 360.0;
            if (_raMin == 360.0) {
                _raMin = 0.0;
            }
        }
        _raMax += alpha;
        if (_raMax > 360.0) {
            _raMax -= 360.0;
        }
    }
    _decMin = clampDec(_decMin - radius);
    _decMax = clampDec(_decMax + radius);
}

double SphericalBox::area() const {
    return RAD_PER_DEG*getRaExtent() *
           (sin(RAD_PER_DEG*_decMax) - sin(RAD_PER_DEG*_decMin));
}

vector<uint32_t> const SphericalBox::htmIds(int level) const {
    vector<uint32_t> ids;
    if (level < 0 || level > HTM_MAX_LEVEL) {
        throw runtime_error("Invalid HTM subdivision level.");
    }
    for (int r = 0; r < 8; ++r) {
        Matrix3d m;
        m.col(0) = *htmRootVert[r*3];
        m.col(1) = *htmRootVert[r*3 + 1];
        m.col(2) = *htmRootVert[r*3 + 2];
        findIds(r + 8, level, m, ids);
    }
    return ids;
}

// Slow method for finding triangles overlapping a box. For the subdivision
// levels and box sizes encountered in practice, this is very unlikely to be
// a performance problem.
void SphericalBox::findIds(
    uint32_t id,
    int level,
    Matrix3d const & m,
    vector<uint32_t> & ids
) const {
    if (!intersects(SphericalBox(m.col(0), m.col(1), m.col(2)))) {
        return;
    } else if (level == 0) {
        ids.push_back(id);
        return;
    }
    Matrix3d mChild;
    Vector3d const sv0 = (m.col(1) + m.col(2)).normalized();
    Vector3d const sv1 = (m.col(2) + m.col(0)).normalized();
    Vector3d const sv2 = (m.col(0) + m.col(1)).normalized();
    mChild.col(0) = m.col(0);
    mChild.col(1) = sv2;
    mChild.col(2) = sv1;
    findIds(id*4, level - 1, mChild, ids);
    mChild.col(0) = m.col(1);
    mChild.col(1) = sv0;
    mChild.col(2) = sv2;
    findIds(id*4 + 1, level - 1, mChild, ids);
    mChild.col(0) = m.col(2);
    mChild.col(1) = sv1;
    mChild.col(2) = sv0;
    findIds(id*4 + 2, level - 1, mChild, ids);
    mChild.col(0) = sv0;
    mChild.col(1) = sv1;
    mChild.col(2) = sv2;
    findIds(id*4 + 3, level - 1, mChild, ids);
}

}}}} // namespace lsst::qserv::admin::dupr
