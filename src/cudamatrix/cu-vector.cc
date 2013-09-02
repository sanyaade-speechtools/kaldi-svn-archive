#if HAVE_CUDA == 1
#include <cuda_runtime_api.h>
#include <cublas.h>
#endif

#include "util/timer.h"
#include "cudamatrix/cu-common.h"
#include "cudamatrix/cu-vector.h"
#include "cudamatrix/cu-device.h"
#include "cudamatrix/cu-kernels.h"
#include "cudamatrix/cu-randkernels.h"
#include "cudamatrix/cu-math.h"
#include "cudamatrix/cu-vector.h"
#include "cudamatrix/cu-matrix.h"
#include "cudamatrix/cu-rand.h"
#include "cudamatrix/cu-tp-matrix.h"
#include "cudamatrix/cu-sp-matrix.h"

namespace kaldi {


#if HAVE_CUDA == 1
inline float cublas_dot(int n, const float* x, int incx,
                        const float* y, int incy) {
  return cublasSdot(n,x,incx,y,incy);
}
inline double cublas_dot(int n, const double* x, int incx,
                         const double* y, int incy) {
  return cublasDdot(n,x,incx,y,incy);


}
inline float cublas_sum(int n, const float* x, int incx) {
  return cublasSasum(n, x, incx);
}
inline float cublas_sum(int n, const double* x, int incx) {
  return cublasDasum(n, x, incx);
}
#endif


template<typename Real>
Real VecVec(const CuVectorBase<Real> &a,
            const CuVectorBase<Real> &b) {
  //MatrixIndexT a_dim = a.Dim();
  KALDI_ASSERT(a.Dim() == b.Dim());
  Real result = 0;
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    Timer tim;

    result = cublas_dot(a.Dim(), a.Data(), 1, b.Data(), 1);

    CU_SAFE_CALL(cublasGetError());
    CuDevice::Instantiate().AccuProfile(__func__, tim.Elapsed());
} else
#endif
  {
    result = VecVec(a.Vec(), b.Vec());
  }
  return result;
}

template
float VecVec<>(const CuVectorBase<float> &a,
               const CuVectorBase<float> &b);

template
double VecVec<>(const CuVectorBase<double> &a,
                const CuVectorBase<double> &b);

template<typename Real>
void CuVectorBase<Real>::CopyColFromMat(const CuMatrixBase<Real> &mat, MatrixIndexT col) {
  KALDI_ASSERT(col < mat.NumCols());
  KALDI_ASSERT(dim_ == mat.NumRows());
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    
    Timer tim;
    int dimBlock(CU2DBLOCK);
    int dimGrid(n_blocks(dim_,CU2DBLOCK));

    cuda_copy_col_from_mat(dimGrid, dimBlock, data_, col, mat.Data(), mat.Dim(), dim_);
    CuDevice::Instantiate().AccuProfile("CuVectorBase::CopyColFromMat", tim.Elapsed());
  } else
#endif
  {
    Vec().CopyColFromMat(mat.Mat(),col);
  }
}

template<>
template<>
void CuVectorBase<double>::CopyColFromMat(const CuMatrixBase<float> &mat, MatrixIndexT col) {
  KALDI_ASSERT(col < mat.NumCols());
  KALDI_ASSERT(dim_ == mat.NumRows());
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    Timer tim;
    int dimBlock(CU2DBLOCK);
    int dimGrid(n_blocks(dim_,CU2DBLOCK));

    cuda_copy_col_from_mat_df(dimGrid, dimBlock, data_, col, mat.Data(), mat.Dim(), dim_);
    CuDevice::Instantiate().AccuProfile("CuVectorBase::CopyColFromMat", tim.Elapsed());
  } else
#endif
  {
    Vec().CopyColFromMat(mat.Mat(), col);
  }
}


template<>
template<>
void CuVectorBase<float>::CopyColFromMat(const CuMatrixBase<double> &mat, MatrixIndexT col) {
  KALDI_ASSERT(col < mat.NumCols());
  KALDI_ASSERT(dim_ == mat.NumRows());
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    Timer tim;
    int dimBlock(CU2DBLOCK);
    int dimGrid(n_blocks(dim_,CU2DBLOCK));

    cuda_copy_col_from_mat_fd(dimGrid, dimBlock, data_, col, mat.Data(), mat.Dim(), dim_);
    CuDevice::Instantiate().AccuProfile("CuVectorBase::CopyColFromMat", tim.Elapsed());   
  } else
