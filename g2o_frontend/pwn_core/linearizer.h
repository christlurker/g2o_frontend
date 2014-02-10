#pragma once

#include "bm_se3.h"
#include "homogeneousvector4f.h"
#include "informationmatrix.h"

using namespace Eigen;

namespace pwn {

  class Aligner;
  
  /** \class Linearizer linearizer.h "linearizer.h"
   *  \brief Class that implements a linearizing algorithm used by the Aligner.
   *  
   *  This class provides a linearing algorithm used by the Aligner to align
   *  two point clouds.
   */
  class Linearizer {
  public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW;

    /**
     *  Empty constructor.
     *  This constructor creates a Linearizer with default values for all its attributes.
     *  All the pointers to objects implementing an algorithm have to be setted since
     *  this constructor sets them to zero.
     */
    Linearizer();

    /**
     *  Destructor.
     */
    virtual ~Linearizer() {}

    /**
     *  Method that returns a pointer to the Aligner used by the Linearizer.
     *  @return a pointer to the Linearizer's Aligner.
     *  @see setAligner()
     */
    inline Aligner *aligner() const { return _aligner; }  

    /**
     *  Method that set the Aligner used by the Linearizer to the one given in input.
     *  @param aligner_ is a pointer to the Aligner used to update the Linearizer's Aligner. 
     *  @see aligner()
     */
    inline void setAligner(Aligner * const aligner_) { _aligner = aligner_; }

    /**
     *  Method that returns the transformation set to the Linearizer.
     *  @return the transformation set to the Linearizer.
     *  @see setT()
     */
    inline Isometry3f T() const { return _T; }  

    /**
     *  Method that set the transformation used by the Linearizer to the one given in input.
     *  @param T_ is an isometry transformation used to update the transformation set of the Linearizer.
     *  @see T()
     */
    inline void setT(const Isometry3f T_) { 
      _T = T_; 
      _T.matrix().block<1, 4>(3, 0) << 0.0f, 0.0f, 0.0f, 1.0f; 
    }

    /**
     *  Method that returns the chi square threshold set to the Linearizer.
     *  @return the chi square threshold set to the Linearizer.
     *  @see setInlierMaxChi2()
     */
    inline float inlierMaxChi2() const { return _inlierMaxChi2; }

    /**
     *  Method that set the chi square threshold to the one given in input.
     *  @param inlierMaxChi2_ is a float value used to update the chi square threshold set of the Linearizer.
     *  @see inlierMaxChi2()
     */
    inline void setInlierMaxChi2(const float inlierMaxChi2_) { _inlierMaxChi2 = inlierMaxChi2_; }

    /**
     *  Method that returns a bool value that indicates if the Linearizer is in robust kernel mode or not.
     *  @return true if the robust kernel is used, false otherwise.
     *  @see setRobustKernel()
     */
    inline bool robustKernel() const { return _robustKernel; }

    /**
     *  Method that set the Linearizer to use or not the robust kernel mode.
     *  @param robustKernel_ is a bool value used to set the robust kernel mode of the Linearizer true or false.
     *  @see robustKernel()
     */
    inline void setRobustKernel(bool robustKernel_) { _robustKernel = robustKernel_; }
    
    /**
     *  Method that returns the Hessian matrix of the least squares problem computed by the Linearizer.
     *  @return the 6x6 float matrix representing the Hessian matrix of the least squares problem computed 
     *  by the Linearizer.
     *  @see b()
     */
    inline Matrix6f H() const { return _H; }  

    /**
     *  Method that returns the b vector of the least squares problem computed by the Linearizer.
     *  @return the 6 elements float vector representing the b vector of the least squares problem computed 
     *  by the Linearizer.
     *  @see H()
     */
    inline Vector6f b() const { return _b; }  
    
    /**
     *  Method that returns the error generated by the Linearizer in the update step.
     *  @return a float value representing the error generated by the Linearizer in the update step.
     *  @see inliers()
     */
    inline float error() const { return _error; }

    /**
     *  Method that returns the inliers floud by the Linearizer in the update step.
     *  @return a float value representing the inoiers found by the Linearizer in the update step.
     *  @see error()
     */
    inline int inliers() const { return _inliers; }
    
    /**
     *  This method compute the update step calculating the new Hessian matrix and b vector of the 
     *  least squares problem used to compute the alignment between two point clouds.
     */    
    void update();

  protected:
    Aligner *_aligner; /**< Pointer to the Aligner used by the Linearizer to access some Aligner's objects. */

    Isometry3f _T; /**< Isometry transformation used by the Linearizer to update the point clouds to align. */
    float _inlierMaxChi2; /**< Chi square error threshold used by the Linearizer update step. */

    Matrix6f _H; /**< Hessian matrix of the least squares problem computed by the Linearizer. */
    Vector6f _b; /**< b vector of the least squares problem computed by the Linearizer. */
    float _error; /**< Error generated by the Linearizer after the update step. */
    int _inliers; /**< Inliers found by the Linearizer after the update step. */
    bool _robustKernel; /**< Bool value used to say to the Linearizer to use robust kernel mode or not. */
  };

}
