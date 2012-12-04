// idlaktxp/mod-pronounce.cc

// Copyright 2012 CereProc Ltd.  (Author: Matthew Aylett)

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
//

#include "./txpmodule.h"

namespace kaldi {

TxpPronounce::TxpPronounce(const std::string &tpdb, const std::string &configf)
    : TxpModule("pronounce", tpdb, configf), nrules_("lexicon", "default"),
      lex_("lexicon", "default"), lts_("ccart", "default") {
  nrules_.Parse(tpdb.c_str());
  lex_.Parse(tpdb.c_str());
  lts_.Parse(tpdb.c_str());
}

TxpPronounce::~TxpPronounce() {
}

bool TxpPronounce::Process(pugi::xml_document * input) {
  pugi::xml_node parent, child;
  pugi::xpath_node_set tks = input->document_element().select_nodes("//tk");
  const char * lex_entry;
  const char * lex_pron;
  const char * word, *p;
  const std::string * symbol;
  std::string utfchar, word_str;
  TxpLexiconLkp lexlkp;
  TxpUtf8 utf8;
  int32 clen;
  tks.sort();
  for (pugi::xpath_node_set::const_iterator it = tks.begin();
       it != tks.end();
       ++it) {
    pugi::xml_node node = (*it).node();
    word = node.attribute("norm").value();
    lexlkp.Reset();
    // Check to see if token is first daughter of a lex tag
    parent = node.parent();
    lex_entry = NULL;
    lex_pron = NULL;
    while (parent) {
      if (!strcmp(parent.name(), "lex")) {
        if ((*(parent.select_nodes("descendant::tk[1]").begin())).node()
            == node) {
          lex_entry = parent.attribute("entry").value();
          lex_pron = parent.attribute("pron").value();
        }
      }
      parent = parent.parent();
    }
    word_str = std::string(word);
    // If pron is set use that pron
    if (lex_pron && *lex_pron) {
      node.append_attribute("pron").set_value(lex_pron);
    } else if (!nrules_.IsAlpha(word_str)) {
      // If normalised content has non lexical chars then read out using lookup
      p = word;
      while (*p) {
        clen = utf8.Clen(p);
        utfchar = std::string(p, clen);
        if (!nrules_.IsAlpha(utfchar)) {
          symbol = nrules_.Lkp(std::string("symbols"), utfchar);
          if (symbol) {
            AppendPron(NULL, *symbol, lexlkp);
          } else {
            symbol = nrules_.Lkp(std::string("asdigits"), utfchar);
            if (symbol) AppendPron(NULL, *symbol, lexlkp);
          }
        } else {
          AppendPron(NULL, utfchar, lexlkp);
        }
        p += clen;
      }
      node.append_attribute("pron").set_value(lexlkp.pron.c_str());
    } else {
      // standard lookup of word
      AppendPron(lex_entry, std::string(word), lexlkp);
      node.append_attribute("pron").set_value(lexlkp.pron.c_str());
    }
  }
  return true;
}

void TxpPronounce::AppendPron(const char * entry,
                              const std::string &word,
                              TxpLexiconLkp &lexlkp) {
  bool found = false;
  if (!lexlkp.pron.empty()) lexlkp.pron += " ";
  if (entry) {
    found = lex_.GetPron(word, std::string(entry), lexlkp);
    if (!found) {
      found = lex_.GetPron(word, std::string(""), lexlkp);
    }
  } else {
    found = lex_.GetPron(word, std::string(""), lexlkp);
  }
  if (!found)
    found = lts_.GetPron(word, lexlkp);
}

}  // namespace kaldi
