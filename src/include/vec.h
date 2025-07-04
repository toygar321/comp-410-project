//////////////////////////////////////////////////////////////////////////////
//
//  --- vec.h ---
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __ANGEL_VEC_H__
#define __ANGEL_VEC_H__

#include "Angel.h"
#include <cmath>

namespace Angel {
    
    //////////////////////////////////////////////////////////////////////////////
    //
    //  vec2.h - 2D vector
    //
    
    struct vec2 {
        
        GLfloat  x;
        GLfloat  y;
        
        //
        //  --- Constructors and Destructors ---
        //
        
        vec2( GLfloat s = GLfloat(0.0) ) :
        x(s), y(s) {}
        
        vec2( GLfloat x, GLfloat y ) :
        x(x), y(y) {}
        
        vec2( const vec2& v )
        { x = v.x;  y = v.y;  }
        
        //
        //  --- Indexing Operator ---
        //
        
        GLfloat& operator [] ( int i ) { return *(&x + i); }
        const GLfloat operator [] ( int i ) const { return *(&x + i); }
        
        //
        //  --- (non-modifying) Arithematic Operators ---
        //
        
        vec2 operator - () const // unary minus operator
        { return vec2( -x, -y ); }
        
        vec2 operator + ( const vec2& v ) const
        { return vec2( x + v.x, y + v.y ); }
        
        vec2 operator - ( const vec2& v ) const
        { return vec2( x - v.x, y - v.y ); }
        
        vec2 operator * ( const GLfloat s ) const
        { return vec2( s*x, s*y ); }
        
        vec2 operator * ( const vec2& v ) const
        { return vec2( x*v.x, y*v.y ); }
        
        friend vec2 operator * ( const GLfloat s, const vec2& v )
        { return v * s; }
        
        vec2 operator / ( const GLfloat s ) const {
#ifdef DEBUG
            if ( std::fabs(s) < DivideByZeroTolerance ) {
                std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] "
                << "Division by zero" << std::endl;
                return vec2();
            }
#endif // DEBUG
            
            GLfloat r = GLfloat(1.0) / s;
            return *this * r;
        }
        
        //
        //  --- (modifying) Arithematic Operators ---
        //
        
        vec2& operator += ( const vec2& v )
        { x += v.x;  y += v.y;   return *this; }
        
        vec2& operator -= ( const vec2& v )
        { x -= v.x;  y -= v.y;  return *this; }
        
        vec2& operator *= ( const GLfloat s )
        { x *= s;  y *= s;   return *this; }
        
        vec2& operator *= ( const vec2& v )
        { x *= v.x;  y *= v.y; return *this; }
        
        vec2& operator /= ( const GLfloat s ) {
#ifdef DEBUG
            if ( std::fabs(s) < DivideByZeroTolerance ) {
                std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] "
                << "Division by zero" << std::endl;
            }
#endif // DEBUG
            
            GLfloat r = GLfloat(1.0) / s;
            *this *= r;
            