#endif
  {
    Vec().CopyColFromMat(mat.Mat(), col);
  }
}

template<typename Real>
void CuVectorBase<Real>::CopyRowsFromMat(const CuMatrixBase<Real> &mat) {
  KALDI_ASSERT(dim_ == mat.NumCols() * mat.NumRows());
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    Timer tim;
    if (mat.Stride() == mat.NumCols()) {
      cudaMemcpy(data_, mat.Data(), sizeof(Real)*dim_, cudaMemcpyDeviceToDevice);
    } else {
      Real* vec_data = data_;
      for (MatrixIndexT r = 0; r < mat.NumRows(); r++) {
        cudaMemcpy(vec_data, mat.RowData(r), sizeof(Real) * mat.NumCols(), cudaMemcpyDeviceToDevice);
        vec_data += mat.NumCols();
      }
    }
    CuDevice::Instantiate().AccuProfile("CuVectorBase::CopyRowsFromMat", tim.Elapsed());
  } else
#endif
  {
    Vec().CopyRowsFromMat(mat.Mat());
  }
}

template<typename Real>
void CuVectorBase<Real>::SetRandn() {
  CuRand<Real> tmp;
  tmp.RandGaussian(this);
}


template<typename Real>
Real CuVectorBase<Real>::Sum() const {
  Real sum_value = 0;
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    //* this is working now 
    Timer tim;
    int dimBlock(CU1DBLOCK);
    int dimGrid = 1;// only 1 block here. we have loops in each thread  //(n_blocks(dim_, CU1DBLOCK));
    Real *device_sum_value;
    CU_SAFE_CALL(cudaMalloc(reinterpret_cast<void**>(&device_sum_value), sizeof(Real)));
    CU_SAFE_CALL(cudaMemset(device_sum_value, 0, sizeof(Real)));
    cuda_vec_sum(dimGrid, dimBlock, data_, device_sum_value, dim_);
    CU_SAFE_CALL(cudaMemcpy(&sum_value, device_sum_value, sizeof(Real), cudaMemcpyDeviceToHost));
    CU_SAFE_CALL(cudaFree(device_sum_value));
    CuDevice::Instantiate().AccuProfile("CuVectorBase::Sum", tim.Elapsed());
//    */

 /*/added by hxu, alternative implementation
    // sum_value = cublas_sum(Dim(), Data(), 1); //this doesn't work cuz it calculates the sum of abs of elements
    CuVector<Real> ones(dim_);
    ones.Set(1.0);   
    sum_value = VecVec(*this, ones);
*/

  } else
#endif
  {
    sum_value = Vec().Sum();
  }
  return sum_value;
} 


template<typename Real>
void CuVectorBase<Real>::ApplySoftMax() {
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    Timer tim;
    int dimBlock(CU1DBLOCK);
    int dimGrid(n_blocks(dim_, CU1DBLOCK));

    cuda_vec_soft_max(dimGrid, dimBlock, data_, dim_);
    CuDevice::Instantiate().AccuProfile("CuVectorBase::ApplySoftMax", tim.Elapsed());
  } else
#endif
  {
    Vec().ApplySoftMax();
  }
}

template<typename Real>
MatrixIndexT CuVectorBase<Real>::ApplyFloor(Real floor_val) {
  MatrixIndexT num_floored = 0;
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {

    Timer tim;
    int dimBlock(CU2DBLOCK);
    int dimGrid(n_blocks(dim_,CU2DBLOCK));

    CuVector<float> count_vec(dim_, kUndefined);
    
    cuda_vec_apply_floor(dimGrid, dimBlock, data_, floor_val, count_vec.Data(), dim_);
    num_floored = count_vec.Sum();
    CuDevice::Instantiate().AccuProfile("CuVectorBase::ApplyFloor", tim.Elapsed());
  } else
#endif
  {
    num_floored = Vec().ApplyFloor(floor_val);
  }
  return num_floored;

}

template<typename Real>
void CuVectorBase<Real>::ApplyPow(Real power) {
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {

    Timer tim;
    // for this particular kernel, x is #rows, y is #cols.  so
    // fake matrix with 1 row, Dim() cols.
    dim3 dimBlock(1, CU1DBLOCK);
    dim3 dimGrid(1, n_blocks(Dim(), CU1DBLOCK));
    ::MatrixDim fake_matrix_dim = { 1, Dim(), 1 };
    // num_cols is Dim(), num_rows is 1, stride is 1 (it's a don't-care).
    cuda_apply_pow(dimGrid, dimBlock, data_, power, fake_matrix_dim);
    CuDevice::Instantiate().AccuProfile("CuVectorBase::ApplyFloor", tim.Elapsed());
  } else
#endif
  {
    Vec().ApplyPow(power);
  }
}


