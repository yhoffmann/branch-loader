#include <TObjArray.h>
#include <TString.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <array>
#include <cstddef>
#include <iostream>
#include <utility>

namespace BranchLoader {
std::vector<bool> getBranchStatuses(TTree* const t) {
  TObjArray* branches = t->GetListOfBranches();
  std::size_t numBranches = branches->GetEntries();
  std::vector<bool> branchStatuses(numBranches);
  for (std::size_t i = 0; i < numBranches; i++) {
    const TString branchName = ((TBranch*)branches->At(i))->GetName();
    branchStatuses[i] = t->GetBranchStatus(branchName);
  }

  return branchStatuses;
}

void setBranchStatuses(TTree* t, const std::vector<bool>& statuses) {
  TObjArray* branches = ((TTree*)t)->GetListOfBranches();
  const std::size_t numBranches = branches->GetEntries();
  if (numBranches != statuses.size()) {
    std::cerr << "ERR: Mismatch in number of branches specified in the status"
                 " array and the number of branches in the tree.\nNo change "
                 "was made!";
    return;
  }
  for (std::size_t i = 0; i < numBranches; i++) {
    const TString branchName = ((TBranch*)branches->At(i))->GetName();
    t->SetBranchStatus(branchName, statuses[i]);
  }
}

template <typename T>
std::vector<T> getFromBranch(TTree* t, const TString& branchName) {
  const std::size_t numEntries = t->GetEntries();

  const std::vector<bool> initialBranchStatuses = getBranchStatuses(t);
  t->SetBranchStatus("*", kFALSE);
  t->SetBranchStatus(branchName, kTRUE);

  const std::vector<T> entries = [t, &branchName, numEntries]() {
    std::vector<T> temp(numEntries);
    TTreeReader reader(t);
    TTreeReaderValue<T> currentVal(reader, branchName);
    while (reader.Next()) {
      temp.push_back(*currentVal);
    }

    return temp;
  }();

  setBranchStatuses(t, initialBranchStatuses);

  return entries;
}

template <typename... Ts, std::size_t... Is>
void setBranchAddress(TTree* t,
                      const std::array<TString, sizeof...(Ts)>& branchNames,
                      std::tuple<Ts...>& entry, std::index_sequence<Is...>) {
  (t->SetBranchAddress(branchNames[Is], &std::get<Is>(entry)), ...);
}

template <typename... Ts, std::size_t... Is>
std::tuple<TTreeReaderValue<Ts>...> readerHandles(
    TTreeReader* reader, const std::array<TString, sizeof...(Ts)>& branchNames,
    std::index_sequence<Is...>) {
  return {TTreeReaderValue<Ts>(*reader, branchNames[Is])...};
}

template <typename... Ts>
std::vector<std::tuple<Ts...>> getFromBranchMulti(
    TTree* t, const std::array<TString, sizeof...(Ts)>& branchNames) {
  const std::size_t numEntries = t->GetEntries();
  const std::vector<bool> initialBranchStatuses = getBranchStatuses(t);
  t->SetBranchStatus("*", kFALSE);
  for (const auto& branchName : branchNames) {
    t->SetBranchStatus(branchName, kTRUE);
  }

  std::vector<std::tuple<Ts...>> ret;
  ret.reserve(numEntries);

  TTreeReader reader(t);
  std::tuple<TTreeReaderValue<Ts>...> readers = readerHandles<Ts...>(
      &reader, branchNames, std::index_sequence_for<Ts...>{});

  while (reader.Next()) {
    std::tuple<Ts...> values = std::apply(
        [](TTreeReaderValue<Ts>&... readers) {
          return std::make_tuple(*readers...);
        },
        readers);
    ret.push_back(values);
  }

  setBranchStatuses(t, initialBranchStatuses);

  return ret;
}
}  // namespace BranchLoader
