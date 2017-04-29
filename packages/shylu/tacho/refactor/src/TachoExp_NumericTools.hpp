#ifndef __TACHOEXP_NUMERIC_TOOLS_HPP__
#define __TACHOEXP_NUMERIC_TOOLS_HPP__

/// \file Tacho_CrsMatrixTools.hpp
/// \author Kyungjoo Kim (kyukim@sandia.gov)

#include "TachoExp_Util.hpp"

namespace Tacho {

  namespace Experimental {

    template<typename ValueType, typename ExecSpace>
    class NumericTools {
    public:
      typedef ValueType value_type;

      typedef Kokkos::DefaultHostExecutionSpace host_exec_space;
      typedef Kokkos::View<ordinal_type*,host_exec_space> ordinal_type_array_host;
      typedef Kokkos::View<size_type*,   host_exec_space> size_type_array_host;
      typedef Kokkos::View<value_type*,  host_exec_space> value_type_array_host;

      typedef ExecSpace device_exec_space;
      typedef Kokkos::View<ordinal_type*,device_exec_space> ordinal_type_array_device;
      typedef Kokkos::View<size_type*,   device_exec_space> size_type_array_device;
      typedef Kokkos::View<value_type*,  device_exec_space> value_type_array_device;

      typedef Kokkos::pair<ordinal_type,ordinal_type> range_type;

      ///
      /// get the panel dimension of sid
      ///
      inline
      static void
      getSuperPanelSize(const ordinal_type sid,
                        const ordinal_type_array_host &supernodes,
                        const size_type_array_host &sid_super_panel_ptr,
                        const ordinal_type_array_host &blk_super_panel_colidx,
                        /* */ ordinal_type &m,
                        /* */ ordinal_type &n) {
        m = supernodes(sid+1) - supernodes(sid);
        n = blk_super_panel_colidx(sid_super_panel_ptr(sid+1)-1);
      }

      ///
      /// allocate factor space
      ///
      inline
      static void
      allocateSuperPanels(const ordinal_type nsupernodes,
                          const ordinal_type_array_host &supernodes,
                          const size_type_array_host &sid_super_panel_ptr,
                          const ordinal_type_array_host &blk_super_panel_colidx,
                          /* */ size_type_array_host &spanel_ptr,
                          /* */ value_type_array_host &spanel_buf,
                          const ordinal_type_array_host &work) {
        ordinal_type m, n;
        for (ordinal_type sid=0;sid<nsupernodes;++sid) {
          getSuperPanelSize(sid, supernodes, sid_super_panel_ptr, blk_super_panel_colidx, m, n);
          work(sid) = m*n;
        }

        // prefix scan
        spanel_ptr = size_type_array_host("spanel_ptr", nsupernodes+1);
        for (ordinal_type sid=0;sid<nsupernodes;++sid)
          spanel_ptr(sid+1) = spanel_ptr(sid) + work(sid);
        spanel_buf = value_type_array_host("spanel_buf", spanel_ptr(nsupernodes));
      }

      ///
      /// copy sparse matrix to super panels
      ///
      inline
      static void
      copySparseToSuperPanels(// input from sparse matrix
                              const size_type_array_host &ap,
                              const ordinal_type_array_host &aj,
                              const value_type_array_host &ax,
                              const ordinal_type_array_host &perm,
                              const ordinal_type_array_host &peri,
                              // supernodes
                              const ordinal_type nsupernodes,
                              const ordinal_type_array_host &supernodes,
                              const size_type_array_host &gid_super_panel_ptr,
                              const ordinal_type_array_host &gid_super_panel_colidx,
                              const size_type_array_host &sid_super_panel_ptr,
                              const ordinal_type_array_host &blk_super_panel_colidx,
                              // super panel data array
                              const size_type_array_host &super_panel_ptr,
                              const value_type_array_host &super_panel_buf,
                              // work array to store map
                              const ordinal_type_array_host &work) {
        ordinal_type m, n;
        for (ordinal_type sid=0;sid<nsupernodes;++sid) {
          // get panel for this sid
          getSuperPanelSize(sid, supernodes, sid_super_panel_ptr, blk_super_panel_colidx, m, n);
          Kokkos::View<value_type**,Kokkos::LayoutLeft,
            typename value_type_array_host::execution_space,
            Kokkos::MemoryUnmanaged> tgt(&super_panel_buf(super_panel_ptr(sid)), m, n);

          // local to global map
          const ordinal_type goffset = gid_super_panel_ptr(sid);
          for (ordinal_type j=0;j<n;++j) {
            const ordinal_type col = peri(gid_super_panel_colidx(j+goffset)); // global idx in sparse matrix
            work(col) = j;
          }

          // row major access to sparse src
          const ordinal_type soffset = supernodes(sid);
          for (ordinal_type i=0;i<m;++i) {
            const ordinal_type row = peri(i+soffset); // row in sparse matrix
            for (ordinal_type k=ap(row);k<ap(row+1);++k)
              tgt(i, work(aj(k))) = ax(k);
          }
        }
      }

