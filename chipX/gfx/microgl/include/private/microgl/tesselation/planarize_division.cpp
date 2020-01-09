//#include "planarize_division.h"

#include "planarize_division.h"

namespace tessellation {

    // Use the sign of the determinant of vectors (AB,AM), where M(X,Y) is the query point:
    // position = sign((Bx - Ax) * (Y - Ay) - (By - Ay) * (X - Ax))
    //    Input:  three points p, a, b
    //    Return: >0 for P2 left of the line through a and b
    //            =0 for P2  on the line
    //            <0 for P2  right of the line
    //    See: Algorithm 1 "Area of Triangles and Polygons"
    template <typename number>
    inline int
    planarize_division<number>::classify_point(const vertex & point, const vertex &a, const vertex & b)
    {
        auto result = ((b.x - a.x) * (point.y - a.y)
                       - (point.x - a.x) * (b.y - a.y) );

        if(result <0)
            return 1;
        else if(result > 0)
            return -1;
        else return 0;
    }

    template<typename number>
    auto planarize_division<number>::create_frame(const chunker<vertex> &pieces,
                                                  static_pool & pool) -> half_edge_face * {
        // find bbox of all
        const auto pieces_length = pieces.size();

        vertex left_top=pieces.raw_data()[0];
        vertex right_bottom=left_top;

        for (int ix = 0; ix < pieces_length; ++ix) {
            auto const piece = pieces[ix];
            const auto piece_size = piece.size;
            for (int jx = 0; jx < piece_size; ++jx) {
                const auto & current_vertex = piece.data[jx];
                if(current_vertex.x<left_top.x)
                    left_top.x = current_vertex.x;
                if(current_vertex.y<left_top.y)
                    left_top.y = current_vertex.y;
                if(current_vertex.x>right_bottom.x)
                    right_bottom.x = current_vertex.x;
                if(current_vertex.y>right_bottom.y)
                    right_bottom.y = current_vertex.y;

            }
        }

        left_top.x -= number(10);
        left_top.y -= number(10);
        right_bottom.x += number(10);
        right_bottom.y += number(10);

        auto * v0 = pool.get_vertex();
        auto * v1 = pool.get_vertex();
        auto * v2 = pool.get_vertex();
        auto * v3 = pool.get_vertex();

        v0->coords = left_top;
        v1->coords = {left_top.x, right_bottom.y};
        v2->coords = right_bottom;
        v3->coords = {right_bottom.x, left_top.y};

        // note, no need for twin edges
        auto * edge_0 = pool.get_edge();
        auto * edge_1 = pool.get_edge();
        auto * edge_2 = pool.get_edge();
        auto * edge_3 = pool.get_edge();

        // ccw from left-top vertex
        edge_0->origin = v0;
        edge_1->origin = v1;
        edge_2->origin = v2;
        edge_3->origin = v3;

        edge_0->winding = 0;
        edge_1->winding = 0;
        edge_2->winding = 0;
        edge_3->winding = 0;

        v0->edge = edge_0;
        v1->edge = edge_1;
        v2->edge = edge_2;
        v3->edge = edge_3;

        edge_0->next = edge_1;
        edge_1->next = edge_2;
        edge_2->next = edge_3;
        edge_3->next = edge_0;

        edge_0->prev = edge_3;
        edge_1->prev = edge_0;
        edge_2->prev = edge_1;
        edge_3->prev = edge_2;

        auto * face = pool.get_face();
        edge_0->face = face;
        edge_1->face = face;
        edge_2->face = face;
        edge_3->face = face;

        // CCW around face, face is always to the left of the walk
        face->edge = edge_0;

        return face;
        // note: we might find out that edge-face pointer and edge-prev are not required.
    }

    template<typename number>
    auto planarize_division<number>::build_edges_and_conflicts(const chunker<vertex> &pieces,
                                                               half_edge_face & main_frame,
                                                               static_pool & pool) -> half_edge ** {

        const auto pieces_length = pieces.size();
        conflict * conflict_first = nullptr;
        //conflict * conflict_last = nullptr;
        const auto v_size = pieces.unchunked_size();
        auto ** edges_list = new half_edge*[v_size];
        index edges_list_counter = 0;

        // build edges and twins, do not make next/prev connections
        for (int ix = 0; ix < pieces_length; ++ix) {
            half_edge * edge_first = nullptr;
            half_edge * edge_last = nullptr;

            auto const piece = pieces[ix];
            const auto piece_size = piece.size;
            for (int jx = 0; jx < piece_size; ++jx) {
                // v, e, c
                auto * v = pool.get_vertex();
                auto * e = pool.get_edge();
                auto * c = pool.get_conflict_node();

                // hook current v to e and c
                v->coords = piece.data[jx];
                v->edge = e;
                e->origin = v;
                e->type = edge_type::unknown;
                // only the first half edge records conflict info, the twin will not record !!
                e->conflict_face = &main_frame;
                c->edge = e;
                edges_list[edges_list_counter++] = e;

                // record first edge of polygon
                if(edge_first==nullptr) {
                    edge_first = e;
                }

                // set last twin
                if(edge_last) {
                    auto * e_last_twin = pool.get_edge();
                    e_last_twin->origin = v;
                    edge_last->twin = e_last_twin;
                    e_last_twin->twin = edge_last;
                }

//                if(conflict_last) {
//                    conflict_last->next = c;
//                } else {
//                    conflict_first = c;
//                }

                if(conflict_first)
                    c->next = conflict_first;

                conflict_first = c;
                edge_last = e;
                //conflict_last = c;
            }

            // hook the last edge, from last vertex into first vertex
            // of the current polygon
            if(edge_last) {
                auto * e_last_twin = pool.get_edge();
                e_last_twin->origin = edge_first->origin;
                edge_last->twin = e_last_twin;
                e_last_twin->twin = edge_last;
            }

        }

        main_frame.conflict_list = conflict_first;

        return edges_list;
    }

    template<typename number>
    auto planarize_division<number>::infer_trapeze(const half_edge_face *face) -> trapeze_t {
        auto * e = face->edge;
        const auto * e_end = face->edge;
        trapeze_t trapeze;
        trapeze.left_top = trapeze.left_bottom = trapeze.right_bottom = trapeze.right_top = e;

        do {
            const auto * v = e->origin;
            auto curr_x = v->coords.x;
            auto curr_y = v->coords.y;

            if(curr_x < trapeze.left_top->origin->coords.x || (curr_x == trapeze.left_top->origin->coords.x && curr_y < trapeze.left_top->origin->coords.y))
                trapeze.left_top = e;
            if(curr_x < trapeze.left_bottom->origin->coords.x || (curr_x == trapeze.left_bottom->origin->coords.x && curr_y > trapeze.left_bottom->origin->coords.y))
                trapeze.left_bottom = e;
            if(curr_x > trapeze.right_bottom->origin->coords.x || (curr_x == trapeze.right_bottom->origin->coords.x && curr_y > trapeze.right_bottom->origin->coords.y))
                trapeze.right_bottom = e;
            if(curr_x > trapeze.right_top->origin->coords.x || (curr_x == trapeze.right_top->origin->coords.x && curr_y < trapeze.right_top->origin->coords.y))
                trapeze.right_top = e;

            e=e->next;
        } while(e!=e_end);

        return trapeze;
    }

