// Copyright 2017, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Authors: Patrick Brosi <brosi@informatik.uni-freiburg.de>

#include <fstream>
#include "octi/Octilinearizer.h"
#include "octi/combgraph/Drawing.h"
#include "octi/gridgraph/NodeCost.h"
#include "util/Misc.h"
#include "util/geo/output/GeoGraphJsonOutput.h"
#include "util/graph/Dijkstra.h"
#include "util/log/Log.h"

#include "ilp/ILPGridOptimizer.h"

using namespace octi;
using namespace gridgraph;

using octi::combgraph::Drawing;
using combgraph::EdgeOrdering;
using util::graph::Dijkstra;
using util::graph::Dijkstra;
using util::geo::len;
using util::geo::dist;
using util::geo::DPoint;

// _____________________________________________________________________________
void Octilinearizer::removeEdgesShorterThan(TransitGraph* g, double d) {
start:
  for (auto n1 : *g->getNds()) {
    for (auto e1 : n1->getAdjList()) {
      if (e1->pl().getPolyline().getLength() < d) {
        if (e1->getOtherNd(n1)->getAdjList().size() > 1 &&
            n1->getAdjList().size() > 1 &&
            (n1->pl().getStops().size() == 0 ||
             e1->getOtherNd(n1)->pl().getStops().size() == 0)) {
          auto otherP = e1->getFrom()->pl().getGeom();

          TransitNode* n = 0;

          if (e1->getTo()->pl().getStops().size() > 0) {
            n = g->mergeNds(e1->getFrom(), e1->getTo());
          } else {
            n = g->mergeNds(e1->getTo(), e1->getFrom());
          }

          n->pl().setGeom(
              DPoint((n->pl().getGeom()->getX() + otherP->getX()) / 2,
                     (n->pl().getGeom()->getY() + otherP->getY()) / 2));
          goto start;
        }
      }
    }
  }
}

// _____________________________________________________________________________
TransitGraph Octilinearizer::drawILP(TransitGraph* tg, GridGraph** retGg,
                                     const Penalties& pens, double gridSize,
                                     double borderRad) {
  removeEdgesShorterThan(tg, gridSize / 2);
  CombGraph cg(tg);
  auto box = tg->getBBox();
  auto gg = new GridGraph(box, gridSize, borderRad, pens);
  Drawing drawing(gg);

  ilp::ILPGridOptimizer ilpoptim;

  ilpoptim.optimize(gg, cg, &drawing);

  TransitGraph ret;
  drawing.getTransitGraph(&ret);

  *retGg = gg;

  return ret;
}

