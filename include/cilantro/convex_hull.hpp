#pragma once

#include <libqhullcpp/Qhull.h>
#include <libqhullcpp/QhullFacetSet.h>
#include <libqhullcpp/QhullFacetList.h>
#include <libqhullcpp/QhullVertexSet.h>
#include <eigen_quadprog/eiquadprog.hpp>
#include <cilantro/point_cloud.hpp>

template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
bool convexHullFromPoints(const Eigen::Ref<const Eigen::Matrix<InputScalarT,EigenDim,Eigen::Dynamic> > &points,
                          std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > &hull_points,
                          std::vector<Eigen::Matrix<OutputScalarT,EigenDim+1,1> > &halfspaces,
                          std::vector<std::vector<size_t> > &faces,
                          std::vector<std::vector<size_t> > &point_neighbor_faces,
                          std::vector<std::vector<size_t> > &face_neighbor_faces,
                          std::vector<size_t> &hull_point_indices,
                          double &area, double &volume,
                          bool simplicial_faces = true,
                          realT merge_tol = 0.0)
{
    size_t num_points = points.cols();
    Eigen::Matrix<realT,EigenDim,Eigen::Dynamic> data(points.template cast<realT>());

    orgQhull::Qhull qh;
    if (simplicial_faces) qh.qh()->TRIangulate = True;
    qh.qh()->premerge_centrum = merge_tol;
    qh.runQhull("", EigenDim, num_points, data.data(), "");
    qh.defineVertexNeighborFacets();
    orgQhull::QhullFacetList facets = qh.facetList();

    // Establish mapping between hull vertex ids and hull points indices
    size_t max_id = 0;
    for (auto vi = qh.vertexList().begin(); vi != qh.vertexList().end(); ++vi)
        if (max_id < vi->id()) max_id = vi->id();
    std::vector<size_t> vid_to_ptidx(max_id + 1);
    size_t k = 0;
    for (auto vi = qh.vertexList().begin(); vi != qh.vertexList().end(); ++vi)
        vid_to_ptidx[vi->id()] = k++;

    // Establish mapping between face ids and face indices
    max_id = 0;
    for (auto fi = facets.begin(); fi != facets.end(); ++fi)
        if (max_id < fi->id()) max_id = fi->id();
    std::vector<size_t> fid_to_fidx(max_id + 1);
    k = 0;
    for (auto fi = facets.begin(); fi != facets.end(); ++fi)
        fid_to_fidx[fi->id()] = k++;

    // Populate hull points and their indices in the input cloud
    k = 0;
    hull_points.resize(qh.vertexCount());
    point_neighbor_faces.resize(qh.vertexCount());
    hull_point_indices.resize(qh.vertexCount());
    for (auto vi = qh.vertexList().begin(); vi != qh.vertexList().end(); ++vi) {
        size_t i = 0;
        Eigen::Matrix<coordT,EigenDim,1> v;
        for (auto ci = vi->point().begin(); ci != vi->point().end(); ++ci) {
            v(i++) = *ci;
        }
        hull_points[k] = v.template cast<OutputScalarT>();

        i = 0;
        point_neighbor_faces[k].resize(vi->neighborFacets().size());
        for (auto fi = vi->neighborFacets().begin(); fi != vi->neighborFacets().end(); ++fi) {
            point_neighbor_faces[k][i++] = fid_to_fidx[(*fi).id()];
        }

        hull_point_indices[k] = vi->point().id();
        k++;
    }

    // Populate halfspaces and faces (indices in the hull cloud)
    k = 0;
    halfspaces.resize(facets.size());
    faces.resize(facets.size());
    face_neighbor_faces.resize(facets.size());
    for (auto fi = facets.begin(); fi != facets.end(); ++fi) {
        size_t i = 0;
        Eigen::Matrix<coordT,EigenDim+1,1> hp;
        for (auto hpi = fi->hyperplane().begin(); hpi != fi->hyperplane().end(); ++hpi) {
            hp(i++) = *hpi;
        }
        hp(EigenDim) = fi->hyperplane().offset();
        halfspaces[k] = hp.template cast<OutputScalarT>();

        faces[k].resize(fi->vertices().size());
        if (fi->isTopOrient()) {
            i = faces[k].size() - 1;
            for (auto vi = fi->vertices().begin(); vi != fi->vertices().end(); ++vi) {
                faces[k][i--] = vid_to_ptidx[(*vi).id()];
            }
        } else {
            i = 0;
            for (auto vi = fi->vertices().begin(); vi != fi->vertices().end(); ++vi) {
                faces[k][i++] = vid_to_ptidx[(*vi).id()];
            }
        }

        i = 0;
        face_neighbor_faces[k].resize(fi->neighborFacets().size());
        for (auto nfi = fi->neighborFacets().begin(); nfi != fi->neighborFacets().end(); ++nfi) {
            face_neighbor_faces[k][i++] = fid_to_fidx[(*nfi).id()];
        }

        k++;
    }

    area = qh.area();
    volume = qh.volume();

    return true;
}