template<typename Real>
void CuVectorBase<Real>::ApplyExp() {
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {

    Timer tim;
    int dimBlock(CU2DBLOCK);
    int dimGrid(n_blocks(dim_,CU2DBLOCK));

    cuda_vec_apply_exp(dimGrid, dimBlock, data_, dim_);
    CuDevice::Instantiate().AccuProfile("CuVectorBase::ApplyExp", tim.Elapsed());
  } else
#endif
  {
    Vec().ApplyExp();
  }
}


template<typename Real>
void CuVectorBase<Real>::ApplyLog() {
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {

    Timer tim;
    int dimBlock(CU2DBLOCK);
    int dimGrid(n_blocks(dim_, CU2DBLOCK));

    Real* device_flag;
    CU_SAFE_CALL(cudaMalloc(reinterpret_cast<void**>(&device_flag), sizeof(Real)));
    CU_SAFE_CALL(cudaMemset(device_flag, 0, sizeof(Real)));
    cuda_vec_apply_log(dimGrid, dimBlock, data_, device_flag, dim_);
    Real host_flag = 0.0;
    CU_SAFE_CALL(cudaMemcpy(&host_flag, device_flag, sizeof(Real), cudaMemcpyDeviceToHost));
    if (host_flag > 0)
      KALDI_ERR << "Trying to take log of a negative number.";
    CU_SAFE_CALL(cudaFree(device_flag));
    CuDevice::Instantiate().AccuProfile("CuVectorBase::ApplyLog", tim.Elapsed());

  } else
#endif
  {
    Vec().ApplyLog();
  }
}

#if HAVE_CUDA == 1
inline void cublas_gemv(char trans, int m, int n, float alpha,
                                   const float* A, int lda, const float* x,
                                   int incx, float beta, float* y, int incy) {
  cublasSgemv(trans,m,n,alpha,A,lda,x,incx,beta,y,incy);
}
inline void cublas_gemv(char trans, int m, int n, double alpha,
                                   const double* A, int lda, const double* x,
                                   int incx, double beta, double* y, int incy) {
  cublasDgemv(trans,m,n,alpha,A,lda,x,incx,beta,y,incy);
}
#endif

template<typename Real>
void CuVectorBase<Real>::AddMatVec(const Real alpha,
                                   const CuMatrixBase<Real> &M,
                                   MatrixTransposeType trans,
                                   const CuVectorBase<Real> &v,
                                   const Real beta) {
  KALDI_ASSERT((trans == kNoTrans && M.NumCols() == v.dim_ && M.NumRows() == dim_) || (trans == kTrans && M.NumRows() == v.dim_ && M.NumCols() == dim_));
  KALDI_ASSERT(&v != this);
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    Timer tim;
    
  cublas_gemv((trans==kTrans?'T':'N'), M.NumRows(), M.NumCols(), alpha, M.Data(), M.Stride(), v.Data(), 1, beta, data_, 1);

  CU_SAFE_CALL(cublasGetError());
  CuDevice::Instantiate().AccuProfile(__func__, tim.Elapsed());
  } else
#endif
  {
    Vec().AddMatVec(alpha,M.Mat(),trans,v.Vec(),beta);
  }
}

template<typename Real>
void CuVectorBase<Real>::AddVecVec(Real alpha, const CuVectorBase<Real> &v,
                                   const CuVectorBase<Real> &r, Real beta) {
  KALDI_ASSERT((dim_ == v.dim_ && dim_ == r.dim_));
  KALDI_ASSERT(this != &v && this != &r);
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {

    Timer tim;
    int dimBlock(CU2DBLOCK);
    int dimGrid(n_blocks(dim_,CU2DBLOCK));

    cuda_add_vec_vec(dimGrid, dimBlock, alpha, data_, v.Data(), r.Data(), beta, dim_);
    CuDevice::Instantiate().AccuProfile("CuVectorBase::AddVecVec", tim.Elapsed());
  } else
#endif
  {
//    KALDI_LOG << "Salam!" << '\n';
    Vec().AddVecVec(alpha, v.Vec(), r.Vec(), beta);
  }
}


