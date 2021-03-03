#pragma once

#include <microgl/sampler.h>
#include <microgl/vec2.h>

namespace microgl {
    namespace sampling {

        /**
         * given a line, compute gradient in the [0,1]x[0,1] box.
         * experiment with line inside the [0,1]x[0,1] box in order to see pronounced results.
         * @tparam number
         * @tparam N
         */
        template <typename number, unsigned N=10, typename rgba_= void, enum precision $precision=precision::medium, bool useBigIntegers=false>
        class line_linear_gradient : public sampler<rgba_, line_linear_gradient<number, N, rgba_, $precision, useBigIntegers>> {
            using base= sampler<rgba_, line_linear_gradient<number, N, rgba_, $precision, useBigIntegers>>;
            using rint_big=int64_t;
            using rint= typename microgl::traits::conditional<useBigIntegers, int64_t, int32_t>::type;
            static constexpr precision_t p_bits= static_cast<precision_t>($precision);
            static constexpr precision_t p_bits_double= p_bits<<1;
            using point_int= vec2<rint>;
            using point= vec2<number>;

            // ax + by + c = 0
            struct line_t {
                void updateLine(const point_int &p, const point_int & n) {
                    // a,b are in P bits,but c is in 2P bits.
                    // keeping c in 2P space helps reducing bit shifting (therefore faster evaluation)
                    // in the distance function,
                    // BUT may cause overflow, so keep tabs on it and the distance function
                    a=n.x, b=n.y, c= -(n.dot(p)>>p_bits); // todo
                    rint sqr=microgl::math::sqrt(int64_t (n.x*n.x + n.y*n.y));
                    if(sqr) inv_normal_length = (rint_big(1)<<p_bits_double)/sqr;
                }

                // dist(ax + by + c = 0, (x0, y0)) = (a*x0 + b*y0 + c)/sqrt(a^2 + b^2)
                rint distance(const rint &x, const rint &y) const {
                    // i do it the long way to avoid overflow
                    rint dd = ((((a*x)>>p_bits) + ((b*y)>>p_bits) + c)*inv_normal_length)>>p_bits;
                    return dd;
//                    return (((a*x) + b*y + c)*inv_normal_length)>>p_bits_double;
                }

                bool leftOf(const rint &x, const rint &y) const {
                    return (a*x + b*y + c)<0;
                }

                rint inv_normal_length=0; // 1/ sqrt(a^2 + b^2)
                rint a=0, b=0, c=0;
            };

            struct stop_t {
                rint where=0;
                rint length_inverse=0;
                line_t line{};
                color_t color{};
            };

            static inline
            rint convert(rint from_value, int from_precision, int to_precision) {
                const int pp= int(from_precision) - int(to_precision);
                if(pp==0) return from_value;
                else if(pp > 0) return from_value>>pp;
                else return from_value<<(-pp);
            }

            point_int _start{}, _end{}, _direction{};
            rint _length=0;
            unsigned index= 0;
            stop_t _stops[N];

        public:
            line_linear_gradient() : base{} {}
            line_linear_gradient(const vec2<number> & start, const vec2<number> & end) :
                    line_linear_gradient() {
                setNewLine(start, end);
            };

            void setNewLine(const vec2<number> & start, const vec2<number> & end) {
                const auto dir= end-start;
                const auto length= math::length(dir.x, dir.y);
#define f math::to_fixed
                _start= {f(start.x, p_bits), f(start.y, p_bits)};
                _end= {f(end.x, p_bits), f(end.y, p_bits)};
                _direction= {f(dir.x, p_bits), f(dir.y, p_bits)};
                _length= f(length, p_bits);
#undef f
                reset();
            }

            void addStop(const number & where, const color_t &color) {
                const rint where_fixed= math::to_fixed(where, p_bits);
                const auto p_64= _start+((_direction * where_fixed) >> p_bits);
                auto & stop = _stops[index];

                stop.line.updateLine(p_64, _direction);
                stop.where= where_fixed;
                stop.color= color;

                if(index>0) {
                    rint l= (rint_big(_stops[index].where-_stops[index-1].where)*_length)>>p_bits;
                    rint l_inverse= (rint_big(1)<<p_bits_double)/l;
                    _stops[index].length_inverse= l_inverse;
                }
                index++;
            }

            inline void sample(const int u, const int v,
                               const unsigned bits, color_t &output) const {
                // note:: inside the critical sampling we use only one type of integer, to allow for 32
                //        bit computers for example to be efficient and avoid double registers
                const auto u_tag= convert(u, bits, p_bits);
                const auto v_tag= convert(v, bits, p_bits);
                unsigned pos=0;
                const auto top= index;
                rint distance= 0;
                for (pos=0; pos<top; ++pos) {
                    const rint d= _stops[pos].line.distance(u_tag, v_tag);
                    if(d<0) break;
                    distance=d;
                }
                if(pos==top) {
                    output=_stops[top-1].color; return;
                } else if(pos==0) {
                    output=_stops[0].color; return;
                }
                const auto & stop_0= _stops[pos-1];
                const auto & stop_1= _stops[pos];
                const auto & l_inverse= _stops[pos].length_inverse;
                const rint factor= (distance*l_inverse)>>p_bits;
                output.r= rint(stop_0.color.r) + ((rint(stop_1.color.r-stop_0.color.r)*factor)>>p_bits);
                output.g= rint(stop_0.color.g) + ((rint(stop_1.color.g-stop_0.color.g)*factor)>>p_bits);
                output.b= rint(stop_0.color.b) + ((rint(stop_1.color.b-stop_0.color.b)*factor)>>p_bits);
                output.a= rint(stop_0.color.a) + ((rint(stop_1.color.a-stop_0.color.a)*factor)>>p_bits);
            }

            void reset() {
                index=0;
            }

        private:

        };

    }
}