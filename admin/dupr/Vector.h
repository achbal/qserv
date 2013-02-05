/** Vector algebra in 3 dimensions.

    The Vector3d and Matrix3d classes provide the basic vector algebra
    operations required by the duplicator/partitioner. They mimic a subset
    of the Eigen Vector3d/Matrix3d APIs, which should make switching
    to that library easy if more advanced functionality becomes necessary.
  */
#ifndef VECTOR_H
#define VECTOR_H

#include <cmath>


namespace dupr {

/// A 3-component column vector.
class Vector3d {
public:
    Vector3d() { }
    Vector3d(double x, double y, double z) { _c[0] = x; _c[1] = y; _c[2] = z; }

    // Return the i-th component of this vector.
    double & operator()(int i)       { return _c[i]; }
    double   operator()(int i) const { return _c[i]; }

    /// Return the dot (a.k.a. inner) product of this vector and v.
    double dot(Vector3d const & v) const {
        return _c[0]*v._c[0] + _c[1]*v._c[1] + _c[2]*v._c[2];
    }

    /// Return the L_2 norm of this vector.
    double norm() const {
        return std::sqrt(dot(*this));
    }

    /// Return a normalized copy of this vector.
    Vector3d const normalized() const {
        double n = norm();
        return Vector3d(_c[0] / n, _c[1] / n,  _c[2] / n);
    }

    /// Return the cross product of this vector and v.
    Vector3d const cross(Vector3d const & v) const {
        return Vector3d(_c[1]*v._c[2] - _c[2]*v._c[1],
                        _c[2]*v._c[0] - _c[0]*v._c[2],
                        _c[0]*v._c[1] - _c[1]*v._c[0]);
    }

    /// Return the component-wise product of this vector with scalar s.
    Vector3d const operator*(double s) const {
        return Vector3d(_c[0]*s, _c[1]*s, _c[2]*s);
    }

    /// Return the sum of this vector with v.
    Vector3d const operator+(Vector3d const & v) const {
        return Vector3d(_c[0] + v._c[0], _c[1] + v._c[1], _c[2] + v._c[2]);
    }

    /// Return the difference between this vector and v.
    Vector3d const operator-(Vector3d const & v) const {
        return Vector3d(_c[0] - v._c[0], _c[1] - v._c[1], _c[2] - v._c[2]);
    }

private:
    double _c[3]; ///< vector components
};

inline Vector3d const operator*(double s, Vector3d const & v) {
    return v*s;
}


/// A 3x3 matrix.
class Matrix3d {
public:

    // Return the c-th matrix column; bounds are not checked.
    Vector3d       & col(int c)       { return _col[c]; }
    Vector3d const & col(int c) const { return _col[c]; }

    // Return the scalar at row r and column c; bounds are not checked.
    double & operator()(int r, int c)       { return _col[c](r); }
    double   operator()(int r, int c) const { return _col[c](r); }

    /// Return the identity matrix.
    static Matrix3d const Identity() {
        Matrix3d m;
        m(0,0) = 1.0; m(0,1) = 0.0; m(0,2) = 0.0;
        m(1,0) = 0.0; m(1,1) = 1.0; m(1,2) = 0.0;
        m(2,0) = 0.0; m(2,1) = 0.0; m(2,2) = 1.0;
        return m;
    }

    /// Return the product of this matrix with vector v.
    Vector3d const operator*(Vector3d const & v) const {
        return Vector3d(col(0)*v(0) + col(1)*v(1) + col(2)*v(2));
    }

    /// Return the product of this matrix with matrix m.
    Matrix3d const operator*(Matrix3d const & m) const {
        Matrix3d r;
        r.col(0) = this->operator*(m.col(0));
        r.col(1) = this->operator*(m.col(1));
        r.col(2) = this->operator*(m.col(2));
        return r;
    }

    /// Return the inverse of this matrix.
    Matrix3d const inverse() const {
        Matrix3d inv;
        Matrix3d const & m = *this;
        // first column of adjugate matrix
        Vector3d a0(m(1,1)*m(2,2) - m(2,1)*m(1,2),
                    m(1,2)*m(2,0) - m(2,2)*m(1,0),
                    m(1,0)*m(2,1) - m(2,0)*m(1,1));
        // find the inverse of the determinant of m
        double invDet = 1.0 / (a0(0)*m(0,0) + a0(1)*m(0,1) + a0(2)*m(0,2));
        // column 0
        inv.col(0) = a0 * invDet;
        // column 1
        inv(0,1) = (m(0,2)*m(2,1) - m(2,2)*m(0,1)) * invDet;
        inv(1,1) = (m(0,0)*m(2,2) - m(2,0)*m(0,2)) * invDet;
        inv(2,1) = (m(0,1)*m(2,0) - m(2,1)*m(0,0)) * invDet;
        // column 2
        inv(0,2) = (m(0,1)*m(1,2) - m(1,1)*m(0,2)) * invDet;
        inv(1,2) = (m(0,2)*m(1,0) - m(1,2)*m(0,0)) * invDet;
        inv(2,2) = (m(0,0)*m(1,1) - m(1,0)*m(0,1)) * invDet;
        return inv;
    }

private:
    Vector3d _col[3]; ///< Matrix columns.
};

} // namespace dupr

#endif // VECTOR_H_