template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
inline bool convexHullFromPoints(const std::vector<Eigen::Matrix<InputScalarT,EigenDim,1> > &points,
                                 std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > &hull_points,
                                 std::vector<Eigen::Matrix<OutputScalarT,EigenDim+1,1> > &halfspaces,
                                 std::vector<std::vector<size_t> > &faces,
                                 std::vector<std::vector<size_t> > &point_neighbor_faces,
                                 std::vector<std::vector<size_t> > &face_neighbor_faces,
                                 std::vector<size_t> &hull_point_indices,
                                 double &area, double &volume,
                                 bool simplicial_faces = true,
                                 realT merge_tol = 0.0)
{
    return convexHullFromPoints<InputScalarT,OutputScalarT,EigenDim>(Eigen::Map<Eigen::Matrix<InputScalarT,EigenDim,Eigen::Dynamic> >((InputScalarT *)points.data(),EigenDim,points.size()), hull_points, halfspaces, faces, point_neighbor_faces, face_neighbor_faces, hull_point_indices, area, volume, simplicial_faces, merge_tol);
}

//template <typename ScalarT, ptrdiff_t EigenDim>
//bool findFeasiblePointInHalfspaceIntersection(ScalarT * halfspaces,
//                                              size_t num_halfspaces,
//                                              Eigen::Matrix<ScalarT,EigenDim,1> &feasible_point)
//{
//    // Objective
////    Eigen::MatrixXd G(Eigen::MatrixXd::Zero(EigenDim+2,EigenDim+2));
//    Eigen::MatrixXd G(Eigen::MatrixXd::Identity(EigenDim+2,EigenDim+2));
//    Eigen::VectorXd g0(Eigen::VectorXd::Zero(EigenDim+2));
//    g0(EigenDim+1) = -1.0;
//
//    // Equality constraints
//    Eigen::MatrixXd CE(EigenDim+2,0);
//    Eigen::VectorXd ce0(0);
//
//    // Inequality constraints
////    Eigen::MatrixXd CI(EigenDim+2,num_halfspaces);
//////    Eigen::MatrixXd ineq_data(Eigen::Map<Eigen::Matrix<ScalarT,EigenDim+1,Eigen::Dynamic> >(halfspaces,EigenDim+1,num_halfspaces).template cast<double>());
//////    CI.topRows(EigenDim+1) = -ineq_data;
////    CI.topRows(EigenDim+1) = -(Eigen::Map<Eigen::Matrix<ScalarT,EigenDim+1,Eigen::Dynamic> >(halfspaces,EigenDim+1,num_halfspaces).template cast<double>());
////    CI.row(EigenDim+1) = -Eigen::VectorXd::Ones(num_halfspaces);
////    Eigen::VectorXd ci0(Eigen::VectorXd::Zero(num_halfspaces));
//
//    Eigen::MatrixXd ineq_data(Eigen::Map<Eigen::Matrix<ScalarT,EigenDim+1,Eigen::Dynamic> >(halfspaces,EigenDim+1,num_halfspaces).template cast<double>());
//    for (size_t i = 0; i < num_halfspaces; i++) {
//        ineq_data.col(i) /= ineq_data.col(i).head(EigenDim).norm();
//    }
//
//    Eigen::MatrixXd CI(EigenDim+2,num_halfspaces+2);
//    CI.block(0,0,EigenDim+1,num_halfspaces) = -ineq_data;
//    CI.block(EigenDim+1,0,1,num_halfspaces) = -Eigen::VectorXd::Ones(num_halfspaces);
//    CI.block(0,num_halfspaces,EigenDim,2).setZero();
//    CI.block(EigenDim,num_halfspaces,2,2) = Eigen::Matrix2d::Identity();
//    Eigen::VectorXd ci0(Eigen::VectorXd::Zero(num_halfspaces+2));
//
//    // Optimization
//    Eigen::VectorXd x(EigenDim+2);
//    double val = solve_quadprog(G, g0, CE, ce0, CI, ci0, x);
//
//    std::cout << val << " for: " << x.transpose() << std::endl;
//
//    Eigen::Matrix<double,EigenDim,1> feasible_point_d(x.head(EigenDim)/x(EigenDim));
//    feasible_point = feasible_point_d.template cast<ScalarT>();
//
//    if (std::isinf(val) || std::isnan(val) || std::abs(x(EigenDim)) < 1e-10 || x.array().isNaN().any() || x.array().isInf().any())
//        return false;
//
//    return true;
//}

