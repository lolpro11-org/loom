// Copyright 2016, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Authors: Patrick Brosi <brosi@informatik.uni-freiburg.de>

#include "transitmap/optim/OptGraph.h"
#include "util/String.h"
#include "util/graph/Algorithm.h"
#include "util/log/Log.h"

using namespace transitmapper;
using namespace optim;
using transitmapper::graph::RouteOccurance;

// _____________________________________________________________________________
void OptGraph::upFirstLastEdg(OptEdge* optEdg) {
  size_t i = 0;
  for (const auto e : optEdg->pl().etgs) {
    if (e.etg->getFrom() == optEdg->getFrom()->pl().node ||
        e.etg->getTo() == optEdg->getFrom()->pl().node) {
      break;
    }
    i++;
  }

  size_t j = 0;
  for (const auto e : optEdg->pl().etgs) {
    if (e.etg->getFrom() == optEdg->getTo()->pl().node ||
        e.etg->getTo() == optEdg->getTo()->pl().node) {
      break;
    }
    j++;
  }
  optEdg->pl().firstEtg = i;
  optEdg->pl().lastEtg = j;
}

// _____________________________________________________________________________
EtgPart OptGraph::getFirstEdg(const OptEdge* optEdg) {
  size_t i = 0;
  for (const auto e : optEdg->pl().etgs) {
    if (e.etg->getFrom() == optEdg->getFrom()->pl().node ||
        e.etg->getTo() == optEdg->getFrom()->pl().node) {
      break;
    }
    i++;
  }

  assert(i == optEdg->pl().firstEtg);
  return optEdg->pl().etgs[optEdg->pl().firstEtg];
}

// _____________________________________________________________________________
EtgPart OptGraph::getLastEdg(const OptEdge* optEdg) {
  size_t i = 0;
  for (const auto e : optEdg->pl().etgs) {
    if (e.etg->getFrom() == optEdg->getTo()->pl().node ||
        e.etg->getTo() == optEdg->getTo()->pl().node) {
      break;
    }
    i++;
  }
  assert(i == optEdg->pl().lastEtg);
  return optEdg->pl().etgs[optEdg->pl().lastEtg];
}

// _____________________________________________________________________________
const std::vector<OptRO>& OptEdgePL::getRoutes() const { return routes; }

// _____________________________________________________________________________
size_t OptEdgePL::getCardinality() const {
  size_t ret = 0;

  for (const auto& ro : getRoutes()) {
    if (ro.route->relativeTo() == 0) ret++;
  }

  return ret;
}

// _____________________________________________________________________________
std::string OptEdgePL::getStrRepr() const {
  const void* address = static_cast<const void*>(this);
  std::stringstream ss;
  ss << address;

  return ss.str();
}

// _____________________________________________________________________________
EtgPart OptGraph::getAdjEtgp(const OptEdge* e, const OptNode* n) {
  if (e->getFrom() == n) {
    return getFirstEdg(e);
  } else if (e->getTo() == n) {
    return getLastEdg(e);
  }

  assert(false);
}

// _____________________________________________________________________________
Edge* OptGraph::getAdjEdg(const OptEdge* e, const OptNode* n) {
  if (e->getFrom() == n) {
    return getFirstEdg(e).etg;
  } else if (e->getTo() == n) {
    return getLastEdg(e).etg;
  }

  return 0;
}

// _____________________________________________________________________________
OptGraph::OptGraph(TransitGraph* toOptim, const Scorer* scorer)
    : _g(toOptim), _scorer(scorer) {
  build();
}

// _____________________________________________________________________________
void OptGraph::build() {
  for (graph::Node* n : *_g->getNodes()) {
    for (graph::Edge* e : n->getAdjListOut()) {
      graph::Node* fromTn = e->getFrom();
      graph::Node* toTn = e->getTo();

      OptNode* from = getNodeForTransitNode(fromTn);
      OptNode* to = getNodeForTransitNode(toTn);

      if (!from) from = addNd(fromTn);
      if (!to) to = addNd(toTn);

      OptEdge* edge = addEdg(from, to);

      edge->pl().etgs.push_back(EtgPart(e, e->getTo() == toTn));
      for (auto roOld : *e->getRoutes()) {
        edge->pl().routes.push_back(OptRO(roOld.route, roOld.direction));
      }
    }
  }

  writeEdgeOrder();
}

// _____________________________________________________________________________
OptNode* OptGraph::getNodeForTransitNode(const Node* tn) const {
  for (auto n : getNds()) {
    if (n->pl().node == tn) return n;
  }

  return 0;
}

// _____________________________________________________________________________
void OptGraph::split() {
  std::vector<OptEdge*> toCut;

  // collect edges to cut
  for (OptNode* n : *getNds()) {
    for (OptEdge* e : n->getAdjList()) {
      if (e->getFrom() != n) continue;

      // we only cut if both nodes have a deg > 1 - otherwise, we would cut
      // forever...
      if (e->getFrom()->getDeg() < 2 || e->getTo()->getDeg() < 2) continue;

      if (e->pl().getCardinality() == 1) toCut.push_back(e);
    }
  }

  for (OptEdge* e : toCut) {
    // cut this edge

    // add two new opt nodes, with slight offsets at the center of this
    // edge (this is for debug visualization purposes only)

    util::geo::PolyLine<double> edgeGeom(*e->getFrom()->pl().getGeom(),
                                         *e->getTo()->pl().getGeom());

    OptNode* leftN = addNd(edgeGeom.getPointAt(0.45).p);
    OptNode* rightN = addNd(edgeGeom.getPointAt(0.55).p);
    OptNode* eFrom = e->getFrom();
    OptNode* eTo = e->getTo();

    addEdg(eFrom, leftN, e->pl());
    auto newE = addEdg(rightN, eTo, e->pl());
    for (auto& etg : newE->pl().etgs) etg.wasCut = true;

    delEdg(eFrom, eTo);

    updateEdgeOrder(leftN);
    updateEdgeOrder(rightN);

    updateEdgeOrder(eFrom);
    updateEdgeOrder(eTo);
  }
}

// _____________________________________________________________________________
void OptGraph::simplify() {
  while (simplifyStep()) {
  }
}