    template<typename number>
    auto planarize_division<number>::classify_point_conflicting_trapeze(vertex & point, const trapeze_t &trapeze) -> point_class_with_trapeze {
        // given that point that should belong to trapeze, BUT might have not been added yet: completely inside or on boundary
        // we assume that the vertex DOES belong to the trapeze even if it may
        // be outside. WE ASSUME, that if it is outside or what not, then it is because
        // of numeric precision errors, that were carried over because vertical splits.
        // This can happen only for y coordinates because x coords are very precise. In this case,
        // we clamp the coord, if it flows the left/right walls y coords. As for inaccurate y coords
        // strictly inside the bottom/top walls, we report the point is inside without fixing
        // it's position, because other procedures can handle this case gracefully (as far as
        // I am aware of it).
        if(point==trapeze.left_top->origin->coords) return point_class_with_trapeze::boundary_vertex;
        if(point==trapeze.left_bottom->origin->coords) return point_class_with_trapeze::boundary_vertex;
        if(point==trapeze.right_bottom->origin->coords) return point_class_with_trapeze::boundary_vertex;
        if(point==trapeze.right_top->origin->coords) return point_class_with_trapeze::boundary_vertex;

        // left wall
        bool on_left_wall = point.x==trapeze.left_top->origin->coords.x;
        if(on_left_wall) {
            clamp(point.y, trapeze.left_top->origin->coords.y, trapeze.left_bottom->origin->coords.y);
            if(point==trapeze.left_top->origin->coords) return point_class_with_trapeze::boundary_vertex;
            if(point==trapeze.left_bottom->origin->coords) return point_class_with_trapeze::boundary_vertex;
            return point_class_with_trapeze::left_wall;
        }
        // right wall
        bool on_right_wall = point.x==trapeze.right_top->origin->coords.x;
        if(on_right_wall) {
            clamp(point.y, trapeze.right_top->origin->coords.y, trapeze.right_bottom->origin->coords.y);
            if(point==trapeze.right_top->origin->coords) return point_class_with_trapeze::boundary_vertex;
            if(point==trapeze.right_bottom->origin->coords) return point_class_with_trapeze::boundary_vertex;
            return point_class_with_trapeze::right_wall;
        }
        // bottom wall
        int bottom_wall = classify_point(point, trapeze.left_bottom->origin->coords, trapeze.right_bottom->origin->coords);
        if(bottom_wall==0) return point_class_with_trapeze::bottom_wall;
        // top wall
        int top_wall = classify_point(point, trapeze.right_top->origin->coords, trapeze.left_top->origin->coords);
        if(top_wall==0) return point_class_with_trapeze::top_wall;

        return point_class_with_trapeze::strictly_inside;
    }

    template<typename number>
    auto planarize_division<number>::locate_and_classify_point_that_is_already_on_trapeze(vertex & point,
            const trapeze_t &trapeze) -> point_class_with_trapeze {
        // given that the point IS present on the trapeze boundary, as opposed to a conflicting point
        // that is classified to belong but was not added yet.
        // this procedure is very ROBUST, and may be optimized later with other methods
        if(point==trapeze.left_top->origin->coords) return point_class_with_trapeze::boundary_vertex;
        if(point==trapeze.left_bottom->origin->coords) return point_class_with_trapeze::boundary_vertex;
        if(point==trapeze.right_bottom->origin->coords) return point_class_with_trapeze::boundary_vertex;
        if(point==trapeze.right_top->origin->coords) return point_class_with_trapeze::boundary_vertex;

        // left wall
        bool on_left_wall = point.x==trapeze.left_top->origin->coords.x;
        if(on_left_wall) {
            return point_class_with_trapeze::left_wall;
        }
        // right wall
        bool on_right_wall = point.x==trapeze.right_top->origin->coords.x;
        if(on_right_wall) {
            return point_class_with_trapeze::right_wall;
        }

        auto * start = trapeze.left_bottom;
        auto * end = trapeze.right_bottom;

        while(start!=end) {
            if(point==start->origin->coords)
                return point_class_with_trapeze::bottom_wall;
            start=start->next;
        }
        start = trapeze.right_top;
        end = trapeze.left_top;
        while(start!=end) {
            if(point==start->origin->coords)
                return point_class_with_trapeze::top_wall;
            start=start->next;
        }

        return point_class_with_trapeze::unknown;
    }

    template<typename number>
    auto planarize_division<number>::classify_arbitrary_point_with_trapeze(const vertex & point, const trapeze_t &trapeze) -> point_class_with_trapeze {
        // given any point, classify it against the trapeze, this is robust
        if(point==trapeze.left_top->origin->coords) return point_class_with_trapeze::boundary_vertex;
        if(point==trapeze.left_bottom->origin->coords) return point_class_with_trapeze::boundary_vertex;
        if(point==trapeze.right_bottom->origin->coords) return point_class_with_trapeze::boundary_vertex;
        if(point==trapeze.right_top->origin->coords) return point_class_with_trapeze::boundary_vertex;

        // left wall
        number left_wall = point.x-trapeze.left_top->origin->coords.x;
        number right_wall = -point.x+trapeze.right_top->origin->coords.x;
        int bottom_wall = classify_point(point, trapeze.left_bottom->origin->coords, trapeze.right_bottom->origin->coords);
        int top_wall = classify_point(point, trapeze.right_top->origin->coords, trapeze.left_top->origin->coords);

        bool inside_or_boundary = left_wall>=0 && right_wall>=0 && bottom_wall>=0 && top_wall>=0;
        if(inside_or_boundary) {
            if(left_wall==0) return point_class_with_trapeze::left_wall;
            if(right_wall==0) return point_class_with_trapeze::right_wall;
            if(bottom_wall==0) return point_class_with_trapeze::bottom_wall;
            if(top_wall==0) return point_class_with_trapeze::top_wall;

            return point_class_with_trapeze::strictly_inside;
        }
        return point_class_with_trapeze::outside;
    }

    // note: two vertices on the same wall are symbolically on a straight line if if they have numeric inaccuracies

    template<typename number>
    auto planarize_division<number>::try_split_edge_at(const vertex& point,
                                                       half_edge *edge, dynamic_pool & pool) -> half_edge * {
        // let's shorten both edge and it's twin,each from it's side
        // main frame does not have twins because are needed.
        //  --e0---*--e1--->
        // <--et1--*--et0--
        // if the point is already one of the edges endpoints skip fast
        if(point==edge->origin->coords)
            return edge;
        bool has_twin = edge->twin!=nullptr;
        if(has_twin && point==edge->twin->origin->coords)
            return edge->twin;

        auto * e_0 = edge;
        auto * e_1 = pool.create_edge();
        auto * e_t_0 = edge->twin;
        auto * e_t_1 = has_twin ? pool.create_edge() : nullptr;

        e_1->origin = pool.create_vertex(point);
        e_1->prev = e_0;
        e_1->next = e_0->next;
        e_0->next->prev = e_1;

        e_0->next = e_1;
        // inherit face and winding properties
        e_1->face = e_0->face;
        e_1->winding = e_0->winding;

        if(has_twin) {
            e_t_1->origin = e_1->origin;
            e_t_1->prev = e_t_0;
            e_t_1->next = e_t_0->next;
            e_t_0->next->prev = e_t_1;

            e_t_1->twin = e_0;
            e_0->twin = e_t_1;

            e_t_0->next = e_t_1;
            e_t_0->twin = e_1;
            e_1->twin = e_t_0;
            // inherit face and winding properties
            e_t_1->face = e_t_0->face;
            e_t_1->winding = e_t_0->winding;

        }

        // make sure point refers to any edge leaving it
        e_1->origin->edge = e_1;

        return e_1;
    }

