#pragma once

#include <cilantro/convex_hull_utilities.hpp>

namespace cilantro {
    template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
    class ConvexPolytope {
    public:
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW

        template <ptrdiff_t Dim = EigenDim, class = typename std::enable_if<Dim != Eigen::Dynamic>::type>
        ConvexPolytope() : dim_(EigenDim), is_empty_(true), is_bounded_(true), area_(0.0), volume_(0.0) {
            halfspaces_.resize(EigenDim+1, 2);
            halfspaces_.setZero();
            halfspaces_(0,0) = 1.0;
            halfspaces_(EigenDim,0) = 1.0;
            halfspaces_(0,1) = -1.0;
            halfspaces_(EigenDim,1) = 1.0;
            interior_point_.setConstant(EigenDim, 1, std::numeric_limits<OutputScalarT>::quiet_NaN());
        }

        template <ptrdiff_t Dim = EigenDim, class = typename std::enable_if<Dim == Eigen::Dynamic>::type>
        ConvexPolytope(size_t dim = 2) : dim_(dim), is_empty_(true), is_bounded_(true), area_(0.0), volume_(0.0) {
            halfspaces_.resize(dim+1, 2);
            halfspaces_.setZero();
            halfspaces_(0,0) = 1.0;
            halfspaces_(dim,0) = 1.0;
            halfspaces_(0,1) = -1.0;
            halfspaces_(dim,1) = 1.0;
            interior_point_.setConstant(dim, 1, std::numeric_limits<OutputScalarT>::quiet_NaN());
        }

        template <ptrdiff_t Dim = EigenDim, class = typename std::enable_if<Dim != Eigen::Dynamic>::type>
        ConvexPolytope(const ConstDataMatrixMap<InputScalarT,EigenDim> &points, bool compute_topology = false, bool simplicial_facets = false, double merge_tol = 0.0)
                : dim_(EigenDim)
        {
            init_points_(points, compute_topology, simplicial_facets, merge_tol);
        }

        template <ptrdiff_t Dim = EigenDim, class = typename std::enable_if<Dim != Eigen::Dynamic>::type>
        ConvexPolytope(const ConstInequalityDataMatrixMap<InputScalarT,EigenDim> &halfspaces, bool compute_topology = false, bool simplicial_facets = false, double merge_tol = 0.0, double dist_tol = std::numeric_limits<InputScalarT>::epsilon())
                : dim_(EigenDim)
        {
            init_halfspaces_(halfspaces, compute_topology, simplicial_facets, merge_tol, dist_tol);
        }

        template <ptrdiff_t Dim = EigenDim, class = typename std::enable_if<Dim == Eigen::Dynamic>::type>
        ConvexPolytope(const ConstDataMatrixMap<InputScalarT,EigenDim> &input_data, size_t dim, bool compute_topology = false, bool simplicial_facets = false, double merge_tol = 0.0, double dist_tol = std::numeric_limits<InputScalarT>::epsilon())
                : dim_(dim)
        {
            if (dim == input_data.rows()) {
                init_points_(input_data, compute_topology, simplicial_facets, merge_tol);
            } else if (dim == input_data.rows() - 1) {
                init_halfspaces_(input_data, compute_topology, simplicial_facets, merge_tol, dist_tol);
            } else {
                *this = ConvexPolytope<InputScalarT,OutputScalarT,EigenDim>(dim);
            }
        }

        ~ConvexPolytope() {}

        template <ptrdiff_t Dim = EigenDim>
        typename std::enable_if<Dim != Eigen::Dynamic, ConvexPolytope>::type intersectionWith(const ConvexPolytope &poly, bool compute_topology = false, bool simplicial_facets = false, double merge_tol = 0.0, double dist_tol = std::numeric_limits<InputScalarT>::epsilon()) const {
            InequalityMatrix<OutputScalarT,EigenDim> hs_intersection(EigenDim+1, halfspaces_.cols() + poly.halfspaces_.cols());
            hs_intersection.leftCols(halfspaces_.cols()) = halfspaces_;
            hs_intersection.rightCols(poly.halfspaces_.cols()) = poly.halfspaces_;
            return ConvexPolytope(hs_intersection, compute_topology, simplicial_facets, merge_tol, dist_tol);
        }