// _____________________________________________________________________________
void OptGraph::untangle() {
  while (untangleStumpStep()) {
  }

  while (untangleFullCross()) {
  }

  while (untangleYStep()) {
  }

  while (untanglePartialYStep()) {
  }

  while (untangleDogBoneStep()) {
  }

  while (untanglePartialDogBoneStep()) {
  }
}

// _____________________________________________________________________________
bool OptGraph::simplifyStep() {
  for (OptNode* n : *getNds()) {
    if (n->getDeg() == 2) {
      OptEdge* first = n->getAdjList().front();
      OptEdge* second = n->getAdjList().back();

      assert(n->pl().node);

      if (first->pl().getCardinality() > 1 && first->pl().getCardinality() > 1 && n->pl().node->getAdjList().size() != 2) {
        bool cheaper = false;
        if (first->getOtherNd(n)->pl().node) {
          if (_scorer->getSplittingPenalty(n->pl().node) >= _scorer->getSplittingPenalty(first->getOtherNd(n)->pl().node)  && _scorer->getCrossingPenaltySameSeg(n->pl().node) >= _scorer->getCrossingPenaltySameSeg(first->getOtherNd(n)->pl().node)  && _scorer->getCrossingPenaltyDiffSeg(n->pl().node) >= _scorer->getCrossingPenaltyDiffSeg(first->getOtherNd(n)->pl().node)) {
            cheaper = true;
          }
        }
        if (!cheaper) {
          if (second->getOtherNd(n)->pl().node) {
            if (_scorer->getSplittingPenalty(n->pl().node) >= _scorer->getSplittingPenalty(second->getOtherNd(n)->pl().node)  && _scorer->getCrossingPenaltySameSeg(n->pl().node) >= _scorer->getCrossingPenaltySameSeg(second->getOtherNd(n)->pl().node)  && _scorer->getCrossingPenaltyDiffSeg(n->pl().node) >= _scorer->getCrossingPenaltyDiffSeg(second->getOtherNd(n)->pl().node)) {
              cheaper = true;
            }
          }
        }

        if (!cheaper) continue;
      }


      if (dirRouteEqualIn(first, second)) {
        OptNode* newFrom = 0;
        OptNode* newTo = 0;

        bool firstReverted;
        bool secondReverted;

        // add new edge
        if (first->getTo() != n) {
          newFrom = first->getTo();
          firstReverted = true;
        } else {
          newFrom = first->getFrom();
          firstReverted = false;
        }

        if (second->getTo() != n) {
          newTo = second->getTo();
          secondReverted = false;
        } else {
          newTo = second->getFrom();
          secondReverted = true;
        }

        // Important: dont create a multigraph, dont add self-edges
        if (newFrom == newTo || getEdg(newFrom, newTo)) continue;

        OptEdge* newEdge = addEdg(newFrom, newTo);

        // add etgs...
        for (EtgPart& etgp : first->pl().etgs) {
          newEdge->pl().etgs.push_back(EtgPart(
              etgp.etg, (etgp.dir ^ firstReverted), etgp.order, etgp.wasCut));
        }

        for (EtgPart& etgp : second->pl().etgs) {
          newEdge->pl().etgs.push_back(EtgPart(
              etgp.etg, (etgp.dir ^ secondReverted), etgp.order, etgp.wasCut));
        }

        upFirstLastEdg(newEdge);

        newEdge->pl().depth = std::max(first->pl().depth, second->pl().depth);

        newEdge->pl().routes = first->pl().routes;

        // update direction markers
        for (auto& ro : newEdge->pl().routes) {
          if (ro.direction == n->pl().node) ro.direction = newTo->pl().node;
        }

        assert(newFrom != n);
        assert(newTo != n);

        delNd(n);

        updateEdgeOrder(newFrom);
        updateEdgeOrder(newTo);

        return true;
      }
    }
  }

  return false;
}

// _____________________________________________________________________________
TransitGraph* OptGraph::getGraph() const { return _g; }

// _____________________________________________________________________________
size_t OptGraph::getNumNodes() const { return getNds().size(); }

// _____________________________________________________________________________
size_t OptGraph::getNumNodes(bool topo) const {
  size_t ret = 0;
  for (auto n : getNds()) {
    if ((n->pl().node->getStops().size() == 0) ^ !topo) ret++;
  }

  return ret;
}

// _____________________________________________________________________________
size_t OptGraph::getNumEdges() const {
  size_t ret = 0;

  for (auto n : getNds()) {
    for (auto e : n->getAdjList()) {
      if (e->getFrom() != n) continue;
      ret += 1;
    }
  }

  return ret;
}

// _____________________________________________________________________________
size_t OptGraph::getNumRoutes() const {
  std::set<const graph::Route*> routes;

  for (auto n : getNds()) {
    for (auto e : n->getAdjList()) {
      if (e->getFrom() != n) continue;
      for (const auto& to : *getFirstEdg(e).etg->getRoutes()) {
        if (to.route->relativeTo()) continue;
        routes.insert(to.route);
      }
    }
  }

  return routes.size();
}

// _____________________________________________________________________________
size_t OptGraph::getMaxCardinality() const {
  size_t ret = 0;
  for (auto n : getNds()) {
    for (auto e : n->getAdjList()) {
      if (e->getFrom() != n) continue;
      if (getFirstEdg(e).etg->getCardinality(true) > ret) {
        ret = getFirstEdg(e).etg->getCardinality(true);
      }
    }
  }

  return ret;
}

// _____________________________________________________________________________
const util::geo::Line<double>* OptEdgePL::getGeom() { return 0; }

// _____________________________________________________________________________
util::json::Dict OptEdgePL::getAttrs() {
  util::json::Dict ret;
  std::string lines;

  for (const auto& r : getRoutes()) {
    if (r.route->relativeTo())
      lines += "(" + r.route->relativeTo()->getLabel() + "+" +
               r.route->getLabel() + "[" + r.route->getColor() + ", -> " +
               util::toString(r.direction) + "), ";
    else
      lines += r.route->getLabel() + "[" + r.route->getColor() + ", -> " +
               util::toString(r.direction) + "), ";
  }

  ret["lines"] = lines;
  ret["num_etgs"] = util::toString(etgs.size());
  ret["depth"] = util::toString(depth);

  return ret;
}