    template<typename number>
    auto planarize_division<number>::try_insert_vertex_on_trapeze_boundary_at(const vertex & v,
                                                                              const trapeze_t & trapeze,
                                                                              point_class_with_trapeze where_boundary,
                                                                              dynamic_pool & pool) -> half_edge * {
        // given where on the boundary: left, top, right, bottom
        // walk along that boundary ray and insert the vertex at the right place.
        // if the vertex already exists, do nothing ?
        // otherwise, insert a vertex and split the correct edge segment of the ray.
        // at the end, return the corresponding half-edge, whose origin is the vertex.
        // note:: it is better to return the edge than only the vertex, because
        // the edge locates the vertex, while the vertex locates any edge that leaves it,
        // and therefore would require traversing around the vertex to find the edge that belongs
        // to the face we are interested in.
        half_edge * e_start = nullptr;
        half_edge * e_end = nullptr;
        bool compare_y = false;
        bool reverse_direction = false;

        switch (where_boundary) {
            case point_class_with_trapeze::strictly_inside:
                return nullptr;
            case point_class_with_trapeze::boundary_vertex:
                if(v==trapeze.left_top->origin->coords) return trapeze.left_top;
                if(v==trapeze.left_bottom->origin->coords) return trapeze.left_bottom;
                if(v==trapeze.right_bottom->origin->coords) return trapeze.right_bottom;
                if(v==trapeze.right_top->origin->coords) return trapeze.right_top;
                break;
            case point_class_with_trapeze::left_wall:
                e_start = trapeze.left_top; e_end = trapeze.left_bottom->next;
                compare_y=true;reverse_direction=false;
                break;
            case point_class_with_trapeze::right_wall:
                e_start = trapeze.right_bottom; e_end = trapeze.right_top->next;
                compare_y=true;reverse_direction=true;
                break;
            case point_class_with_trapeze::top_wall:
                e_start = trapeze.right_top; e_end = trapeze.left_top->next;
                compare_y=false;reverse_direction=true;
                break;
            case point_class_with_trapeze::bottom_wall:
                e_start = trapeze.left_bottom; e_end = trapeze.right_bottom->next;
                compare_y=false;reverse_direction=false;
                break;
            default:
                return nullptr;

        }

        const auto * e_end_ref = e_end;

        do {
            auto coord_0 = compare_y ? e_start->origin->coords.y : e_start->origin->coords.x;
            auto coord_1 = compare_y ? e_start->next->origin->coords.y : e_start->next->origin->coords.x;
            auto v_coord = compare_y ? v.y : v.x;
            bool on_segment = reverse_direction ? v_coord>coord_1 : v_coord<coord_1;

            // there can only be one vertex on the principal axis with the same 1D coord(x or y)
            // v lies on e
            if(on_segment) {
                // check endpoints first
                if(v_coord==coord_0)
                    return e_start;
                return try_split_edge_at(v, e_start, pool);
            }
            e_start=e_start->next;
        } while(e_start!=e_end_ref);

    }

    template<typename number>
    number planarize_division<number>::evaluate_line_at_x(const number x, const vertex &a, const vertex &b) {
        auto slope = (b.y-a.y)/(b.x-a.x);
        auto y = a.y + slope*(x-a.x);
        return y;
    }

    /*
    template<typename number>
    void planarize_division<number>::remove_edge_from_conflict_list(half_edge * edge) {
        auto * conflict_face = edge->origin->conflict_face;
        conflict * list_ref = conflict_face->conflict_list;
        conflict * list_ref_prev = nullptr;
        const auto * edge_ref = edge;

        while(list_ref!= nullptr) {
            if(list_ref->edge==edge_ref) {

                if(list_ref_prev!=nullptr)
                    list_ref_prev->next = list_ref->next;
                else // in case, the first node was to be removed
                    conflict_face->conflict_list=list_ref->next;

                // reset the node
                list_ref->next=nullptr;

                break;
            }

            list_ref_prev = list_ref;
            list_ref=list_ref->next;
        }

        // clear the ref from vertex to face
        edge->origin->conflict_face = nullptr;
    }
     */

    template<typename number>
    void planarize_division<number>::re_distribute_conflicts_of_split_face(conflict *conflict_list,
                                                                           const half_edge* face_separator) {
        // given that a face f was just split into two faces with
        // face_separator edge, let's redistribute the conflicts

        auto * f1 = face_separator->face;
        auto * f2 = face_separator->twin->face;

        f1->conflict_list = nullptr;
        f2->conflict_list = nullptr;

        conflict * list_ref = conflict_list;

        while(list_ref!= nullptr) {
            auto * current_ref = list_ref;  // record head
            list_ref=list_ref->next;        // advance
            current_ref->next = nullptr;    // de-attach
            auto * e = current_ref->edge;
            // find the face to which the edge is classified
            auto * f = classify_conflict_against_two_faces(face_separator, e);
            // insert edge into head of conflict list
            current_ref->next = f->conflict_list;
            f->conflict_list = current_ref;
            // pointer from edge to conflicting face
            e->conflict_face=f;
        }


        /*

        auto * f1 = face_separator->face;
        auto * f2 = face_separator->twin->face;

        f1->conflict_list = nullptr;
        f2->conflict_list = nullptr;

        conflict * list_ref = conflict_list;

        while(list_ref!= nullptr) {
            auto * current_ref = list_ref;
            // de-attach and advance
            current_ref->next = nullptr;
            auto * e = current_ref->edge;
            // find the face to which the edge is classified
            auto * f = classify_conflict_against_two_faces(face_separator, e);
            // insert edge into head of conflict list
            current_ref->next = f->conflict_list;
            f->conflict_list = current_ref;
            // pointer from edge to conflicting face
            e->conflict_face=f;
            list_ref=list_ref->next; // advance
        }
         */

    }

    template<typename number>
    auto planarize_division<number>::classify_conflict_against_two_faces(const half_edge* face_separator,
                                                                         const half_edge* edge) -> half_edge_face *{
        // note:: edge's face always points to the face that lies to it's left.
        // 1. if the first point lie completely to the left of the edge, then they belong to f1, other wise f2
        // 2. if they lie exactly on the edge, then we test the second end-point
        // 2.1 if the second lies completely in f1, then the first vertex is in f1, otherwise f2
        // 2.2 if the second lies on the face_separator as well, choose whoever face you want, let's say f1
        const auto & a = face_separator->origin->coords;
        const auto & b = face_separator->twin->origin->coords;
        const auto & c = edge->origin->coords;
        const auto & d = edge->twin->origin->coords;
        int cls = classify_point(c, a, b);

        if(cls>0) // if strictly left of
            return face_separator->face; // face points always to the face left of edge
        else if(cls<0)// if strictly right of
            return face_separator->twin->face;
        else { // is exactly on separator
            int cls2 = classify_point(d, a, b);
            if(cls2>0)// if strictly left of
                return face_separator->face;
            else if(cls2<0)// if strictly right of
                return face_separator->twin->face;
        }
        // both lie on the separator
        return face_separator->face;
    }

    template<typename number>
    void planarize_division<number>::walk_and_update_edges_face(half_edge * edge_start, half_edge_face * face) {
        // If I only use this once, then embed it inside it's only consumer
        auto * e_ref = edge_start;
        const auto * e_end = edge_start;
        do {
            e_ref->face = face;
            e_ref = e_ref->next;
        } while(e_ref!=nullptr && e_ref!=e_end);
    }

