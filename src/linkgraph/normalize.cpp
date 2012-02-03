/** @file normalize.h Definition of normalizing link graph handler. */

#include "../stdafx.h"
#include "normalize.h"

void Normalizer::ReroutePassby(LinkGraphComponent *graph, NodeID node_id, NodeID export_id, NodeID other_id)
{
	Node &node = graph->GetNode(node_id);
	Edge &passby_edge = graph->GetEdge(export_id, other_id);
	if (passby_edge.capacity == 0) return;
	/* next node in passby chain */
	uint reroute = min(node.supply, passby_edge.capacity);
	node.supply -= reroute; // supply being misused as capacity
	graph->GetEdge(node_id, other_id).capacity += reroute;
	passby_edge.capacity -= reroute;
}

// TODO: this is not thread save. We're modifying the input!
Normalizer::Normalizer(LinkGraphComponent *graph)
{
	for (NodeID node_id = 0; node_id < graph->GetSize(); ++node_id) {
		Node &node = graph->GetNode(node_id);
		if (node.import_node != INVALID_NODE) {
			if (node.passby_flag != IS_PASSBY_NODE) {
				/* Regular import node:
				 * 1. Make an additional edge from base node to
				 * import node.
				 * 2. Move supply and demand to import node.
				 * 3. Update export_node field of import_node.
				 */
				graph->AddEdge(node_id, node.import_node, UINT_MAX);
				Node &import_node = graph->GetNode(node.import_node);
				import_node.supply = node.supply;
				import_node.undelivered_supply = node.undelivered_supply;
				import_node.demand = node.demand;
				node.supply = 0;
				node.undelivered_supply = 0;
				node.demand = 0;
				import_node.export_node = node.export_node;
				import_node.import_node = node.import_node;
			} else {
				/* Passby node: Base node is given as import/export.
				 * 1. Make an additional edge of capacity
				 * "supply" from passby_node to node for Station
				 * "station".
				 * 2. Reduce capacity of edge from either base
				 * node or export node to "station" by "supply".
				 * 3. Make an additional edge from either export
				 * or base node to passby node.
				 * 4. Set supply, demand to 0, station to the
				 * same as base node.
				 *
				 * Keep import and export as they are to mark
				 * the node as passby for the flowmapper.
				 */
				Node &base_node = graph->GetNode(node.passby_base);
				NodeID export_id = base_node.export_node == INVALID_NODE ?
						node.passby_base : base_node.export_node;
				for (NodeID other_id = 0; other_id != graph->GetSize(); ++other_id) {
					Node &other = graph->GetNode(other_id);
					if (other.station == node.passby_to) {
						/* final end of passby chain */
						node.passby_to = other.import_node == INVALID_STATION ? other_id :
								other.import_node;
					} else if (other.passby_flag == IS_PASSBY_NODE && other.passby_to == node.passby_to) {
						this->ReroutePassby(graph, node_id, export_id, other_id);
						if (node.supply == 0) break; // no more capacity to be distributed
					}
				}
				assert(node.passby_to < graph->GetSize());
				if (node.supply > 0) {
					this->ReroutePassby(graph, node_id, export_id, node.passby_to);
				}
				node.supply = 0;
				node.undelivered_supply = 0;
				node.demand = 0;
				graph->AddEdge(export_id, node_id, UINT_MAX);
			}
		}
		if (node.export_node != INVALID_NODE && node.passby_flag != IS_PASSBY_NODE) {
			/* Regular export node:
			 * 1. Move all outgoing links from base node to export
			 * node.
			 * 2. Make an additional edge from base node to export
			 * node.
			 * 3. Clear demand and supply of export node.
			 * 4. Update import_node field of export_node.
			 */
			for (NodeID other_id = 0; other_id != graph->GetSize(); ++other_id) {
				Edge &edge = graph->GetEdge(node_id, other_id);
				if (edge.capacity > 0) {
					graph->GetEdge(node.export_node, other_id).capacity = edge.capacity;
					edge.capacity = 0;
				}
			}
			graph->AddEdge(node_id, node.export_node, UINT_MAX);
			Node &export_node = graph->GetNode(node.export_node);
			export_node.demand = 0;
			export_node.supply = 0;
			export_node.undelivered_supply = 0;
			export_node.import_node = node.import_node;
			export_node.export_node = node.export_node;
		}
	}
}