// _____________________________________________________________________________
std::string OptEdgePL::toStr() const {
  std::string lines;
  for (const auto& r : getRoutes()) {
    if (r.route->relativeTo())
      lines += "(" + r.route->relativeTo()->getLabel() + "+" +
               r.route->getLabel() + "[" + r.route->getColor() + ", -> " +
               util::toString(r.direction) + "), ";
    else
      lines += r.route->getLabel() + "[" + r.route->getColor() + ", -> " +
               util::toString(r.direction) + "), ";
  }

  return "[" + lines + "]";
}

// _____________________________________________________________________________
const util::geo::Point<double>* OptNodePL::getGeom() { return &p; }

// _____________________________________________________________________________
util::json::Dict OptNodePL::getAttrs() {
  util::json::Dict ret;
  ret["orig_node"] = util::toString(node);
  if (!node) {
    ret["stat_name"] = "<dummy>";
  } else {
    if (node->getStops().size()) {
      ret["stat_name"] = node->getStops()[0].name;
    } else {
      ret["stat_name"] = "";
    }
  }
  std::string edges;
  for (auto e : orderedEdges) {
    edges += util::toString(e) + ",";
  }

  ret["edge_order"] = edges;
  return ret;
}

// _____________________________________________________________________________
std::pair<OptEdge*, OptEdge*> OptGraph::isFullCross(OptNode* n) const {
  if (n->getDeg() < 3) return std::pair<OptEdge*, OptEdge*>(0, 0);
  std::pair<OptEdge*, OptEdge*> ret(0, 0);

  for (auto ea : n->getAdjList()) {
    for (auto eb : n->getAdjList()) {
      if (ea == eb) continue;
      if (dirRouteContains(eb, ea) && dirRouteContains(ea, eb)) {
        ret.first = ea;
        ret.second = eb;
      }
    }

    if (ret.first) {
      bool nope = false;
      for (auto e : n->getAdjList()) {
        if (e == ret.second || e == ret.first) continue;
        if (dirPartialContinuedOver(ret.first, e) || dirPartialContinuedOver(ret.second, e)) {
          nope = true;
          break;
        }
      }
      if (!nope) return ret;
    }
  }

  return std::pair<OptEdge*, OptEdge*>(0, 0);
}

// _____________________________________________________________________________
bool OptGraph::untangleFullCross() {
  double DO = 100;  // only relevant for debug output
  for (OptNode* n : *getNds()) {
    std::pair<OptEdge*, OptEdge*> cross;
    if ((cross = isFullCross(n)).first) {
      LOG(DEBUG) << "Found full cross at node " << n << " between "
                 << cross.first << "(" << cross.first->pl().toStr() << ") and "
                 << cross.second << " (" << cross.second->pl().toStr() << ")";

      auto newN = addNd(util::geo::DPoint(n->pl().getGeom()->getX() + DO,
                                          n->pl().getGeom()->getY() + DO));
      newN->pl().node = n->pl().node;

      if (cross.first->getFrom() == n) {
        addEdg(newN, cross.first->getTo(), cross.first->pl());
      } else {
        addEdg(cross.first->getFrom(), newN, cross.first->pl());
      }

      if (cross.second->getFrom() == n) {
        addEdg(newN, cross.second->getTo(), cross.second->pl());
      } else {
        addEdg(cross.second->getFrom(), newN, cross.second->pl());
      }

      delEdg(cross.first->getFrom(), cross.first->getTo());
      delEdg(cross.second->getFrom(), cross.second->getTo());

      updateEdgeOrder(n);
      updateEdgeOrder(newN);

      return true;
    }
  }
  return false;
}

// _____________________________________________________________________________
std::vector<OptNode*> OptGraph::explodeNodeAlong(OptNode* nd, const PolyLine<double>& pl, size_t n) {
  std::vector<OptNode*> ret(n);
  for (size_t i = 0; i < n; i++) {
    double p = (n - 1 - i) / (double)n;
    ret[i] = addNd(pl.getPointAt(p).p);
    ret[i]->pl().node = nd->pl().node;
  }

  return ret;
}

// _____________________________________________________________________________
bool OptGraph::untanglePartialYStep() {
  double DO = 100;  // only relevant for debug output
  for (OptNode* na : *getNds()) {
    if (na->getDeg() != 1) continue;  // only look at terminus nodes

    // the only outgoing edge
    OptEdge* ea = na->getAdjList().front();
    OptNode* nb = ea->getOtherNd(na);

    if (isPartialYAt(ea, nb)) {
      assert(nb->pl().node);
      assert(na->pl().node);

      LOG(DEBUG) << "Found partial Y at node " << nb << " with main leg " << ea
                 << " (" << ea->pl().toStr() << ")";

      // the geometry of the main leg
      util::geo::PolyLine<double> pl(*nb->pl().getGeom(), *na->pl().getGeom());
      double bandW = (nb->getDeg() - 1) * (DO / (ea->pl().depth + 1));
      auto ortho = pl.getOrthoLineAtDist(pl.getLength(), bandW);

      // each leg, in clockwise fashion
      auto minLgs = partialClockwEdges(ea, nb);

      assert(minLgs.size() <= ea->pl().getCardinality());

      // for each minor leg of the Y, create a new node at the origin
      std::vector<OptNode*> origNds = explodeNodeAlong(na, ortho, minLgs.size());

      size_t offset = 0;
      for (size_t i = 0; i < minLgs.size(); i++) {
        size_t j = i;
        OptEdgePL pl;
        if (ea->getFrom() == nb) {
          if (ea->pl().etgs[0].dir) j = minLgs.size() - 1 - i;
          pl = getPartialView(ea, minLgs[j], offset);
          addEdg(nb, origNds[j], pl);
        } else {
          if (!ea->pl().etgs[0].dir) j = minLgs.size() - 1 - i;
          pl = getPartialView(ea, minLgs[j], offset);
          addEdg(origNds[j], nb, pl);
        }
        offset += pl.getRoutes().size();
      }

      // delete remaining stuff
      delNd(na);

      // update orderings
      for (auto n : origNds) updateEdgeOrder(n);
      updateEdgeOrder(nb);

      return true;
    }
  }
  return false;
}

