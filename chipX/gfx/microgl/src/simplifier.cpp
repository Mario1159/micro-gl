#include <microgl/tesselation/simplifier.h>

namespace tessellation {

    struct node {
        // may have missing null children, that were removed
        dynamic_array<node *> children;
        int index_poly = -1;
        int accumulated_winding = 0;
    };

    // Use the sign of the determinant of vectors (AB,AM), where M(X,Y) is the query point:
    // position = sign((Bx - Ax) * (Y - Ay) - (By - Ay) * (X - Ax))

    //    Input:  three points p, a, b
    //    Return: >0 for P2 left of the line through a and b
    //            =0 for P2  on the line
    //            <0 for P2  right of the line
    //    See: Algorithm 1 "Area of Triangles and Polygons"
    inline int
    classify_point(const vertex & point, const vertex &a, const vertex & b)
    {
        auto result = ((b.x - a.x) * (point.y - a.y)
                   - (point.x - a.x) * (b.y - a.y) );

        if(result <0)
            return 1;
        else if(result > 0)
            return -1;
        else return 0;
    }

    inline int
    isLeft(const vertex & point, const vertex &a, const vertex & b)
    {
        return classify_point(point, a, b) > 0;
    }

    int
    point_inside_simple_polygon_wn(const vertex &point,
                                   const vertex *poly,
                                   const int size)
    {
        // the  winding number counter
        int wn = 0;

        // loop through all edges of the polygon
        // edge from V[i] to  V[i+1]
        for (int ix=0; ix < size; ix++) {
            // start y <= P.y
            if (poly[ix].y < point.y) {
                // an upward crossing
                if (poly[ix + 1].y >= point.y)
                    // P left of  edge
                    if (classify_point(point, poly[ix], poly[ix + 1]) > 0)
                        ++wn;
            }
            else {                        // start y > P.y (no test needed)
                // a downward crossing
                if (poly[ix + 1].y < point.y)
                    // P right of  edge
                    if (classify_point(point, poly[ix], poly[ix + 1]) < 0)
                        --wn;
            }
        }

        return wn!=0;
    }

    bool point_inside_simple_polygon_cn(const vertex &point,
                                        const vertex *poly,
                                        const int size) {

        int cn = 0;

        // loop through all edges of the polygon
        for (int ix=0; ix < size; ix++) {
            // an upward crossing
//            if ((poly[ix].y<=point.y && poly[ix + 1].y>point.y)
                // a downward crossing
//                || (poly[ix].y>point.y && poly[ix + 1].y<=point.y)) {
            if ((poly[ix].y<point.y && poly[ix + 1].y>=point.y)
                // a downward crossing
                || (poly[ix].y>=point.y && poly[ix + 1].y<point.y)) {
                // compute  the actual edge-ray intersect x-coordinate
                float vt = (point.y  - poly[ix].y) /
                            (poly[ix + 1].y - poly[ix].y);

                // P.x < intersect
                if (point.x < poly[ix].x + vt * (poly[ix + 1].x - poly[ix].x))
                    // a valid crossing of y=P.y right of P.x
                    ++cn;
            }
        }

        // 0 if even (out), and 1 if  odd (in)
        return (cn&1);
    }

    // tests if a point is completely inside, excluding boundary
    bool point_inside_convex_poly_interior(const vertex &point,
                                           const vertex * poly,
                                           int size,
                                           bool CCW = true) {
        bool fails = true;
        int direction = CCW ? 1 : -1;

        for (int ix = 0; ix < size; ++ix) {
            int a = ix;
            int b = (ix+1)==size ? 0 : ix+1;

            // handle a common degenerate case
            if(poly[a]==poly[b])
                continue;

            int classify = classify_point(point, poly[a], poly[b]);

            // we also fail on boundary
            fails = classify * direction >= 0;

            if(fails)
                return false;
        }

        return true;
    }

    // the extremal left-bottom most vertex is always a convex vertex
    int find_left_bottom_most_vertex(vertex * poly,
                                     const int size) {
        int index = 0;
        vertex value = poly[0];

        for (int ix = 0; ix < size; ++ix) {
            auto & v = poly[ix];

            if(v.x < value.x) {
                value = v;
                index = ix;
            }
            else if(v.x==value.x) {
                if(v.y > value.y) {
                    value = v;
                    index = ix;
                }
            }

        }

        return index;
    }


    int find_next_unique_vertex(const int idx,
                                  vertex * poly,
                                  const int size) {
        const vertex &point = poly[idx];
        auto follow_idx = idx;
        while(true) {
            if(++follow_idx==size-1)
                follow_idx=0;

            // completed a cycle and haven't found
            if(follow_idx==idx)
                return -1;
            const vertex & follow_vertex = poly[follow_idx];
            if(point.x!=follow_vertex.x || point.y!=follow_vertex.y)
                return follow_idx;
        }
    }

