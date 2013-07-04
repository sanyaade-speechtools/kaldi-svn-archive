// cudamatrix/cuda-cu-sp-matrix-test.cc
//
//
// UnitTests for testing cu-sp-matrix.h methods.
//

#include <iostream>
#include <vector>
#include <cstdlib>

#include "base/kaldi-common.h"
#include "cudamatrix/cu-sp-matrix.h"
#include "cudamatrix/cu-vector.h"
#include "cudamatrix/cu-math.h"

using namespace kaldi;

namespace kaldi {

/*
 * INITIALIZERS
 */
// SetRandn() could be used.

/*
 * ASSERTS
 */
template<class Real>
static void AssertEqual(VectorBase<Real> &A, VectorBase<Real> &B, float tol = 0.001) {
  KALDI_ASSERT(A.Dim() == B.Dim());
  for (MatrixIndexT i = 0; i < A.Dim(); i++)
    KALDI_ASSERT(std::abs(A(i)-B(i)) < tol);
}

template<class Real>
static bool ApproxEqual(VectorBase<Real> &A, VectorBase<Real> &B, float tol = 0.001) {
  KALDI_ASSERT(A.Dim() == B.Dim());
  for (MatrixIndexT i = 0; i < A.Dim(); i++)
    if (std::abs(A(i)-B(i)) > tol) return false;
  return true;
}

static void AssertEqual(std::vector<int32> &A, std::vector<int32> &B) {
  KALDI_ASSERT(A.size() == B.size());
  for (size_t i = 0; i < A.size(); i++)
    KALDI_ASSERT(A[i] == B[i]);
}

template<class Real>
static void AssertEqual(const SpMatrix<Real> &A,
                        const SpMatrix<Real> &B,
                        float tol = 0.001) {
  KALDI_ASSERT(A.NumRows() == B.NumRows());
  for (MatrixIndexT i = 0; i < A.NumRows(); i++)
    for (MatrixIndexT j = 0; j <= i; j++)
      KALDI_ASSERT(std::abs(A(i, j) - B(i, j))
                   < tol * std::max(1.0, (double) (std::abs(A(i, j)) + std::abs(B(i, j)))));
}

template<class Real>
static bool ApproxEqual(const SpMatrix<Real> &A,
                        const SpMatrix<Real> &B, Real tol = 0.001) {
  KALDI_ASSERT(A.NumRows() == B.NumRows());
  SpMatrix<Real> diff(A);
  diff.AddSp(1.0, B);
  Real a = std::max(A.Max(), -A.Min()), b = std::max(B.Max(), -B.Min()),
      d = std::max(diff.Max(), -diff.Min());
  return (d <= tol * std::max(a, b));
}

/*
 * Unit Tests
 */
// test the CuSpMatrix(CuMatrixBase, SpCopyType) constructor
template<class Real>
static void UnitTestCuSpMatrixConstructor() {
  for (MatrixIndexT i = 1; i < 10; i++) {
    MatrixIndexT dim = 10 * i;

    Matrix<Real> A(dim, dim);
    A.SetRandn();
    SpMatrix<Real> B(A, kTakeLower);

    CuMatrix<Real> C(A);
    CuSpMatrix<Real> D(C, kTakeLower);
    SpMatrix<Real> E(dim);
    D.CopyToSp(&E);
    
    AssertEqual(B, E);
  }
}

// test the operator()
template<class Real>
static void UnitTestCuSpMatrixOperator() {
  SpMatrix<Real> A(100);
  A.SetRandn();

  CuSpMatrix<Real> B(100);
  B.CopyFromSp(A);

  for (MatrixIndexT i = 0; i < A.NumRows(); i++) {
    for (MatrixIndexT j = 0; j <= i; j++)
      KALDI_ASSERT(std::abs(A(i, j) - B(i, j)) < 0.0001);
  }
}

// test the Invert() method
template<class Real>
static void UnitTestCuSpMatrixInvert() {
  for (MatrixIndexT i = 1; i < 10; i++) {
    MatrixIndexT dim = 10 * i;
    SpMatrix<Real> A(dim);
    A.SetRandn();
    CuSpMatrix<Real> B(A);

    A.Invert();
    B.Invert();

    SpMatrix<Real> C(dim);
    B.CopyToSp(&C);

    AssertEqual(A, C);    
  }
}

// test AddVec2
// TODO (variani) : fails for dim = 0 
template<class Real>
static void UnitTestCuSpMatrixAddVec2() {
  for (int32 i = 0; i < 50; i++) {
    MatrixIndexT dim = 1 + rand() % 200;
    SpMatrix<Real> A(dim);
    A.SetRandn();
    CuSpMatrix<Real> B(A);
    
    Vector<Real> C(dim);
    C.SetRandn();
    CuVector<Real> D(C);
    Real alpha = RandGauss();

    A.AddVec2(alpha, C);
    B.AddVec2(alpha, D);

    SpMatrix<Real> E(dim);
    B.CopyToSp(&E);

    AssertEqual(A, E);
  }
}

// test AddMat2
template<class Real>
static void UnitTestCuSpMatrixAddMat2() {
  for (MatrixIndexT i = 1; i < 10; i++) {
    MatrixIndexT dim_row = 15 * i + rand() % 10;
    MatrixIndexT dim_col = 7 *i + rand() % 10;
    Matrix<Real> A(dim_row, dim_col);
    A.SetRandn();
    CuMatrix<Real> B(A);

    SpMatrix<Real> C(dim_col);
    C.SetRandn();
    CuSpMatrix<Real> D(C);

    const Real alpha = 2.0;
    const Real beta = 3.0;

    C.AddMat2(alpha, A, kTrans, beta);
    D.AddMat2(alpha, B, kTrans, beta);

    SpMatrix<Real> E(dim_col);
    D.CopyToSp(&E);

    AssertEqual(C, E);
        
  }
}

template<class Real>
static void UnitTestCuSpMatrixAddSp() {
  for (MatrixIndexT i = 1; i < 50; i++) {
    MatrixIndexT dim = 7 * i + rand() % 10;
    
    SpMatrix<Real> A(dim);
    A.SetRandn();
    CuSpMatrix<Real> B(A);

    SpMatrix<Real> C(dim);
    C.SetRandn();
    const CuSpMatrix<Real> D(C);

    const Real alpha = 2.0;
    
    A.AddSp(alpha, C);
    B.AddSp(alpha, D);

    SpMatrix<Real> E(dim);
    B.CopyToSp(&E);

    AssertEqual(A, E);
  }
}

template<class Real>
static void UnitTestCuSpMatrixTraceSpSp() {
  for (MatrixIndexT i = 1; i < 50; i++) {
    MatrixIndexT dim = 5 * i + rand() % 10;
    KALDI_LOG << "dim is : " << dim;
    
    SpMatrix<Real> A(dim);
    A.SetRandn();
    const CuSpMatrix<Real> B(A);

    SpMatrix<Real> C(dim);
    C.SetRandn();
    const CuSpMatrix<Real> D(C);

    Real a = TraceSpSp(A, C);
    Real b = TraceSpSp(B, D);
    
    KALDI_LOG << a;
    KALDI_LOG << b;
    
    KALDI_ASSERT((a-b)==0);
  }
}

template<class Real> void CudaSpMatrixUnitTest() {
  UnitTestCuSpMatrixConstructor<Real>();
  UnitTestCuSpMatrixOperator<Real>();
  UnitTestCuSpMatrixInvert<Real>();
  UnitTestCuSpMatrixAddVec2<Real>();
  UnitTestCuSpMatrixAddMat2<Real>();
  UnitTestCuSpMatrixAddSp<Real>();
  UnitTestCuSpMatrixTraceSpSp<Real>();
}

} // namespace kaldi


int main() {
  using namespace kaldi;
  // Select the GPU
  kaldi::int32 use_gpu_id = -2;
#if HAVE_CUDA == 1
  CuDevice::Instantiate().SelectGpuId(use_gpu_id);
#endif
  kaldi::CudaSpMatrixUnitTest<float>();
  kaldi::CudaSpMatrixUnitTest<double>();
  KALDI_LOG << "Tests succeeded";
  return 0;
}