// _____________________________________________________________________________
OptEdge* OptGraph::isStump(OptEdge* e) const {
  OptEdge* ret;
  if ((ret = isStumpAt(e, e->getFrom()))) return ret;
  if ((ret = isStumpAt(e, e->getTo()))) return ret;

  return 0;
}

// _____________________________________________________________________________
OptEdge* OptGraph::isStumpAt(OptEdge* e, OptNode* n) const {
  if (n->getDeg() != 2) return 0;

  auto branches = branchesAt(e, e->getOtherNd(n));
  if (!branches.size()) branches = partiallyBranchesAt(e, e->getOtherNd(n));

  OptEdge* mainBranch = 0;

  for (auto branch : branches) {
    for (auto otherBranch : n->getAdjList()) {
      if (otherBranch == e) continue;
      if (dirContinuedOver(otherBranch, e, branch)) {
        if (mainBranch) return 0;
        mainBranch = otherBranch;
      }
    }
  }

  return mainBranch;
}

// _____________________________________________________________________________
bool OptGraph::untangleStumpStep() {
  double DO = 100;  // only relevant for debug output
  for (OptNode* n : *getNds()) {
    for (OptEdge* mainLeg : n->getAdjList()) {
      if (mainLeg->getFrom() != n) continue;

      OptEdge* stumpEdg = 0;
      OptNode* stumpN = 0;

      if ((stumpEdg = isStump(mainLeg))) {
        stumpN = sharedNode(mainLeg, stumpEdg);
        OptNode* notStumpN = mainLeg->getOtherNd(stumpN);

        LOG(DEBUG) << "Found stump with main leg " << mainLeg << " ("
                   << mainLeg->pl().toStr() << ") at node " << stumpN
                   << " with main stump branch " << stumpEdg << " ("
                   << stumpEdg->pl().toStr() << ")";

        // the geometry of the main leg
        util::geo::PolyLine<double> pl(*notStumpN->pl().getGeom(),
                                       *stumpN->pl().getGeom());
        double bandW =
            (notStumpN->getDeg() - 1) * (DO / (mainLeg->pl().depth + 1));
        auto ortho = pl.getOrthoLineAtDist(pl.getLength(), bandW);
        ortho.reverse();

        // each leg, in clockwise fashion
        auto stumpLegs = partialClockwEdges(mainLeg, notStumpN);
        std::reverse(stumpLegs.begin(), stumpLegs.end());

        // for each minor leg of the stump, create a new node at the origin
        std::vector<OptNode*> stumpNds = explodeNodeAlong(stumpN, ortho, stumpLegs.size());

        size_t mainLegNode = 0;

        size_t offset = 0;
        for (size_t i = 0; i < stumpLegs.size(); i++) {
          size_t j = i;
          OptEdgePL pl;
          if (mainLeg->getFrom() == stumpN) {
            if (mainLeg->pl().etgs[0].dir) j = stumpNds.size() - 1 - i;
            pl = getPartialView(mainLeg, stumpLegs[j], offset);
            addEdg(stumpNds[j], notStumpN, pl);
          } else {
            if (!mainLeg->pl().etgs[0].dir) j = stumpLegs.size() - 1 - i;
            pl = getPartialView(mainLeg, stumpLegs[j], offset);
            addEdg(notStumpN, stumpNds[j], pl);
          }
          if (dirContinuedOver(stumpLegs[j], mainLeg, stumpEdg))
            mainLegNode = j;
          offset += pl.getRoutes().size();
        }

        if (stumpEdg->getFrom() == stumpN)
          addEdg(stumpNds[mainLegNode], stumpEdg->getTo(), stumpEdg->pl());
        else
          addEdg(stumpEdg->getFrom(), stumpNds[mainLegNode], stumpEdg->pl());

        delNd(stumpN);

        // update orderings
        for (auto n : stumpNds) {
          updateEdgeOrder(n);
          for (auto e : n->getAdjList()) updateEdgeOrder(e->getOtherNd(n));
        }
        updateEdgeOrder(notStumpN);

        return true;
      }
    }
  }

  return false;
}

// _____________________________________________________________________________
bool OptGraph::untangleYStep() {
  double DO = 100;  // only relevant for debug output
  for (OptNode* na : *getNds()) {
    if (na->getDeg() != 1) continue;  // only look at terminus nodes

    // the only outgoing edge
    OptEdge* ea = na->getAdjList().front();
    OptNode* nb = ea->getOtherNd(na);

    if (isYAt(ea, nb)) {
      assert(nb->pl().node);
      assert(na->pl().node);

      LOG(DEBUG) << "Found full Y at node " << nb << " with main leg " << ea
                 << " (" << ea->pl().toStr() << ")";

      // the geometry of the main leg
      util::geo::PolyLine<double> pl(*nb->pl().getGeom(), *na->pl().getGeom());
      double bandW = (nb->getDeg() - 1) * (DO / (ea->pl().depth + 1));
      auto orthoPl = pl.getOrthoLineAtDist(0, bandW);
      auto orthoPlOrig = pl.getOrthoLineAtDist(pl.getLength(), bandW);

      // for each minor leg of the stump, create a new node at the origin
      std::vector<OptNode*> centerNds = explodeNodeAlong(nb, orthoPl, nb->getDeg() - 1);
      std::vector<OptNode*> origNds = explodeNodeAlong(na, orthoPlOrig, nb->getDeg() - 1);

      // each leg, in clockwise fashion
      auto minLgs = clockwEdges(ea, nb);
      std::vector<OptEdge*> minLgsN(minLgs.size());
      assert(minLgs.size() == nb->pl().orderedEdges.size() - 1);

      for (size_t i = 0; i < minLgs.size(); i++) {
        if (minLgs[i]->getFrom() == nb)
          minLgsN[i] =
              addEdg(centerNds[i], minLgs[i]->getTo(), minLgs[i]->pl());
        else
          minLgsN[i] =
              addEdg(minLgs[i]->getFrom(), centerNds[i], minLgs[i]->pl());
      }

      size_t offset = 0;
      for (size_t i = 0; i < minLgs.size(); i++) {
        size_t j = i;
        OptEdgePL pl;
        if (ea->getFrom() == nb) {
          if (ea->pl().etgs[0].dir) j = minLgs.size() - 1 - i;
          pl = getView(ea, minLgs[j], offset);
          addEdg(centerNds[j], origNds[j], pl);
        } else {
          if (!ea->pl().etgs[0].dir) j = minLgs.size() - 1 - i;
          pl = getView(ea, minLgs[j], offset);
          addEdg(origNds[j], centerNds[j], pl);
        }
        offset += pl.getRoutes().size();
      }

      // delete remaining stuff
      delNd(nb);
      delNd(na);

      // update orderings
      for (auto n : origNds) updateEdgeOrder(n);  // TODO: is this redundant?
      for (auto n : centerNds) {
        updateEdgeOrder(n);
        for (auto e : n->getAdjList()) updateEdgeOrder(e->getOtherNd(n));
      }

      return true;
    }
  }
  return false;
}

