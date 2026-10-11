//
// Created by Fredrik Robertsen on 7/13/24.
//

#include "RangeCollection.h"


thread_local std::vector<std::string> RangeCollection::pathStack{};
std::unordered_map<std::string, RecordedRange> RangeCollection::recorded{};
std::mutex RangeCollection::writeMtx{};