    direction compute_polygon_direction(vertex * poly,
                                        const int size) {
        // find a convex vertex
        int vi = find_left_bottom_most_vertex(poly, size);
        // this should always be unique unless the entire poly is a single vertex
        int ai = vi-1 < 0 ? size-1 : vi-1;
        // avoid degenerate cases
        int bi = find_next_unique_vertex(vi, poly, size);
        // poly is one point
        if(bi==-1)
            return direction::unknown;

        int classify = classify_point(poly[bi], poly[ai], poly[vi]);

        return classify < 0 ? direction::cw : direction::ccw;
    }

    // find a point via the diagonal method, a linear time algorithm
    vertex find_point_in_simple_polygon_interior(vertex * poly,
                                        const int size,
                                        bool CCW = true) {
        // find a convex vertex
        int vi = find_left_bottom_most_vertex(poly, size);
        // this should always be unique unless the entire poly is a single vertex
        int ai = vi-1 < 0 ? size-1 : vi-1;
//        int bi = vi+1 == size ? 0 : vi+1;
        // avoid degenerate cases
        int bi = find_next_unique_vertex(vi, poly, size);
        // poly is one point
        if(bi==-1)
            return poly[vi];

        vertex v = poly[vi];
        vertex a = poly[ai];
        vertex b = poly[bi];
        vertex triangle[3] = {a, v, b};

        vertex q;
        // pick a distance
        auto min_qv_distance = (a.x - v.x)*(a.x - v.x) + (a.y - v.y)*(a.y - v.y);
        bool found_candidate = false;

        for (int ix = 0; ix < size; ++ix) {
            const auto & q_candidate = poly[ix];

            // avoid degenerate case
            if(v==q)
                continue;

            if(point_inside_convex_poly_interior(q_candidate, triangle, 3, CCW)) {
                auto qv_distance = (q_candidate.x-v.x)*(q_candidate.x-v.x) +
                        (q_candidate.y-v.y)*(q_candidate.x-v.y);
                if(qv_distance < min_qv_distance) {
                    found_candidate = true;
                    q = q_candidate;
                    min_qv_distance = qv_distance;
                }

            }

        }

        // if candidate not found, take halfway the diagonal
        if(!found_candidate)
            return (a+b)/2;

        return (q+v)/2;
    }

    /**
     * since polygons are non-intersecting, then it is enough to test one point inclusion.
     * if one point is inside a polygon, then the entire polygon is inside, otherwise
     * it will contradict the fact that they are non-intersecting.
     * polygons may touch one another. this is a nice and fast algorithm
     *
     * 1=poly 1 inside poly 2
     * -1=poly 2 inside poly 1
     * 0=poly 1 and poly 2 are disjoint/separable
     */
    int compare_simple_non_intersecting_polygons(vertex * poly_1, index size_1, bool poly_1_CCW,
                                                 vertex * poly_2, index size_2, bool poly_2_CCW) {

        vertex sample = find_point_in_simple_polygon_interior(poly_1, size_1, poly_1_CCW);

        bool poly_1_inside_2 = point_inside_simple_polygon_wn(sample, poly_2, size_2);

        if(poly_1_inside_2)
            return 1;

        sample = find_point_in_simple_polygon_interior(poly_2, size_2, poly_2_CCW);
        bool poly_2_inside_1 = point_inside_simple_polygon_wn(sample, poly_1, size_1);

        if(poly_2_inside_1)
            return -1;

        return 0;
    }

    void compute_component_tree_recurse(node * root,
                                        node * current,
                                        chunker<vec2_f> & components,
                                        const dynamic_array<direction> &directions) {
        int root_children_count = root->children.size();
        int compare;

        auto poly_current = components[current->index_poly];

        if(root->index_poly != -1) {
            auto poly_root = components[root->index_poly];

            compare = compare_simple_non_intersecting_polygons(
                    poly_current.data,
                    poly_current.size - 1,
                    directions[current->index_poly]==direction::ccw,
                    poly_root.data,
                    poly_root.size - 1,
                    directions[root->index_poly]==direction::ccw);

            // compare against the root polygon

            switch (compare) {
                case 1: // current inside the root node
                    // we need to find a suitable place for it among children later
                    break;
                case -1: // root inside current node
                    // bubble root down to current and exit
                    compute_component_tree_recurse(current, root, components, directions);
                    return;
//                    throw std::runtime_error("weird !!!");
                    break;
                case 0: // complete strangers, let's exit
                    return;
            }

        }

        // lets' go over the root's children
        for (int ix = 0; ix < root_children_count; ++ix) {
            auto * child_node = root->children[ix];

            if(child_node== nullptr)
                continue;

            int child_poly_index = child_node->index_poly;
            auto child_poly = components[child_poly_index];

            compare = compare_simple_non_intersecting_polygons(
                    poly_current.data,
                    poly_current.size-1,
                    directions[current->index_poly]==direction::ccw,
                    child_poly.data,
                    child_poly.size-1,
                    directions[child_poly_index]==direction::ccw);

            switch (compare) {
                case 1: // current inside the child, bubble it down, and exit
                    compute_component_tree_recurse(child_node, current, components, directions);
                    return;
                case -1: // child inside current node, so add it to the current node
                    // and also, disconnect the child from the root, so it will have one parent
                    // this is why I need a linked list
                    current->children.push_back(child_node);
                    root->children[ix] = nullptr;
                    break;
                case 0: // complete strangers
                    break;
            }

        }

        // if we made it here, it means current node is direct child of this root
        root->children.push_back(current);
    }