    private:
      // matrix input
      ordinal_type _m;
      size_type_array_host _ap;
      ordinal_type_array_host _aj;
      value_type_array_host _ax;

      // graph ordering input
      ordinal_type_array_host _perm, _peri;

      // supernodes input
      ordinal_type _nsupernodes;
      ordinal_type_array_host _supernodes;

      // dof mapping to sparse matrix
      size_type_array_host _gid_super_panel_ptr;
      ordinal_type_array_host _gid_super_panel_colidx;

      // supernode map and panel size configuration
      size_type_array_host _sid_super_panel_ptr;
      ordinal_type_array_host _sid_super_panel_colidx, _blk_super_panel_colidx;

      // supernode tree
      size_type_array_host _stree_ptr;
      ordinal_type_array_host _stree_children;

      // output : factors
      size_type_array_host _super_panel_ptr;                                                                         
      value_type_array_host _super_panel_buf;  

      ordinal_type_array_host _work;

    public:
      NumericTools() = default;
      NumericTools(const NumericTools &b) = default;

      ///
      /// construction (assume input matrix and symbolic are from host)
      ///
      NumericTools(// input matrix A
                   const ordinal_type m,
                   const size_type_array_host &ap,
                   const ordinal_type_array_host &aj,
                   const value_type_array_host &ax,
                   // input permutation
                   const ordinal_type_array_host &perm,
                   const ordinal_type_array_host &peri,
                   // supernodes
                   const ordinal_type nsupernodes,
                   const ordinal_type_array_host &supernodes,
                   const size_type_array_host &gid_super_panel_ptr,
                   const ordinal_type_array_host &gid_super_panel_colidx,
                   const size_type_array_host &sid_super_panel_ptr,
                   const ordinal_type_array_host &sid_super_panel_colidx,
                   const ordinal_type_array_host &blk_super_panel_colidx,
                   const size_type_array_host &stree_ptr,
                   const ordinal_type_array_host &stree_children)
        : _m(m),
          _ap(ap),
          _aj(aj),
          _ax(ax),
          _perm(perm),
          _peri(peri),
          _nsupernodes(nsupernodes),
          _supernodes(supernodes),
          _gid_super_panel_ptr(gid_super_panel_ptr),
          _gid_super_panel_colidx(gid_super_panel_colidx),
          _sid_super_panel_ptr(sid_super_panel_ptr),
          _sid_super_panel_colidx(sid_super_panel_colidx),
          _blk_super_panel_colidx(blk_super_panel_colidx),
          _stree_ptr(stree_ptr),
          _stree_children(stree_children) {}

      ///
      /// host only input (value array can be rewritten in the same sparse structure)
      ///
      inline
      void
      CholeskyFactorize() {
        _work = ordinal_type_array_host("work", _m+1);

        /// allocate for supernode panels
        allocateSuperPanels(_nsupernodes, _supernodes, _sid_super_panel_ptr, _blk_super_panel_colidx,
                            _super_panel_ptr, _super_panel_buf, _work);

        // /// copy the input matrix into spanel_buf;
        // copySparseToSuperPanels(_ap, _aj, _ax, _perm, _peri,
        //                         _nsupernodes, _supernodes,
        //                         _gid_super_panel_ptr, _gid_super_panel_colidx,
        //                         _sid_super_panel_ptr, _blk_super_panel_colidx,
        //                         _super_panel_ptr, _super_panel_buf,
        //                         _work);
      }

      // matrix values are only changed (keep workspace)
      inline
      void
      CholeskyFactorize(const value_type_array_host &ax) {
        _ax = ax;

        //Kokkos::deep_copy(_work, 0);
        Kokkos::deep_copy(_super_panel_buf, 0);
        copySparseToSuperPanels(_ap, _aj, _ax, _perm, _peri,
                                _nsupernodes, _supernodes,
                                _gid_super_panel_ptr, _gid_super_panel_colidx,
                                _sid_super_panel_ptr, _blk_super_panel_colidx,
                                _super_panel_ptr, _super_panel_buf,
                                _work);

      }
    };

  }
}
#endif
