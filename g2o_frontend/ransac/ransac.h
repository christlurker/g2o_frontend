#ifndef _G2O_FRONTEND_RANSAC_H_
#define _G2O_FRONTEND_RANSAC_H_
#include <vector>
#include <algorithm>
#include <assert.h>
#include <Eigen/Geometry>
#include "g2o/core/optimizable_graph.h"
//for debug
#include "g2o/types/slam2d_addons/types_slam2d_addons.h"

#include <iostream>
#include <fstream>

namespace g2o_frontend {

using namespace std;

struct Correspondence {
    Correspondence(g2o::OptimizableGraph::Edge* edge_, double score_=0):  _edge(edge_), _score(score_){}
    inline g2o::OptimizableGraph::Edge* edge() {return _edge;}
    inline const g2o::OptimizableGraph::Edge* edge() const {return _edge;}
    inline double score() const {return _score;}
    inline bool operator<(const Correspondence& c2) const { return score()>c2.score(); }  // sorts the corresopondences is ascending order
protected:
    g2o::OptimizableGraph::Edge* _edge;
    double _score;
};

typedef std::vector<Correspondence> CorrespondenceVector;
typedef std::vector<int> IndexVector;

class CorrespondenceValidator{
public:
    CorrespondenceValidator(int minimalSetSize_):_minimalSetSize(minimalSetSize_){}
    virtual bool operator()(const CorrespondenceVector& correspondences, const IndexVector& indices, int k) = 0;
    virtual ~CorrespondenceValidator();
    inline int minimalSetSize() const {return _minimalSetSize;}
protected:
    int _minimalSetSize;
};

typedef std::vector<CorrespondenceValidator*> CorrespondenceValidatorPtrVector;


template <typename TransformType_, typename PointVertexType_>
class AlignmentAlgorithm{
public:
    typedef TransformType_ TransformType;
    typedef PointVertexType_ PointVertexType;
    typedef typename PointVertexType_::EstimateType PointEstimateType;
    
    AlignmentAlgorithm(int minimalSetSize_) {_minimalSetSize = minimalSetSize_;}
    virtual bool operator()(TransformType& transform, const CorrespondenceVector& correspondences, const IndexVector& indices) = 0;
    inline int minimalSetSize() const  {return _minimalSetSize;}
protected:
    int _minimalSetSize;
};

//typedef AlignmentAlgorithm<g2o::SE2,g2o::VertexLine2D>  AlignmentAlgorithmSE2Line2D;
//typedef AlignmentAlgorithm<Eigen::Isometry3d,Slam3dAddons::VertexLine3D>   AlignmentAlgorithmSE3Line3D;

struct ciError{
  int idx;
  double err;
};
typedef std::vector<ciError> ciErrVector;
typedef std::map<int, ciErrVector> viCorrMap;

class BaseGeneralizedRansac{
public:
    BaseGeneralizedRansac(int _minimalSetSize);
    ~BaseGeneralizedRansac();

    void setCorrespondences(const CorrespondenceVector& correspondences_);
    inline const CorrespondenceVector& correspondences() const {return _correspondences;}
    bool validateCorrespondences(int k);
    inline CorrespondenceValidatorPtrVector& correspondenceValidators() {return _correspondenceValidators;}
    inline const CorrespondenceValidatorPtrVector& correspondenceValidators() const {return _correspondenceValidators;}
    inline void setInlierErrorThreshold(double inlierErrorThreshold_) {_inlierErrorTheshold = inlierErrorThreshold_;}
    inline double inlierErrorThreshold() const {return _inlierErrorTheshold;}
    inline void setInlierStopFraction(double inlierStopFraction_) {_inlierStopFraction = inlierStopFraction_;}
    inline double inlierStopFraction() const {return _inlierStopFraction;}
    inline const std::vector<double> errors() const { return _errors;}
    inline int minimalSetSize() const {return _minimalSetSize;}
    inline const IndexVector getInliersIndeces() const {return _indices;}
    int maxIterations() const {return _maxIterations;}
    void setMaxIterations(double maxIterations_) { _maxIterations = maxIterations_;}
    virtual void keepBestFriend(std::vector<int>& bestInliers_, const std::vector<double> _errors, std::vector<int>& inliers_);

protected:
    bool _init();
    bool _cleanup();