// _____________________________________________________________________________
OptEdgePL OptGraph::getView(OptEdge* parent, OptEdge* leg, size_t offset) {
  OptEdgePL ret(parent->pl());
  ret.depth++;

  for (auto& etg : ret.etgs) {
    if (parent->pl().etgs[0].dir ^ etg.dir)
      etg.order += parent->pl().getRoutes().size() -
                   leg->pl().getRoutes().size() - offset;
    else
      etg.order += offset;
  }

  ret.routes.clear();

  for (auto ro : leg->pl().getRoutes()) {
    auto routes = getCtdRoutesIn(ro.route, ro.direction, leg, parent);
    assert(sharedNode(leg, parent));
    ret.routes.insert(ret.routes.begin(), routes.begin(), routes.end());
  }

  return ret;
}

// _____________________________________________________________________________
OptEdgePL OptGraph::getPartialView(OptEdge* parent, OptEdge* leg,
                                   size_t offset) {
  OptEdgePL ret(parent->pl());
  ret.depth++;

  auto ctdR = getCtdRoutesIn(parent, leg);
  size_t shared = ctdR.size();

  for (auto& etg : ret.etgs) {
    if (parent->pl().etgs[0].dir ^ etg.dir)
      etg.order += parent->pl().getRoutes().size() - shared - offset;
    else
      etg.order += offset;
  }

  ret.routes.clear();

  for (auto ro : leg->pl().getRoutes()) {
    auto routes = getCtdRoutesIn(ro.route, ro.direction, leg, parent);
    ret.routes.insert(ret.routes.begin(), routes.begin(), routes.end());
  }

  return ret;
}

// _____________________________________________________________________________
bool OptGraph::untanglePartialDogBoneStep() {
  double DO = 400;  // only relevant for debug output
  for (OptNode* na : *getNds()) {
    if (na->getDeg() < 3) continue;  // only look at terminus nodes

    for (OptEdge* mainLeg : na->getAdjList()) {
      if (mainLeg->getFrom() != na) continue;
      OptNode* notPartN = 0;
      if ((notPartN = isPartialDogBone(mainLeg))) {
        LOG(DEBUG) << "Found partial dog bone with main leg " << mainLeg << " ("
                   << mainLeg->pl().toStr() << ") at node " << notPartN;

        OptNode* partN = mainLeg->getOtherNd(notPartN);

        // the geometry of the main leg
        util::geo::PolyLine<double> pl(*notPartN->pl().getGeom(),
                                       *partN->pl().getGeom());
        double bandW = (partN->getDeg() - 1) * (DO / (mainLeg->pl().depth + 1));
        auto orthoPlOrig = pl.getOrthoLineAtDist(pl.getLength(), bandW);
        orthoPlOrig.reverse();

        // each leg, in clockwise fashion
        auto minLgs = clockwEdges(mainLeg, partN);
        auto minLgsNotPart = partialClockwEdges(mainLeg, notPartN);
        std::reverse(minLgsNotPart.begin(), minLgsNotPart.end());
        std::vector<OptEdge*> minLgsNew(minLgs.size());

        assert(minLgs.size() <= mainLeg->pl().getCardinality());
        // map positions in b to positions in a
        std::vector<size_t> aToB = mapPositions(minLgsNotPart, mainLeg, minLgs);
        std::vector<size_t> iden(aToB.size());
        for (size_t i = 0; i < iden.size(); i++) iden[i] = i;

        // for each minor leg of the Y, create a new node at the origin
        std::vector<OptNode*> partNds = explodeNodeAlong(partN, orthoPlOrig, minLgs.size());

        for (size_t i = 0; i < minLgs.size(); i++) {
          if (minLgs[i]->getFrom() == partN)
            minLgsNew[i] =
                addEdg(partNds[i], minLgs[i]->getTo(), minLgs[i]->pl());
          else
            minLgsNew[i] =
                addEdg(minLgs[i]->getFrom(), partNds[i], minLgs[i]->pl());
        }

        std::vector<OptEdge*>& refLegs = minLgs;
        std::vector<size_t>& toB = iden;
        if (_scorer->getCrossingPenaltyDiffSeg(partN->pl().node) <
            _scorer->getCrossingPenaltyDiffSeg(notPartN->pl().node)) {
          refLegs = minLgsNotPart;
          toB = aToB;
        }

        size_t offset = 0;
        for (size_t i = 0; i < minLgs.size(); i++) {
          size_t j = i;
          OptEdgePL pl;
          if (mainLeg->getFrom() == partN) {
            if (mainLeg->pl().etgs[0].dir) j = minLgs.size() - 1 - i;
            pl = getPartialView(mainLeg, refLegs[j], offset);
            addEdg(partNds[toB[j]], notPartN, pl);
          } else {
            if (!mainLeg->pl().etgs[0].dir) j = minLgs.size() - 1 - i;
            pl = getPartialView(mainLeg, refLegs[j], offset);
            addEdg(notPartN, partNds[toB[j]], pl);
          }
          offset += pl.getRoutes().size();
        }

        // delete remaining stuff
        delNd(partN);

        // update orderings
        for (auto n : partNds) {
          updateEdgeOrder(n);
          for (auto e : n->getAdjList()) updateEdgeOrder(e->getOtherNd(n));
        }
        updateEdgeOrder(notPartN);

        return true;
      }
    }
  }

  return false;
}