    template<typename number>
    auto planarize_division<number>::insert_edge_between_non_co_linear_vertices(half_edge *vertex_a_edge,
                                                                                half_edge *vertex_b_edge,
                                                                                dynamic_pool & pool) -> half_edge * {
        // insert edge between two vertices in a face, that are not co linear located
        // by their leaving edges. co linearity means the vertices lie on the same boundary ray.
        // for that case, we have a different procedure to handle.
        // first create two half edges
        auto * face = vertex_a_edge->face;
        auto * face_2 = pool.create_face();

        auto * e = pool.create_edge();
        auto * e_twin = pool.create_edge();
        // e is rooted at a, e_twin at b
        e->origin = vertex_a_edge->origin;
        e_twin->origin = vertex_b_edge->origin;
        // make them twins
        e->twin = e_twin;
        e_twin->twin = e;
        // rewire outgoing
        vertex_a_edge->prev->next = e;
        e->prev = vertex_a_edge->prev;
        vertex_b_edge->prev->next = e_twin;
        e_twin->prev = vertex_b_edge->prev;
        // rewire incoming
        e->next = vertex_b_edge;
        vertex_b_edge->prev = e;
        e_twin->next = vertex_a_edge;
        vertex_a_edge->prev = e_twin;

        // now, we need to update conflicts lists and link some edges to their new faces
        // so, now face will be f1, the face left of e. while e-twin will have f2 as left face
        e->face = face;
        e_twin->face = face_2;
        face->edge= e;
        face_2->edge= e_twin;
        // now, iterate on all of the conflicts of f, and move the correct ones to f2
        re_distribute_conflicts_of_split_face(e->face->conflict_list, e);
        // all edges of face_2 point to the old face_1, we need to change that
        walk_and_update_edges_face(e_twin, face_2);
        return e;
    }


    template<typename number>
    auto planarize_division<number>::handle_vertical_face_cut(const trapeze_t &trapeze,
                                                              vertex & a,
                                                              const point_class_with_trapeze &a_classs,
                                                              dynamic_pool & dynamic_pool) -> vertical_face_cut_result {
        // the procedure returns the half edge of the face for which it's origin vertex
        // is the insertion point. the half_edge_vertex *a will be inserted in place if it didn't exist yet.
        // the mission of this method is to split a little to create an intermediate face
        // for the regular procedure, this procedure might split a face to two or just
        // split an edge on which the vertex lies on.
        // todo: what about degenerate trapeze with three vertices ?
        // IMPORTANT NOTE:
        // suppose we have an edge E that is conflicting with a trapeze, and we split the trapeze
        // vertically into two trapezes because of another edge, also suppose that E was sitting on
        // the boundary or very close to the boundary of the top and bottom rays. after the split,
        // because of numeric precision, we should expect, that the classification of the edge should be
        // wrong against one of those rays. we need to deal with this:
        // 1. if E was on the left/right boundary exactly on the endpoints, then we need to clamp it
        // 2. if E was strictly inside the bottom/top rays, we can proceed normally with the x split,
        //    BUT, when we create the vertical edge, we need to clamp E against that edge !!!

        // so,
        // two things can occur, the vertex is completely inside the trapeze
        // 1. the vertex is strictly inside the horizontal part of the trapeze,
        //    therefore, add a vertical segment that cuts the top/bottom segments
        // 1.1. this also induces a splitting of the face into two faces including updating
        //      references and conflicts
        // 1.2. if the vertex was completely inside the trapeze(not on the top/bottom boundary),
        //      then split that vertical line as well. Note, that the vertical line is unique because
        //      of the strict horizontal inclusion.
        // 2. the vertex is on the left/right walls (including endpoints, i.e not necessarily strict inside),
        //    in this case just split the vertical left or right walls vertically.
        // 3. btw, in case, a vertex already exists, we do not split anything

        // boundary
        bool on_boundary = a_classs != point_class_with_trapeze::strictly_inside;
        bool on_boundary_vertices = a_classs == point_class_with_trapeze::boundary_vertex;
        // completely inside minus the endpoints
        bool in_left_wall = a_classs == point_class_with_trapeze::left_wall;
        bool in_right_wall = a_classs == point_class_with_trapeze::right_wall;
        bool in_top_wall = a_classs == point_class_with_trapeze::top_wall;
        bool in_bottom_wall = a_classs == point_class_with_trapeze::bottom_wall;
        bool strictly_in_left_or_right_walls = (in_left_wall || in_right_wall) && !on_boundary_vertices;
        // should split the horizontal parts of the trapeze
        bool should_try_split_horizontal_trapeze_parts = !in_left_wall && !in_right_wall && !on_boundary_vertices;
        vertical_face_cut_result result{};
        result.left_trapeze = result.right_trapeze = trapeze;
        // vertical edge that a sits on
        half_edge * outgoing_vertex_edge=nullptr;
        // should split the horizontal parts of the trapeze
        if(should_try_split_horizontal_trapeze_parts) {
            half_edge *top_vertex_edge= nullptr, *bottom_vertex_edge= nullptr;
            // we are on the top or bottom boundary, we should try insert a vertex there,
            // this helps with future errors like geometric rounding, because the point is
            // already present there.
            if(on_boundary) {
                // split boundary and return the edge whose origin is the split vertex
                half_edge * e = try_insert_vertex_on_trapeze_boundary_at(a, trapeze, a_classs,
                        dynamic_pool);
                if(in_top_wall) top_vertex_edge=e;
                else bottom_vertex_edge=e;
                outgoing_vertex_edge = e;
            }

            if(top_vertex_edge== nullptr) {
                auto y = evaluate_line_at_x(a.x, trapeze.left_top->origin->coords,
                        trapeze.right_top->origin->coords);
                top_vertex_edge=try_insert_vertex_on_trapeze_boundary_at({a.x,y}, trapeze,
                                                                         point_class_with_trapeze::top_wall,
                                                                         dynamic_pool);
            }

            if(bottom_vertex_edge== nullptr) {
                auto y = evaluate_line_at_x(a.x, trapeze.left_bottom->origin->coords,
                        trapeze.right_bottom->origin->coords);
                bottom_vertex_edge=try_insert_vertex_on_trapeze_boundary_at({a.x,y}, trapeze,
                                                                            point_class_with_trapeze::bottom_wall,
                                                                            dynamic_pool);
            }

            // now, we need to split face in two
            // edge cannot exist yet because we are strictly inside horizontal part.
            // we insert a vertical edge, that starts at bottom edge into the top wall (bottom-to-top)
            auto *start_vertical_wall = insert_edge_between_non_co_linear_vertices(bottom_vertex_edge,
                    top_vertex_edge,dynamic_pool);
            // clamp vertex to this new edge endpoints if it is before or after
            // this fights the geometric numeric precision errors, that can happen in y coords
            clamp_vertex(a, start_vertical_wall->origin->coords,
                    start_vertical_wall->twin->origin->coords);
            // update resulting trapezes
            result.left_trapeze.right_bottom = start_vertical_wall;
            result.left_trapeze.right_top = start_vertical_wall->next;

            result.right_trapeze.left_top = start_vertical_wall->twin;
            result.right_trapeze.left_bottom = start_vertical_wall->twin->next;
            // if the vertex is on the edge boundary, it will not split of course
            outgoing_vertex_edge = try_split_edge_at(a, start_vertical_wall, dynamic_pool);
        } // else we are on left or right walls already
        else {
            // we are on left or right boundary
            outgoing_vertex_edge=try_insert_vertex_on_trapeze_boundary_at(a, trapeze,
                                                                          a_classs, dynamic_pool);
        }

        result.face_was_split = should_try_split_horizontal_trapeze_parts;
        result.vertex_a_edge_split_edge = outgoing_vertex_edge;

        return result;
    }

    template<typename number>
    void planarize_division<number>::handle_co_linear_edge_with_trapeze(const trapeze_t &trapeze,
                                                                       const vertex & a,
                                                                       const vertex & b,
                                                                       const point_class_with_trapeze &wall_class,
                                                                       half_edge ** result_edge_a,
                                                                       half_edge ** result_edge_b,
                                                                       dynamic_pool& pool) {
        // given that (a,b) is co-linear with one of the 4 boundaries of the trapeze,
        // try inserting vertices on that boundary, update windings between them and return the
        // corresponding start and end half edges whose start points are a and b
        auto * edge_vertex_a =
                try_insert_vertex_on_trapeze_boundary_at(a, trapeze, wall_class, pool);
        auto * edge_vertex_b =
                try_insert_vertex_on_trapeze_boundary_at(b, trapeze, wall_class, pool);

        if(result_edge_a)
            *result_edge_a = edge_vertex_a;
        if(result_edge_b)
            *result_edge_b = edge_vertex_b;

        // find out winding
        const int winding = infer_edge_winding(a, b);
        if(winding==0)
            return;

        auto *start = edge_vertex_a;
        auto *end = edge_vertex_b;
        // sort if needed
        bool before = is_a_before_or_equal_b_on_same_boundary(a, b, wall_class);
        if(!before) {
            auto *temp=start;start=end;end=temp;
        }

        while(start!=end) {
            start->winding+=winding;
            start->twin->winding=start->winding;
            start=start->next;
        }

    }