            return *this;
        }
        
        //
        //  --- Insertion and Extraction Operators ---
        //
        
        friend std::ostream& operator << ( std::ostream& os, const vec2& v ) {
            return os << "( " << v.x << ", " << v.y <<  " )";
        }
        
        friend std::istream& operator >> ( std::istream& is, vec2& v )
        { return is >> v.x >> v.y ; }
        
        //
        //  --- Conversion Operators ---
        //
        
        operator const GLfloat* () const
        { return static_cast<const GLfloat*>( &x ); }
        
        operator GLfloat* ()
        { return static_cast<GLfloat*>( &x ); }
    };
    
    //----------------------------------------------------------------------------
    //
    //  Non-class vec2 Methods
    //
    
    inline
    GLfloat dot( const vec2& u, const vec2& v ) {
        return u.x * v.x + u.y * v.y;
    }
    
    inline
    GLfloat length( const vec2& v ) {
        return std::sqrt( dot(v,v) );
    }
    
    inline
    vec2 normalize( const vec2& v ) {
        return v / length(v);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    //  ivec2.h - 2D int vector
    //
    
    struct ivec2 {
        
        GLint  x;
        GLint  y;
        
        //
        //  --- Constructors and Destructors ---
        //
        
        ivec2( GLint s = GLint(0) ) :
        x(s), y(s) {}
        
        ivec2( GLint x, GLint y ) :
        x(x), y(y) {}
        
        ivec2( const ivec2& v )
        { x = v.x;  y = v.y;  }
        
        //
        //  --- Indexing Operator ---
        //
        
        GLint& operator [] ( int i ) { return *(&x + i); }
        const GLint operator [] ( int i ) const { return *(&x + i); }
        
        //
        //  --- (non-modifying) Arithematic Operators ---
        //
        
        ivec2 operator - () const // unary minus operator
        { return ivec2( -x, -y ); }
        
        ivec2 operator + ( const ivec2& v ) const
        { return ivec2( x + v.x, y + v.y ); }
        
        ivec2 operator - ( const ivec2& v ) const
        { return ivec2( x - v.x, y - v.y ); }
        
        ivec2 operator * ( const GLint s ) const
        { return ivec2( s*x, s*y ); }
        
        ivec2 operator * ( const ivec2& v ) const
        { return ivec2( x*v.x, y*v.y ); }
        
        friend ivec2 operator * ( const GLint s, const ivec2& v )
        { return v * s; }
        
        ivec2 operator / ( const GLint s ) const {
#ifdef DEBUG
            if ( s == 0 ) {
                std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] "
                << "Division by zero" << std::endl;
                return ivec2();
            }
#endif // DEBUG
            
            return ivec2(x / s, y / s);
        }
        
        //
        //  --- (modifying) Arithematic Operators ---
        //
        
        ivec2& operator += ( const ivec2& v )
        { x += v.x;  y += v.y;   return *this; }
        
        ivec2& operator -= ( const ivec2& v )
        { x -= v.x;  y -= v.y;  return *this; }
        
        ivec2& operator *= ( const GLint s )
        { x *= s;  y *= s;   return *this; }
        
        ivec2& operator *= ( const ivec2& v )
        { x *= v.x;  y *= v.y; return *this; }
        
        ivec2& operator /= ( const GLint s ) {
#ifdef DEBUG
            if ( s == 0 ) {
                std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] "
                << "Division by zero" << std::endl;
                return *this;c
            }
#endif // DEBUG

            x /= s;
            y /= s;
            
            return *this;
        }
        
        //
        //  --- Insertion and Extraction Operators ---
        //
        
        friend std::ostream& operator << ( std::ostream& os, const ivec2& v ) {
            return os << "( " << v.x << ", " << v.y <<  " )";
        }
        
        friend std::istream& operator >> ( std::istream& is, ivec2& v )
        { return is >> v.x >> v.y ; }
        
        //
        //  --- Conversion Operators ---
        //
        
        operator const GLint* () const
        { return static_cast<const GLint*>( &x ); }
        
        operator GLint* ()
        { return static_cast<GLint*>( &x ); }
    };
    
    //----------------------------------------------------------------------------
    //
    //  Non-class ivec2 Methods
    //
    
    inline
    GLint dot( const ivec2& u, const ivec2& v ) {
        return u.x * v.x + u.y * v.y;
    }
    
    inline
    GLfloat length( const ivec2& v ) {
        return std::sqrt( dot(v,v) );
    }
    
    inline
    vec2 normalize( const ivec2& v ) {
        const float l = length(v);
        if (l == 0.0f) return vec2(0.0f);
        return vec2(v.x / l, v.y / l);
    }
    
    //////////////////////////////////////////////////////////////////////////////
    //
    //  vec3.h - 3D vector
    //
    //////////////////////////////////////////////////////////////////////////////
    
    struct vec3 {
        
        GLfloat  x;
        GLfloat  y;
        GLfloat  z;
        
        //
        //  --- Constructors and Destructors ---
        //
        
        vec3( GLfloat s = GLfloat(0.0) ) :
        x(s), y(s), z(s) {}
        
        vec3( GLfloat x, GLfloat y, GLfloat z ) :
        x(x), y(y), z(z) {}
        
        vec3( const vec3& v ) { x = v.x;  y = v.y;  z = v.z; }
        
        vec3( const vec2& v, const float f ) { x = v.x;  y = v.y;  z = f; }
        
        //
        //  --- Indexing Operator ---
        //
        
        GLfloat& operator [] ( int i ) { return *(&x + i); }
        const GLfloat operator [] ( int i ) const { return *(&x + i); }
        
        //
        //  --- (non-modifying) Arithematic Operators ---
        //
        
        vec3 operator - () const  // unary minus operator
        { return vec3( -x, -y, -z ); }
        
        vec3 operator + ( const vec3& v ) const
        { return vec3( x + v.x, y + v.y, z + v.z ); }
        
        vec3 operator - ( const vec3& v ) const
        { return vec3( x - v.x, y - v.y, z - v.z ); }
        
        vec3 operator * ( const GLfloat s ) const
        { return vec3( s*x, s*y, s*z ); }
        
        vec3 operator * ( const vec3& v ) const
        { return vec3( x*v.x, y*v.y, z*v.z ); }
        
        friend vec3 operator * ( const GLfloat s, const vec3& v )
        { return v * s; }
        
        vec3 operator / ( const GLfloat s ) const {
#ifdef DEBUG
            if ( std::fabs(s) < DivideByZeroTolerance ) {
                std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] "
                << "Division by zero" << std::endl;
                return vec3();
            }
#endif // DEBUG
            
            GLfloat r = GLfloat(1.0) / s;
            return *this * r;
        }
        
        //
        //  --- (modifying) Arithematic Operators ---
        //
        
        vec3& operator += ( const vec3& v )
        { x += v.x;  y += v.y;  z += v.z;  return *this; }
        
        vec3& operator -= ( const vec3& v )
        { x -= v.x;  y -= v.y;  z -= v.z;  return *this; }
        
        vec3& operator *= ( const GLfloat s )
        { x *= s;  y *= s;  z *= s;  return *this; }
        
        vec3& operator *= ( const vec3& v )
        { x *= v.x;  y *= v.y;  z *= v.z;  return *this; }
        
        vec3& operator /= ( const GLfloat s ) {
#ifdef DEBUG
            if ( std::fabs(s) < DivideByZeroTolerance ) {
                std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] "
                << "Division by zero" << std::endl;
            }
