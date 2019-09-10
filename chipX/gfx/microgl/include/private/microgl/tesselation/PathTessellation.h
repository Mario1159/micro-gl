#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma once

#include <microgl/vec2.h>
#include <microgl/triangles.h>
#include <microgl/array_container.h>

namespace tessellation {

#define abs(a) ((a)<0 ? -(a) : (a))
    using index = unsigned int;
    using precision = unsigned char;
    using namespace microgl;

    class PathTessellation {
    public:

        enum class gravity {
            center, inward, outward
        };

        explicit PathTessellation(bool DEBUG = false);;

        void compute(index stroke_size,
                     const vec2_f * $pts,
                     index size,
                     array_container<index> & indices_buffer_tessellation,
                     array_container<vec2_32i> & output_vertices_buffer_tessellation,
                     array_container<triangles::boundary_info> * boundary_buffer,
                     const triangles::TrianglesIndices &requested =
                            triangles::TrianglesIndices::TRIANGLES_STRIP
                            );

        static void compute(int stroke_size,
                     bool closePath,
                     gravity gravity,
                     const vec2_32i * points,
                     index size,
                     precision precision,
                     array_container<index> & indices_buffer_tessellation,
                     array_container<vec2_32i> & output_vertices_buffer_tessellation,
                     array_container<triangles::boundary_info> * boundary_buffer,
                     const triangles::TrianglesIndices &requested =
                            triangles::TrianglesIndices::TRIANGLES_STRIP
                             );

        /**
         * computes the required indices buffer size for requested triangulation
         *
         * @param polygon_size
         * @param requested
         * @return
         */
        static index required_indices_size(index path_size,
                                           const triangles::TrianglesIndices &requested =
                                           triangles::TrianglesIndices::TRIANGLES_STRIP);

        /**
         * computes the required indices buffer size for requested triangulation
         *
         * @param polygon_size
         * @param requested
         * @return
         */
        static index required_vertices_size(index path_size);

    private:

        bool _DEBUG = false;
    };


}

#pragma clang diagnostic pop