// _____________________________________________________________________________
bool OptGraph::untangleDogBoneStep() {
  double DO = 200;  // only relevant for debug output
  for (OptNode* na : *getNds()) {
    if (na->getDeg() < 3) continue;  // only look at terminus nodes

    for (OptEdge* mainLeg : na->getAdjList()) {
      if (mainLeg->getFrom() != na) continue;
      if (isDogBone(mainLeg)) {
        LOG(DEBUG) << "Found full dog bone with main leg " << mainLeg << " ("
                   << mainLeg->pl().toStr() << ")";

        OptNode* nb = mainLeg->getOtherNd(na);
        // the geometry of the main leg
        util::geo::PolyLine<double> pl(*na->pl().getGeom(),
                                       *nb->pl().getGeom());
        double bandW = (nb->getDeg() - 1) * (DO / (mainLeg->pl().depth + 1));
        auto orthoPlA = pl.getOrthoLineAtDist(0, bandW);
        auto orthoPlB = pl.getOrthoLineAtDist(pl.getLength(), bandW);

        // for each minor leg at node a, create a new node
        std::vector<OptNode*> aNds = explodeNodeAlong(na, orthoPlA, na->getDeg() - 1);

        // each leg in a, in clockwise fashion
        auto minLgsA = clockwEdges(mainLeg, na);
        std::vector<OptEdge*> minLgsANew(minLgsA.size());
        assert(minLgsA.size() == na->pl().orderedEdges.size() - 1);

        std::vector<OptNode*> bNds = explodeNodeAlong(nb, orthoPlB, nb->getDeg() - 1);

        // each leg in b, in counter-clockwise fashion
        auto minLgsB = clockwEdges(mainLeg, nb);
        std::vector<OptEdge*> minLgsBNew(minLgsB.size());
        assert(minLgsB.size() == nb->pl().orderedEdges.size() - 1);

        std::reverse(minLgsB.begin(), minLgsB.end());

        // map positions in b to positions in a
        std::vector<size_t> bToA = mapPositions(minLgsA, mainLeg, minLgsB);
        std::vector<size_t> aToB = mapPositions(minLgsB, mainLeg, minLgsA);
        std::vector<size_t> iden(bToA.size());
        for (size_t i = 0; i < iden.size(); i++) iden[i] = i;

        for (size_t i = 0; i < minLgsA.size(); i++) {
          if (minLgsA[i]->getFrom() == na) {
            minLgsANew[i] =
                addEdg(aNds[i], minLgsA[i]->getTo(), minLgsA[i]->pl());
          } else {
            minLgsANew[i] =
                addEdg(minLgsA[i]->getFrom(), aNds[i], minLgsA[i]->pl());
          }
        }

        for (size_t i = 0; i < minLgsB.size(); i++) {
          if (minLgsB[i]->getFrom() == nb)
            minLgsBNew[i] =
                addEdg(bNds[i], minLgsB[i]->getTo(), minLgsB[i]->pl());
          else
            minLgsBNew[i] =
                addEdg(minLgsB[i]->getFrom(), bNds[i], minLgsB[i]->pl());
        }

        std::vector<OptEdge*>& refLegs = minLgsA;
        std::vector<size_t>& toA = bToA;
        std::vector<size_t>& toB = iden;
        if (_scorer->getCrossingPenaltyDiffSeg(na->pl().node) <
            _scorer->getCrossingPenaltyDiffSeg(nb->pl().node)) {
          refLegs = minLgsB;
          toA = iden;
          toB = aToB;
        }

        size_t offset = 0;
        for (size_t i = 0; i < minLgsA.size(); i++) {
          size_t j = i;
          OptEdgePL pl;
          if (mainLeg->getFrom() == na) {
            if (mainLeg->pl().etgs[0].dir) j = minLgsA.size() - 1 - i;
            pl = getView(mainLeg, refLegs[j], offset);
            addEdg(aNds[toB[j]], bNds[toA[j]], pl);
          } else {
            if (!mainLeg->pl().etgs[0].dir) j = minLgsA.size() - 1 - i;
            pl = getView(mainLeg, refLegs[j], offset);
            addEdg(bNds[toA[j]], aNds[toB[j]], pl);
          }
          offset += pl.getRoutes().size();
        }

        // delete remaining stuff
        delNd(nb);
        delNd(na);

        // update orderings
        for (auto n : aNds) {
          updateEdgeOrder(n);
          for (auto e : n->getAdjList()) updateEdgeOrder(e->getOtherNd(n));
        }
        for (auto n : bNds) {
          updateEdgeOrder(n);
          for (auto e : n->getAdjList()) updateEdgeOrder(e->getOtherNd(n));
        }

        return true;
      }
    }
  }

  return false;
}

// _____________________________________________________________________________
void OptGraph::writeEdgeOrder() {
  for (auto nd : *getNds()) {
    updateEdgeOrder(nd);
  }
}

// _____________________________________________________________________________
void OptGraph::updateEdgeOrder(OptNode* n) {
  n->pl().orderedEdges.clear();

  if (n->getDeg() == 1) {
    n->pl().orderedEdges.push_back(n->getAdjList().front());
    return;
  }

  for (auto e : n->getAdjList()) {
    n->pl().orderedEdges.push_back(e);
  }
  std::sort(n->pl().orderedEdges.begin(), n->pl().orderedEdges.end(), cmpEdge);
}

// _____________________________________________________________________________
bool OptGraph::dirRouteEndsIn(const OptEdge* a, const OptEdge* b) {
  OptNode* na = sharedNode(a, b);
  if (!na) return false;
  OptNode* nb = b->getOtherNd(na);

  for (auto& to : b->pl().getRoutes()) {
    if (!dirContinuedOver(to, b, a)) continue;

    for (auto c : nb->getAdjList()) {
      if (b == c) continue;
      if (dirContinuedOver(to, b, c)) return false;
    }
  }
  return true;
}