#endif // DEBUG
            
            GLfloat r = GLfloat(1.0) / s;
            *this *= r;
            
            return *this;
        }
        
        //
        //  --- Insertion and Extraction Operators ---
        //
        
        friend std::ostream& operator << ( std::ostream& os, const vec3& v ) {
            return os << "( " << v.x << ", " << v.y << ", " << v.z <<  " )";
        }
        
        friend std::istream& operator >> ( std::istream& is, vec3& v )
        { return is >> v.x >> v.y >> v.z ; }
        
        //
        //  --- Conversion Operators ---
        //
        
        operator const GLfloat* () const
        { return static_cast<const GLfloat*>( &x ); }
        
        operator GLfloat* ()
        { return static_cast<GLfloat*>( &x ); }
    };
    
    //----------------------------------------------------------------------------
    //
    //  Non-class vec3 Methods
    //
    
    inline
    GLfloat dot( const vec3& u, const vec3& v ) {
        return u.x*v.x + u.y*v.y + u.z*v.z ;
    }
    
    inline
    GLfloat length( const vec3& v ) {
        return std::sqrt( dot(v,v) );
    }
    
    inline
    vec3 normalize( const vec3& v ) {
        return v / length(v);
    }
    
    inline
    vec3 cross(const vec3& a, const vec3& b )
    {
        return vec3( a.y * b.z - a.z * b.y,
                    a.z * b.x - a.x * b.z,
                    a.x * b.y - a.y * b.x );
    }

    inline vec3 pow(const vec3& v, float p) {
        return vec3(std::pow(v.x, p), std::pow(v.y, p), std::pow(v.z, p));
    }
    
    
    //////////////////////////////////////////////////////////////////////////////
    //
    //  vec4 - 4D vector
    //
    //////////////////////////////////////////////////////////////////////////////
    
    struct vec4 {
        
        GLfloat  x;
        GLfloat  y;
        GLfloat  z;
        GLfloat  w;
        
        //
        //  --- Constructors and Destructors ---
        //
        
        vec4( GLfloat s = GLfloat(0.0) ) :
        x(s), y(s), z(s), w(s) {}
        
        vec4( GLfloat x, GLfloat y, GLfloat z, GLfloat w ) :
        x(x), y(y), z(z), w(w) {}
        
        vec4( const vec4& v ) { x = v.x;  y = v.y;  z = v.z;  w = v.w; }
        
        vec4( const vec3& v, const float s = 1.0 ) : w(w = 0)
        { x = v.x;  y = v.y;  z = v.z; }
        
        vec4( const vec2& v, const float z, const float w ) : z(z), w(w)
        { x = v.x;  y = v.y; }
        
        //
        //  --- Indexing Operator ---
        //
        
        GLfloat& operator [] ( int i ) { return *(&x + i); }
        const GLfloat operator [] ( int i ) const { return *(&x + i); }
        
        //
        //  --- (non-modifying) Arithematic Operators ---
        //
        
        vec4 operator - () const  // unary minus operator
        { return vec4( -x, -y, -z, -w ); }
        
        vec4 operator + ( const vec4& v ) const
        { return vec4( x + v.x, y + v.y, z + v.z, w + v.w ); }
        
        vec4 operator - ( const vec4& v ) const
        { return vec4( x - v.x, y - v.y, z - v.z, w - v.w ); }
        
        vec4 operator * ( const GLfloat s ) const
        { return vec4( s*x, s*y, s*z, s*w ); }
        
        vec4 operator * ( const vec4& v ) const
        { return vec4( x*v.x, y*v.y, z*v.z, w*v.z ); }
        
        friend vec4 operator * ( const GLfloat s, const vec4& v )
        { return v * s; }
        
        vec4 operator / ( const GLfloat s ) const {
#ifdef DEBUG
            if ( std::fabs(s) < DivideByZeroTolerance ) {
                std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] "
                << "Division by zero" << std::endl;
                return vec4();
            }
#endif // DEBUG
            
            GLfloat r = GLfloat(1.0) / s;
            return *this * r;
        }
        
        //
        //  --- (modifying) Arithematic Operators ---
        //
        
        vec4& operator += ( const vec4& v )
        { x += v.x;  y += v.y;  z += v.z;  w += v.w;  return *this; }
        
        vec4& operator -= ( const vec4& v )
        { x -= v.x;  y -= v.y;  z -= v.z;  w -= v.w;  return *this; }
        
        vec4& operator *= ( const GLfloat s )
        { x *= s;  y *= s;  z *= s;  w *= s;  return *this; }
        
        vec4& operator *= ( const vec4& v )
        { x *= v.x; y *= v.y; z *= v.z; w *= v.w;  return *this; }
        
        vec4& operator /= ( const GLfloat s ) {
#ifdef DEBUG
            if ( std::fabs(s) < DivideByZeroTolerance ) {
                std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] "
                << "Division by zero" << std::endl;
            }
