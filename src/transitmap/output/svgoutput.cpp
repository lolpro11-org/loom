// Copyright 2016, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Authors: Patrick Brosi <brosip@informatik.uni-freiburg.de>

#include <ostream>
#include "./svgoutput.h"
#include "../geo/PolyLine.h"

using namespace transitmapper;
using namespace output;

// _____________________________________________________________________________
SvgOutput::SvgOutput(std::ostream* o) : _o(o), _w(o) {

}

// _____________________________________________________________________________
void SvgOutput::print(const graph::TransitGraph& outG) {
  std::map<std::string, std::string> params;

  params["width"] = "1000px";
  params["height"] = "1000px";

  _w.openTag("svg", params);

  // TODO: output edges

  outputNodes(outG);
  outputEdges(outG);

  _w.closeTags();
}

// _____________________________________________________________________________
void SvgOutput::outputNodes(const graph::TransitGraph& outputGraph) {
  _w.openTag("g");
  for (graph::Node* n : outputGraph.getNodes()) {
    std::map<std::string, std::string> params;
    params["cx"] = std::to_string(n->getPos().get<0>());
    params["cy"] = std::to_string(n->getPos().get<1>());
    params["r"] = "10";
    params["stroke"] = "black";
    params["stroke-width"] = "4";
    params["fill"] = "white";
    _w.openTag("circle", params);
    _w.closeTag();
  }
  _w.closeTag();
}

// _____________________________________________________________________________
void SvgOutput::outputEdges(const graph::TransitGraph& outputGraph) {
  _w.openTag("g");
  for (graph::Node* n : outputGraph.getNodes()) {
    std::map<std::string, std::string> params;
    params["cx"] = std::to_string(n->getPos().get<0>());
    params["cy"] = std::to_string(n->getPos().get<1>());
    params["r"] = "10";
    params["stroke"] = "black";
    params["stroke-width"] = "4";
    params["fill"] = "white";
    _w.openTag("circle", params);
    _w.closeTag();
  }

  _w.closeTag();
}

// _____________________________________________________________________________
void SvgOutput::printLine(const transitmapper::geo::PolyLine& l,
													const std::string& style) {
	std::map<std::string, std::string> params;
	params["style"] = style;
	std::stringstream points;

	for (auto& p : l.getLine()) {
		points << " " << p.get<0>() << "," << p.get<1>();
	}

	params["points"] = points.str();

	_w.openTag("polyline", params);

	_w.closeTag();
}