//template <typename ScalarT, ptrdiff_t EigenDim>
//bool findFeasiblePointInHalfspaceIntersection(ScalarT * halfspaces,
//                                              size_t num_halfspaces,
//                                              Eigen::Matrix<ScalarT,EigenDim,1> &feasible_point)
//{
//    // Objective
////    Eigen::MatrixXd G(Eigen::MatrixXd::Zero(EigenDim+1,EigenDim+1));
//    Eigen::MatrixXd G(1e-6*Eigen::MatrixXd::Identity(EigenDim+1,EigenDim+1));
//    Eigen::VectorXd g0(Eigen::VectorXd::Zero(EigenDim+1));
//    g0(EigenDim) = -1.0;
//
//    // Equality constraints
//    Eigen::MatrixXd CE(EigenDim+1,0);
//    Eigen::VectorXd ce0(0);
//
//    // Inequality constraints
//    Eigen::MatrixXd ineq_data(Eigen::Map<Eigen::Matrix<ScalarT,EigenDim+1,Eigen::Dynamic> >(halfspaces,EigenDim+1,num_halfspaces).template cast<double>());
//    Eigen::MatrixXd CI(EigenDim+1,num_halfspaces);
//    CI.topRows(EigenDim) = -ineq_data.topRows(EigenDim);
//    CI.row(EigenDim) = -ineq_data.topRows(EigenDim).colwise().norm();
//    Eigen::VectorXd ci0(-ineq_data.row(EigenDim));
//
//    // Optimization
//    Eigen::VectorXd x(EigenDim+1);
//    double val = solve_quadprog(G, g0, CE, ce0, CI, ci0, x);
//
//    std::cout << val << " for: " << x.transpose() << std::endl;
//
//    feasible_point = x.head(EigenDim).template cast<ScalarT>();
//
//    if (std::isinf(val) || std::isnan(val) || x(EigenDim) < 0.0 || x.array().isNaN().any() || x.array().isInf().any())
//        return false;
//
//    return true;
//}