template<typename Real>
bool CuVectorBase<Real>::ApproxEqual(const CuVectorBase<Real> &other, float tol) const {
  if (dim_ != other.dim_) KALDI_ERR << "ApproxEqual: size mismatch "
                                    << dim_ << " vs. " << other.dim_;
  KALDI_ASSERT(tol >= 0.0);
  CuVector<Real> tmp(*this);
  tmp.AddVec(-1.0, other);
  BaseFloat tmp_norm = sqrt(VecVec(tmp, tmp)), this_norm = sqrt(VecVec(*this, *this));
  KALDI_LOG  << "tmp norm is " << tmp_norm << ", this_norm =" << this_norm;
  return tmp_norm <= static_cast<Real>(tol) * this_norm;
}


template<typename Real>
void CuVectorBase<Real>::AddDiagMat2(Real alpha, const CuMatrixBase<Real> &M,
                                     MatrixTransposeType trans, Real beta) {
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    Timer tim;
    int dimBlock(CU2DBLOCK);
    int dimGrid(n_blocks(dim_,CU2DBLOCK));
    
    if (trans == kNoTrans) {
      cuda_add_diag_mat(dimGrid, dimBlock, alpha, data_, M.Data(), beta, M.Dim(), dim_);
    } else {
      cuda_add_diag_mat_trans(dimGrid, dimBlock, alpha, data_, M.Data(), beta, M.Dim(), dim_);
    }
    CuDevice::Instantiate().AccuProfile("CuVectorBase::AddDiagMat2", tim.Elapsed());
  } else
#endif
  {
    Vec().AddDiagMat2(alpha, M.Mat(), trans, beta);
  }
  
}

template<typename Real>
void CuVectorBase<Real>::AddTpVec(const Real alpha, const CuTpMatrix<Real> &M,
                                  const MatrixTransposeType trans,
                                  const CuVectorBase<Real> &v,
                                  const Real beta) {
  KALDI_ASSERT(dim_ == v.dim_ && dim_ == M.NumRows());
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    Timer tim;
    if (beta == 0.0) {
      if (&v != this) CopyFromVec(v);
      MulTp(M, trans);
      if (alpha != 1.0) Scale(alpha);
    } else {
      CuVector<Real> tmp(v);
      tmp.MulTp(M, trans);
      if (beta != 1.0) Scale(beta);  // *this <-- beta * *this
      AddVec(alpha, tmp, 1.0);          // *this += alpha * M * v
    }      
  } else
#endif
  {
    Vec().AddTpVec(alpha, M.Mat(), trans, v.Vec(), beta);
  }
}

#if HAVE_CUDA == 1
// Use caution with these, the 'transpose' argument is the opposite of what it
// should really be, due to CUDA storing things in column major order.  We also
// had to switch 'l' to 'u'; we view our packed matrices as lower-triangular,
// row-by-row, but CUDA views the same layout as upper-triangular,
// column-by-column.
inline void cublas_tpmv(char trans, int n,
                        const float* Ap, float* x, int incx) {
  return cublasStpmv('u', trans, 'n', n, Ap, x, incx);
}
inline void cublas_tpmv(char trans, int n, const double* Ap,
                        double* x,int incx) {
  return cublasDtpmv('u', trans, 'n', n, Ap, x, incx);
}
#endif


template<typename Real>
void CuVectorBase<Real>::MulTp(const CuTpMatrix<Real> &M, const MatrixTransposeType trans) {
  KALDI_ASSERT(M.NumRows() == dim_);
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    Timer tim;
    cublas_tpmv((trans==kTrans?'N':'T'), M.NumRows(), M.Data(), data_, 1);
    CuDevice::Instantiate().AccuProfile("CuVectorBase::MulTp", tim.Elapsed());    
  } else
#endif
  {
    Vec().MulTp(M.Mat(), trans);
  }
}


template<typename Real>
Real CuVectorBase<Real>::Min() const {
  Real result = 0.0;
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    Timer tim;
    int dimBlock(CU2DBLOCK);
    int dimGrid(n_blocks(dim_,CU2DBLOCK));
    Real* device_value;
    CU_SAFE_CALL(cudaMalloc(reinterpret_cast<void**>(&device_value), sizeof(Real)));
    CU_SAFE_CALL(cudaMemset(device_value, 0, sizeof(Real)));
    cuda_min(dimGrid, dimBlock, data_, device_value, dim_);
    CU_SAFE_CALL(cudaGetLastError());
    CU_SAFE_CALL(cudaMemcpy(&result, device_value, sizeof(Real), cudaMemcpyDeviceToHost));
    CU_SAFE_CALL(cudaFree(device_value));
    CuDevice::Instantiate().AccuProfile("CuVectorBase::Min", tim.Elapsed());

  } else