        template <ptrdiff_t Dim = EigenDim>
        typename std::enable_if<Dim == Eigen::Dynamic, ConvexPolytope>::type intersectionWith(const ConvexPolytope &poly, bool compute_topology = false, bool simplicial_facets = false, double merge_tol = 0.0, double dist_tol = std::numeric_limits<InputScalarT>::epsilon()) const {
            InequalityMatrix<OutputScalarT,EigenDim> hs_intersection(dim_+1, halfspaces_.cols() + poly.halfspaces_.cols());
            hs_intersection.leftCols(halfspaces_.cols()) = halfspaces_;
            hs_intersection.rightCols(poly.halfspaces_.cols()) = poly.halfspaces_;
            return ConvexPolytope(hs_intersection, dim_, compute_topology, simplicial_facets, merge_tol, dist_tol);
        }

        inline size_t getSpaceDimension() const { return dim_; }

        inline bool isEmpty() const { return is_empty_; }

        inline bool isBounded() const { return is_bounded_; }

        inline double getArea() const { return area_; }

        inline double getVolume() const { return volume_; }

        inline const PointMatrix<OutputScalarT,EigenDim>& getVertices() const { return vertices_; }

        inline const InequalityMatrix<OutputScalarT,EigenDim>& getFacetHyperplanes() const { return halfspaces_; }

        inline const Eigen::Matrix<OutputScalarT,EigenDim,1>& getInteriorPoint() const { return interior_point_; }

        inline bool containsPoint(const Eigen::Ref<const Eigen::Matrix<OutputScalarT,EigenDim,1>> &point, OutputScalarT offset = 0.0) const {
            for (size_t i = 0; i < halfspaces_.cols(); i++) {
                if (point.dot(halfspaces_.col(i).head(dim_)) + halfspaces_(dim_,i) > -offset) return false;
            }
            return true;
        }

        inline Eigen::Matrix<OutputScalarT,Eigen::Dynamic,Eigen::Dynamic> getPointSignedDistancesFromFacets(const ConstDataMatrixMap<OutputScalarT,EigenDim> &points) const {
            return (halfspaces_.topRows(dim_).transpose()*points).colwise() + halfspaces_.row(dim_).transpose();
        }

        Eigen::Matrix<bool,1,Eigen::Dynamic> getInteriorPointsIndexMask(const ConstDataMatrixMap<OutputScalarT,EigenDim> &points, OutputScalarT offset = 0.0) const {
            Eigen::Matrix<bool,1,Eigen::Dynamic> mask(1,points.cols());
            for (size_t i = 0; i < points.cols(); i++) {
                mask(i) = containsPoint(points.col(i), offset);
            }
            return mask;
        }

        std::vector<size_t> getInteriorPointIndices(const ConstDataMatrixMap<OutputScalarT,EigenDim> &points, OutputScalarT offset = 0.0) const {
            std::vector<size_t> indices;
            indices.reserve(points.cols());
            for (size_t i = 0; i < points.cols(); i++) {
                if (containsPoint(points.col(i), offset)) indices.emplace_back(i);
            }
            return indices;
        }

        inline const std::vector<std::vector<size_t>>& getFacetVertexIndices() const { return faces_; }

        inline const std::vector<std::vector<size_t>>& getVertexNeighborFacets() const { return vertex_neighbor_faces_; }

        inline const std::vector<std::vector<size_t>>& getFacetNeighborFacets() const { return face_neighbor_faces_; }

        inline const std::vector<size_t>& getVertexPointIndices() const { return vertex_point_indices_; }

        ConvexPolytope& transform(const Eigen::Ref<const Eigen::Matrix<OutputScalarT,EigenDim,EigenDim>> &rotation, const Eigen::Ref<const Eigen::Matrix<OutputScalarT,EigenDim,1>> &translation) {
            if (is_empty_) return *this;

            vertices_ = (rotation*vertices_).colwise() + translation;
            interior_point_ = rotation*interior_point_ + translation;

            typename std::conditional<EigenDim == Eigen::Dynamic, Eigen::Matrix<OutputScalarT,EigenDim,EigenDim>, Eigen::Matrix<OutputScalarT,EigenDim+1,EigenDim+1>>::type hs_tform(dim_+1,dim_+1);
            hs_tform.topLeftCorner(dim_,dim_) = rotation;
            hs_tform.block(dim_,0,1,dim_) = -translation.transpose()*rotation;
            hs_tform.col(dim_).setZero();
            hs_tform(dim_,dim_) = 1.0;

            halfspaces_ = hs_tform*halfspaces_;

            return *this;
        }