    template<typename number>
    bool planarize_division<number>::is_a_before_or_equal_b_on_same_boundary(const vertex &a, const vertex &b,
            const point_class_with_trapeze &wall) {
        switch (wall) {
            case point_class_with_trapeze::left_wall:
                return (a.y<=b.y);
            case point_class_with_trapeze::right_wall:
                return (b.y<=a.y);
            case point_class_with_trapeze::bottom_wall:
                return (a.x<=b.x);
            case point_class_with_trapeze::top_wall:
                return (b.x<=a.x);
            default: // this might be problematic, we may need to report error
                return false;
        }

    }

    template<typename number>
    auto planarize_division<number>::locate_half_edge_of_face_rooted_at_vertex(const half_edge_vertex *root,
                                                                               const half_edge_face * face) -> half_edge * {
        auto *iter = root->edge;
        const auto *end = root->edge;
        do {
            if(iter->face==face)
                return iter;
            iter=iter->twin->next;
        } while(iter!=end);

        return nullptr;
    }

    template<typename number>
    auto planarize_division<number>::locate_face_of_a_b(const half_edge_vertex &a,
                                                        const vertex &b) -> half_edge * {
        // given edge (a,b) as half_edge_vertex a and a vertex b, find out to which
        // adjacent face does this edge should belong. we return the half_edge that
        // has this face to it's left and vertex 'a' as an origin. we walk CW around
        // the vertex to find which subdivision. The reason that the natural order around
        // a vertex is CW is BECAUSE, that the face edges are CCW. If one draws it on paper
        // then it will become super clear.
        // NOTE:: another method to compute the face is to test for cone inclusion, which
        //        might be faster because if we find a cone we are done, but it is more error
        //        prone to implement.
        half_edge *iter = a.edge;
        half_edge *candidate = nullptr;
        const half_edge *end = iter;
        // we walk in CCW order around the vertex
        do {
            int cls= classify_point(b, iter->origin->coords,
                    iter->twin->origin->coords);
            if(cls>=0) { // b is strictly to left of the edge, it is a candidate
                if(candidate== nullptr)
                    candidate = iter;
                else {
                    // we have to test if current candidate is more left than new one
                    int cls_2= classify_point(iter->twin->origin->coords, candidate->origin->coords,
                                              candidate->twin->origin->coords);
                    if(cls_2>=0)
                        candidate= iter;
                }
            }
            if(cls<0) { // b is strictly to right of the edge
                // if we had a candidate, then because we walk CCW, then the last
                // candidate had to be the best candidate because we transitioned
                // from left to right classification
                // this is an early dropout optimization
                if(candidate)
                    return candidate;
                // else do nothing
            }

            iter=iter->twin->next;
        } while(iter!=end);

        return candidate;

        /*
                 half_edge *iter = a.edge;
        half_edge *candidate = nullptr;
        const half_edge *end = iter;
        // we walk in CCW order around the vertex
        do {
            int cls= classify_point(b, iter->origin->coords,
                    iter->twin->origin->coords);
            if(cls==0) // b is exactly on the edge, bingo
                return iter;
            if(cls>0) // b is strictly to left of the edge, it is a candidate
                candidate= iter;
            if(cls<0) { // b is strictly to right of the edge
                // if we had a candidate, then because we walk CCW, then the last
                // candidate had to be the best candidate because we transitioned
                // from left to right classification
                if(candidate)
                    return candidate;
                // else do nothing
            }

            iter=iter->twin->next;
        } while(iter!=end);

        return candidate;

         */
    }

    template<typename number>
    auto planarize_division<number>::handle_face_split(const trapeze_t & trapeze,
                                                       vertex &a, vertex &b,
                                                       const point_class_with_trapeze &a_class,
                                                       const point_class_with_trapeze &b_class,
                                                       dynamic_pool & dynamic_pool) -> half_edge * {
        // given that edge (a,b) should split the face, i.e inside the face and is NOT
        // co-linear with a boundary wall of trapeze (for that case we have another procedure),
        // split the face to up to 4 pieces, strategy is:
        // 1. use a vertical that goes through "a" to try and split the trapeze
        // 2. if a cut has happened, two trapezes were created and "a" now lies on
        //    the vertical wall between them.
        // 3. infer the correct trapeze where "b" is in and perform roughly the same
        //    operation
        // 4. connect the two points, so they again split a face (in case they are not vertical)

        int winding = infer_edge_winding(a, b);
        // first, try to split vertically with a endpoint
        vertical_face_cut_result a_cut_result = handle_vertical_face_cut(trapeze,
                a, a_class, dynamic_pool);
        // let's find out where b ended up after a split (that might have not happened)
        const trapeze_t * trapeze_of_b = &trapeze;
        if(a_cut_result.face_was_split) {
            // a vertical split happened, let's see where b is
            bool left = b.x<=a_cut_result.vertex_a_edge_split_edge->origin->coords.x;
            trapeze_of_b = left ? &a_cut_result.left_trapeze : &a_cut_result.right_trapeze;
        }
        // since, we deem b belongs to the trapeze, we classify with the following method
        point_class_with_trapeze b_new_class = classify_point_conflicting_trapeze(b, *trapeze_of_b);
        // next, try to split trapeze_of_b vertically with b endpoint
        vertical_face_cut_result b_cut_result = handle_vertical_face_cut(*trapeze_of_b,
                b, b_new_class, dynamic_pool);
        const trapeze_t *mutual_trapeze = trapeze_of_b;
        // now, infer the mutual trapeze of a and b after the vertical splits have occured
        if(b_cut_result.face_was_split) {
            // a vertical split happened, let's see where b is
            bool left = a.x<=b_cut_result.vertex_a_edge_split_edge->origin->coords.x;
            mutual_trapeze = left ? &b_cut_result.left_trapeze : &b_cut_result.right_trapeze;
        }

        // two options ?
        // 1. (a,b) is vertical, need to only update it's winding
        // 2. (a,b) is NOT vertical, connect a new edge between them, split
        // 3. BUT, first we need to infer the correct half edges, that go out of a and b
        //    and have the mutual face to their left

        // any edge of the trapeze of b will tell us the face
        const auto * face = mutual_trapeze->right_bottom->face;
        // a_edge/b_edge both are half edges rooted at a/b respectively and such that both belong to same face
        auto * a_edge = locate_half_edge_of_face_rooted_at_vertex(a_cut_result.vertex_a_edge_split_edge->origin, face);
        auto * b_edge = locate_half_edge_of_face_rooted_at_vertex(b_cut_result.vertex_a_edge_split_edge->origin, face);
        auto * inserted_edge = a_edge;
        bool is_a_b_vertical = a_edge->origin->coords.x==b_edge->origin->coords.x;
        if(!is_a_b_vertical)
            inserted_edge = insert_edge_between_non_co_linear_vertices(a_edge, b_edge,
                    dynamic_pool);

        inserted_edge->winding += winding;
        // a_edge->twin is b_edge
        inserted_edge->twin->winding = inserted_edge->winding;

        return inserted_edge;
    }