// _____________________________________________________________________________
bool OptGraph::dirRouteContains(const OptEdge* a, const OptEdge* b) {
  for (auto& to : b->pl().getRoutes()) {
    if (!getCtdRoutesIn(to.route, to.direction, b, a).size()) {
      return false;
    }
  }
  return true;
}

// _____________________________________________________________________________
bool OptGraph::dirRouteEqualIn(const OptEdge* a, const OptEdge* b) {


  if (a->pl().getCardinality() != b->pl().getCardinality()) {
    return false;
  }

  for (auto& to : a->pl().getRoutes()) {
    if (!getSameDirRoutesIn(to.route, to.direction, a, b).size()) {
      return false;
    }
  }

  for (auto& to : b->pl().getRoutes()) {
    if (!getSameDirRoutesIn(to.route, to.direction, b, a).size()) {
      return false;
    }
  }
  return true;
}

// _____________________________________________________________________________
bool OptGraph::isYAt(OptEdge* eLeg, OptNode* n) const {
  if (eLeg->getOtherNd(n)->getDeg() != 1) return false;
  return branchesAt(eLeg, n).size();
}
// _____________________________________________________________________________
bool OptGraph::isPartialYAt(OptEdge* eLeg, OptNode* n) const {
  if (eLeg->getOtherNd(n)->getDeg() != 1) return false;

  return partiallyBranchesAt(eLeg, n).size();
}

// _____________________________________________________________________________
bool OptGraph::isDogBone(OptEdge* leg) const {
  if (leg->getFrom()->getDeg() != leg->getTo()->getDeg()) return false;

  auto branches = branchesAt(leg, leg->getFrom());
  if (!branches.size()) return false;

  return branchesAtInto(leg, leg->getTo(), branches);
}

// _____________________________________________________________________________
OptNode* OptGraph::isPartialDogBone(OptEdge* leg) const {
  auto branchesA = branchesAt(leg, leg->getFrom());
  if (branchesA.size() && partiallyBranchesAtInto(leg, leg->getTo(), branchesA))
    return leg->getTo();

  branchesA = branchesAt(leg, leg->getTo());
  if (branchesA.size() &&
      partiallyBranchesAtInto(leg, leg->getFrom(), branchesA))
    return leg->getFrom();

  return 0;
}

// _____________________________________________________________________________
bool OptGraph::partiallyBranchesAtInto(OptEdge* leg, OptNode* n,
                                       std::vector<OptEdge*> branchesA) const {
  auto branchesB = partiallyBranchesAt(leg, n);
  if (branchesA.size() != branchesB.size()) return false;

  std::set<OptEdge*> used;

  for (auto branch : branchesA) {
    bool found = false;
    for (auto oBranch : branchesB) {
      if (!used.count(oBranch) && dirContinuedOver(branch, leg, oBranch)) {
        found = true;
        used.insert(oBranch);
        break;
      }
    }
    if (!found) return false;
  }

  return true;
}