// _____________________________________________________________________________
TransitGraph Octilinearizer::draw(TransitGraph* tg, GridGraph** retGg,
                                  const Penalties& pens, double gridSize,
                                  double borderRad) {
  size_t cores = 1;
  std::vector<GridGraph*> ggs(cores);

  removeEdgesShorterThan(tg, gridSize / 2);
  CombGraph cg(tg);
  auto box = tg->getBBox();

  for (size_t i = 0; i < cores; i++){
    ggs[i] = new GridGraph(box, gridSize, borderRad, pens);
  }

  auto order = getOrdering(cg);

  Drawing drawing(ggs[0]);
  bool found = draw(order, ggs[0], &drawing, std::numeric_limits<double>::infinity());

  if (!found) std::cerr << "(no initial embedding found)" << std::endl;

  Drawing bestIterDraw = drawing;
  drawing.eraseFromGrid(ggs[0]);
  bool iterFound = false;

  size_t rands = 10;
  size_t ITERS = 100;

  // size_t rands = 0;
  // size_t ITERS = 0;

  size_t jobs = rands / cores;
  std::cerr << "Jobs per core: " << jobs << std::endl;

  for (size_t c = 0; c < cores; c++) {
    for (size_t i = 0; i < jobs; i++) {
      T_START(draw);
      auto iterOrder = getOrdering(cg);

      Drawing nextDrawing(ggs[c]);
      bool locFound = draw(iterOrder, ggs[c], &nextDrawing, bestIterDraw.score());

      if (locFound) {
        double imp = (bestIterDraw.score() - nextDrawing.score());
        std::cerr << " ++ Random try " << ((c * jobs) + (i + 1)) << ", best " << bestIterDraw.score()
                  << ", next " << nextDrawing.score() << " ("
                  << (imp >= 0 ? "+" : "") << imp << ", took " << T_STOP(draw) << " ms)" << std::endl;

        if (!iterFound || nextDrawing.score() < bestIterDraw.score()) {
          bestIterDraw = nextDrawing;
          iterFound = true;
        }
      } else {
        std::cerr << " ++ Random try " << i << ", best " << bestIterDraw.score()
                  << ", next <not found>" << " (took " << T_STOP(draw) << " ms)" << std::endl;
      }

      nextDrawing.eraseFromGrid(ggs[c]);
    }
  }

  std::cerr << "Done.." << std::endl;

  if (iterFound) {
    drawing = bestIterDraw;
    found = true;
  }

  if (!found) {
    LOG(ERROR) << "Could not find planar embedding for input graph.";
    exit(1);
  }

  drawing.applyToGrid(ggs[0]);

  size_t iters = 0;

  std::cerr << "Iterating..." << std::endl;

  for (; iters < ITERS; iters++) {
    Drawing bestFromIter = drawing;
    for (auto a : *cg.getNds()) {
      if (a->getDeg() == 0) continue;
      assert(drawing.getGrNd(a));

      Drawing drawingCp = drawing;
      size_t origX = drawing.getGrNd(a)->pl().getX();
      size_t origY = drawing.getGrNd(a)->pl().getY();

      // reverting a
      std::vector<CombEdge*> test;
      for (auto ce : a->getAdjList()) {
        assert(drawingCp.drawn(ce));
        test.push_back(ce);

        drawingCp.eraseFromGrid(ce, ggs[0]);
        drawingCp.erase(ce);
      }

      drawingCp.erase(a);
      ggs[0]->unSettleNd(a);

      for (size_t pos = 0; pos < 9; pos++) {
        Drawing run = drawingCp;
        SettledPos p;

        if (pos == 1) p[a] = {origX + 1, origY + 1};
        if (pos == 2) p[a] = {origX + 1, origY};
        if (pos == 3) p[a] = {origX + 1, origY - 1};

        if (pos == 7) p[a] = {origX - 1, origY + 1};
        if (pos == 6) p[a] = {origX - 1, origY};
        if (pos == 5) p[a] = {origX - 1, origY - 1};

        if (pos == 0) p[a] = {origX, origY + 1};
        if (pos == 8) p[a] = {origX, origY};
        if (pos == 4) p[a] = {origX, origY - 1};

        bool found = draw(test, p, ggs[0], &run, std::numeric_limits<double>::infinity());

        if (found && bestFromIter.score() > run.score()) {
          bestFromIter = run;
        }

        // reset grid
        for (auto ce : a->getAdjList()) run.eraseFromGrid(ce, ggs[0]);

        if (ggs[0]->isSettled(a)) ggs[0]->unSettleNd(a);
      }

      ggs[0]->settleNd(const_cast<GridNode*>(drawing.getGrNd(a)), a);

      // RE-SETTLE EDGES!
      for (auto ce : a->getAdjList()) drawing.applyToGrid(ce, ggs[0]);
    }

    double imp = (drawing.score() - bestFromIter.score());
    std::cerr << " ++ Iter " << iters << ", prev " << drawing.score()
              << ", next " << bestFromIter.score() << " ("
              << (imp >= 0 ? "+" : "") << imp << ")" << std::endl;

    if (imp < 0.05) break;

    drawing.eraseFromGrid(ggs[0]);
    bestFromIter.applyToGrid(ggs[0]);
    drawing = bestFromIter;
  }

  TransitGraph ret;
  drawing.getTransitGraph(&ret);

  *retGg = ggs[0];

  return ret;
}

// _____________________________________________________________________________
void Octilinearizer::settleRes(GridNode* frGrNd, GridNode* toGrNd,
                               GridGraph* gg, CombNode* from, CombNode* to,
                               const GrEdgList& res, CombEdge* e) {
  gg->settleNd(toGrNd, to);
  gg->settleNd(frGrNd, from);

  // balance edges
  for (auto f : res) {
    if (f->pl().isSecondary()) continue;
    gg->settleEdg(f->getFrom()->pl().getParent(), f->getTo()->pl().getParent(),
                  e);
  }
}

// _____________________________________________________________________________
void Octilinearizer::writeNdCosts(GridNode* n, CombNode* origNode, CombEdge* e,
                                  GridGraph* g) {
  NodeCost c;
  c += g->topoBlockPenalty(n, origNode, e);
  c += g->spacingPenalty(n, origNode, e);
  c += g->nodeBendPenalty(n, e);

  g->addCostVector(n, c);
}