    void tag_and_merge(node * root,
                       const dynamic_array<direction> &directions) {
        int root_accumulated_winding = root->accumulated_winding;

        // lets' go over the root's children, this list may expend
        // during the iteration, therefore we constantly query it.
        for (index ix = 0; ix < root->children.size(); ++ix) {
            auto * child_node = root->children[ix];

            if(child_node== nullptr)
                continue;

            auto child_winding = directions[child_node->index_poly]==direction::cw ? 1 : -1;
            child_winding += root_accumulated_winding;

            child_node->accumulated_winding += child_winding;

            bool fill_root = root_accumulated_winding!=0;
            bool fill_child = child_node->accumulated_winding!=0;

            // we merge nodes with similar fill status, we do it by disconnecting
            // them from the root parent and adding their children to the root
            // so they can also be processed soon
            if(fill_root == fill_child) {
                root->children[ix] = nullptr;

                // insert candidate children, they will be processed at end of loop
                for (index jx = 0; jx < child_node->children.size(); ++jx) {
                    auto * grandson = child_node->children[jx];

                    if(grandson) {
                        // we added a grandson which will be picked up soon later, so let's subtract
                        // the root winding, since it will be added again when this node will be processed.
                        grandson->accumulated_winding += child_node->accumulated_winding
                                                - root_accumulated_winding;
                        root->children.push_back(grandson);
                    }
                }
            } else {
                tag_and_merge(child_node, directions);
            }

        }

    }

    bool test_intersect(const vertex &a, const vertex &b,
                        const vertex &c, const vertex &d) {
        auto ab = b - a;
        auto cd = d - c;
        auto ca = a - c;

        auto ab_cd = ab.x * cd.y - cd.x * ab.y;
        auto s = (ab.x * ca.y - ab.y * ca.x);
        auto t = (cd.x * ca.y - cd.y * ca.x);

        bool test = s >= 0 && s <= ab_cd && t >= 0 && t <= ab_cd;

        return test;
    }

    int find_mutually_visible_vertex_in_polygon(const vertex & main_vertex,
                                                vertex * poly_2, int size_2) {

        for (int ix = 0; ix < size_2; ++ix) {
            vertex against = poly_2[ix];
            bool fails = false;

            for (int jx = 0; jx < size_2; ++jx) {
                // (main_vertex, against)
                if(test_intersect(main_vertex,
                        against,
                        poly_2[jx],
                        poly_2[(jx+1)%size_2])) {
                    fails = true;
                    break;
                }

            }

            if(!fails) {
                return ix;
            }

        }

        return -1;
    }

    void merge_hole() {

    }

    void extract_components(node * root,
                            chunker<vec2_f> & components,
                            chunker<vec2_f> & result) {

        for (index ix = 0; ix < root->children.size(); ++ix) {
            auto * child_node = root->children[ix];

            if(child_node== nullptr)
                continue;



        }

    }

    void compute_component_tree(chunker<vec2_f> & components,
                                const dynamic_array<direction> &directions,
                                chunker<vec2_f> & result) {
        const index components_size = components.size();
        node nodes[components_size];
        node root{};

        // build components inclusion tree
        for (unsigned long ix = 0; ix < components_size; ++ix)
        {
            auto * current = &(nodes[ix]);
            current->index_poly = int(ix);

            // now start hustling
            compute_component_tree_recurse(
                    &root,
                    current,
                    components,
                    directions);

        }

        // tag and compress similar
        index root_children_count = root.children.size();
        for (unsigned long ix = 0; ix < root_children_count; ++ix)
        {
            auto * current = root.children[ix];
            if(current== nullptr)
                continue;

            auto current_winding = directions[current->index_poly]==direction::cw ? 1 : -1;
            current->accumulated_winding = current_winding;
            // now start hustling
            tag_and_merge(
                    current,
                    directions);

        }

        // extract components with holes
        root_children_count = root.children.size();
        for (unsigned long ix = 0; ix < root_children_count; ++ix)
        {
            auto * current = root.children[ix];

            if(current== nullptr)
                continue;

            // now start hustling
            extract_components(
                    current,
                    components,
                    result);

        }

        int a = 0;
    }

    void compute_directions(chunker<vec2_f> & components,
                            dynamic_array<direction> &directions) {
        const auto count = components.size();
        for (index ix = 0; ix < count; ++ix) {
            auto comp = components[ix];
            auto direction = compute_polygon_direction(comp.data, comp.size);
            directions.push_back(direction);
        }

    }


    void simplifier::compute(chunker<vec2_f> & pieces,
                             chunker<vec2_f> & result) {

        dynamic_array<direction> components_directions;
        chunker<vec2_f> simplified_components;

        simplify_components::compute(
                pieces,
                simplified_components,
                components_directions);

        compute_directions(simplified_components, components_directions);

        // experiment
        compute_component_tree(
                simplified_components,
                components_directions,
                result);

        result = simplified_components;
        int a =0;
    }

}