template <typename ScalarT, ptrdiff_t EigenDim>
bool checkLinearInequalityConstraintRedundancy(const Eigen::Matrix<ScalarT,EigenDim+1,1> &ineq_to_test,
                                               const Eigen::Ref<const Eigen::Matrix<ScalarT,EigenDim+1,Eigen::Dynamic> > &inequalities,
                                               const Eigen::Matrix<ScalarT,EigenDim,1> &feasible_point,
                                               ScalarT dist_tol = std::numeric_limits<ScalarT>::epsilon())
{
    size_t num_inequalities = inequalities.cols();
    Eigen::MatrixXd ineq_data(inequalities.template cast<double>());
    Eigen::VectorXd ineq_test(ineq_to_test.template cast<double>());

    // Normalize input
    // Force unit length normals
    for (size_t i = 0; i < num_inequalities; i++) {
        ineq_data.col(i) /= ineq_data.col(i).head(EigenDim).norm();
    }
    ineq_test /= ineq_test.head(EigenDim).norm();

    // Center halfspaces around origin and then scale dimensions
    Eigen::VectorXd t_vec(-feasible_point.template cast<double>());
    ineq_data.row(EigenDim) = ineq_data.row(EigenDim) - t_vec.transpose()*ineq_data.topRows(EigenDim);
    ineq_test(EigenDim) = ineq_test(EigenDim) - t_vec.dot(ineq_test.head(EigenDim));
    double max_abs_dist = ineq_data.row(EigenDim).cwiseAbs().maxCoeff();
    double scale = (max_abs_dist < dist_tol) ? 1.0 : 1.0/max_abs_dist;
    ineq_data.row(EigenDim) *= scale;
    ineq_test(EigenDim) *= scale;

    // Objective
    // 'Preconditioned' quadratic term
    ScalarT tol_sq = dist_tol*dist_tol;
    Eigen::MatrixXd G(EigenDim+2,EigenDim+2);
    G.topLeftCorner(EigenDim+1,EigenDim+1) = ineq_test*(ineq_test.transpose());
    G.row(EigenDim+1).setZero();
    G.col(EigenDim+1).setZero();
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(G, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::VectorXd S(svd.singularValues());
    for (size_t i = 0; i < S.size(); i++) {
        if (S(i) < tol_sq) S(i) = tol_sq;
    }
    G = svd.matrixU()*(S.asDiagonal())*(svd.matrixV().transpose());

    // Linear term
    Eigen::VectorXd g0(EigenDim+2);
    g0.head(EigenDim) = -ineq_test.head(EigenDim);
    g0(EigenDim) = 0.0;
    g0(EigenDim+1) = 1.0;

    // Equality constraints
    Eigen::MatrixXd CE(Eigen::VectorXd::Zero(EigenDim+2));
    CE(EigenDim) = 1.0;
    Eigen::VectorXd ce0(1);
    ce0(0) = -1.0;

    // Inequality constraints
    Eigen::MatrixXd CI(EigenDim+2,num_inequalities+1);
    CI.topLeftCorner(EigenDim+1,num_inequalities) = -ineq_data;
    CI.row(EigenDim+1).setConstant(-1.0);
    CI.col(num_inequalities).setZero();
    CI(EigenDim+1,num_inequalities) = 1.0;
    Eigen::VectorXd ci0(Eigen::VectorXd::Zero(num_inequalities+1));

    // Optimization
    Eigen::VectorXd x(EigenDim+2);
    double val = solve_quadprog(G, g0, CE, ce0, CI, ci0, x);

    std::cout << val << ", for: " << x.transpose() << std::endl;
    std::cout << x.head(EigenDim).dot(ineq_test.head(EigenDim)) << " OFFSET: " << ineq_test(EigenDim) << " DIST: " << std::abs(x.head(EigenDim).dot(ineq_test.head(EigenDim)) + ineq_test(EigenDim)) << std::endl << std::endl;

    return std::abs(x.head(EigenDim).dot(ineq_test.head(EigenDim)) + ineq_test(EigenDim)) >= dist_tol;

}

template <typename ScalarT, ptrdiff_t EigenDim>
inline bool checkLinearInequalityConstraintRedundancy(const Eigen::Matrix<ScalarT,EigenDim+1,1> &ineq_to_test,
                                                      const std::vector<Eigen::Matrix<ScalarT,EigenDim+1,1> > &inequalities,
                                                      const Eigen::Matrix<ScalarT,EigenDim,1> &feasible_point,
                                                      ScalarT dist_tol = std::numeric_limits<ScalarT>::epsilon())
{
    return checkLinearInequalityConstraintRedundancy<ScalarT,EigenDim>(ineq_to_test, Eigen::Map<Eigen::Matrix<ScalarT,EigenDim+1,Eigen::Dynamic> >((ScalarT *)inequalities.data(),EigenDim+1,inequalities.size()), feasible_point, dist_tol);
}

template <typename ScalarT, ptrdiff_t EigenDim>
bool findFeasiblePointInHalfspaceIntersection(const Eigen::Ref<const Eigen::Matrix<ScalarT,EigenDim+1,Eigen::Dynamic> > &halfspaces,
                                              Eigen::Matrix<ScalarT,EigenDim,1> &feasible_point,
                                              ScalarT dist_tol = std::numeric_limits<ScalarT>::epsilon(),
                                              bool force_strictly_interior = true)
{
    size_t num_halfspaces = halfspaces.cols();
    Eigen::MatrixXd ineq_data(halfspaces.template cast<double>());

    // Normalize input
    // Force unit length normals
    for (size_t i = 0; i < num_halfspaces; i++) {
        ineq_data.col(i) /= ineq_data.col(i).head(EigenDim).norm();
    }

    // Center halfspaces around origin and then scale dimensions
    Eigen::Matrix<double,EigenDim,1> t_vec((ineq_data.topRows(EigenDim).array().rowwise()*ineq_data.row(EigenDim).array().abs()).rowwise().mean());
    ineq_data.row(EigenDim) = ineq_data.row(EigenDim) - t_vec.transpose()*ineq_data.topRows(EigenDim);
    double max_abs_dist = ineq_data.row(EigenDim).cwiseAbs().maxCoeff();
    double scale = (max_abs_dist < dist_tol) ? 1.0 : 1.0/max_abs_dist;
    ineq_data.row(EigenDim) *= scale;

//    std::cout << ineq_data << std::endl << std::endl;

    // Objective
    // 'Preconditioned' quadratic term
    ScalarT tol_sq = dist_tol*dist_tol;
    Eigen::MatrixXd G(EigenDim+2,EigenDim+2);
    G.topLeftCorner(EigenDim+1,EigenDim+1) = ineq_data*(ineq_data.transpose());
    G.row(EigenDim+1).setZero();
    G.col(EigenDim+1).setZero();
    G(EigenDim+1,EigenDim+1) = 1.0;
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(G, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::VectorXd S(svd.singularValues());
    for (size_t i = 0; i < S.size(); i++) {
        if (S(i) < tol_sq) S(i) = tol_sq;
    }
    G = svd.matrixU()*(S.asDiagonal())*(svd.matrixV().transpose());

    // Linear term
    Eigen::VectorXd g0(Eigen::VectorXd::Zero(EigenDim+2));
    g0(EigenDim+1) = -1.0;

    // Equality constraints
    Eigen::MatrixXd CE(Eigen::VectorXd::Zero(EigenDim+2));
    CE(EigenDim) = 1.0;
    Eigen::VectorXd ce0(1);
    ce0(0) = -1.0;

    // Inequality constraints
    Eigen::MatrixXd CI(EigenDim+2,num_halfspaces+1);
    CI.topLeftCorner(EigenDim+1,num_halfspaces) = -ineq_data;
    CI.row(EigenDim+1).setConstant(-1.0);
    CI.col(num_halfspaces).setZero();
    CI(EigenDim+1,num_halfspaces) = 1.0;
    Eigen::VectorXd ci0(Eigen::VectorXd::Zero(num_halfspaces+1));

    // Optimization
    Eigen::VectorXd x(EigenDim+2);
    double val = solve_quadprog(G, g0, CE, ce0, CI, ci0, x);
    Eigen::Matrix<double,EigenDim,1> fp(x.head(EigenDim));
    feasible_point = (fp/scale - t_vec).template cast<ScalarT>();

//    std::cout << val << ", for: " << x.transpose() << "   (" << feasible_point.transpose() << ")" << std::endl;

    if (std::isinf(val) || std::isnan(val) || x.array().isNaN().any() || x.array().isInf().any()) return false;

    // Useful in case of unbounded intersections
    if (force_strictly_interior && x(EigenDim+1) < dist_tol) {
        size_t num_additional = 0;
        std::vector<size_t> tight_ind(num_halfspaces);
        for (size_t i = 0; i < num_halfspaces; i++) {
            if (std::abs(ineq_data.col(i).dot(x.head(EigenDim+1))) < dist_tol) {
                tight_ind[num_additional++] = i;
            }// else ineq_data(EigenDim,i) = -ineq_data.col(i).head(EigenDim).dot(fp) - dist_tol;
        }
        tight_ind.resize(num_additional);

        if (num_additional > 0) {
            double offset = (double)(num_halfspaces - 1);
            Eigen::Matrix<double,EigenDim+1,Eigen::Dynamic> halfspaces_tight(EigenDim+1,num_halfspaces+num_additional);
            halfspaces_tight.leftCols(num_halfspaces) = ineq_data;
            num_additional = num_halfspaces;
            for (size_t i = 0; i < tight_ind.size(); i++) {
                halfspaces_tight.col(num_additional) = -ineq_data.col(tight_ind[i]);
                halfspaces_tight(EigenDim,num_additional) -= offset;
                num_additional++;
            }

            bool res = findFeasiblePointInHalfspaceIntersection<double,EigenDim>(halfspaces_tight, fp, dist_tol, false);
            feasible_point = (fp/scale - t_vec).template cast<ScalarT>();
            return res;
        }
    }

    return num_halfspaces > 0;
}

template <typename ScalarT, ptrdiff_t EigenDim>
inline bool findFeasiblePointInHalfspaceIntersection(const std::vector<Eigen::Matrix<ScalarT,EigenDim+1,1> > &halfspaces,
                                                     Eigen::Matrix<ScalarT,EigenDim,1> &feasible_point,
                                                     ScalarT dist_tol = std::numeric_limits<ScalarT>::epsilon(),
                                                     bool force_strictly_interior = true)
{
    return findFeasiblePointInHalfspaceIntersection<ScalarT,EigenDim>(Eigen::Map<Eigen::Matrix<ScalarT,EigenDim+1,Eigen::Dynamic> >((ScalarT *)halfspaces.data(),EigenDim+1,halfspaces.size()), feasible_point, dist_tol, force_strictly_interior);
}



template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
bool evaluateHalfspaceIntersection(const Eigen::Ref<const Eigen::Matrix<InputScalarT,EigenDim+1,Eigen::Dynamic> > &halfspaces,
                                   Eigen::Matrix<OutputScalarT,EigenDim,1> &interior_point,
                                   std::vector<Eigen::Matrix<OutputScalarT,EigenDim+1,1> > &facet_halfspaces,
                                   std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > &facet_intersections,
                                   bool &is_bounded,
                                   InputScalarT dist_tol = std::numeric_limits<InputScalarT>::epsilon(),
                                   realT merge_tol = 0.0)
{
    Eigen::FullPivLU<Eigen::Matrix<InputScalarT,EigenDim,Eigen::Dynamic> > lu(halfspaces.topRows(EigenDim));
    std::cout << "By default, the rank of A is found to be " << lu.rank() << std::endl;
    lu.setThreshold(dist_tol);
    std::cout << "With threshold, the rank of A is found to be " << lu.rank() << std::endl;


}











template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
bool evaluateHalfspaceIntersection(const Eigen::Ref<const Eigen::Matrix<InputScalarT,EigenDim+1,Eigen::Dynamic> > &halfspaces,
                                   const Eigen::Matrix<InputScalarT,EigenDim,1> &interior_point,
                                   std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > &vertices,
                                   realT merge_tol = 0.0)
{
    size_t num_halfspaces = halfspaces.cols();
    Eigen::Matrix<realT,EigenDim+1,Eigen::Dynamic> data(halfspaces.template cast<realT>());
    for (size_t i = 0; i < num_halfspaces; i++) {
        data.col(i) /= data.col(i).head(EigenDim).norm();
    }

    Eigen::Matrix<coordT,EigenDim,1> feasible_point(interior_point.template cast<coordT>());
    std::vector<coordT> fpv(EigenDim);
    Eigen::Matrix<coordT,EigenDim,1>::Map(fpv.data()) = feasible_point;

    orgQhull::Qhull qh;
    qh.qh()->HALFspace = True;
    qh.qh()->PRINTprecision = False;
//    qh.qh()->JOGGLEmax = 0.0;
    qh.qh()->TRIangulate = False;
    qh.qh()->premerge_centrum = merge_tol;
    qh.setFeasiblePoint(orgQhull::Coordinates(fpv));
    qh.runQhull("", EigenDim+1, num_halfspaces, data.data(), "");

    orgQhull::QhullFacetList facets = qh.facetList();


    std::cout << "Points:" << std::endl;
    std::vector<Eigen::Matrix<coordT,EigenDim,1> > pts;

    // Get hull points from dual facets
    size_t k = 0;
    vertices.resize(facets.size());
    for (auto fi = facets.begin(); fi != facets.end(); ++fi) {
        Eigen::Matrix<coordT,EigenDim,1> normal;
        size_t i = 0;
        for (auto hpi = fi->hyperplane().begin(); hpi != fi->hyperplane().end(); ++hpi) {
            normal(i++) = *hpi;
        }
        vertices[k++] = (-normal/fi->hyperplane().offset() + feasible_point).template cast<OutputScalarT>();


        std::cout << fi->id() << ": " << vertices[k-1].transpose() << ", is inf: " << (fi->hyperplane().offset() >= 0.0) << std::endl;
        if (fi->hyperplane().offset() < 0.0) {
            pts.push_back(vertices[k-1].template cast<coordT>());
        }
//        std::cout << fi->id() << ": " << normal.transpose() << " with " << fi->hyperplane().offset() << std::endl;

    }

    ////////

    std::cout << "Halfspaces:" << std::endl;
    for (auto vi = qh.vertexList().begin(); vi != qh.vertexList().end(); ++vi) {
        Eigen::Matrix<realT,EigenDim+1,1> hs(data.col(vi->point().id()));
        Eigen::Matrix<realT,1,Eigen::Dynamic> dists(1,pts.size());
        for (size_t i = 0; i < pts.size(); i++) {
            dists(i) = pts[i].dot(hs.head(EigenDim)) + hs(EigenDim);
        }


        std::cout << hs.transpose() << ", DISTS: " << dists << std::endl;
    }
    std::cout << std::endl;

    ////////


    return true;
}

template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
inline bool evaluateHalfspaceIntersection(const std::vector<Eigen::Matrix<InputScalarT,EigenDim+1,1> > &halfspaces,
                                          const Eigen::Matrix<InputScalarT,EigenDim,1> &interior_point,
                                          std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > &vertices,
                                          realT merge_tol = 0.0)
{
    return evaluateHalfspaceIntersection<InputScalarT,OutputScalarT,EigenDim>(Eigen::Map<Eigen::Matrix<InputScalarT,EigenDim+1,Eigen::Dynamic> >((InputScalarT *)halfspaces.data(),EigenDim+1,halfspaces.size()), interior_point, vertices, merge_tol);
}

template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
inline bool evaluateHalfspaceIntersection(const Eigen::Ref<const Eigen::Matrix<InputScalarT,EigenDim+1,Eigen::Dynamic> > &halfspaces,
                                          std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > &vertices,
                                          InputScalarT dist_tol = std::numeric_limits<InputScalarT>::epsilon(),
                                          realT merge_tol = 0.0)
{
    Eigen::Matrix<InputScalarT,EigenDim,1> interior_point;
    if (findFeasiblePointInHalfspaceIntersection<InputScalarT,EigenDim>(halfspaces, interior_point, dist_tol))
        return evaluateHalfspaceIntersection<InputScalarT,OutputScalarT,EigenDim>(halfspaces, interior_point, vertices, merge_tol);
    return false;
}

template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
inline bool evaluateHalfspaceIntersection(const std::vector<Eigen::Matrix<InputScalarT,EigenDim+1,1> > &halfspaces,
                                          std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > &vertices,
                                          InputScalarT dist_tol = std::numeric_limits<InputScalarT>::epsilon(),
                                          realT merge_tol = 0.0)
{
    return evaluateHalfspaceIntersection<InputScalarT,OutputScalarT,EigenDim>(Eigen::Map<Eigen::Matrix<InputScalarT,EigenDim+1,Eigen::Dynamic> >((InputScalarT *)halfspaces.data(),EigenDim+1,halfspaces.size()), vertices, dist_tol, merge_tol);
}

template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
inline bool convexHullFromHalfspaceIntersection(const Eigen::Ref<const Eigen::Matrix<InputScalarT,EigenDim+1,Eigen::Dynamic> > &halfspaces,
                                                const Eigen::Matrix<InputScalarT,EigenDim,1> &interior_point,
                                                std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > &hull_points,
                                                std::vector<Eigen::Matrix<OutputScalarT,EigenDim+1,1> > &halfspaces_out,
                                                std::vector<std::vector<size_t> > &faces,
                                                std::vector<std::vector<size_t> > &point_neighbor_faces,
                                                std::vector<std::vector<size_t> > &face_neighbor_faces,
                                                std::vector<size_t> &hull_point_indices,
                                                double &area, double &volume,
                                                bool simplicial_faces = true,
                                                realT merge_tol = 0.0)
{
    std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > hull_points_tmp;
    if (evaluateHalfspaceIntersection<InputScalarT,OutputScalarT,EigenDim>(halfspaces, interior_point, hull_points_tmp, merge_tol))
        return convexHullFromPoints<InputScalarT,OutputScalarT,EigenDim>(hull_points_tmp, hull_points, halfspaces_out, faces, point_neighbor_faces, face_neighbor_faces, hull_point_indices, area, volume, simplicial_faces, merge_tol);
    return false;
}

template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
inline bool convexHullFromHalfspaceIntersection(const std::vector<Eigen::Matrix<InputScalarT,EigenDim+1,1> > &halfspaces,
                                                const Eigen::Matrix<InputScalarT,EigenDim,1> &interior_point,
                                                std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > &hull_points,
                                                std::vector<Eigen::Matrix<OutputScalarT,EigenDim+1,1> > &halfspaces_out,
                                                std::vector<std::vector<size_t> > &faces,
                                                std::vector<std::vector<size_t> > &point_neighbor_faces,
                                                std::vector<std::vector<size_t> > &face_neighbor_faces,
                                                std::vector<size_t> &hull_point_indices,
                                                double &area, double &volume,
                                                bool simplicial_faces = true,
                                                realT merge_tol = 0.0)
{
    std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > hull_points_tmp;
    if (evaluateHalfspaceIntersection<InputScalarT,OutputScalarT,EigenDim>(halfspaces, interior_point, hull_points_tmp, merge_tol))
        return convexHullFromPoints<InputScalarT,OutputScalarT,EigenDim>(hull_points_tmp, hull_points, halfspaces_out, faces, point_neighbor_faces, face_neighbor_faces, hull_point_indices, area, volume, simplicial_faces, merge_tol);
    return false;
}

template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
inline bool convexHullFromHalfspaceIntersection(const Eigen::Ref<const Eigen::Matrix<InputScalarT,EigenDim+1,Eigen::Dynamic> > &halfspaces,
                                                std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > &hull_points,
                                                std::vector<Eigen::Matrix<OutputScalarT,EigenDim+1,1> > &halfspaces_out,
                                                std::vector<std::vector<size_t> > &faces,
                                                std::vector<std::vector<size_t> > &point_neighbor_faces,
                                                std::vector<std::vector<size_t> > &face_neighbor_faces,
                                                std::vector<size_t> &hull_point_indices,
                                                double &area, double &volume,
                                                bool simplicial_faces = true,
                                                InputScalarT dist_tol = std::numeric_limits<InputScalarT>::epsilon(),
                                                realT merge_tol = 0.0)
{
    std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > hull_points_tmp;
    if (evaluateHalfspaceIntersection<InputScalarT,OutputScalarT,EigenDim>(halfspaces, hull_points_tmp, merge_tol, dist_tol))
        return convexHullFromPoints<InputScalarT,OutputScalarT,EigenDim>(hull_points_tmp, hull_points, halfspaces_out, faces, point_neighbor_faces, face_neighbor_faces, hull_point_indices, area, volume, simplicial_faces, merge_tol);
    return false;
}

template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
inline bool convexHullFromHalfspaceIntersection(const std::vector<Eigen::Matrix<InputScalarT,EigenDim+1,1> > &halfspaces,
                                                std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > &hull_points,
                                                std::vector<Eigen::Matrix<OutputScalarT,EigenDim+1,1> > &halfspaces_out,
                                                std::vector<std::vector<size_t> > &faces,
                                                std::vector<std::vector<size_t> > &point_neighbor_faces,
                                                std::vector<std::vector<size_t> > &face_neighbor_faces,
                                                std::vector<size_t> &hull_point_indices,
                                                double &area, double &volume,
                                                bool simplicial_faces = true,
                                                InputScalarT dist_tol = std::numeric_limits<InputScalarT>::epsilon(),
                                                realT merge_tol = 0.0)
{
    std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > hull_points_tmp;
    if (evaluateHalfspaceIntersection<InputScalarT,OutputScalarT,EigenDim>(halfspaces, hull_points_tmp, merge_tol, dist_tol))
        return convexHullFromPoints<InputScalarT,OutputScalarT,EigenDim>(hull_points_tmp, hull_points, halfspaces_out, point_neighbor_faces, face_neighbor_faces, faces, hull_point_indices, area, volume, simplicial_faces, merge_tol);
    return false;
}

template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
class ConvexHull {
public:
    ConvexHull(const Eigen::Ref<const Eigen::Matrix<InputScalarT,EigenDim,Eigen::Dynamic> > &points, bool simplicial_facets = true, realT merge_tol = 0.0) {
        is_empty_ = !convexHullFromPoints<InputScalarT,OutputScalarT,EigenDim>(points, hull_points_, halfspaces_, faces_, point_neighbor_faces_, face_neighbor_faces_, hull_point_indices_, area_, volume_, simplicial_facets, merge_tol);
        init_();
    }
    ConvexHull(const std::vector<Eigen::Matrix<InputScalarT,EigenDim,1> > &points, bool simplicial_facets = true, realT merge_tol = 0.0) {
        is_empty_ = !convexHullFromPoints<InputScalarT,OutputScalarT,EigenDim>(points, hull_points_, halfspaces_, faces_, point_neighbor_faces_, face_neighbor_faces_, hull_point_indices_, area_, volume_, simplicial_facets, merge_tol);
        init_();
    }
    ConvexHull(const Eigen::Ref<const Eigen::Matrix<InputScalarT,EigenDim+1,Eigen::Dynamic> > &halfspaces, bool simplicial_facets = true, InputScalarT dist_tol = std::numeric_limits<InputScalarT>::epsilon(), realT merge_tol = 0.0) {
        is_empty_ = !convexHullFromHalfspaceIntersection<InputScalarT,OutputScalarT,EigenDim>(halfspaces, hull_points_, halfspaces_, faces_, point_neighbor_faces_, face_neighbor_faces_, hull_point_indices_, area_, volume_, simplicial_facets, dist_tol, merge_tol);
        init_();
    }
    ConvexHull(const std::vector<Eigen::Matrix<InputScalarT,EigenDim+1,1> > &halfspaces, bool simplicial_facets = true, InputScalarT dist_tol = std::numeric_limits<InputScalarT>::epsilon(), realT merge_tol = 0.0) {
        is_empty_ = !convexHullFromHalfspaceIntersection<InputScalarT,OutputScalarT,EigenDim>(halfspaces, hull_points_, halfspaces_, faces_, point_neighbor_faces_, face_neighbor_faces_, hull_point_indices_, area_, volume_, simplicial_facets, dist_tol, merge_tol);
        init_();
    }

    ~ConvexHull() {}

    inline const std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> >& getVertices() const { return hull_points_; }
    inline const std::vector<Eigen::Matrix<OutputScalarT,EigenDim+1,1> >& getFacetHyperplanes() const { return halfspaces_; }
    inline const std::vector<std::vector<size_t> >& getFacetVertexIndices() const { return faces_; }
    inline const std::vector<std::vector<size_t> >& getVertexNeighborFacets() const { return point_neighbor_faces_; }
    inline const std::vector<std::vector<size_t> >& getFacetNeighborFacets() const { return face_neighbor_faces_; }
    inline const std::vector<size_t>& getVertexPointIndices() const { return hull_point_indices_; }

    inline double getArea() const { return area_; }
    inline double getVolume() const { return volume_; }

    bool isPointInHull(const Eigen::Matrix<OutputScalarT,EigenDim,1> &point) const {
        Eigen::Matrix<OutputScalarT,Eigen::Dynamic,1> distances = halfspace_normals_*point + halfspace_offsets_;
        return (distances.array() <= 0.0f).all();
    }

    Eigen::Matrix<OutputScalarT,Eigen::Dynamic,Eigen::Dynamic> getSignedDistancesFromFacets(const std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > &points) const {
        Eigen::Map<Eigen::Matrix<OutputScalarT,EigenDim,Eigen::Dynamic> > map((OutputScalarT *)points.data(), EigenDim, points.size());
        Eigen::Matrix<OutputScalarT,Eigen::Dynamic,Eigen::Dynamic> distances = (halfspace_normals_*map).colwise() + halfspace_offsets_;
        return distances;
    }

    std::vector<size_t> getInteriorPointIndices(const std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > &points, OutputScalarT offset = 0.0) const {
        Eigen::Matrix<bool,1,Eigen::Dynamic> distance_test((getSignedDistancesFromFacets(points).array() <= -offset).colwise().all());
        std::vector<size_t> indices;
        indices.reserve(points.size());
        for (size_t i = 0; i < points.size(); i++) {
            if (distance_test[i]) indices.push_back(i);
        }
        return indices;
    }

    bool isEmpty() const { return is_empty_; }

private:
    Eigen::Matrix<OutputScalarT,Eigen::Dynamic,EigenDim> halfspace_normals_;
    Eigen::Matrix<OutputScalarT,Eigen::Dynamic,1> halfspace_offsets_;
    bool is_empty_;

    std::vector<Eigen::Matrix<OutputScalarT,EigenDim,1> > hull_points_;
    std::vector<Eigen::Matrix<OutputScalarT,EigenDim+1,1> > halfspaces_;
    std::vector<std::vector<size_t> > faces_;
    std::vector<std::vector<size_t> > point_neighbor_faces_;
    std::vector<std::vector<size_t> > face_neighbor_faces_;
    std::vector<size_t> hull_point_indices_;
    double area_;
    double volume_;

    void init_() {
        Eigen::Map<Eigen::Matrix<OutputScalarT,EigenDim+1,Eigen::Dynamic> > map((OutputScalarT *)halfspaces_.data(), EigenDim+1, halfspaces_.size());
        halfspace_normals_ = map.topRows(EigenDim).transpose();
        halfspace_offsets_ = map.row(EigenDim);
    }
};

typedef ConvexHull<float,float,2> ConvexHull2D;
typedef ConvexHull<float,float,3> ConvexHull3D;

class CloudHullFlat : public ConvexHull2D {
public:
    CloudHullFlat(const std::vector<Eigen::Vector3f> &points, bool simplicial_facets = true, realT merge_tol = 0.0);
    CloudHullFlat(const PointCloud &cloud, bool simplicial_facets = true, realT merge_tol = 0.0);

    inline const std::vector<Eigen::Vector3f>& getVertices3D() const { return vertices_3d_; }
private:
    std::vector<Eigen::Vector3f> vertices_3d_;
    void init_(const std::vector<Eigen::Vector3f> &points);
};

class CloudHull : public ConvexHull3D {
public:
    using ConvexHull3D::getSignedDistancesFromFacets;
    using ConvexHull3D::getInteriorPointIndices;
    CloudHull(const PointCloud &cloud, bool simplicial_facets = true, realT merge_tol = 0.0);
    Eigen::MatrixXf getSignedDistancesFromFacets(const PointCloud &cloud) const;
    std::vector<size_t> getInteriorPointIndices(const PointCloud &cloud, float offset = 0.0) const;
};
