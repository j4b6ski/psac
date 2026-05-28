#pragma once
#ifndef NDEBUG
#define debug_dump(...) debug::dump(__VA_ARGS__)
#include <algorithm>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "types.hpp"

namespace debug
{

using Id = uintptr_t;
struct Pos { double x; double y; };
enum class NodeType { Seq = 'S', Par = 'P', Read = 'R', Mod = 'M' };
enum class EdgeType { Child = 'c', Write = 'w', Read = 'r', Alloc = 'a' };
struct GephiNode { Id id; NodeType type; Pos pos; };
struct GephiEdge { Id src; Id dst; EdgeType type; };
struct GephiGraph
{
    std::unordered_map<Id, GephiNode> nodes;
    std::vector<GephiEdge> edges;
};

// Simple, sparse postorder layout:
// - leaf nodes are assigned the next unused x-position
// - parent nodes are assigned the midpoint above their children
inline void layout_nodes_mods(GephiGraph* g, const psac::SPNode* const root)
{
    int next_x = 0;
    const auto go = [&](auto const& self, const psac::SPNode* const n, const int d) -> int {
        const Id id = reinterpret_cast<Id>(n);
        const auto* const l = n->left.get();
        const auto* const r = n->right.get();
        const int x = [&]{
            if (l && r) {
                const int lx = self(self, l, d+1);
                const int rx = self(self, r, d+1);
                return (lx + rx) / 2;
            } else if (l) {
                return self(self, l, d+1);
            } else {
                assert(!r);
                return next_x++;
            }
        }();
        g->nodes[id].pos = {x, d};
        for (auto const* const ms = n->dynamic_mods.head.get(); ms; ms = ms->next.get())
        {
            auto const* const mods = ms->mod.get_base()->mods();
            for (int i = 0; i < mods.size(); ++i)
            {
                auto const id = reinterpret_cast<Id>(mods[i]);
                g->nodes[id].pos = {x - mods.size/2 + i, d + 0.5};
            }
        }
        return x;
    };
    go(go, root, 0);
}

// Layout given mods on a row at the bottom
inline void layout_global_mods(GephiGraph* g, std::vector<psac::ModBase*> const* const globals)
{
    //double min_x = inf;
    //double max_x = -inf;
    double max_y = 0;
    for (auto const& [id, n] : g->nodes)
    {
        //min_x = std::min(min_x, n.pos.x);
        //max_x = std::max(max_x, n.pos.x);
        max_y = std::max(max_y, n.pos.x);
    }

    int next_x = 0;
    for (auto const& m : *globals)
    {
        auto const id = reinterpret_cast<Id>(m);
        g->nodes[id] = {.id=id, .type=NodeType::Mod, .pos = {next_x++, max_y+1}};
    }
}
 

// This is GephiLiteFileFormat serialized as JSON
// https://github.com/gephi/gephi-lite/blob/main/packages/gephi-lite/src/core/file/types.ts#L14
inline nlohmann::ordered_json write_gephi(GephiGraph const& g)
{
    double const DX = 40;
    double const DY = 50;
    using json = nlohmann::ordered_json;
    json json_nodes = json::array();
    json json_nodeData = json::object();
    json json_layout   = json::object();
    for (const auto& [id, n] : g.nodes) {
        json_nodes.push_back({ {"key", std::to_string(id)} });
        auto data = json{ {"label", std::string(1, std::to_underlying(n.type))} };
        if (n.type == NodeType::Mod) {
            auto* m = reinterpret_cast<psac::ModBase*>(id);
            if (m->source) data["source"] = m->source;
        }
        json_nodeData[id] = std::move(data);
        json_layout[id]   = { {"x", DX * n.pos.x}, {"y", DY * n.pos.y} };
    }

    json json_edges = json::array();
    json json_edgeData = json::object();
    for (const auto& [i, e] : std::views::enumerate(g.edges)) {
        const auto key = "e" + std::to_string(i);
        json_edges.push_back({
            {"key", key},
            {"source", std::format("{}", e.src)},
            {"target", std::format("{}", e.dst)}
        });
        json_edgeData[key] = { {"kind", std::string(1, std::to_underlying(e.type))} };
    }

    const json nodeLabelField  = { {"id", "label"},  {"itemType", "nodes"}, {"type", "category"} };
    const json nodeSourceField = { {"id", "source"}, {"itemType", "nodes"}, {"type", "text"} };
    const json edgeKindField   = { {"id", "kind"},   {"itemType", "edges"}, {"type", "category"} };
    return json{
        {"type",    "gephi-lite"},
        {"version", "1.0.2"},
        // https://github.com/gephi/gephi-lite/blob/main/packages/sdk/src/graph/types.ts#L151
        {"graphDataset", {
            {"metadata", { {"title", "psac computation"} }},
            {"fullGraph", {
                {"options", { {"type", "directed"}, {"multi", true}, {"allowSelfLoops", true} }},
                {"attributes", json::object()},
                {"nodes", std::move(json_nodes)},
                {"edges", std::move(json_edges)},
            }},
            {"nodeData", std::move(json_nodeData)},
            {"edgeData", std::move(json_edgeData)},
            {"layout",   std::move(json_layout)},
            {"nodeFields", json::array({ nodeLabelField, nodeSourceField })},
            {"edgeFields", json::array({ edgeKindField })},
        }},
        {"filters", { {"filters", json::array()} }},
        // https://github.com/gephi/gephi-lite/blob/main/packages/sdk/src/appearance/types.ts#L110
        {"appearance", {
            {"showEdges",       { {"value", true} }},
            {"backgroundColor", "#ffffff"},
            {"layoutGridColor", "#dddddd"},
            {"nodesSize",       { {"type", "fixed"}, {"value", 10} }},
            {"edgesSize",       { {"type", "fixed"}, {"value", 1} }},
            {"nodesColor", {
                {"type", "partition"},
                {"field", nodeLabelField},
                {"colorPalette", { {"S","#00FF00"}, {"P","#0000FF"},
                                   {"R","#FF0000"}, {"M","#FFFF00"} }},
                {"missingColor", "#999999"}
            }},
            {"edgesColor", {
                {"type", "partition"},
                {"field", edgeKindField},
                {"colorPalette", { {"w","#FFC0CB"}, {"r","#FF0000"} }},
                {"missingColor", "#999999"}
            }},
            {"nodesLabel",         { {"type", "field"}, {"field", nodeLabelField}, {"missingValue", nullptr} }},
            {"edgesLabel",         { {"type", "none"} }},
            {"nodesLabelSize",     { {"type", "fixed"}, {"value", 14}, {"density", 1}, {"zoomCorrelation", 0} }},
            {"edgesLabelSize",     { {"type", "fixed"}, {"value", 12}, {"density", 1}, {"zoomCorrelation", 0} }},
            {"nodesLabelEllipsis", { {"type", "ellipsis"}, {"enabled", false}, {"maxLength", 25} }},
            {"edgesLabelEllipsis", { {"type", "ellipsis"}, {"enabled", false}, {"maxLength", 25} }},
            {"nodesImage",         { {"type", "none"} }},
            {"edgesZIndex",        { {"type", "none"} }},
        }},
    };
}


inline GephiGraph walk_sp_tree(psac::SPNode const* const root)
{
    GephiGraph g;
    const auto go = [&](auto const& self, psac::SPNode const* const n, psac::SPNode const* const p) -> void {
        char const t = dynamic_cast<psac::PNode*>(n) ? NodeType::Par
                     : dynamic_cast<psac::RNode*>(n) ? NodeType::Read
                     : NodeType::Seq;
        g.edges.push_back({.src=p, .dst=n, .type=EdgeType::Child});
        g.nodes.push_back({.ptr=n, .type=t, .pos={0,0});

        if (r = dynamic_cast<psac::RNode*>(n))
            for (auto const* const m : r->get_read_mods())
                g.edges.push_back({m, n, EdgeType::Reads});


        for (auto const* const ms = n->dynamic_mods.head.get(); ms; ms = ms->next.get())
            for (ModBase const* const m : ms->mod.get_base()->mods())
            {
                const Id id = reinterpret_cast<Id>(n);
                g.nodes[id]={.id=id, .type = NodeType::Mod, .pos = {0, 0}});
                g.edges.push_back({n, id, EdgeType::Alloc});
            }

        if (auto* l = n->left.get())
            self(self, l, n);
        if (auto* r = n->right.get())
            self(self, r, n);
    };
    go(go, root, nullptr);
    return g;
}

inline void dump(
    const char* path,
    const Computation& comp,
    std::vector<ModBase*> globals = {})
{
    GephiGraph g = walk_sp_tree(comp.root.get());
    layout_nodes_mods(&g, comp.root.get());
    layout_global_mods(&g, &globals);
    const auto j = write_gephi(nodes, edges);

    std::cout << "Writing " << path << std::endl;
    std::ofstream of(path);
    of.exceptions(~std::ios::goodbit);
    of << j.dump(2);
    std::cout << "Done! Go to https://lite.gephi.org/ and Open the file" << std::endl;
}

} // namespace debug
#else
#define debug_dump(...)
#endif