// _____________________________________________________________________________
bool Octilinearizer::draw(const std::vector<CombEdge*>& order, GridGraph* gg,
                          Drawing* drawing, double cutoff) {
  SettledPos emptyPos;
  return draw(order, emptyPos, gg, drawing, cutoff);
}

// _____________________________________________________________________________
bool Octilinearizer::draw(const std::vector<CombEdge*>& ord,
                          const SettledPos& settled, GridGraph* gg,
                          Drawing* drawing, double globCutoff) {
  double c_0 = gg->getPenalties().p_45 - gg->getPenalties().p_135;

  SettledPos retPos;

  double dijCost = 0;
  double dijIters = 0;

  for (auto cmbEdg : ord) {
    double cutoff = globCutoff - drawing->score();
    bool rev = false;
    auto frCmbNd = cmbEdg->getFrom();
    auto toCmbNd = cmbEdg->getTo();

    std::set<GridNode *> frGrNds, toGrNds;
    std::tie(frGrNds, toGrNds) = getRtPair(frCmbNd, toCmbNd, settled, gg);

    if (frGrNds.size() == 0 || toGrNds.size() == 0) {
      return false;
    }

    if (toGrNds.size() > frGrNds.size()) {
      auto tmp = frCmbNd;
      frCmbNd = toCmbNd;
      toCmbNd = tmp;
      auto tmp2 = frGrNds;
      frGrNds = toGrNds;
      toGrNds = tmp2;
      rev = true;
    }

    // why not distance based? (TODO, balance this with edge costs)
    double penPerGrid = 3 + c_0 + fmax(gg->getPenalties().diagonalPen,
                                       gg->getPenalties().horizontalPen);

    // if we open node sinks, we have to offset their cost by the highest
    // possible turn cost + 1 to not distort turn penalties
    double costOffsetFrom = 0;
    double costOffsetTo = 0;

    // open the source nodes
    for (auto n : frGrNds) {
      double gridD = floor(dist(*n->pl().getGeom(), *frCmbNd->pl().getGeom()));
      gridD = gridD / gg->getCellSize();

      if (gg->isSettled(frCmbNd)) {
        // only count displacement penalty ONCE
        gg->openNodeSink(n, 0);
      } else {
        costOffsetFrom = (gg->getPenalties().p_45 - gg->getPenalties().p_135);
        gg->openNodeSink(n, costOffsetFrom + gridD * penPerGrid);
      }
    }

    // open the target nodes
    for (auto n : toGrNds) {
      double gridD = floor(dist(*n->pl().getGeom(), *toCmbNd->pl().getGeom()));
      gridD = gridD / gg->getCellSize();

      if (gg->isSettled(toCmbNd)) {
        // only count displacement penalty ONCE
        gg->openNodeSink(n, 0);
      } else {
        costOffsetTo = (gg->getPenalties().p_45 - gg->getPenalties().p_135);
        gg->openNodeSink(n, costOffsetTo + gridD * penPerGrid);
      }
    }

    // IMPORTANT: node costs are only written to sinks if they are already
    // settled. There is no need to add node costs before, as they handle
    // relations between two or more adjacent edges. If the node has not
    // already been settled, such a relation does not exist.
    //
    // Even more importantly, is a node is settled, its turn edges have
    // already been closed.
    //
      // the size() == 1 check is important, because nd cost writing will
      // not work if the to node is not already settled!

    if (frGrNds.size() == 1 && gg->isSettled(frCmbNd)) {
      writeNdCosts(*frGrNds.begin(), frCmbNd, cmbEdg, gg);
    }

    if (toGrNds.size() == 1 && gg->isSettled(toCmbNd)) {
      writeNdCosts(*toGrNds.begin(), toCmbNd, cmbEdg, gg);
    }

    GrEdgList eL;
    GrNdList nL;
    GridNode* toGrNd = 0;
    GridNode* frGrNd = 0;

    auto heur = GridHeur(gg, toGrNds);
    auto cost = GridCost(cutoff);
    T_START(draw);
    size_t itt = Dijkstra::ITERS;
    Dijkstra::shortestPath(frGrNds, toGrNds, cost, heur, &eL, &nL);
    dijCost += T_STOP(draw);
    dijIters += Dijkstra::ITERS - itt;

    if (!nL.size()) {
      // cleanup
      for (auto n : toGrNds) gg->closeNodeSink(n);
      for (auto n : frGrNds) gg->closeNodeSink(n);

      return false;
    }

    toGrNd = nL.front();
    frGrNd = nL.back();

    // remove the cost offsets to not distort final costs
    eL.front()->pl().setCost(eL.front()->pl().cost() - costOffsetTo);
    eL.back()->pl().setCost(eL.back()->pl().cost() - costOffsetFrom);

    // draw
    drawing->draw(cmbEdg, eL, rev);

    // close the source and target node
    for (auto n : toGrNds) gg->closeNodeSink(n);
    for (auto n : frGrNds) gg->closeNodeSink(n);

    settleRes(frGrNd, toGrNd, gg, frCmbNd, toCmbNd, eL, cmbEdg);
  }

  // std::cerr << "Dijkstra: " << dijCost << ", " << dijIters << " iters, " << dijIters / dijCost << " iters per millisec" << std::endl;

  return true;
}