    int _minimalSetSize;
    int _maxIterations;
    IndexVector _indices;
    CorrespondenceVector _correspondences;
    CorrespondenceValidatorPtrVector _correspondenceValidators;
    double _inlierErrorTheshold;
    double _inlierStopFraction;
    std::vector<double> _errors;
    std::vector<g2o::OptimizableGraph::Vertex*> _vertices1;
    std::vector<g2o::OptimizableGraph::Vertex*> _vertices2;
    bool _verticesPushed;
};

template <typename AlignmentAlgorithmType>
class GeneralizedRansac : public BaseGeneralizedRansac{
public:
    typedef AlignmentAlgorithmType AlignerType;
    typedef typename AlignmentAlgorithmType::TransformType TransformType;
    typedef typename AlignmentAlgorithmType::PointVertexType PointVertexType;
    typedef typename AlignmentAlgorithmType::PointVertexType::EstimateType PointEstimateType;

    GeneralizedRansac(int minimalSetSize_) :BaseGeneralizedRansac(minimalSetSize_){
    }


    inline AlignmentAlgorithmType& alignmentAlgorithm() {return _alignmentAlgorithm;}
    inline const AlignmentAlgorithmType& alignmentAlgorithm() const {return _alignmentAlgorithm;}

    ostream& pindex(ostream& os, const std::vector<int> v, size_t k) {
        for (size_t i=0; i<=k && i<v.size(); i++){
            os << v[i] << " " ;
        }
        os << endl;
        return os;
    }

    bool computeMinimalSet(TransformType& t, int k){
        // base step
        bool transformFound = false;
        int maxIndex =_correspondences.size();
        while (_indices[k]<maxIndex && ! transformFound){
            bool validated = validateCorrespondences(k);
            if (! validated) {
                _indices[k] ++;
                continue;
            } else {
                if (k==minimalSetSize()-1){
                    transformFound = _alignmentAlgorithm(t,_correspondences,_indices);
                    //cerr << "Inner Iteration (" << k << ") : ";
                    // pindex(cerr, _indices, k);
                    // cerr << endl;
                    _indices[k]++;
                } else {
                    if (_indices[k+1]<_indices[k])
                        _indices[k+1]=_indices[k]+1;

                    transformFound = computeMinimalSet(t,k+1);

                    if(_indices[k+1]>maxIndex-((int)_indices.size()-k)){
                        _indices[k]++;
                        _indices[k+1]=_indices[k]+1;
                    }
                }
            }
        }
        return transformFound;
    }

