#include "cylindricalpointprojector.h"

namespace pwn {

CylindricalPointProjector::CylindricalPointProjector() : PointProjector() {
  _cameraMatrix << 
    1.0, 0.0, 0.5, 
    0.0, 1.0, 0.5,
    0.0, 0.0, 1;
  _baseline = 0.075f;
  _alpha = 0.1f;
  _updateMatrices();
  setAngularResolution(360);
}

CylindricalPointProjector::~CylindricalPointProjector() {}

void inline CylindricalPointProjector::setCameraMatrix(const Eigen::Matrix3f &cameraMatrix_) {
  _cameraMatrix = cameraMatrix_;
  _updateMatrices();
}

void CylindricalPointProjector::_updateMatrices() {
  _iT =_transform.inverse();
  _iT.matrix().block<1, 4>(3, 0) << 0, 0, 0, 1;
  _iK = _cameraMatrix.inverse();
  _KR = _cameraMatrix * _iT.linear();
  _Kt = _cameraMatrix * _iT.translation();
  _iKR = _transform.linear() * _iK;
  _iKt = _transform.translation();
  _KRt.setIdentity();
  _iKRt.setIdentity();
  _KRt.block<3, 3>(0, 0) = _KR; 
  _KRt.block<3, 1>(0, 3) = _Kt;
  _iKRt.block<3, 3>(0, 0) = _iKR; 
  _iKRt.block<3, 1>(0, 3) = _iKt;
}

inline bool CylindricalPointProjector::project(int &x, int &y, float &f, const Point &p) const {
  return _project(x, y, f, p);
}

inline bool CylindricalPointProjector::unProject(Point &p, const int x, const int y, const float d) const {
  return _unProject(p, x, y, d);
}

inline int CylindricalPointProjector::projectInterval(const int x, const int y, const float d, const float worldRadius) const {
  return _projectInterval(x, y, d, worldRadius);
}

void CylindricalPointProjector::project(Eigen::MatrixXi &indexImage,
				    Eigen::MatrixXf &depthImage, 
				    const PointVector &points) const {
  depthImage.resize(indexImage.rows(), indexImage.cols());
  depthImage.fill(std::numeric_limits<float>::max());
  indexImage.fill(-1);
  const Point *point = &points[0];
  for (size_t i=0; i<points.size(); i++, point++){
    int x, y;
    float d;
    if (!_project(x, y, d, *point)||
	d<_minDistance || 
	d>_maxDistance ||
	x<0 || x>=indexImage.rows() ||
	y<0 || y>=indexImage.cols()  )
      continue;
    float &otherDistance = depthImage.coeffRef(x,y);
    int &otherIndex = indexImage.coeffRef(x,y);
    if (otherDistance>d) {
      otherDistance = d;
      otherIndex = i;
    }
  }
}

void CylindricalPointProjector::projectIntervals(Eigen::MatrixXi& intervalImage, 
					     const Eigen::MatrixXf& depthImage, 
					     const float worldRadius) const {
  intervalImage.resize(depthImage.rows(), depthImage.cols());
  int cpix = 0;
  for (int c=0; c<depthImage.cols(); c++){
    const float *f = &depthImage(0,c);
    int *i = &intervalImage(0,c);
    for (int r=0; r<depthImage.rows(); r++, f++, i++){
      *i = _projectInterval(r, c, *f, worldRadius);
      cpix++;
    }
  }
}

void CylindricalPointProjector::unProject(PointVector& points, 
				      Eigen::MatrixXi& indexImage,
				      const Eigen::MatrixXf& depthImage) const {
  points.resize(depthImage.rows()*depthImage.cols());
  int count = 0;
  indexImage.resize(depthImage.rows(), depthImage.cols());
  Point* point = &points[0];
  int cpix=0;
  for (int c=0; c<depthImage.cols(); c++){
    const float* f = &depthImage(0,c);
    int* i =&indexImage(0,c);
    for (int r=0; r<depthImage.rows(); r++, f++, i++){
      if (!_unProject(*point, r,c,*f)){
	*i=-1;
	continue;
      }
      point++;
      cpix++;
      *i=count;
      count++;
    }
  }
  points.resize(count);
}

void CylindricalPointProjector::unProject(PointVector &points, 
				      Gaussian3fVector &gaussians,
				      Eigen::MatrixXi &indexImage,
				      const Eigen::MatrixXf &depthImage) const {
  points.resize(depthImage.rows()*depthImage.cols());
  gaussians.resize(depthImage.rows()*depthImage.cols());
  indexImage.resize(depthImage.rows(), depthImage.cols());
  int count = 0;
  Point *point = &points[0];
  Gaussian3f *gaussian = &gaussians[0];
  int cpix = 0;
  float fB = _baseline * _cameraMatrix(0,0);
  Eigen::Matrix3f J;
  for (int c = 0; c < depthImage.cols(); c++) {
    const float *f = &depthImage(0, c);
    int *i = &indexImage(0, c);
    for (int r=0; r<depthImage.rows(); r++, f++, i++){      
      if(!_unProject(*point, r, c, *f)) {
	*i = -1;
	continue;
      }
      float z = *f;
      float zVariation = (_alpha * z * z) / (fB + z * _alpha);
       J <<       
	z, 0, (float)r,
	0, z, (float)c,
	0, 0, 1;
      J = _iK * J;
      Diagonal3f imageCovariance(1.0f, 1.0f, zVariation);
      Eigen::Matrix3f cov = J * imageCovariance * J.transpose();
      *gaussian = Gaussian3f(point->head<3>(), cov);
      gaussian++;
      point++;
      cpix++;
      *i = count;
      count++;
    }
  }
  points.resize(count);
  gaussians.resize(count);
}

}