    template<typename number>
    auto planarize_division<number>::handle_face_merge(const half_edge_vertex *vertex_on_vertical_wall
                                                       ) -> void {
        // given that vertex v is on a past vertical wall (left/right) boundary, we want to check 2 things:
        // 1. the vertical top/bottom edge that adjacent to it, is single(not divided) in it's adjacent trapezes
        // 2. has winding==0
        // 3. horizontal (top/bottom) wall that touches it from both side is almost a line
        // if all the above happens then we can remove that edge and maintain a variant of the contraction principle.
        // now, try to test if we can shrink the top or bottom or both edges adjacent to the vertex
        // how we do it: examine upwards/downwards half edge
        // 1. if it has winding!=0 skip
        // 2. if there is another edge after it on the vertical wall, skip as well
        // 3. examine how close the horizontal top/bottom edges that lie at the end
        // 4. if they are close than an epsilon, remove the vertical edge
        // 4.1. concatenate the conflicting edges list
        // 4.2. walk on the right face's edges and re-point all half edges to the left face
        const half_edge_vertex *v = vertex_on_vertical_wall;
        half_edge * candidates[2] = {nullptr, nullptr};
        int index=0;
        half_edge *iter = v->edge;
        half_edge *top_edge=nullptr, *bottom_edge=nullptr;
        const half_edge *end = iter;
        // we walk in CCW order around the vertex
        do {
            bool is_vertical_edge = iter->twin->origin->coords.x==v->coords.x;
            if(is_vertical_edge) {
                bool is_top = iter->twin->origin->coords.y < v->coords.y; // equality cannot happen
                if(is_top)
                    top_edge= iter;
                else
                    bottom_edge= iter;

                candidates[index++]= iter;
            }

            if(index==2)
                break;

            iter=iter->twin->next;
        } while(iter!=end);

        // there have to be top and bottom because v was strictly inside the wall
        if(top_edge== nullptr || bottom_edge== nullptr) {
#if DEBUG_PLANAR==true
            throw std::runtime_error("handle_face_merge::have not found bottom or top edge !!!");
#endif
            return;
        }

        // iterate over up to two candidates
        for (int ix = 0; ix < index; ++ix) {
            auto * candidate_edge = candidates[ix];
            if(!candidate_edge)
                break;

            // start with top edge
            // perform test 1 & 2
            bool valid = candidate_edge->winding==0 &&
                         (candidate_edge->next->twin->origin->coords.x!=candidate_edge->origin->coords.x);
            if(valid) {
                // top edge goes up and CCW in it's face
                // a___b___c
                //     |
                // ----v----
                //     |
                // c'__b'__a'
                // now test how much is a-b-c looks like a line
                // we do it by calculating the distance from vertex c to line (a,b)
                // the illustration above is for top_edge= v-->b
                const auto & a = candidate_edge->next->twin->origin->coords;
                const auto & b = candidate_edge->twin->origin->coords;
                const auto & c = candidate_edge->twin->prev->origin->coords;
                // perform test #3
                bool is_abc_almost_a_line = is_distance_to_line_less_than_epsilon(c, a, b, number(1));
                if(is_abc_almost_a_line) {
                    // if it is almost a line, then (v,b) edge eligible for removal
                    remove_edge(candidate_edge);
                }

            }

        }

    }

    template<typename number>
    void planarize_division<number>::remove_edge(half_edge *edge) {
        // remove an edge and it's twin, then:
        // 1.  re-connect adjacent edges
        // 2.  merge faces
        // 3.  move face_2's conflict list into face_1
        auto * face_1 = edge->face;
        const auto * face_2 = edge->twin->face;
        // update right adjacent face edges' to point left face
        walk_and_update_edges_face(edge->twin, edge->face);
        // re-connect start segment
        edge->prev->next = edge->twin->next;
        edge->twin->next->prev = edge->prev;
        // re-connect end segment
        edge->twin->prev->next = edge->next;
        edge->next->prev= edge->twin->prev;
        // move face_2's conflict list into face_1
        auto * conflict_ref = face_1->conflict_list;
        // move the pointer to the last conflict of face_1
        while (conflict_ref->next!=nullptr)
            conflict_ref=conflict_ref->next;
        // hook the last conflict of face_1 to the first conflict of face_2
        conflict_ref->next = face_2->conflict_list;
        // now update the conflicting edges with the correct face
        while (conflict_ref!=nullptr) {
            conflict_ref->edge->conflict_face=face_1;
            conflict_ref=conflict_ref->next;
        }

    }

    template<typename number>
    bool planarize_division<number>::is_distance_to_line_less_than_epsilon(const vertex &v,
            const vertex &a, const vertex &b, number epsilon) {
        // we use the equation 2*A = h*d(a,b)
        // where A = area of triangle spanned by (a,b,v), h= distance of v to (a,b)
        // we raise everything to quads to avoid square function and we avoid division.
        // while this is more robust, you have to make sure that your number type will
        // not overflow (use 64 bit for fixed point integers)
        //number numerator= abs((b.y - a.y)*v.x - (b.x - a.x)*v.y + b.x*a.y - b.y*a.x);
        number numerator= abs((b.x - a.x) * (v.y - a.y) - (v.x - a.x) * (b.y - a.y)); // 2*A
        number numerator_quad = numerator*numerator; // (2A)^2
        number ab_length_quad = (b.y - a.y)*(b.y - a.y) + (b.x - a.x)*(b.x - a.x); // (length(a,b))^2
        number epsilon_quad = epsilon*epsilon;
        return numerator_quad < epsilon_quad*ab_length_quad;
    }

    template<typename number>
    void planarize_division<number>::insert_edge(half_edge *edge, dynamic_pool & dynamic_pool) {
        // start with the conflicting face and iterate carefully
        // a---b'---b
        bool are_we_done = false;
        int count = 0;
        vertex a, b, b_tag;
        half_edge *a_vertex_edge, *b_tag_vertex_edge;
        half_edge_face * face = edge->conflict_face;
        point_class_with_trapeze wall_result;
        // represent the face as trapeze for efficient info
        trapeze_t trapeze=infer_trapeze(face);
        // classify possibly un added vertex, that should belong to trapeze
        point_class_with_trapeze class_a =
                classify_point_conflicting_trapeze(edge->origin->coords, trapeze);
        a= edge->origin->coords;
        b=edge->twin->origin->coords;
        half_edge_vertex * merge_vertex_seed_candidate = nullptr;

        while(!are_we_done) {

            // the reporting of and class of the next interesting point of the edge against current trapeze
            conflicting_edge_intersection_status edge_status =
                    compute_conflicting_edge_intersection_against_trapeze(trapeze, a, b);
            b_tag = edge_status.point_of_interest;
            point_class_with_trapeze class_b_tag = edge_status.class_of_interest_point;

            // does edge (a,b') is co linear with boundary ? if so treat it
            bool co_linear_with_boundary = do_a_b_lies_on_same_trapeze_wall(trapeze, a, b_tag,
                                                                            class_a, class_b_tag,
                                                                            wall_result);
            if(co_linear_with_boundary) {
                // co-linear, so let's just insert vertices on the boundary and handle windings
                // we do not need to split face in this case
                // Explain:: why don't we send vertical splits lines for boundary ? I think
                // because the input is always a closed polyline so it doesn't matter
                handle_co_linear_edge_with_trapeze(trapeze, a, b_tag, wall_result,
                        &a_vertex_edge, &b_tag_vertex_edge, dynamic_pool);
            } else {
                // not co-linear so we have to split the trapeze into up to 4 faces
                // btw, we at-least split vertically into two top and bottom pieces
                a_vertex_edge= handle_face_split(trapeze, a, b_tag, class_a, class_b_tag,
                        dynamic_pool);
                b_tag_vertex_edge= a_vertex_edge->twin;

                if(count==0) {
                    // handle_face_split ,ethod might change/clamp 'a' vertex. If this is
                    // the first endpoint, we would like to copy this change to source.
                    // this should contribute to robustness
                    edge->origin->coords=a;
                }
            }

            // now, we need to merge faces if we split a vertical wall, i.e, if
            // the new 'a' coord strictly lies in the left/right wall
            // record last split vertex if it was on a vertical wall and not the first vertex
            // and not a co-linear segment on the boundary
            bool candidate = (count>=1) && !co_linear_with_boundary &&
                            (class_a==point_class_with_trapeze::left_wall ||
                             class_a==point_class_with_trapeze::right_wall);

            if(candidate)
                handle_face_merge(a_vertex_edge->origin);

            // increment
            // if b'==b we are done
            are_we_done = b==b_tag;
            if(are_we_done)
                break;
            // todo: if b is the last endpoint of the edge, update it's corrdinates with the original
            // todo: source copy, in case there was a clamping

            // ITERATION STEP, therefore update:
            // 1. now iterate on the sub-line (b', b), by assigning a=b'
            // 2. locate the face, that (b', b) is intruding into / conflicting
            // 3. infer the trapeze of the face
            // 4. infer the class of the updated vertex a=b' in this face
            a =b_tag;
            half_edge * located_face_edge= locate_face_of_a_b(*b_tag_vertex_edge->origin, b);
            face = located_face_edge->face;
            trapeze=infer_trapeze(face);
            class_a = classify_point_conflicting_trapeze(a, trapeze);
            count+=1;
        }


    }