    //mal
    bool operator()(TransformType& treturn, std::vector<int>& inliers_, bool debug = false, int bestFriendFilter = 0){
        if ((int)_correspondences.size()<minimalSetSize())
            return false;
        if (!_init())
            assert(0 && "_ERROR_IN_INITIALIZATION_");

        std::vector<int> bestInliers;
        bestInliers.reserve(_correspondences.size());
        double bestError=std::numeric_limits<double>::max();
        TransformType bestTransform = treturn;

        bool transformFound = false;
        for (int i=0; i<_maxIterations && !(_indices[0]>(int)_correspondences.size()-(int)_indices.size());  i++){
            TransformType t;
            if(debug)
                cerr << "iteration: " << i << endl;
            if (! computeMinimalSet(t,0)) {
                if(debug)
                    cerr << "FAIL" << endl;
                continue;
            }


            //debug: checking the minimalset and the transform found
            cout << "transform found after computeMinimalSet: " << (int) transformFound << endl;
            cerr << "Inner Iteration (" << i << ") : ";
            pindex(cerr, _indices, i);
            cerr << endl;
            cerr << "transform: " << t.toIsometry().matrix() << endl;
            ofstream os1minsetoctave("l1minset_octave.dat");
            ofstream os2minsetoctave("l2minset_octave.dat");
            ofstream os1minset("l1minset.dat");
            ofstream os2minset("l2minset.dat");
            ofstream os2minsetRem("l2minsetRem.dat");
            ofstream oscminset("cr_minset.dat");
            for (size_t j = 0; j<_indices.size(); j++){
                Correspondence& c=_correspondences[j];
                g2o::OptimizableGraph::Edge* e=c.edge();
                g2o::VertexLine2D* vl1 = static_cast<g2o::VertexLine2D*>(e->vertex(0));
                g2o::VertexLine2D* vl2 = static_cast<g2o::VertexLine2D*>(e->vertex(1));

                g2o::OptimizableGraph* g = vl1->graph();
                Eigen::Vector2d p11=dynamic_cast<g2o::VertexPointXY*>(g->vertex(vl1->p1Id))->estimate();
                Eigen::Vector2d p12=dynamic_cast<g2o::VertexPointXY*>(g->vertex(vl1->p2Id))->estimate();
                Eigen::Vector2d p21=dynamic_cast<g2o::VertexPointXY*>(g->vertex(vl2->p1Id))->estimate();
                Eigen::Vector2d p22=dynamic_cast<g2o::VertexPointXY*>(g->vertex(vl2->p2Id))->estimate();

                PointEstimateType v1est = vl1->estimate();
                Eigen::Vector3d line1 = Eigen::Vector3d(cos(v1est(0)), sin(v1est(0)), v1est(1));
                os1minsetoctave << line1.transpose() << endl;

                PointEstimateType v2est = vl2->estimate();
                Eigen::Vector3d line2 = Eigen::Vector3d(cos(v2est(0)), sin(v2est(0)), v2est(1));
                os2minsetoctave << line2.transpose() << endl;

                os1minset << p11.transpose() << endl;
                os1minset << p12.transpose() << endl;
                os1minset << endl;

                os2minset << p21.transpose() << endl;
                os2minset << p22.transpose() << endl;
                os2minset << endl;

                p21 = t * p21;
                p22 = t * p22;
                os2minsetRem << p21.transpose() << endl;
                os2minsetRem << p22.transpose() << endl;
                os2minsetRem << endl;

                //link for the correspondances
                Eigen::Vector2d pm1 = (p11+p12)*0.5;
                Eigen::Vector2d pm2 = (p21+p22)*0.5;
                oscminset << pm1.transpose() << endl;
                oscminset << pm2.transpose() << endl;
                oscminset << endl;
            }


            if(debug)
                cerr << "OK" << endl;
            std::vector<int> inliers;
            inliers.reserve(_correspondences.size());
            std::vector<double> currentErrors(_correspondences.size());
            double error = 0;

            // a transform has been found, now we need to apply it to all points to determine if they are inliers
            for(size_t k=0; k<_correspondences.size(); k++){
                Correspondence& c=_correspondences[k];
                g2o::OptimizableGraph::Edge* e=c.edge();
                PointVertexType* v1=static_cast<PointVertexType*>(e->vertex(0));
                PointVertexType* v2=static_cast<PointVertexType*>(e->vertex(1));
                PointEstimateType ebackup = v2->estimate();
                v2->setEstimate(t*ebackup);
                e->computeError();
                currentErrors[k] = e->chi2();
                //cerr << "e: " << e->chi2() << endl;
                if (e->chi2()<_inlierErrorTheshold){
                    if (debug) {
                    cerr << "**************** INLIER ****************" << endl;
                        cerr << endl << "v1 " << v1->id() << " ";
                        v1->write(cerr);
                        cerr << endl;
                        v2->setEstimate(ebackup);
                        cerr <<  "v2 " << v2->id() << " ";
                        v2->write(cerr);
                        cerr << endl;
                        v2->setEstimate(t*ebackup);
                        cerr << "remappedV2 ";
                        v2->write(cerr);
                        cerr << endl;
                        cerr << "chi2: " << e->chi2() << endl;
                        cerr << "error: " << error << endl;
                    }
                    inliers.push_back(k);
                    error+=e->chi2();
                }
                v2->setEstimate(ebackup);
            }

            //martina
            //debug
//            for (size_t i = 0; i<inliers.size(); i++){
//                cerr << inliers[i] << " ";
//            }
//            cerr << endl;
//            for (size_t i = 0; i<inliers.size(); i++){
//                int idx = inliers[i];
//                cerr << _errors[idx] << " ";
//            }
//            cerr << endl;
            cerr << "inliers are: ";
            for (size_t i = 0; i<inliers.size(); i++){
              int idx = inliers[i];
              Correspondence& c=_correspondences[idx];
              double err = _errors[idx];
              g2o::OptimizableGraph::Edge* e=c.edge();
              g2o::OptimizableGraph::Vertex* v1=static_cast<g2o::OptimizableGraph::Vertex*>(e->vertex(0));
              g2o::OptimizableGraph::Vertex* v2=static_cast<g2o::OptimizableGraph::Vertex*>(e->vertex(1));
              cerr << "(" << idx << ","<< e << ","<< v1->id() << "," << v2->id() << "," << err << "), ";
            }
            cerr << endl;
            if(bestFriendFilter) {
                cerr << endl << "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" << endl;
                std::vector<int> bestFriendInliers;
                keepBestFriend(bestFriendInliers, currentErrors, inliers);
            }
            cerr << "BestFriendFilter? " << bestFriendFilter << endl;

            //saving the correspondances (inliers) into files for octave uses
//            ofstream os1corr("l1_octave.dat");
//            ofstream os2corr("l2_octave.dat");
//            for (size_t i = 0; i<inliers.size(); i++){
//                int idx = inliers[i];
//                Correspondence& c=_correspondences[idx];
//                g2o::OptimizableGraph::Edge* e=c.edge();

//                PointVertexType* v1=static_cast<PointVertexType*>(e->vertex(0));
//                const PointEstimateType theLine1 = v1->estimate();
//                Eigen::Vector3d line1 = Eigen::Vector3d(cos(theLine1(0)), sin(theLine1(0)), theLine1(1));
//                os1corr << line1.transpose() << endl;

//                PointVertexType* v2=static_cast<PointVertexType*>(e->vertex(1));
//                const PointEstimateType theLine2 = v2->estimate();
//                Eigen::Vector3d line2 = Eigen::Vector3d(cos(theLine2(0)), sin(theLine2(0)), theLine2(1));
//                os2corr << line2.transpose() << endl;
//            }

            if(debug)
                cerr << "transformFound:" << (int) transformFound << endl;
            if (debug)
                cerr << "inliers:" << inliers.size() << endl;
            if ((int)inliers.size()<minimalSetSize()) {
                if(debug)
                    cerr << "too few inliers: " << (int)inliers.size() <<  endl;
                continue;
            }
            if(debug)
            cerr << "error:" << error/inliers.size() << endl;

            if (inliers.size()>bestInliers.size()){
                if(debug)
                    cerr << "enough inliers: " << (int)inliers.size() <<  endl;
                double currentError = error/inliers.size();
                if (currentError<bestError){
                    if(debug)
                        cerr << "good error: " << currentError <<  endl;
                    bestError= currentError;
                    cerr << "saving inliers: " << inliers.size() << "in bestinliers (before)" << bestInliers.size() << endl;

                    bestInliers = inliers;
                    cerr << "bestinliers (after)" << bestInliers.size() << endl;

                    _errors = currentErrors;
                    bestTransform = t;
                    transformFound = true;

                    //mal
                    //inliers_=bestInliers;
                }
                if ((double)bestInliers.size()/(double)_correspondences.size() > _inlierStopFraction){
                    transformFound = true;
                    if(debug)
                        cerr << "excellent inlier fraction: "
                             << 1e2 *(double)inliers.size()/(double)_correspondences.size() <<"%"  <<endl;
                    break;
                }
            }
        }
        //martina
//        if(bestFriendFilter) {
//            cerr << endl << "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" << endl;
//            std::vector<int> bestFriendInliers;
//            keepBestFriend(bestFriendInliers, _errors, bestInliers);
//        }
//        cerr << "BestFriendFilter? " << bestFriendFilter << endl;


        //saving the correspondances (inliers) into files for octave uses
//        ofstream os1corr("l1_octave.dat");
//        ofstream os2corr("l2_octave.dat");
//        for (size_t i = 0; i<bestInliers.size(); i++){
//            int idx = bestInliers[i];
//            Correspondence& c=_correspondences[idx];
//            g2o::OptimizableGraph::Edge* e=c.edge();

//            PointVertexType* v1=static_cast<PointVertexType*>(e->vertex(0));
//            const PointEstimateType theLine1 = v1->estimate();
//            Eigen::Vector3d line1 = Eigen::Vector3d(cos(theLine1(0)), sin(theLine1(0)), theLine1(1));
//            os1corr << line1.transpose() << endl;

//            PointVertexType* v2=static_cast<PointVertexType*>(e->vertex(1));
//            const PointEstimateType theLine2 = v2->estimate();
//            Eigen::Vector3d line2 = Eigen::Vector3d(cos(theLine2(0)), sin(theLine2(0)), theLine2(1));
//            os2corr << line2.transpose() << endl;
//        }

        //mal
        inliers_=bestInliers;
        cerr << "saving bestinliers: " << bestInliers.size() << " in inliers_ " << inliers_.size() << endl;

        if (transformFound){
            _alignmentAlgorithm(bestTransform,_correspondences,bestInliers);
        }
        treturn = bestTransform;
        if (!_cleanup())
            assert(0 && "_ERROR_IN_CLEANUP_");
        return transformFound;
    }

protected:
    AlignmentAlgorithmType _alignmentAlgorithm;
};

}

#endif
