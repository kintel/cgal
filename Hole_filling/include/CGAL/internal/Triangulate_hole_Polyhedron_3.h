#ifndef CGAL_HOLE_FILLING_TRIANGULATE_HOLE_POLYHEDRON_3_H
#define CGAL_HOLE_FILLING_TRIANGULATE_HOLE_POLYHEDRON_3_H

#include <vector>
#include <CGAL/assertions.h>
#include <CGAL/internal/Triangulate_hole_polyline.h>
#include <boost/tuple/tuple.hpp>
#include <CGAL/Timer.h>
namespace CGAL {

/**
 * @brief Function triangulating a hole in surface mesh.
 * 
 * @tparam Polyhedron a %CGAL polyhedron
 * @tparam OutputIterator iterator holding 'Polyhedron::Facet_handle' for patch facets.
 *
 * @param[in, out] polyhedron surface mesh which has the hole
 * @param border_halfedge a border halfedge incident to the hole
 * @param[out] output iterator over patch facets.
 * 
 * @return `output`
 */
template<class Polyhedron, class OutputIterator>
OutputIterator 
triangulate_hole(Polyhedron& polyhedron, 
  typename Polyhedron::Halfedge_handle border_halfedge, 
  OutputIterator output
  )
{
  typedef typename Polyhedron::Halfedge_around_vertex_circulator  Halfedge_around_vertex_circulator;
  typedef typename Polyhedron::Halfedge_around_facet_circulator   Halfedge_around_facet_circulator;
  typedef typename Polyhedron::Vertex_handle                      Vertex_handle;
  typedef typename Polyhedron::Halfedge_handle                    Halfedge_handle;
  typedef typename Polyhedron::Traits::Point_3                    Point_3;

  std::vector<Point_3>         P, Q;
  std::vector<Halfedge_handle> P_edges;
  std::map<Vertex_handle, int> vertex_set;

  int n = 0;
  Halfedge_around_facet_circulator circ(border_halfedge), done(circ);
  do{
    P.push_back(circ->vertex()->point());
    Q.push_back(circ->next()->opposite()->next()->vertex()->point());
    P_edges.push_back(circ);
    vertex_set.insert(std::make_pair(circ->vertex(), n++));
  } while (++circ != done);
  
  // existing_edges contains neighborhood information between boundary vertices
  // more precisely if v_i is neighbor to any other vertex than v_(i-1) and v_(i+1),
  // this edge is put into existing_edges
  std::set<std::pair<int, int> > existing_edges;
  for(typename std::map<Vertex_handle, int>::iterator v_it = vertex_set.begin(); v_it != vertex_set.end(); ++v_it) {
    int v_it_id = v_it->second;
    int v_it_prev = v_it_id == 0   ? n-1 : v_it_id-1;
    int v_it_next = v_it_id == n-1 ? 0   : v_it_id+1;

    Halfedge_around_vertex_circulator circ_vertex(v_it->first->vertex_begin()), done_vertex(circ_vertex);
    do {
      Vertex_handle v_it_neigh = circ_vertex->opposite()->vertex();
      typename std::map<Vertex_handle, int>::iterator v_it_neigh_it = 
        vertex_set.find(v_it_neigh);
      if(v_it_neigh_it != vertex_set.end())
      {
        int v_it_neigh_id = v_it_neigh_it->second;
        if( v_it_neigh_id > v_it_id && 
            v_it_neigh_id != v_it_prev &&
            v_it_neigh_id != v_it_next )
        {
          bool inserted = existing_edges.insert(std::make_pair(v_it_id, v_it_neigh_id)).second;
          CGAL_assertion(inserted);
          CGAL_assertion(existing_edges.find(std::make_pair(v_it_neigh_id, v_it_id)) == existing_edges.end());
        }
      }
    } while(++circ_vertex != done_vertex);
  }

  CGAL::Timer timer; timer.start();

  // fill hole using polyline function
  std::vector<boost::tuple<int, int, int> > tris;
  internal::triangulate_hole_polyline<boost::tuple<int, int, int> >
    (P.begin(), P.end(), Q.begin(), Q.end(), back_inserter(tris), existing_edges);

  CGAL_TRACE_STREAM << "Hole filling: " << timer.time() << " sc." << std::endl; timer.reset();

  // construct patch
  // TODO: performance might be improved,
  //       although test on RedCircleBox.off shows that cost of this part is really insignificant compared to hole filling
  if(tris.empty()) { return output; }
  polyhedron.fill_hole(border_halfedge);
  *output++ = border_halfedge->facet();
  for(std::vector<boost::tuple<int, int, int> >::iterator tris_it = tris.begin(); tris_it != tris.end(); ++tris_it) {

    for(int i = 0; i < 3; ++i) { // iterate over each edge in triangle
      int v0_id, v1_id;
      if(i == 0)      { v0_id = tris_it->get<0>(); v1_id = tris_it->get<1>(); }
      else if(i == 1) { v0_id = tris_it->get<1>(); v1_id = tris_it->get<2>(); }
      else            { v0_id = tris_it->get<2>(); v1_id = tris_it->get<0>(); }

      bool border_edge = (v0_id + 1 == v1_id) || (v0_id == 0 && v1_id == P.size() -1); 
      bool smaller = v0_id < v1_id;

      if( smaller &&  /* to process one edge just one time */  
         !border_edge /* border edges don't require split  */ ) 
      { // this part is going to be executed exactly N-3 times

        Vertex_handle v0 = P_edges[v0_id]->vertex();
        Vertex_handle v1 = P_edges[v1_id]->vertex();

        Halfedge_around_vertex_circulator circ_v0(v0->vertex_begin()), circ_v1(v1->vertex_begin());
        bool found = false;
        // find halfedges sharing the common facet for split
        do { 
          circ_v1 = v1->vertex_begin();
          do {
            if(circ_v0->facet() == circ_v1->facet()) 
            {
              if(vertex_set.find(circ_v0->opposite()->vertex()) != vertex_set.end() &&
                 vertex_set.find(circ_v1->opposite()->vertex()) != vertex_set.end() )
              { found = true; }
            }
          } while(!found && ++circ_v1 != v1->vertex_begin());
        } while(!found && ++circ_v0 != v0->vertex_begin());

        CGAL_assertion(found);
        polyhedron.split_facet(circ_v0, circ_v1);
        *output++ = circ_v1->facet();
      }
    }
  }
  CGAL_TRACE_STREAM << "Patch construction: " << timer.time() << " sc." << std::endl;
  return output;
}

}//namespace CGAL
#endif //CGAL_HOLE_FILLING_TRIANGULATE_HOLE_POLYHEDRON_3_H