    template<typename number>
    void planarize_division<number>::compute(const chunker<vertex> &pieces) {

        // vertices size is also edges size since these are polygons
        const auto v_size = pieces.unchunked_size();
        // plus 4s for the sake of frame
        static_pool static_pool(v_size + 4, 4 + v_size*2, 1, v_size);
        dynamic_pool dynamic_pool{};
        // create the main frame
        auto * main_face = create_frame(pieces, static_pool);
        // create edges and conflict lists
        auto ** edges_list = build_edges_and_conflicts(pieces, *main_face, static_pool);
        // todo: here create a random permutation of edges

        // now start iterations
        for (int ix = 0; ix < v_size; ++ix) {
            auto * e = edges_list[ix];

            //remove_edge_from_conflict_list(e);
            insert_edge(e, dynamic_pool);
        }


    }


    template<typename number>
    void planarize_division<number>::compute_DEBUG(const chunker<vertex> &pieces,
            dynamic_array<vertex> &debug_trapezes) {

        // vertices size is also edges size since these are polygons
        const auto v_size = pieces.unchunked_size();
        // plus 4s for the sake of frame
        static_pool static_pool(v_size + 4, 4 + v_size*2, 1, v_size);
        dynamic_pool dynamic_pool{};
        // create the main frame
        auto * main_face = create_frame(pieces, static_pool);
        // create edges and conflict lists
        auto ** edges_list = build_edges_and_conflicts(pieces, *main_face, static_pool);
        // todo: here create a random permutation of edges

        // now start iterations
        for (int ix = 0; ix < v_size; ++ix) {
            auto * e = edges_list[ix];

            //remove_edge_from_conflict_list(e);
            insert_edge(e, dynamic_pool);
        }

        // collect trapezes so far
        face_to_trapeze_vertices(main_face, debug_trapezes);
        auto &faces = dynamic_pool.getFaces();
        for (int ix = 0; ix < faces.size(); ++ix) {
            face_to_trapeze_vertices(faces[ix], debug_trapezes);
        }
    }

    template<typename number>
    void planarize_division<number>::clamp(number &val, number & a, number &b) {
        if(val<a) val = a;
        if(val>b) val = b;
    }

    template<typename number>
    void planarize_division<number>::clamp_vertex(vertex &v, vertex a, vertex b) {
        // clamp a vertex to endpoint if it has over/under flowed it
        bool is_a_before_b = a.x<b.x || (a.x==b.x && a.y<b.y);
        // sort, so a is before b
        if(!is_a_before_b) {
            vertex c=a;a=b;b=c;
        }
        bool is_v_before_a = v.x<a.x || (v.x==a.x && v.y<a.y);
        if(is_v_before_a) v=a;
        bool is_v_after_b = v.x>b.x || (v.x==b.x && v.y>b.y);
        if(is_v_after_b) v=b;
    }

    template<typename number>
    bool planarize_division<number>::is_trapeze_degenerate(const trapeze_t & trapeze) {
        return (trapeze.left_top->origin==trapeze.left_bottom->origin) ||
                (trapeze.right_top->origin==trapeze.right_bottom->origin);
    }

    template<typename number>
    int planarize_division<number>::infer_edge_winding(const vertex & a, const vertex & b) {
        // infer winding of edge (a,b)
        if(b.y<a.y) return 1; // rising/ascending edge
        if(b.y>a.y) return -1; // descending edge
        return 0;

    }

    template<typename number>
    bool planarize_division<number>::do_a_b_lies_on_same_trapeze_wall(const trapeze_t & trapeze,
                                                                      const vertex &a,
                                                                      const vertex &b,
                                                                      const point_class_with_trapeze & a_class,
                                                                      const point_class_with_trapeze & b_class,
                                                                      point_class_with_trapeze &resulting_wall) {
        // given an edge (a,b) where both a and b belong to trapeze, test if the edge is co-linear with one of the
        // boundaries of the trapeze. we classify symbolically rather than analytically in order to be robust.
        // this procedure also handles degenerate trapezes. if true, then resulting_wall will indicate which wall
        // can be one of four: left_wall, right_wall, top_wall, bottom_wall
        // skip inside
        if(a_class==point_class_with_trapeze::strictly_inside ||
            b_class==point_class_with_trapeze::strictly_inside)
            return false;

        bool a_on_boundary_vertex = a_class==point_class_with_trapeze::boundary_vertex;
        bool b_on_boundary_vertex = b_class==point_class_with_trapeze::boundary_vertex;
        bool same_class = a_class==b_class;

        // test if on same wall
        if(same_class && !a_on_boundary_vertex && !b_on_boundary_vertex) {
            resulting_wall=a_class;
            return true;
        }
        // todo: what happens when trapeze is degenerate - a triangle
        bool a_on_wall, b_on_wall;
        // test left wall
        a_on_wall = a_class==point_class_with_trapeze::left_wall ||
                              (a_on_boundary_vertex &&
                               (a==trapeze.left_top->origin->coords || a==trapeze.left_bottom->origin->coords));
        b_on_wall = b_class==point_class_with_trapeze::left_wall ||
                              (b_on_boundary_vertex &&
                               (b==trapeze.left_top->origin->coords || b==trapeze.left_bottom->origin->coords));
        if(a_on_wall && b_on_wall) {
            resulting_wall=a_class;
            return true;
        }

        // test right wall
        a_on_wall = a_class==point_class_with_trapeze::right_wall ||
                               (a_on_boundary_vertex &&
                                (a==trapeze.right_top->origin->coords || a==trapeze.right_bottom->origin->coords));
        b_on_wall = b_class==point_class_with_trapeze::right_wall ||
                               (b_on_boundary_vertex &&
                                (b==trapeze.right_top->origin->coords || b==trapeze.right_bottom->origin->coords));
        if(a_on_wall && b_on_wall) {
            resulting_wall=a_class;
            return true;
        }

        // test top wall
        a_on_wall = a_class==point_class_with_trapeze::top_wall ||
                    (a_on_boundary_vertex &&
                     (a==trapeze.right_top->origin->coords || a==trapeze.left_top->origin->coords));
        b_on_wall = b_class==point_class_with_trapeze::top_wall ||
                    (b_on_boundary_vertex &&
                     (b==trapeze.right_top->origin->coords || b==trapeze.left_top->origin->coords));
        if(a_on_wall && b_on_wall) {
            resulting_wall=a_class;
            return true;
        }

        // test bottom wall
        a_on_wall = a_class==point_class_with_trapeze::bottom_wall ||
                    (a_on_boundary_vertex &&
                     (a==trapeze.left_bottom->origin->coords || a==trapeze.right_bottom->origin->coords));
        b_on_wall = b_class==point_class_with_trapeze::bottom_wall ||
                    (b_on_boundary_vertex &&
                     (b==trapeze.left_bottom->origin->coords || b==trapeze.right_bottom->origin->coords));
        if(a_on_wall && b_on_wall) {
            resulting_wall=a_class;
            return true;
        }

        return false;
    }

