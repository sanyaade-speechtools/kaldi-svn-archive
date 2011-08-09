// gmm/mmie-diag-gmm.h

// Copyright 2009-2011  Arnab Ghoshal

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.


#ifndef KALDI_GMM_MMIE_DIAG_GMM_H_
#define KALDI_GMM_MMIE_DIAG_GMM_H_ 1

#include <string>

#include "gmm/diag-gmm.h"
#include "gmm/estimate-diag-gmm.h"
#include "gmm/model-common.h"
#include "util/parse-options.h"

namespace kaldi {

/** \struct MmieDiagGmmOptions
 *  Configuration variables like variance floor, minimum occupancy, etc.
 *  needed in the estimation process.
 */
struct MmieDiagGmmOptions : public MleDiagGmmOptions {
  BaseFloat i_smooth_tau;
  BaseFloat ebw_e;
  MmieDiagGmmOptions() : MleDiagGmmOptions() {
    i_smooth_tau = 100.0;
    ebw_e = 2.0;
  }
  void Register(ParseOptions *po) {
    std::string module = "MmieDiagGmmOptions: ";
    po->Register("min-gaussian-weight", &min_gaussian_weight,
                 module+"Min Gaussian weight before we remove it.");
    po->Register("min-gaussian-occupancy", &min_gaussian_occupancy,
                 module+"Minimum occupancy to update a Gaussian.");
    po->Register("min-variance", &min_variance,
                 module+"Variance floor (absolute variance).");
    po->Register("remove-low-count-gaussians", &remove_low_count_gaussians,
                 module+"If true, remove Gaussians that fall below the floors.");
    po->Register("i-smooth-tau", &i_smooth_tau,
                 module+"Coefficient for I-smoothing.");
    po->Register("ebw-e", &ebw_e, module+"Smoothing constant for EBW update.");
  }
};

class AccumDiagGmm {
 public:
  void Read(std::istream &in_stream, bool binary, bool add);
  void Write(std::ostream &out_stream, bool binary) const;

  /// Allocates memory for accumulators
  void Resize(int32 num_comp, int32 dim, GmmFlagsType flags);
  /// Calls ResizeAccumulators with arguments based on gmm
  void Resize(const DiagGmm &gmm, GmmFlagsType flags);

  /// Returns the number of mixture components
  int32 NumGauss() const { return num_comp_; }
  /// Returns the dimensionality of the feature vectors
  int32 Dim() const { return dim_; }

  void SetZero(GmmFlagsType flags);
  void Scale(BaseFloat f, GmmFlagsType flags);

  /// Accumulate for a single component, given the posterior
  void AccumulateForComponent(const VectorBase<BaseFloat>& data,
                              int32 comp_index, BaseFloat weight);

  /// Accumulate for all components, given the posteriors.
  void AccumulateFromPosteriors(const VectorBase<BaseFloat>& data,
                                const VectorBase<BaseFloat>& gauss_posteriors);

  /// Accumulate for all components given a diagonal-covariance GMM.
  /// Computes posteriors and returns log-likelihood
  BaseFloat AccumulateFromDiag(const DiagGmm &gmm,
                               const VectorBase<BaseFloat>& data,
                               BaseFloat frame_posterior);

  /// Smooths the accumulated counts by adding 'tau' extra frames. An example
  /// use for this is I-smoothing for MMIE/MPE.
  void SmoothStats(BaseFloat tau);

  /// Smooths the accumulated counts using some other accumulator. Performs
  /// a weighted sum of the current accumulator with the given one. Both
  /// accumulators must have the same dimension and number of components.
  void SmoothWithAccum(BaseFloat tau, const AccumDiagGmm& src_acc);

  /// Smooths the accumulated counts using the parameters of a given model.
  /// An example use of this is MAP-adaptation. The model must have the
  /// same dimension and number of components as the current accumulator.
  void SmoothWithModel(BaseFloat tau, const DiagGmm& src_gmm);

  // Accessors
  const GmmFlagsType Flags() const { return flags_; }

 private:
  /// Flags corresponding to the accumulators that are stored.
  GmmFlagsType flags_;

  int32 dim_;
  int32 num_comp_;
  Vector<double> occupancy_;
  Matrix<double> mean_accumulator_;
  Matrix<double> variance_accumulator_;
};

/** Class for computing the maximum-likelihood estimates of the parameters of
 *  a Gaussian mixture model.
 */
class MmieDiagGmm {
 public:
  MmieDiagGmm(): dim_(0), num_comp_(0), flags_(0) {}

  /// Computes the difference between the numerator and denominator accumulators
  /// and applies I-smoothing to the numerator accs, if needed.
  void SubtractAccumulators(const AccumDiagGmm& num_acc,
                            const AccumDiagGmm& den_acc,
                            const MmieDiagGmmOptions& opts);

  void Update(const MmieDiagGmmOptions &config,
              GmmFlagsType flags,
              DiagGmm *gmm,
              BaseFloat *obj_change_out,
              BaseFloat *count_out) const;

  BaseFloat MmiObjective(const DiagGmm& gmm) const;

 private:
  /// Accumulators
  // TODO(arnab): not decided yet whether to store the difference or keep the
  //              num and den accs for mean and var.
  Vector<double> num_occupancy_;
  Vector<double> den_occupancy_;
  Matrix<double> mean_accumulator_;
  Matrix<double> variance_accumulator_;

  BaseFloat ComputeD(const DiagGmm& old_gmm, int32 mix_index, BaseFloat ebw_e);

  // Cannot have copy constructor and assigment operator
  KALDI_DISALLOW_COPY_AND_ASSIGN(MmieDiagGmm);
};

inline void AccumDiagGmm::Resize(const DiagGmm &gmm, GmmFlagsType flags) {
  Resize(gmm.NumGauss(), gmm.Dim(), flags);
}

}  // End namespace kaldi


#endif  // KALDI_GMM_MMIE_DIAG_GMM_H_