        template <ptrdiff_t Dim = EigenDim, class = typename std::enable_if<Dim != Eigen::Dynamic>::type>
        ConvexPolytope& transform(const Eigen::Ref<const Eigen::Matrix<OutputScalarT,EigenDim+1,EigenDim+1>> &rigid_transform) {
            return transform(rigid_transform.topLeftCorner(EigenDim,EigenDim), rigid_transform.topRightCorner(EigenDim,1));
        }

        template <ptrdiff_t Dim = EigenDim, class = typename std::enable_if<Dim == Eigen::Dynamic>::type>
        ConvexPolytope& transform(const Eigen::Ref<const Eigen::Matrix<OutputScalarT,EigenDim,EigenDim>> &rigid_transform) {
            return transform(rigid_transform.topLeftCorner(dim_,dim_), rigid_transform.topRightCorner(dim_,1));
        }

    protected:
        // Polytope properties
        size_t dim_;
        bool is_empty_;
        bool is_bounded_;
        double area_;
        double volume_;

        PointMatrix<OutputScalarT,EigenDim> vertices_;
        InequalityMatrix<OutputScalarT,EigenDim> halfspaces_;
        Eigen::Matrix<OutputScalarT,EigenDim,1> interior_point_;

        // Topological properties: only available for bounded (full-dimensional) polytopes
        std::vector<std::vector<size_t>> faces_;
        std::vector<std::vector<size_t>> vertex_neighbor_faces_;
        std::vector<std::vector<size_t>> face_neighbor_faces_;
        std::vector<size_t> vertex_point_indices_;

        inline void init_points_(const ConstDataMatrixMap<InputScalarT,EigenDim> &points, bool compute_topology, bool simplicial_facets, double merge_tol) {
            is_empty_ = (compute_topology) ? !convexHullFromPoints<InputScalarT,OutputScalarT,EigenDim>(points, vertices_, halfspaces_, faces_, vertex_neighbor_faces_, face_neighbor_faces_, vertex_point_indices_, area_, volume_, simplicial_facets, merge_tol)
                                           : !halfspaceIntersectionFromVertices<InputScalarT,OutputScalarT,EigenDim>(points, vertices_, halfspaces_, area_, volume_, true, merge_tol);
            is_bounded_ = true;
            if (is_empty_) {
                interior_point_.setConstant(dim_, 1, std::numeric_limits<OutputScalarT>::quiet_NaN());
            } else {
                interior_point_ = vertices_.rowwise().mean();
            }
        }

        inline void init_halfspaces_(const ConstInequalityDataMatrixMap<InputScalarT,EigenDim> &halfspaces, bool compute_topology, bool simplicial_facets, double merge_tol, double dist_tol) {
            is_empty_ = !evaluateHalfspaceIntersection<InputScalarT,OutputScalarT,EigenDim>(halfspaces, halfspaces_, vertices_, interior_point_, is_bounded_, dist_tol, merge_tol);
            if (is_empty_) {
                area_ = 0.0;
                volume_ = 0.0;
            } else if (is_bounded_) {
                if (compute_topology) {
                    is_empty_ = !convexHullFromPoints<OutputScalarT,OutputScalarT,EigenDim>(vertices_, vertices_, halfspaces_, faces_, vertex_neighbor_faces_, face_neighbor_faces_, vertex_point_indices_, area_, volume_, simplicial_facets, merge_tol);
                    if (is_empty_) {
                        interior_point_.setConstant(dim_, 1, std::numeric_limits<OutputScalarT>::quiet_NaN());
                    } else {
                        interior_point_ = vertices_.rowwise().mean();
                    }
                } else {
                    computeConvexHullAreaAndVolume<OutputScalarT,EigenDim>(vertices_, area_, volume_, merge_tol);
                }
            } else {
                area_ = std::numeric_limits<double>::infinity();
                volume_ = std::numeric_limits<double>::infinity();
            }
        }
    };

    typedef ConvexPolytope<float,float,2> ConvexPolytope2D;
    typedef ConvexPolytope<float,float,3> ConvexPolytope3D;
}