// _____________________________________________________________________________
std::vector<CombEdge*> Octilinearizer::getOrdering(const CombGraph& cg) const {
  NodePQ globalPq, dangling;

  std::set<CombNode*> settled;
  std::vector<CombEdge*> order;

  for (auto n : cg.getNds()) globalPq.push(n);
  std::set<CombEdge*> done;

  while (!globalPq.empty()) {
    auto n = globalPq.top();
    globalPq.pop();
    dangling.push(n);

    while (!dangling.empty()) {
      auto n = dangling.top();
      dangling.pop();

      if (settled.find(n) != settled.end()) continue;

      auto odSet = n->pl().getEdgeOrdering().getOrderedSet();
      std::random_shuffle(odSet.begin(), odSet.end());

      for (auto ee : odSet) {
        if (done.find(ee.first) != done.end()) continue;
        done.insert(ee.first);
        dangling.push(ee.first->getOtherNd(n));

        order.push_back(ee.first);
      }
      settled.insert(n);
    }
  }

  return order;
}

// _____________________________________________________________________________
RtPair Octilinearizer::getRtPair(CombNode* frCmbNd, CombNode* toCmbNd,
                                 const SettledPos& preSettled, GridGraph* gg) {
  // shortcut
  if (gg->getSettled(frCmbNd) && gg->getSettled(toCmbNd)) {
    return {getCands(frCmbNd, preSettled, gg, 0),
            getCands(toCmbNd, preSettled, gg, 0)};
  }

  double maxDis = gg->getCellSize() * 4;

  std::set<GridNode*> frGrNds;
  std::set<GridNode*> toGrNds;

  while ((!frGrNds.size() || !toGrNds.size()) &&
         maxDis < gg->getCellSize() * 25) {
    std::set<GridNode*> frCands = getCands(frCmbNd, preSettled, gg, maxDis);
    std::set<GridNode*> toCands = getCands(toCmbNd, preSettled, gg, maxDis);

    std::set<GridNode*> isect;
    std::set_intersection(frCands.begin(), frCands.end(), toCands.begin(),
                          toCands.end(), std::inserter(isect, isect.begin()));

    std::set_difference(frCands.begin(), frCands.end(), isect.begin(),
                        isect.end(), std::inserter(frGrNds, frGrNds.begin()));
    std::set_difference(toCands.begin(), toCands.end(), isect.begin(),
                        isect.end(), std::inserter(toGrNds, toGrNds.begin()));

    // this effectively builds a Voronoi diagram
    for (auto iNd : isect) {
      if (util::geo::dist(*iNd->pl().getGeom(), *frCmbNd->pl().getGeom()) <
          util::geo::dist(*iNd->pl().getGeom(), *toCmbNd->pl().getGeom())) {
        frGrNds.insert(iNd);
      } else {
        toGrNds.insert(iNd);
      }
    }

    maxDis *= 2;
  }

  return {frGrNds, toGrNds};
}

// _____________________________________________________________________________
std::set<GridNode*> Octilinearizer::getCands(CombNode* cmbNd,
                                             const SettledPos& preSettled,
                                             GridGraph* gg, double maxDis) {
  std::set<GridNode*> ret;

  if (gg->getSettled(cmbNd)) {
    ret.insert(gg->getSettled(cmbNd));
  } else if (preSettled.count(cmbNd)) {
    auto nd = gg->getNode(preSettled.find(cmbNd)->second.first,
                          preSettled.find(cmbNd)->second.second);
    if (nd && !nd->pl().isClosed()) ret.insert(nd);
  } else {
    ret = gg->getGrNdCands(cmbNd, maxDis);
  }

  return ret;
}
