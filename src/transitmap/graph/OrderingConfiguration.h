// Copyright 2016, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Authors: Patrick Brosi <brosi@informatik.uni-freiburg.de>

#ifndef TRANSITMAP_GRAPH_ORDERINGCONFIGURATION_H_
#define TRANSITMAP_GRAPH_ORDERINGCONFIGURATION_H_

namespace transitmapper {
namespace graph {

class EdgeTripGeom;

typedef std::vector<size_t> Ordering;
typedef std::map<const EdgeTripGeom*, Ordering> Configuration;

}}

#endif  // TRANSITMAP_GRAPH_ORDERINGCONFIGURATION_H_