// _____________________________________________________________________________
bool OptGraph::branchesAtInto(OptEdge* leg, OptNode* n,
                              std::vector<OptEdge*> branchesA) const {
  auto branchesB = branchesAt(leg, n);
  if (branchesA.size() != branchesB.size()) return false;

  for (auto branch : branchesA) {
    bool found = false;
    for (auto oBranch : branchesB) {
      if (dirContinuedOver(branch, leg, oBranch)) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }

  for (auto branch : branchesB) {
    bool found = false;
    for (auto oBranch : branchesA) {
      if (dirContinuedOver(branch, leg, oBranch)) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }

  return true;
}

// _____________________________________________________________________________
std::vector<OptEdge*> OptGraph::branchesAt(OptEdge* e, OptNode* n) const {
  std::vector<OptEdge*> ret;
  if (e->getFrom() != n && e->getTo() != n) return ret;
  if (n->getDeg() < 3) return ret;
  if (e->pl().getCardinality() < 2) return ret;

  size_t c = 0;

  for (OptEdge* ea : n->getAdjList()) {
    if (ea == e) continue;
    if (!dirRouteContains(e, ea)) return std::vector<OptEdge*>();
    c += ea->pl().getCardinality();
    ret.push_back(ea);
  }

  if (c != e->pl().getCardinality()) return std::vector<OptEdge*>();

  return ret;
}

// _____________________________________________________________________________
std::vector<OptEdge*> OptGraph::partiallyBranchesAt(OptEdge* eMain,
                                                    OptNode* n) const {
  std::set<OptEdge*> ret;
  if (n->getDeg() < 3) return std::vector<OptEdge*>();
  if (eMain->pl().getCardinality() < 2) return std::vector<OptEdge*>();
  if (eMain->getFrom() != n && eMain->getTo() != n)
    return std::vector<OptEdge*>();

  // use this to filter out full Ys
  bool onlyPartial = false;
  bool branches = false;
  OptEdge* first = 0;

  for (auto ro : eMain->pl().getRoutes()) {
    bool found = false;

    for (OptEdge* e : n->getAdjList()) {
      if (e == eMain) continue;

      if (dirContinuedOver(ro, eMain, e)) {
        if (found) return std::vector<OptEdge*>();

        found = true;
        if (first && first != e) branches = true;
        if (!first) first = e;
        if (!onlyPartial && !dirRouteContains(eMain, e)) onlyPartial = true;
        ret.insert(e);
      } else {
        onlyPartial = true;
      }
    }
    if (!found) return std::vector<OptEdge*>();
  }

  if (onlyPartial && branches)
    return std::vector<OptEdge*>(ret.begin(), ret.end());
  return std::vector<OptEdge*>();
}

// _____________________________________________________________________________
std::vector<OptEdge*> OptGraph::clockwEdges(OptEdge* noon, OptNode* n) {
  std::vector<OptEdge*> clockwise;

  size_t passed = n->getDeg();
  for (size_t i = 0; i < n->pl().orderedEdges.size(); i++) {
    auto e = n->pl().orderedEdges[i];
    if (e == noon) {
      passed = i;
      continue;
    }
    if (passed > n->getDeg() - 1)
      clockwise.push_back(e);
    else {
      clockwise.insert(clockwise.begin() + (i - 1 - passed), e);
    }
  }

  return clockwise;
}

// _____________________________________________________________________________
std::vector<OptEdge*> OptGraph::partialClockwEdges(OptEdge* noon, OptNode* n) {
  std::vector<OptEdge*> clockwise = clockwEdges(noon, n);

  for (auto i = clockwise.begin(); i != clockwise.end(); i++) {
    auto e = *i;
    if (!dirPartialContinuedOver(noon, e)) {
      i = clockwise.erase(i) - 1;
      continue;
    }
  }

  return clockwise;
}

// _____________________________________________________________________________
bool OptGraph::dirContinuedOver(const OptEdge* a, const OptEdge* b,
                                const OptEdge* c) {
  OptNode* ab = sharedNode(a, b);
  OptNode* bc = sharedNode(b, c);
  if (!ab || !bc) return false;

  for (auto& to : a->pl().getRoutes()) {
    for (auto& too : getCtdRoutesIn(to.route, to.direction, a, b)) {
      if (!getCtdRoutesIn(too.route, too.direction, b, c).size()) return false;
    }
  }
  return true;
}

// _____________________________________________________________________________
bool OptGraph::dirPartialContinuedOver(const OptEdge* a, const OptEdge* b) {
  OptNode* ab = sharedNode(a, b);
  if (!ab) return false;

  for (auto& to : a->pl().getRoutes()) {
    if (getCtdRoutesIn(to.route, to.direction, a, b).size()) return true;
  }

  return false;
}

// _____________________________________________________________________________
bool OptGraph::dirContinuedOver(const OptRO& ro, const OptEdge* a,
                                const OptEdge* b) {
  OptNode* ab = sharedNode(a, b);
  if (!ab) return false;

  return getCtdRoutesIn(ro.route, ro.direction, a, b).size();
}

// _____________________________________________________________________________
OptNode* OptGraph::sharedNode(const OptEdge* a, const OptEdge* b) {
  OptNode* r = 0;
  if (a->getFrom() == b->getFrom() || a->getFrom() == b->getTo())
    r = a->getFrom();
  if (a->getTo() == b->getFrom() || a->getTo() == b->getTo()) r = a->getTo();
  return r;
}

// _____________________________________________________________________________
std::vector<OptRO> OptGraph::getSameDirRoutesIn(const graph::Route* r,
                                                const Node* dir,
                                                const OptEdge* fromEdge,
                                                const OptEdge* toEdge) {
  std::vector<OptRO> ret;
  const OptNode* n = sharedNode(fromEdge, toEdge);
  if (!n || !n->pl().node || n->getDeg() == 1) return ret;

  for (const OptRO& to : toEdge->pl().getRoutes()) {
    if (to.route == r) {
      if ((to.direction == 0 && dir == 0) ||
          (to.direction == n->pl().node && dir != 0 && dir != n->pl().node) ||
          (to.direction != n->pl().node && to.direction != 0 &&
           dir == n->pl().node)) {
        if (n->pl().node->connOccurs(r, getAdjEdg(fromEdge, n),
                                     getAdjEdg(toEdge, n))) {
          ret.push_back(to);
        }
      }
    }
  }

  return ret;
}

// _____________________________________________________________________________
bool OptGraph::hasCtdRoutesIn(const graph::Route* r,
                                            const Node* dir,
                                            const OptEdge* fromEdge,
                                            const OptEdge* toEdge) {
  const OptNode* n = sharedNode(fromEdge, toEdge);
  if (!n || !n->pl().node || n->getDeg() == 1) return false;

  for (const OptRO& to : toEdge->pl().getRoutes()) {
    if (to.route == r) {
      if (to.direction == 0 || dir == 0 ||
          (to.direction == n->pl().node && dir != n->pl().node) ||
          (to.direction != n->pl().node && dir == n->pl().node)) {
        if (n->pl().node->connOccurs(r, getAdjEdg(fromEdge, n),
                                     getAdjEdg(toEdge, n))) {
          return true;
        }
      }
    }
  }

  return false;
}

// _____________________________________________________________________________
std::vector<OptRO> OptGraph::getCtdRoutesIn(const graph::Route* r,
                                            const Node* dir,
                                            const OptEdge* fromEdge,
                                            const OptEdge* toEdge) {
  std::vector<OptRO> ret;
  const OptNode* n = sharedNode(fromEdge, toEdge);
  if (!n || !n->pl().node || n->getDeg() == 1) return ret;

  for (const OptRO& to : toEdge->pl().getRoutes()) {
    if (to.route == r) {
      if (to.direction == 0 || dir == 0 ||
          (to.direction == n->pl().node && dir != n->pl().node) ||
          (to.direction != n->pl().node && dir == n->pl().node)) {
        if (n->pl().node->connOccurs(r, getAdjEdg(fromEdge, n),
                                     getAdjEdg(toEdge, n))) {
          ret.push_back(to);
        }
      }
    }
  }

  return ret;
}

// _____________________________________________________________________________
std::vector<OptRO> OptGraph::getCtdRoutesIn(const OptEdge* fromEdge,
                                            const OptEdge* toEdge) {
  std::vector<OptRO> ret;
  const OptNode* n = sharedNode(fromEdge, toEdge);
  if (!n || !n->pl().node) return ret;

  for (const OptRO& to : fromEdge->pl().getRoutes()) {
    auto r = getCtdRoutesIn(to.route, to.direction, fromEdge, toEdge);
    ret.insert(ret.end(), r.begin(), r.end());
  }

  return ret;
}

// _____________________________________________________________________________
std::vector<size_t> OptGraph::mapPositions(std::vector<OptEdge*> a,
                                           OptEdge* leg,
                                           std::vector<OptEdge*> b) const {
  std::vector<size_t> ret(a.size());
  assert(a.size() == b.size());

  for (size_t i = 0; i < a.size(); i++) {
    bool found = false;
    for (size_t j = 0; j < b.size(); j++) {
      if (dirContinuedOver(a[i], leg, b[j])) {
        ret[i] = j;
        found = true;
        break;
      }
    }
    assert(found);
  }

  return ret;
}