    template<typename number>
    auto planarize_division<number>::compute_conflicting_edge_intersection_against_trapeze(const trapeze_t & trapeze,
                                                                                           vertex &a,
                                                                                           const vertex &b) -> conflicting_edge_intersection_status {
        // given that edge (a,b), a is conflicting, i.e on boundary or completely inside
        // and we know that the edge passes through the trapeze or lies on the boundary,
        // find the second interesting point, intersection or overlap or completely inside
        // NOTES:
        // 1. we want to be robust
        // 2. although endpoint "a" belongs to the trapeze, there is a chance it might be outside geometrically,
        //    because of numeric precision errors, that occur during vertical splits
        // 3. therefore, we might get two proper intersections for (a,b) edge against
        //    top/bottom/left/right boundaries.
        // 4. therefore, always consider the intersection with the biggest alpha as what we want.
        // to make it robust and clear I propose:
        // 1. at first figure out if "b" is strictly inside or the boundary of the trapeze
        // 1.1. if it is return the point as is with the classification, we are done
        // 1.2 if it is outside, the we need to hunt intersections with the biggest alpha or so

        // this can be injected from outside
        point_class_with_trapeze  cla_b = classify_arbitrary_point_with_trapeze(b, trapeze);
        conflicting_edge_intersection_status result{};
        // if it is inside or on the boundary, we are done
        if(cla_b!=point_class_with_trapeze::outside) {
            result.point_of_interest = b;
            result.class_of_interest_point = cla_b;
            return result;
        }

        // we now know, that b is outside, therefore hunt proper intersections
        vertex intersection_point{};
        number alpha, alpha1;
        number biggest_alpha=number(0);
        intersection_status status, biggest_status;
        bool is_bigger_alpha;

        // left-wall
        status = segment_intersection_test(a, b,
                trapeze.left_top->origin->coords, trapeze.left_bottom->origin->coords,
                intersection_point, alpha, alpha1);
        if(status==intersection_status::intersect) {
            is_bigger_alpha = alpha>=biggest_alpha;

            if(is_bigger_alpha) {
                biggest_alpha=alpha;
                result.point_of_interest = intersection_point;
                result.class_of_interest_point = point_class_with_trapeze::left_wall;
                // important to clamp for symbolic reasons as well in case of numeric errors.
                result.point_of_interest.x=trapeze.left_top->origin->coords.x;
                clamp(result.point_of_interest.y, trapeze.left_top->origin->coords.y, trapeze.left_bottom->origin->coords.y);
                if(result.point_of_interest==trapeze.left_top->origin->coords ||
                    result.point_of_interest==trapeze.left_bottom->origin->coords)
                    result.class_of_interest_point = point_class_with_trapeze::boundary_vertex;
            }
        }

        // right wall
        status = segment_intersection_test(a, b,
                                           trapeze.right_bottom->origin->coords, trapeze.right_top->origin->coords,
                                           intersection_point, alpha, alpha1);
        if(status==intersection_status::intersect) {
            is_bigger_alpha = alpha>=biggest_alpha;

            if(is_bigger_alpha) {
                biggest_alpha=alpha;
                result.point_of_interest = intersection_point;
                result.class_of_interest_point = point_class_with_trapeze::right_wall;
                // important to clamp for symbolic reasons as well in case of numeric errors.
                result.point_of_interest.x=trapeze.right_top->origin->coords.x;
                clamp(result.point_of_interest.y, trapeze.right_top->origin->coords.y, trapeze.right_bottom->origin->coords.y);
                if(result.point_of_interest==trapeze.right_top->origin->coords ||
                   result.point_of_interest==trapeze.right_bottom->origin->coords)
                    result.class_of_interest_point = point_class_with_trapeze::boundary_vertex;
            }
        }

        // bottom wall
        status = segment_intersection_test(a, b,
                                           trapeze.left_bottom->origin->coords, trapeze.right_bottom->origin->coords,
                                           intersection_point, alpha, alpha1);
        if(status==intersection_status::intersect) {
            is_bigger_alpha = alpha>=biggest_alpha;

            if(is_bigger_alpha) {
                biggest_alpha=alpha;
                result.point_of_interest = intersection_point;
                result.class_of_interest_point = point_class_with_trapeze::bottom_wall;
                // important to clamp for symbolic reasons as well in case of numeric errors.
                clamp_vertex(result.point_of_interest, trapeze.left_bottom->origin->coords,
                             trapeze.right_bottom->origin->coords);
                if(result.point_of_interest==trapeze.left_bottom->origin->coords ||
                   result.point_of_interest==trapeze.right_bottom->origin->coords)
                    result.class_of_interest_point = point_class_with_trapeze::boundary_vertex;
            }
        }

        // top wall
        status = segment_intersection_test(a, b,
                                           trapeze.right_top->origin->coords, trapeze.left_top->origin->coords,
                                           intersection_point, alpha, alpha1);
        if(status==intersection_status::intersect) {
            is_bigger_alpha = alpha>=biggest_alpha;

            if(is_bigger_alpha) {
                biggest_alpha=alpha;
                result.point_of_interest = intersection_point;
                result.class_of_interest_point = point_class_with_trapeze::top_wall;
                // important to clamp for symbolic reasons as well in case of numeric errors.
                clamp_vertex(result.point_of_interest, trapeze.right_top->origin->coords,
                             trapeze.left_top->origin->coords);
                if(result.point_of_interest==trapeze.right_top->origin->coords ||
                   result.point_of_interest==trapeze.left_top->origin->coords)
                    result.class_of_interest_point = point_class_with_trapeze::boundary_vertex;
            }
        }

        // now classify if (a,b) lies on the same wall

        return result;
    }


    template<typename number>
    auto planarize_division<number>::segment_intersection_test(const vertex &a, const vertex &b,
                                                               const vertex &c, const vertex &d,
                                                               vertex & intersection,
                                                               number &alpha, number &alpha1) -> intersection_status{
        // this procedure will find proper and improper(touches) intersections, but no
        // overlaps, since overlaps induce parallel classfication, this would have to
        // be resolved outside
        // vectors
        auto ab = b - a;
        auto cd = d - c;
        auto dem = ab.x * cd.y - ab.y * cd.x;

        // parallel lines
        // todo:: revisit when thinking about fixed points
//        if (abs(dem) <= 0.0001f)
        if (dem == 0)
            return intersection_status::parallel;
        else {
            auto ca = a - c;
            auto ac = -ca;
            auto numerator = ca.y * cd.x - ca.x * cd.y;

            if (dem > 0) {
                if (numerator < 0 || numerator > dem)
                    return intersection_status::none;
            } else {
                if (numerator > 0 || numerator < dem)
                    return intersection_status::none;
            }

            // a lies on c--d segment
            if(numerator==0) {
                alpha=0;
                intersection = a;
            } // b lies on c--d segment
            else if(numerator==dem) {
                alpha=1;
                intersection = b;
            }
            else { // proper intersection
                alpha = numerator / dem;
//            alpha1 = (ab.y * ac.x - ab.x * ac.y) / dem;
                intersection = a + ab * alpha;
            }
        }

        return intersection_status::intersect;
    }

}