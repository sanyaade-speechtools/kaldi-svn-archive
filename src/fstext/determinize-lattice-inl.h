// fstext/determinize-lattice-inl.h

// Copyright 2009-2011  Microsoft Corporation

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

#ifndef KALDI_FSTEXT_DETERMINIZE_LATTICE_INL_H_
#define KALDI_FSTEXT_DETERMINIZE_LATTICE_INL_H_
#include "base/kaldi-error.h"
// Do not include this file directly.  It is included by determinize-lattice.h

#ifdef _MSC_VER
#include <unordered_map>
#else
#include <tr1/unordered_map>
#endif
#include <vector>
using std::tr1::unordered_map;
#include <climits>

namespace fst {

// This class maps back and forth from/to integer id's to sequences of strings.
// used in determinization algorithm.  It is constructed in such a way that
// finding the string-id of the successor of (string, next-label) has constant time.

// Note: class IntType, typically int32, is the type of the element in the
// string (typically a template argument of the CompactLatticeWeightTpl).

template<class IntType> class LatticeStringRepository {
 public:
  struct Entry {
    const Entry *parent; // NULL for empty string.
    IntType i;
    inline bool operator == (const Entry &other) const {
      return (parent == other.parent && i == other.i);
    }
    Entry(const Entry *parent, IntType i): parent(parent), i(i) {}
    Entry(const Entry &e): parent(e.parent), i(e.i) {}
  };
  // Note: all Entry* pointers returned in function calls are
  // owned by the repository itself, not by the caller!

  // Interface guarantees empty string is NULL.  
  inline const Entry *EmptyString() { return NULL; }  

  // Returns string of "parent" with i appended.  Pointer
  // owned by repository
  const Entry *Successor(const Entry *parent, IntType i) {
    Entry entry(parent, i);
    typename SetType::iterator iter = set_.find(&entry);
    if(iter == set_.end()) { // no such entry already...
      Entry *entry_ptr = new Entry(entry);
      set_.insert(entry_ptr);
      return entry_ptr;
    } else {
      return *iter;
    }
  }
  const Entry *Concatenate (const Entry *a, const Entry *b) {
    if(a == NULL) return b;
    else if(b == NULL) return a;
    vector<IntType> v;
    ConvertToVector(b, &v);
    Entry *ans = a;
    for(size_t i = 0; i < v.size(); i++)
      ans = Successor(ans, v[i]);
    return ans;
  }
  const Entry *CommonPrefix (const Entry *a, const Entry *b) {
    vector<IntType> a_vec, b_vec;
    ConvertToVector(a, &a_vec);
    ConvertToVector(b, &b_vec);
    const Entry *ans = NULL;
    for(size_t i = 0; i < a_vec.size() && i < b_vec.size() &&
            a_vec[i] == b_vec[i]; i++)
      ans = Successor(ans, a_vec[i]);
    return ans;
  }

  // removes any elements from b that are not part of
  // a common prefix with a.
  void ReduceToCommonPrefix(const Entry *a,
                            vector<IntType> *b) {
    vector<IntType> a_vec;
    ConvertToVector(a, &a_vec);
    if(b->size() > a_vec.size())
      b->resize(a_vec.size());
    size_t b_sz = 0, max_sz = std::min(a_vec.size(), b->size());
    while(b_sz < max_sz && (*b)[b_sz] == a_vec[b_sz])
      b_sz++;
    if(b_sz != b->size())
      b->resize(b_sz);
  }

  // removes the first n elements of a.
  const Entry *RemovePrefix(const Entry *a, size_t n) {
    if(n==0) return a;
    vector<IntType> a_vec;
    ConvertToVector(a, &a_vec);
    assert(a_vec.size() >= n);
    Entry *ans = NULL;
    for(size_t i = n; i < a_vec.size(); i++)
      ans = Successor(ans, a_vec[i]);
    return ans;
  }
  
  // Returns true if a is a prefix of b.  If a is prefix of b,
  // time taken is |b| - |a|.  Else, time taken is |b|.
  bool IsPrefixOf(const Entry *a, const Entry *b) const {
    if(a == NULL) return true; // empty string prefix of all.
    if(a == b) return true;
    if(b == NULL) return false;
    return IsPrefixOf(a, b->parent);
  }
  
  void ConvertToVector(const Entry *entry, vector<IntType> *out) const {
    if(entry == NULL) out->clear();
    else {
      ConvertToVector(entry->parent, out);
      out->push_back(entry->i);
    }
  }

  const Entry *ConvertFromVector(const vector<IntType> &vec) {
    Entry *e = NULL;
    for(size_t i = 0; i < vec.size(); i++)
      e = Successor(e, vec[i]);
    return e;
  }
  
  LatticeStringRepository() { }

  void Destroy() {
    for (typename SetType::iterator iter = set_.begin();
         iter != set_.end();
         ++iter)
      delete *iter;
    SetType tmp;
    tmp.swap(set_);
  }
  
  ~LatticeStringRepository() { Destroy(); }
 private:
  
  class EntryKey { // Hash function object.
   public:
    inline size_t operator()(const Entry *entry) const {
      return static_cast<size_t>(entry->i)
          + reinterpret_cast<size_t>(entry->parent);
    }
  };
  class EntryEqual {
   public:
    inline bool operator()(const Entry *e1, const Entry *e2) const {
      return (*e1 == *e2);
    }
  };
  typedef unordered_set<const Entry*, EntryKey, EntryEqual> SetType;
  
  DISALLOW_COPY_AND_ASSIGN(LatticeStringRepository);
  SetType set_;

};



// class LatticeDeterminizer is templated on the same types that
// CompactLatticeWeight is templated on: the base weight (Weight), typically
// LatticeWeightTpl<float> etc. but could also be e.g. TropicalWeight, and the
// IntType, typically int32, used for the output symbols in the compact
// representation of strings [note: the output symbols would usually be
// p.d.f. id's in the anticipated use of this code] It has a special requirement
// on the Weight type: that there should be a Compare function on the weights
// such that Compare(w1, w2) returns -1 if w1 < w2, 0 if w1 == w2, and +1 if w1 >
// w2.  This requires that there be a total order on the weights.

template<class Weight, class IntType> class LatticeDeterminizer {
 public:
  // Output to Gallic acceptor (so the strings go on weights, and there is a 1-1 correspondence
  // between our states and the states in ofst.  If destroy == true, release memory as we go
  // (but we cannot output again).

  typedef CompactLatticeWeightTpl<Weight, IntType> CompactWeight;
  typedef ArcTpl<CompactWeight> CompactArc; // arc in compact, acceptor form of lattice
  typedef ArcTpl<Weight> Arc; // arc in non-compact version of lattice 
  
  void Output(MutableFst<CompactArc>  *ofst, bool destroy = true) {
    typedef typename Arc::StateId StateId;
    StateId nStates = static_cast<StateId>(output_arcs_.size());
    if(destroy)
      FreeMostMemory();
    ofst->DeleteStates();
    ofst->SetStart(kNoStateId);
    if (nStates == 0) {
      return;
    }
    for (StateId s = 0;s < nStates;s++) {
      OutputStateId news = ofst->AddState();
      assert(news == s);
    }
    ofst->SetStart(0);
    // now process transitions.
    for (StateId this_state = 0; this_state < nStates; this_state++) {
      vector<TempArc> &this_vec(output_arcs_[this_state]);
      typename vector<TempArc>::const_iterator iter = this_vec.begin(), end = this_vec.end();

      for (;iter != end; ++iter) {
        const TempArc &temp_arc(*iter);
        CompactArc new_arc;
        vector<Label> seq;
        repository_.ConvertToVector(temp_arc.string, &seq);
        CompactWeight weight(temp_arc.weight, seq);
        if (temp_arc.nextstate == kNoStateId) {  // is really final weight.
          ofst->SetFinal(this_state, weight);
        } else {  // is really an arc.
          new_arc.nextstate = temp_arc.nextstate;
          new_arc.ilabel = temp_arc.ilabel;
          new_arc.olabel = temp_arc.ilabel;  // acceptor.  input == output.
          new_arc.weight = weight;  // includes string and weight.
          ofst->AddArc(this_state, new_arc);
        }
      }
      // Free up memory.  Do this inside the loop as ofst is also allocating memory
      if (destroy) { vector<TempArc> temp; std::swap(temp, this_vec); }
    }
    if (destroy) { vector<vector<TempArc> > temp; std::swap(temp, output_arcs_); }
  }

  // Output to standard FST with Weight as its weight type.  We will create extra
  // states to handle sequences of symbols on the output.  If destroy == true,
  // release memory as we go (but we cannot output again).

  void  Output(MutableFst<Arc> *ofst, bool destroy = true) {
    // Outputs to standard fst.
    OutputStateId nStates = static_cast<OutputStateId>(output_arcs_.size());
    ofst->DeleteStates();
    if (nStates == 0) {
      ofst->SetStart(kNoStateId);
      return;
    }
    if(destroy)
      FreeMostMemory();
    // Add basic states-- but we will add extra ones to account for strings on output.
    for (OutputStateId s = 0;s < nStates;s++) {
      OutputStateId news = ofst->AddState();
      assert(news == s);
    }
    ofst->SetStart(0);
    for (OutputStateId this_state = 0; this_state < nStates; this_state++) {
      vector<TempArc> &this_vec(output_arcs_[this_state]);

      typename vector<TempArc>::const_iterator iter = this_vec.begin(), end = this_vec.end();
      for (;iter != end; ++iter) {
        const TempArc &temp_arc(*iter);
        vector<Label> seq;
        repository_.ConvertToVector(temp_arc.string, &seq);

        if (temp_arc.nextstate == kNoStateId) {  // Really a final weight.
          // Make a sequence of states going to a final state, with the strings
          // as labels.  Put the weight on the first arc.
          OutputStateId cur_state = this_state;
          for (size_t i = 0;i < seq.size();i++) {
            OutputStateId next_state = ofst->AddState();
            Arc arc;
            arc.nextstate = next_state;
            arc.weight = (i == 0 ? temp_arc.weight : Weight::One());
            arc.ilabel = 0;  // epsilon.
            arc.olabel = seq[i];
            ofst->AddArc(cur_state, arc);
            cur_state = next_state;
          }
          ofst->SetFinal(cur_state, (seq.size() == 0 ? temp_arc.weight : Weight::One()));
        } else {  // Really an arc.
          OutputStateId cur_state = this_state;
          // Have to be careful with this integer comparison (i+1 < seq.size()) because unsigned.
          // i < seq.size()-1 could fail for zero-length sequences.
          for (size_t i = 0; i+1 < seq.size();i++) {
            // for all but the last element of seq, create new state.
            OutputStateId next_state = ofst->AddState();
            Arc arc;
            arc.nextstate = next_state;
            arc.weight = (i == 0 ? temp_arc.weight : Weight::One());
            arc.ilabel = (i == 0 ? temp_arc.ilabel : 0);  // put ilabel on first element of seq.
            arc.olabel = seq[i];
            ofst->AddArc(cur_state, arc);
            cur_state = next_state;
          }
          // Add the final arc in the sequence.
          Arc arc;
          arc.nextstate = temp_arc.nextstate;
          arc.weight = (seq.size() <= 1 ? temp_arc.weight : Weight::One());
          arc.ilabel = (seq.size() <= 1 ? temp_arc.ilabel : 0);
          arc.olabel = (seq.size() > 0 ? seq.back() : 0);
          ofst->AddArc(cur_state, arc);
        }
      }
      // Free up memory.  Do this inside the loop as ofst is also allocating memory
      if (destroy) {
        vector<TempArc> temp; temp.swap(this_vec);
      }
    }
    if (destroy) {
      vector<vector<TempArc> > temp;
      temp.swap(output_arcs_);
    }
  }


  // Initializer.  After initializing the object you will typically call 
  // one of the Output functions.  Note: ifst.Copy() will generally do a
  // shallow copy.  We do it like this for memory safety, rather than
  // keeping a reference or pointer to ifst_.
  LatticeDeterminizer(const Fst<Arc> &ifst, float delta = kDelta, bool *debug_ptr = NULL):
      ifst_(ifst.Copy()), delta_(delta),
      equal_(delta),
      minimal_hash_(3, hasher_, equal_), initial_hash_(3, hasher_, equal_) {
    Initialize();
    Determinize(debug_ptr);
  }

  // frees all except output_arcs_, which contains the important info
  // we need to output the FST.
  void FreeMostMemory() {
    if(ifst_) {
      delete ifst_;
      ifst_ = NULL;
    }
    for (typename MinimalSubsetHash::iterator iter = minimal_hash_.begin();
        iter != minimal_hash_.end(); ++iter)
      delete iter->first;
    { MinimalSubsetHash tmp; tmp.swap(minimal_hash_); }
    for (typename InitialSubsetHash::iterator iter = initial_hash_.begin();
        iter != initial_hash_.end(); ++iter)
      delete iter->first;
    { InitialSubsetHash tmp; tmp.swap(initial_hash_); }
    repository_.Destroy();
    { vector<vector<Element*> > output_states_tmp;
      output_states_tmp.swap(output_states_); }
    { vector<char> tmp;  tmp.swap(osymbol_or_final_); }
    { vector<OutputStateId> tmp; tmp.swap(queue_); }
  }
  
  ~LatticeDeterminizer() {
    FreeMostMemory();
  }
 private:
  
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;
  typedef typename Arc::StateId StateId;  // use this when we don't know if it's input or output.
  typedef typename Arc::StateId InputStateId;  // state in the input FST.
  typedef typename Arc::StateId OutputStateId;  // same as above but distinguish
                                                // states in output Fst.


  typedef LatticeStringRepository<IntType> StringRepositoryType;
  typedef typename StringRepositoryType::Entry* StringId;

  // Element of a subset [of original states]
  struct Element {
    StateId state; // use StateId as this is usually InputStateId but in one case
                   // OutputStateId.
    StringId string;
    Weight weight;
  };

  // Arcs in the format we temporarily create in this class (a representation, essentially of
  // a Gallic Fst).
  struct TempArc {
    Label ilabel;
    StringId string;  // Look it up in the StringRepository, it's a sequence of Labels.
    OutputStateId nextstate;  // or kNoState for final weights.
    Weight weight;
  };

  // Comparator function (operator < that compares state id's).  Helps us
  // sort on state id so that we can combine elements with the same state.
  class ElementStateComparator {
   public:
    inline bool operator ()(const Element &e1, const Element &e2) const {  // operator <
      return e1.state < e2.state;
    }
  };

  // Hashing function used in hash of subsets.
  // A subset is a pointer to vector<Element>.
  // The Elements are in sorted order on state id, and without repeated states.
  // Because the order of Elements is fixed, we can use a hashing function that is
  // order-dependent.  However the weights are not included in the hashing function--
  // we hash subsets that differ only in weight to the same key.  This is not optimal
  // in terms of the O(N) performance but typically if we have a lot of determinized
  // states that differ only in weight then the input probably was pathological in some way,
  // or even non-determinizable.
  //   We don't quantize the weights, in order to avoid inexactness in simple cases.
  // Instead we apply the delta when comparing subsets for equality, and allow a small
  // difference.

  class SubsetKey {
   public:
    size_t operator ()(const vector<Element> * subset) const {  // hashes only the state and string.
      size_t hash = 0, factor = 1;
      for (typename vector<Element>::const_iterator iter= subset->begin(); iter != subset->end(); ++iter) {
        hash *= factor;
        hash += iter->state + 103333*iter->string;
        factor *= 23531;  // these numbers are primes.
      }
      return hash;
    }
  };

  // This is the equality operator on subsets.  It checks for exact match on state-id
  // and string, and approximate match on weights.
  class SubsetEqual {
   public:
    bool operator ()(const vector<Element> * s1, const vector<Element> * s2) const {
      size_t sz = s1->size();
      assert(sz>=0);
      if (sz != s2->size()) return false;
      typename vector<Element>::const_iterator iter1 = s1->begin(),
          iter1_end = s1->end(), iter2=s2->begin();
      for (; iter1 < iter1_end; ++iter1, ++iter2) {
        if (iter1->state != iter2->state ||
           iter1->string != iter2->string ||
           ! ApproxEqual(iter1->weight, iter2->weight, delta_)) return false;
      }
      return true;
    }
    float delta_;
    SubsetEqual(float delta): delta_(delta) {}
    SubsetEqual(): delta_(kDelta) {}
  };

  // Operator that says whether two Elements have the same states.
  // Used only for debug.
  class SubsetEqualStates {
   public:
    bool operator ()(const vector<Element> * s1, const vector<Element> * s2) const {
      size_t sz = s1->size();
      assert(sz>=0);
      if (sz != s2->size()) return false;
      typename vector<Element>::const_iterator iter1 = s1->begin(),
          iter1_end = s1->end(), iter2=s2->begin();
      for (; iter1 < iter1_end; ++iter1, ++iter2) {
        if (iter1->state != iter2->state) return false;
      }
      return true;
    }
  };

  // Define the hash type we use to map subsets (in minimal
  // representation) to OutputStateId.
  typedef unordered_map<const vector<Element>*, OutputStateId,
                        SubsetKey, SubsetEqual> MinimalSubsetHash;

  // Define the hash type we use to map subsets (in initial
  // representation) to OutputStateId, together with an
  // extra weight. [note: we interpret the Element.state in here
  // as an OutputStateId even though it's declared as InputStateId;
  // these types are the same anyway].
  typedef unordered_map<const vector<Element>*, Element,
                        SubsetKey, SubsetEqual> InitialSubsetHash;
  

  // This function computes epsilon closure of subset of states by following epsilon links.
  // Called by ProcessSubset.
  // Has no side effects except on the string repository.  The "output_subset" is not
  // necessarily normalized (in the sense of there being no common substring), unless
  // input_subset was.
  void EpsilonClosure(vector<Element> *subset);
  
  // converts the representation of the subset from canonical (all states) to
  // minimal (only states with output symbols on arcs leaving them, and final
  // states).  Output is not necessarily normalized, even if input_subset was.
  void ConvertToMinimal(vector<Element> *subset) const {
    assert(!subset->empty());
    vector<Element>::iterator cur_in = subset->begin(),
        cur_out = subset->begin(), end = subset->end();
    while(cur_in != end) {
      if(IsOsymbolOrFinal(cur_in->state)) {  // keep it...
        *cur_out = *cur_in;
        cur_out++;
      }
      cur_in++;
    }
    subset->resize(cur_out - subset->begin());
  }
  
  // Normalizes a subset, and gives remaining weight and string.
  void NormalizeSubset(vector<Element> *subset,
                       Weight *remaining_weight,
                       StringId *common_prefix);

  // Takes a minimal, normalized subset, and converts it to an OutputStateId.
  // Involves a hash lookup, and possibly adding a new OutputStateId.
  // If it creates a new OutputStateId, it adds it to the queue.
  OutputStateId MinimalToStateId(const vector<Element> &subset) {
    typename MinimalSubsetHash::const_iterator iter
        = minimal_hash_.find(&subset_in);
    if (iter != minimal_hash_.end()) // Found a matching subset.
      return iter->second;
    OutputStateId ans = static_cast<OutputStateId>(output_arcs_.size());
    vector<Element> *subset_ptr = new vector<Element>(subset);
    output_states_.push_back(subset_ptr);
    output_arcs_.push_back(vector<TempArc>());
    minimal_hash_[subset_ptr] = ans;
    queue_.push_back(ans);
    return ans;
  }

  
  // Given a normalized initial subset of elements (i.e. before epsilon closure),
  // compute the corresponding output-state.
  OutputState InitialToStateId(const vector<Element> &subset_in,
                               Weight *remaining_weight,
                               StringId *common_prefix) {
    typename InitialSubsetHash::const_iterator iter
        = initial_hash_.find(&subset_in);
    if (iter != initial_hash_.end()) { // Found a matching subset.
      const Element &elem = iter->second;
      *remaining_weight = elem.weight;
      *common_prefix = elem.string;
      return elem.state;
    }
    // else no matching subset-- have to work it out.
    vector<Element> subset(subset_in);
    // Follow through epsilons.  Will add no duplicate states.  note: after
    // EpsilonClosure, it is the same as "canonical" subset, except not
    // normalized (actually we never compute the normalized canonical subset,
    // only the normalized minimal one).
    EpsilonClosure(&subset); // follow epsilons.
    ConvertToMinimal(&subset); // remove all but emitting and final states.

    Entry entry; // will be used to store remaining weight and string, and
                 // OutputStateId, in initial_hash_;    
    NormalizeSubset(&subset, &entry.weight, &entry.string); // normalize subset; put
    // common string and weight in "entry".  The subset is now a minimal,
    // normalized subset.
    
    OutputStateId ans = MinimalToStateId(subset);
    // Before returning "ans", add the initial subset to the hash,
    // so that we can bypass the epsilon-closure etc., next time
    // we process the same initial subset.
    vector<Element> *initial_subset_ptr = new vector<Element>(subset_in);
    entry.state = ans;
    initial_hash_[initial_subset_ptr] = entry;
    return ans;
  }

  // returns the Compare value (-1 if a < b, 0 if a == b, 1 if a > b) according
  // to the ordering we defined on strings for the CompactLatticeWeightTpl.
  // see function
  // inline int Compare (const CompactLatticeWeightTpl<WeightType,IntType> &w1,
  //                     const CompactLatticeWeightTpl<WeightType,IntType> &w2)
  // in lattice-weight.h.
  // this is the same as that, but optimized for our data structures.
  inline int Compare(const Weight &a_w, StringId a_str,
                     const Weight &b_w, StringId b_str) const {
    int weight_comp = Compare(a_w, b_w);
    if(weight_comp != 0) return weight_comp;
    // now comparing strings.
    if(a_str == b_str) return 0;
    vector<IntType> a_vec, b_vec;
    repository_.ConvertToVector(a_str, &a_vec);
    repository_.ConvertToVector(b_str, &b_vec);
    // First compare their lengths.
    int a_len = a_vec.size(), b_len = b_vec.size();
    if(a_len < b_len) return -1;
    else if(a_len > b_len) return 1;
    for(int i = 0; i < a_len; i++) {
      if(a_vec[i] < b_vec[i]) return -1;
      else if(a_vec[i] > b_vec[i]) return 1;
    }
    assert(0); // because we checked if a_str == b_str above, shouldn't reach here
    return 0;
  }
  
  
  void EpsilonClosure(vector<Element> *subset) {
    // at input, subset must have only one example of each StateId.  [will still
    // be so at output].  This function follows epsilons and augments the subset
    // accordingly.

    std::set<Element, ElementStateComparator> cur_subset;
    typedef typename std::set<Element, ElementStateComparator>::iterator SetIter;
    {
      SetIter iter = cur_subset.end();
      for (size_t i = 0;i < subset.size();i++)
        iter = cur_subset.insert(iter, subset[i]);
      // By providing iterator where we inserted last one, we make insertion more efficient since
      // input subset was already in sorted order.
    }
    // find whether input fst is known to be sorted in input label.  TODO: make sure
    // this is documented.
    bool sorted = ((ifst_->Properties(kILabelSorted, false) & kILabelSorted) != 0);

    vector<Element> queue(subset);  // queue of things to be processed.
    while (queue.size() != 0) {
      Element elem = queue.back();
      queue.pop_back();

      for (ArcIterator<Fst<Arc> > aiter(*ifst_, elem.state); !aiter.Done(); aiter.Next()) {
        const Arc &arc = aiter.Value();
        if (sorted && arc.ilabel != 0) break;  // Break from the loop: due to sorting there will be no
        // more transitions with epsilons as input labels.
        if (arc.ilabel == 0
            && arc.weight != Weight::Zero()) {  // Epsilon transition.
          Element next_elem;
          next_elem.state = arc.nextstate;
          next_elem.weight = Times(elem.weight, arc.weight);
          // now must append strings
          if (arc.olabel == 0)
            next_elem.string = elem.string;
          else
            next_elem.string = repository_.Successor(elem.string, arc.olabel);
          
          pair<SetIter, bool> pr = cur_subset.insert(next_elem);
          if (pr.second) {  // was no such StateId: add to queue.
            queue.push_back(next_elem);
          } else {
            // was not inserted because one already there.  In normal determinization we'd
            // add the weights.  Here, we find which one has the better weight, and
            // keep its corresponding string.
            int comp = Compare(next_elem.weight, next_elem.string,
                               pr.first->weight, pr.first->string);
            if(comp == 1) { // next_elem is better, so use its (weight, string)
              pr.first->string = next_elem.string;
              pr.first->weight = next_elem.weight;
              queue.push_back(next_elem);
            }
            // else it is the same or worse, so use original one.
          }
        }
      }
    }

    {  // copy cur_subset to subset.
      // sorted order is automatic.
      subset->clear();
      subset->reserve(cur_subset.size());
      SetIter iter = cur_subset.begin(), end = cur_subset.end();
      for (; iter != end; ++iter) subset->push_back(*iter);
    }
  }


  // This function works out the final-weight of the determinized state.
  // called by ProcessSubset.
  // Has no side effects except on the variable repository_, and output_arcs_.

  void ProcessFinal(OutputStateId output_state) {
    const vector<Element> &minimal_subset = *(output_states_[output_state]);
    // processes final-weights for this subset.

    // minimal_subset may be empty if the graphs is not connected/trimmed..
    bool is_final = false;
    StringId final_string = 0;  // = 0 to keep compiler happy.
    Weight final_weight;
    typename vector<Element>::const_iterator iter = minimal_subset.begin(), end = minimal_subset.end();
    for (; iter != end; ++iter) {
      const Element &elem = *iter;
      Weight this_final_weight = ifst_->Final(elem.state);
      StringId this_final_string = elem.string;
      if(this_final_weight != Weight::Zero()
         && Compare(this_final_weight, this_final_string,
                    final_weight, final_string) == 1) { // the new
        // (weight, string) pair is more in semiring than our current
        // one.
        is_final = true;
        final_weight = this_final_weight;
        final_string = this_final_string;
      }
    }
    if (is_final) {
      // store final weights in TempArc structure, just like a transition.
      TempArc temp_arc;
      temp_arc.ilabel = 0;
      temp_arc.nextstate = kNoStateId;  // special marker meaning "final weight".
      temp_arc.string = final_string;
      temp_arc.weight = final_weight;
      output_arcs_[state].push_back(temp_arc);
    }
  }

  // NormalizeSubset normalizes the subset "elems" by
  // removing any common string prefix (putting it in common_str),
  // and dividing by the total weight (putting it in tot_weight).
  void NormalizeSubset(vector<Element> *elems,
                       StringId *common_str,
                       Weight *tot_weight) {
    assert(!elems->empty());
    size_t size = elems->size();
    vector<Element> common_prefix;
    repository_.ConvertToVector((*elems)[0].string, &common_prefix);
    Weight weight = (*elems)[0].weight;
    for(size_t i = 1; i < size; i++) {
      weight = Plus(weight, (*elems)[i].weight);
      repository_.ReduceToCommonPrefix((*elems)[i].string, &common_prefix);
    }
    assert(weight != Weight::Zero());
    size_t prefix_len = common_prefix.size();
    for(size_t i = 0; i < size; i++) {
      (*elems)[i].weight = Divide((*elems)[i].weight, weight, DIVIDE_LEFT);
      (*elems)[i].string = RemovePrefix((*elems)[i].string, prefix_len);
    }
    *common_str = repository_.ConvertFromVector(common_prefix);
    *tot_weight = weight;
  }

  // Take a subset of Elements that is sorted on state, and
  // merge any Elements that have the same state (taking the best
  // (weight, string) pair in the semiring).
  void MakeSubsetUnique(vector<Element> *subset) {
    typedef typename vector<Element>::iterator IterType;
    
    // This assert is designed to fail (usually) if the subset is not sorted on
    // state.
    assert(subset->size() < 2 || (*subset)[0].state <= (*subset)[1].state);
    
    IterType cur_in = subset->begin(), cur_out = cur_in, end = subset->end();
    size_t num_out = 0;
    // Merge elements with same state-id
    while (cur_in != end) {  // while we have more elements to process.
      // At this point, cur_out points to location of next place we want to put an element,
      // cur_in points to location of next element we want to process.
      if (cur_in != cur_out) *cur_out = *cur_in;
      cur_in++;
      while (cur_in != end && cur_in->state == cur_out->state) {
        if(Compare(cur_in->weight, cur_in->string,
                   cur_out->weight, cur_out->string) == 1) {
          // if *cur_in > *cur_out in semiring, then take *cur_in.
          cur_out->string = cur_in->string;
          cur_out->weight = cur_in->weight;
        }
        cur_in++;
      }
      cur_out++;
      num_out++;
    }
    subset->resize(num_out);
  }
  
  // ProcessTransition is called from "ProcessTransitions".  Broken out for
  // clarity.  Processes a transition from state "state".  The set of Elements
  // represents a set of next-states with associated weights and strings, each
  // one arising from an arc from some state in a determinized-state; the
  // next-states are not necessarily unique (i.e. there may be >1 entry
  // associated with each), and the Elements have to be merged within this
  // routine.
  void ProcessTransition(OutputStateId state, Label ilabel, vector<Element> *subset) {
    // At input, "subset" may contain duplicates for a given dest state (but in sorted
    // order).  This function removes duplicates from "subset", normalizes it, and adds
    // a transition to the dest. state (possibly affecting Q_ and hash_, if state did not
    // exist).

    MakeSubsetUnique(subset); // remove duplicates with the same state.
    
    StringId common_str;
    Weight tot_weight;
    NormalizeSubset(*subset, &common_str, &tot_weight);

    OutputStateId nextstate;
    {
      StringId next_common_str;
      Weight next_tot_weight;
      nextstate = InitialToStateId(*subset, &next_common_str,
                                   &next_tot_weight);
      common_str = repository_.Concatenate(common_str, next_common_str);
      tot_weight = Times(tot_weight, next_tot_weight);
    }
    
    // Now add an arc to the next state (would have been created if necessary by
    // InitialToStateId).
    TempArc temp_arc;
    temp_arc.ilabel = ilabel;
    temp_arc.nextstate = nextstate;
    temp_arc.string = common_str;
    temp_arc.weight = tot_weight;
    output_arcs_[state].push_back(temp_arc);  // record the arc.
  }


  // "less than" operator for pair<Label, Element>.   Used in ProcessTransitions.
  // Lexicographical order, with comparing the state only for "Element".

  class PairComparator {
   public:
    inline bool operator () (const pair<Label, Element> &p1, const pair<Label, Element> &p2) {
      if (p1.first < p2.first) return true;
      else if (p1.first > p2.first) return false;
      else {
        return p1.second.state < p2.second.state;
      }
    }
  };


  // ProcessTransitions processes emitting transitions (transitions
  // with ilabels) out of this subset of states.
  // Does not consider final states.  Breaks the emitting transitions up by ilabel,
  // and creates a new transition in the determinized FST for each unique ilabel.
  // Does this by creating a big vector of pairs <Label, Element> and then sorting them
  // using a lexicographical ordering, and calling ProcessTransition for each range
  // with the same ilabel.
  // Side effects on repository, and (via ProcessTransition) on Q_, hash_,
  // and output_arcs_.

  
  void ProcessTransitions(OutputStateId output_state) {
    const vector<Element> &minimal_subset = *(output_states_[output_state]);
    // it's possible that minimal_subset could be empty if there are
    // unreachable parts of the graph, so don't check that it's nonempty.
    vector<pair<Label, Element> > all_elems;
    {
      // Push back into "all_elems", elements corresponding to all
      // non-epsilon-input transitions out of all states in "minimal_subset".
      typename vector<Element>::const_iterator iter = minimal_subset.begin(), end = minimal_subset.end();
      for (;iter != end; ++iter) {
        const Element &elem = *iter;
        for (ArcIterator<Fst<Arc> > aiter(*ifst_, elem.state); ! aiter.Done(); aiter.Next()) {
          const Arc &arc = aiter.Value();
          if (arc.ilabel != 0
              && arc.weight != Weight::Zero()) {  // Non-epsilon transition -- ignore epsilons here.
            pair<Label, Element> this_pr;
            this_pr.first = arc.ilabel;
            Element &next_elem(this_pr.second);
            next_elem.state = arc.nextstate;
            next_elem.weight = Times(elem.weight, arc.weight);
            if (arc.olabel == 0) // output epsilon-- this is simple case so
                                 // handle separately for efficiency
              next_elem.string = elem.string;
            else 
              next_elem.string = repository_.Successor(elem.string, arc.olabel);

            all_elems.push_back(this_pr);
          }
        }
      }
    }
    PairComparator pc;
    std::sort(all_elems.begin(), all_elems.end(), pc);
    // now sorted first on input label, then on state.
    typedef typename vector<pair<Label, Element> >::const_iterator PairIter;
    PairIter cur = all_elems.begin(), end = all_elems.end();
    vector<Element> this_subset;
    while (cur != end) {
      // Process ranges that share the same input symbol.
      Label ilabel = cur->first;
      this_subset.clear();
      while (cur != end && cur->first == ilabel) {
        this_subset.push_back(cur->second);
        cur++;
      }
      // We now have a subset for this ilabel.
      ProcessTransition(state, ilabel, &this_subset);
    }
  }



  // ProcessState does the processing of a determinized state, i.e. it creates
  // transitions out of it and the final-probability if any.
  void ProcessState(OutputStateId output_state) {
    ProcessFinal(output_state);
    ProcessTransitions(output_state);
  }
    

  void Debug() {  // this function called if you send a signal
    // SIGUSR1 to the process (and it's caught by the handler in
    // fstdeterminizestar).  It prints out some traceback
    // info and exits.

    KALDI_WARN << "Debug function called (probably SIGUSR1 caught).\n";
    // free up memory from the hash as we need a little memory
    { SubsetHash hash_tmp; std::swap(hash_tmp, hash_); }

    if (output_arcs_.size() <= 2)
      KALDI_ERR << "Nothing to trace back";
    size_t max_state = output_arcs_.size() - 2;  // don't take the last
    // one as we might be halfway into constructing it.

    vector<OutputStateId> predecessor(max_state+1, kNoStateId);
    for (size_t i = 0; i < max_state; i++) {
      for (size_t j = 0; j < output_arcs_[i].size(); j++) {
        OutputStateId nextstate = output_arcs_[i][j].nextstate;
        // always find an earlier-numbered prececessor; this
        // is always possible because of the way the algorithm
        // works.
        if (nextstate <= max_state && nextstate > i)
          predecessor[nextstate] = i;
      }
    }
    vector<pair<Label, StringId> > traceback;
    // traceback is a pair of (ilabel, olabel-seq).
    OutputStateId cur_state = max_state;  // a recently constructed state.

    while (cur_state != 0 && cur_state != kNoStateId) {
      OutputStateId last_state = predecessor[cur_state];
      pair<Label, StringId> p;
      size_t i;
      for (i = 0; i < output_arcs_[last_state].size(); i++) {
        if (output_arcs_[last_state][i].nextstate == cur_state) {
          p.first = output_arcs_[last_state][i].ilabel;
          p.second = output_arcs_[last_state][i].string;
          traceback.push_back(p);
          break;
        }
      }
      assert(i != output_arcs_[last_state].size());  // or fell off loop.
      cur_state = last_state;
    }
    if (cur_state == kNoStateId)
      KALDI_WARN << "Traceback did not reach start state (possibly debug-code error)";

    KALDI_WARN << "Traceback below (or on standard error) in format ilabel (olabel olabel) ilabel (olabel) ...\n";
    for (ssize_t i = traceback.size() - 1; i >= 0; i--) {
      std::cerr << traceback[i].first << ' ' << "( ";
      vector<Label> seq;
      repository_.SeqOfId(traceback[i].second, &seq);
      for (size_t j = 0; j < seq.size(); j++)
        std::cerr << seq[j] << ' ';
      std::cerr << ") ";
    }
    std::cerr << '\n';
    exit(1);
  }

    bool IsOsymbolOrFinal(InputStateId state) { // returns true if this state
      // of the input FST either is final or has an osymbol on an arc out of it.
      // It 
    assert(state >= 0);
    if(osymbol_or_final_.size() <= state)
      osymbol_or_final_.resize(state+1, static_cast<char>(OSF_UNKNOWN));
    if(osymbol_or_final_[state] == static_cast<char>(OSF_NO))
      return false;
    else if(osymbol_or_final_[state] == static_cast<char>(OSF_YES))
      return true;
    // else work it out...
    if(ifst_->Final(state) != Weight::Zero())
      osymbol_or_final_[state] = static_cast<char>(OSF_YES);
    for (ArcIterator<Fst<Arc> > aiter(*ifst_, state);
         !aiter.Done();
         aiter.Next()) {
      const Arc &arc = aiter.Value();
      if(arc.osymbol != 0) {
        osymbol_or_final_[state] = static_cast<char>(OSF_YES);
        return true;
      }
    }
    osymbol_or_final_[state] = static_cast<char>(OSF_NO);
    return false;
  }
  
  void Initialize() {    
    if(ifst_->Properties(kExpanded, false) != 0) { // if we know #states in
      // ifst, it might be a bit more efficient
      // to pre-size the hashes so we're not constantly rebuilding them.
     StateId num_states =
         down_cast<const ExpandedFst<Arc>*, const Fst<Arc> >(ifst_)->NumStates();
     minimal_hash_.rehash(num_states/2 + 3);
     initial_hash_.rehash(num_states/2 + 3);
    }
    InputStateId start_id = ifst_->Start();
    if (start_id != kNoStateId) {
      /* Insert determinized-state corresponding to the start state into hash and
         queue.  Unlike all the other states, we don't "normalize" the representation
         of this determinized-state before we put it into minimal_hash_.  This is actually
         what we want, as otherwise we'd have problems dealing with any extra weight
         and string and might have to create a "super-initial" state which would make
         the output nondeterministic.  Normalization is only needed to make the
         determinized output more minimal anyway, it's not needed for correctness.
         Note, we don't put anything in the initial_hash_.  The initial_hash_ is only
         a lookaside buffer anyway, so this isn't a problem-- it will get populated
         later if it needs to be.
      */ 
      Element elem;
      elem.state = start_id;
      elem.weight = Weight::One();
      elem.string = repository_.EmptyString();  // Id of empty sequence.
      vector<Element> subset;
      subset.push_back(elem);
      EpsilonClosure(&subset); // follow through epsilon-inputs links
      ConvertToMinimal(&subset); // remove all but final states and
      // states with input-labels on arcs out of them.
      vector<Element> *subset_ptr = new vector<Element>(subset);
      assert(output_arcs_.empty() && output_states_.empty());
      // add the new state...
      output_states_.push_back(subset_ptr);
      output_arcs_.push_back(vector<TempArc>());
      OutputStateId initial_state = 0;
      minimal_hash_[subset_ptr] = initial_state;
      queue_.push_back(initial_state);
    }     
  }
  
  void Determinize(bool *debug_ptr) {
    // This determinizes the input fst but leaves it in the "special format"
    // in "output_arcs_".  Must be called after Initialize().  To get the
    // output, call one of the Output routines.
    while (!queue_.empty()) {
      OutputStateId out_state = queue_.back();
      queue_.pop_back();
      ProcessState(out_state);
      if (debug_ptr && *debug_ptr) Debug();  // will exit.
    }
  }

  DISALLOW_COPY_AND_ASSIGN(LatticeDeterminizer);


  vector<vector<Element*>* > output_states_; // maps from output state to
                                            // minimal representation [normalized].
                                            // View pointers as owned in
                                            // minimal_hash_.
  vector<vector<TempArc> > output_arcs_;  // essentially an FST in our format.

  const Fst<Arc> *ifst_;
  float delta_;
  SubsetKey hasher_;  // object that computes keys-- has no data members.
  SubsetEqual equal_;  // object that compares subsets-- only data member is delta_.
  MinimalSubsetHash minimal_hash_;  // hash from Subset to OutputStateId.  Subset is "minimal
                                    // representation" (only include final and states and states with
                                    // nonzero ilabel on arc out of them.  Owns the pointers
                                    // in its keys.
  InitialSubsetHash initial_hash_;   // hash from Subset to Element, which
                                     // represents the OutputStateId together
                                     // with an extra weight and string.  Subset
                                     // is "initial representation".  The extra
                                     // weight and string is needed because after
                                     // we convert to minimal representation and
                                     // normalize, there may be an extra weight
                                     // and string.  Owns the pointers
                                    // in its keys.
  vector<OutputStateId> queue_; // Queue of output-states to process.  Starts with
  // state 0, and increases and then (hopefully) decreases in length during
  // determinization.  LIFO queue (queue discipline doesn't really matter).

  enum OsymbolOrFinal { OSF_UNKNOWN = 0, OSF_NO = 1, OSF_YES = 2 };
  
  vector<char> osymbol_or_final_; // A kind of cache; it says whether
  // each state is (emitting or final) where emitting means it has at least one
  // non-epsilon output arc.  Only accessed by IsOsymbolOrFinal()
  
  StringRepository<IntType> repository_;  // defines a compact and fast way of
  // storing sequences of labels.
};


// normally Weight would be LatticeWeight<float> (which has two floats),
// or possibly TropicalWeightTpl<float>, and IntType would be int32.
template<class Weight, class IntType>
void DeterminizeLattice(const Fst<ArcTpl<Weight> > &ifst,
                        MutableFst<ArcTpl<Weight> > *ofst,
                        float delta = kDelta,
                        bool *debug_ptr) {
  assert(static_cast<const void*>(ofst) != static_cast<const void*>(&ifst));
  ofst->SetInputSymbols(ifst.InputSymbols());
  ofst->SetOutputSymbols(ifst.OutputSymbols());
  LatticeDeterminizer<Weight, IntType> det(ifst, delta, debug_ptr);
  det.Output(ofst);
}


// normally Weight would be LatticeWeight<float> (which has two floats),
// or possibly TropicalWeightTpl<float>, and IntType would be int32.
template<class Weight, class IntType>
void DeterminizeLattice(const Fst<ArcTpl<Weight> >&ifst,
                        MutableFst<ArcTpl<CompactLatticeWeightTpl<Weight, IntType> > >*ofst,
                        float delta = kDelta,
                        bool *debug_ptr) {
  assert(static_cast<const void*>(ofst) != static_cast<const void*>(&ifst));
  ofst->SetInputSymbols(ifst.InputSymbols());
  ofst->SetOutputSymbols(ifst.OutputSymbols());
  LatticeDeterminizer<Weight, IntType> det(ifst, delta, debug_ptr);
  det.Output(ofst);
}



}


#endif
