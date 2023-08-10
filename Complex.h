//#ifndef OBTS_TRANSCEIVER52_COMPLEXCPP_H
//#define OBTS_TRANSCEIVER52_COMPLEXCPP_H
//
//#include <cmath>
//#include <ostream>
//
//// FIXME: This is a template class for complex numbers.  The previous author stated:
////        "unlike the built-in complex<> templates, these inline most operations for speed"
////        which I do not believe to be an accurate statement anymore.  It would be better,
////        in my opinion, to make use of the standard class if possible.
//template<class T>
//class Complex {
//public:
//    T r, i;
//
//    // x=complex()
//    Complex() {
//        r = (T) 0;
//        i = (T) 0;
//    }
//
//    // x=complex(a)
//    explicit Complex(T real) {
//        r = real;
//        i = 0;
//    }
//
//    // x=complex(a,b)
//    Complex(T real, T imag) {
//        r = real;
//        i = imag;
//    }
//
//    // FIXME: Why do these call out specific types?  Shouldn't they just cast to T?
//    // x=complex(z)
//    explicit Complex(const Complex<float> &z) {
//        r = (T) z.r;
//        i = (T) z.i;
//    }
//
//    explicit Complex(const Complex<double> &z) {
//        r = (T) z.r;
//        i = (T) z.i;
//    }
//
//    explicit Complex(const Complex<long double> &z) {
//        r = (T) z.r;
//        i = (T) z.i;
//    }
//
//    Complex &operator=(char a) {
//        r = (T) a;
//        i = (T) 0;
//        return *this;
//    }
//
//    Complex &operator=(int a) {
//        r = (T) a;
//        i = (T) 0;
//        return *this;
//    }
//
//    Complex &operator=(long int a) {
//        r = (T) a;
//        i = (T) 0;
//        return *this;
//    }
//
//    Complex &operator=(short a) {
//        r = (T) a;
//        i = (T) 0;
//        return *this;
//    }
//
//    Complex &operator=(float a) {
//        r = (T) a;
//        i = (T) 0;
//        return *this;
//    }
//
//    Complex &operator=(double a) {
//        r = (T) a;
//        i = (T) 0;
//        return *this;
//    }
//
//    Complex &operator=(long double a) {
//        r = (T) a;
//        i = (T) 0;
//        return *this;
//    }
//
//    Complex operator+(const Complex<T> &a) const {
//        return Complex<T>(r + a.r, i + a.i);
//    }
//
//    Complex operator+(T a) const {
//        return Complex<T>(r + a, i);
//    }
//
//    Complex operator-(const Complex<T> &a) const { return Complex<T>(r - a.r, i - a.i); }
//
//    Complex operator-(T a) const { return Complex<T>(r - a, i); }
//
//    Complex operator*(const Complex<T> &a) const { return Complex<T>(r * a.r - i * a.i, r * a.i + i * a.r); }
//
//    Complex operator*(T a) const { return Complex<T>(r * a, i * a); }
//
//    Complex operator/(const Complex<T> &a) const { return operator*(a.inv()); }
//
//    Complex operator/(T a) const { return Complex<T>(r / a, i / a); }
//
//    Complex operator&(const Complex<T> &a) const { return Complex<T>(r * a.r, i * a.i); }
//
//    Complex &operator+=(const Complex<T> &);
//
//    Complex &operator-=(const Complex<T> &);
//
//    Complex &operator*=(const Complex<T> &);
//
//    Complex &operator/=(const Complex<T> &);
//
//    Complex &operator+=(T);
//
//    Complex &operator-=(T);
//
//    Complex &operator*=(T);
//
//    Complex &operator/=(T);
//
//    bool operator==(const Complex<T> &a) const { return ((i == a.i) && (r == a.r)); }
//
//    bool operator!=(const Complex<T> &a) const { return ((i != a.i) || (r != a.r)); }
//
//    bool operator<(const Complex<T> &a) const { return norm2() < a.norm2(); }
//
//    bool operator>(const Complex<T> &a) const { return norm2() > a.norm2(); }
//
//    Complex inv() const;
//
//    Complex conj() const { return Complex<T>(r, -i); }
//
//    T norm2() const { return i * i + r * r; }
//
//    Complex flip() const { return Complex<T>(i, r); }
//
//    T real() const { return r; }
//
//    T imag() const { return i; }
//
//    Complex neg() const { return Complex<T>(-r, -i); }
//
//    bool isZero() const { return ((r == (T) 0) && (i == (T) 0)); }
//
//    T abs() const { return ::sqrt(norm2()); }
//
//    T arg() const { return ::atan2(i, r); }
//
//    float dB() const { return 10.0 * log10(norm2()); }
//
//    Complex exp() const { return expj(i) * (::exp(r)); }
//
//    Complex unit() const;
//
//    Complex log() const { return Complex(::log(abs()), arg()); }
//
//    Complex pow(double n) const { return expj(arg() * n) * (::pow(abs(), n)); }
//
//    Complex sqrt() const { return pow(0.5); }
//};
//
//
///* standard Complex manifestations */
//typedef Complex<float> complex;
//typedef Complex<double> dcomplex;
//typedef Complex<short> complex16;
//typedef Complex<long> complex32;
//
//
//template<class T>
//inline Complex<T> Complex<T>::inv() const {
//    T nVal;
//
//    nVal = norm2();
//    return Complex<T>(r / nVal, -i / nVal);
//}
//
//template<class T>
//Complex<T> &Complex<T>::operator+=(const Complex<T> &a) {
//    r += a.r;
//    i += a.i;
//    return *this;
//}
//
//template<class T>
//Complex<T> &Complex<T>::operator*=(const Complex<T> &a) {
//    operator*(a);
//    return *this;
//}
//
//template<class T>
//Complex<T> &Complex<T>::operator-=(const Complex<T> &a) {
//    r -= a.r;
//    i -= a.i;
//    return *this;
//}
//
//template<class T>
//Complex<T> &Complex<T>::operator/=(const Complex<T> &a) {
//    operator/(a);
//    return *this;
//}
//
//template<class T>
//Complex<T> &Complex<T>::operator+=(T a) {
//    r += a;
//    return *this;
//}
//
//template<class T>
//Complex<T> &Complex<T>::operator*=(T a) {
//    r *= a;
//    i *= a;
//    return *this;
//}
//
//template<class T>
//Complex<T> &Complex<T>::operator-=(T a) {
//    r -= a;
//    return *this;
//}
//
//template<class T>
//Complex<T> &Complex<T>::operator/=(T a) {
//    r /= a;
//    i /= a;
//    return *this;
//}
//
//template<class T>
//Complex<T> Complex<T>::unit() const {
//    T absVal = abs();
//    return (Complex<T>(r / absVal, i / absVal));
//}
//
//template<class T>
//Complex<T> operator*(T a, const Complex<T> &z) {
//    return Complex<T>(z.r * a, z.i * a);
//}
//
//template<class T>
//Complex<T> operator+(T a, const Complex<T> &z) {
//    return Complex<T>(z.r + a, z.i);
//}
//
//template<class T>
//Complex<T> operator-(T a, const Complex<T> &z) {
//    return Complex<T>(z.r - a, z.i);
//}
//
//
///// e^jphi
//template<class T>
//Complex<T> expj(T phi) {
//    return Complex<T>(cos(phi), sin(phi));
//}
//
//template<class T>
//Complex<T> phasor(T C, T phi) {
//    return (expj(phi) * C);
//}
//
//template<class T>
//std::ostream &operator<<(std::ostream &os, const Complex<T> &z) {
//    os << z.r << ' ';
//    //os << z.r << ", ";
//    //if (z.i>=0) { os << "+"; }
//    os << z.i << "j";
//    return os;
//}
//
//#endif //OBTS_TRANSCEIVER52_COMPLEXCPP_H