#endif
  {
    result = (this->Vec()).Min();
  }
  return result;
}

template<typename Real>
void CuVectorBase<Real>::MulElements(const CuVectorBase<Real> &v) {
  KALDI_ASSERT(dim_ == v.dim_);
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    Timer tim;
    int dimBlock(CU2DBLOCK);
    int dimGrid(n_blocks(dim_, CU2DBLOCK));
    cuda_vec_mul_elements(dimGrid, dimBlock, data_, v.Data(), dim_);
    CuDevice::Instantiate().AccuProfile("CuVectorBase::MulElements", tim.Elapsed());
  } else
#endif
  {
    Vec().MulElements(v.Vec());
  }
}

template<>
template<>
void CuVectorBase<double>::CopyFromVec(const CuVectorBase<float> &src) {
  KALDI_ASSERT(src.Dim() == dim_);
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) { 
    Timer tim;
    int dimBlock(CU2DBLOCK);
    int dimGrid(n_blocks(dim_, CU2DBLOCK));
    cuda_copy_from_vec_df(dimGrid, dimBlock, data_, src.data_, dim_);
    CuDevice::Instantiate().AccuProfile(__func__, tim.Elapsed());    
  } else
#endif
  {
    Vec().CopyFromVec(src.Vec());
  }
}

template<>
template<>
void CuVectorBase<float>::CopyFromVec(const CuVectorBase<double> &src) {
  KALDI_ASSERT(src.Dim() == dim_);
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) { 
    Timer tim;
    int dimBlock(CU2DBLOCK);
    int dimGrid(n_blocks(dim_, CU2DBLOCK));
    cuda_copy_from_vec_fd(dimGrid, dimBlock, data_, src.data_, dim_);
    CuDevice::Instantiate().AccuProfile(__func__, tim.Elapsed());
  } else
#endif
  {
    Vec().CopyFromVec(src.Vec());
  }
}


template<typename Real>
template<typename OtherReal>
void CuVectorBase<Real>::CopyFromVec(const VectorBase<OtherReal> &src) {
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {      
    if (sizeof(Real) != sizeof(OtherReal)) {
      CuVector<OtherReal> temp(dim_, kUndefined);
      temp.CopyFromVec(src);
      this->CopyFromVec(temp);
    } else {
      KALDI_ASSERT(src.Dim() == dim_);
      Timer tim;
      CU_SAFE_CALL(cudaMemcpy(data_, src.Data(), src.Dim()*sizeof(Real), cudaMemcpyHostToDevice));
      CuDevice::Instantiate().AccuProfile("CuVector::CopyFromVecH2D",tim.Elapsed());
    }
  } else
  #endif
  {
    Vec().CopyFromVec(src);
  }
}
// Instantiate the template above.
template
void CuVectorBase<float>::CopyFromVec(const VectorBase<float> &src);
template
void CuVectorBase<double>::CopyFromVec(const VectorBase<float> &src);
template
void CuVectorBase<float>::CopyFromVec(const VectorBase<double> &src);
template
void CuVectorBase<double>::CopyFromVec(const VectorBase<double> &src);

template<typename Real>
template<typename OtherReal>
void CuVectorBase<Real>::CopyToVec(VectorBase<OtherReal> *dst) const {
  KALDI_ASSERT(dim_ == dst->Dim());
#if HAVE_CUDA == 1
  if (CuDevice::Instantiate().Enabled()) {
    if (sizeof(Real) != sizeof(OtherReal)) {
      CuVector<OtherReal> temp(*this);
      temp.CopyToVec(dst);
    } else {
      Timer tim;
      CU_SAFE_CALL(cudaMemcpy(dst->Data(), this->data_,
                              sizeof(Real) * dim_, cudaMemcpyDeviceToHost));
      CuDevice::Instantiate().AccuProfile(__func__, tim.Elapsed());
    }
  } else
#endif
  {
    dst->CopyFromVec(this->Vec());
  }
}

template
void CuVectorBase<float>::CopyToVec(VectorBase<float> *dst) const;
template
void CuVectorBase<double>::CopyToVec(VectorBase<float> *dst) const;
template
void CuVectorBase<float>::CopyToVec(VectorBase<double> *dst) const;
template
void CuVectorBase<double>::CopyToVec(VectorBase<double> *dst) const;

template class CuVectorBase<float>;
template class CuVectorBase<double>;

} // namespace