#endif // DEBUG
            
            GLfloat r = GLfloat(1.0) / s;
            *this *= r;
            
            return *this;
        }
        
        //
        //  --- Insertion and Extraction Operators ---
        //
        
        friend std::ostream& operator << ( std::ostream& os, const vec4& v ) {
            return os << "( " << v.x << ", " << v.y
            << ", " << v.z << ", " << v.w << " )";
        }
        
        friend std::istream& operator >> ( std::istream& is, vec4& v )
        { return is >> v.x >> v.y >> v.z >> v.w; }
        
        //
        //  --- Conversion Operators ---
        //
        
        operator const GLfloat* () const
        { return static_cast<const GLfloat*>( &x ); }
        
        operator GLfloat* ()
        { return static_cast<GLfloat*>( &x ); }
    };
    
    //----------------------------------------------------------------------------
    //
    //  Non-class vec4 Methods
    //
    
    inline
    GLfloat dot( const vec4& u, const vec4& v ) {
        return u.x*v.x + u.y*v.y + u.z*v.z + u.w+v.w;
    }
    
    inline
    GLfloat length( const vec4& v ) {
        return std::sqrt( dot(v,v) );
    }
    
    inline
    vec4 normalize( const vec4& v ) {
        return v / length(v);
    }
    
    inline
    vec3 cross(const vec4& a, const vec4& b )
    {
        return vec3( a.y * b.z - a.z * b.y,
                    a.z * b.x - a.x * b.z,
                    a.x * b.y - a.y * b.x );
    }
    
    //----------------------------------------------------------------------------
    
}  // namespace Angel

#endif // __ANGEL_VEC_H__
