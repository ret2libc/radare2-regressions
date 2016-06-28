#include <r_util.h>
#include <r_list.h>
#include "minunit.h"

bool test_add_del_node(void) {
	RGraph *g = r_graph_new ();
	RGraphNode *n1 = r_graph_add_node (g, (void *)(intptr_t)(1));

	const RList *l = r_graph_get_nodes (g);
	RGraphNode *act_n1 = r_graph_get_node (g, n1->idx);
	mu_assert_eq ((int)(intptr_t)r_list_first (l), (int)(intptr_t)n1,
			"r_graph_get_nodes has wrong elements");
	mu_assert_eq (r_list_length (l), 1,
			"r_graph_get_nodes should have only one element");
	mu_assert_eq ((int)(intptr_t)act_n1, (int)(intptr_t)n1,
			"r_graph_get_node retrieves wrong node");

	r_graph_del_node (g, n1);
	l = r_graph_get_nodes (g);
	act_n1 = r_graph_get_node (g, n1->idx);
	mu_assert_eq (r_list_length (l), 0,
			"r_graph_get_nodes should have 0 elements");
	mu_assert_eq ((int)(intptr_t)act_n1, (int)(intptr_t)NULL,
			"r_graph_get_node retrieves wrong node");

	r_graph_free (g);
	mu_end;
}

bool test_add_del_edge(void) {
	RGraph *g = r_graph_new ();
	RGraphNode *n1 = r_graph_add_node (g, (void *)(intptr_t)(1));
	RGraphNode *n2 = r_graph_add_node (g, (void *)(intptr_t)(2));
	mu_assert ("n1 and n2 are not adjacent yet", !r_graph_adjacent (g, n1, n2));
	r_graph_add_edge (g, n1, n2);
	mu_assert ("n1 and n2 are adjacent now", r_graph_adjacent (g, n1, n2));

	const RList *neighbours = r_graph_get_neighbours (g, n1);
	RGraphNode *act_n2 = r_list_first (neighbours);
	mu_assert_eq (r_list_length (neighbours), 1, "n1 should have one adjacent node");
	mu_assert_eq ((int)(intptr_t)act_n2, (int)(intptr_t)n2,
			"n2 should be the neighbour of n1");

	const RList *innodes = r_graph_innodes(g, n1);
	mu_assert_eq (r_list_length (innodes), 0, "no edges to node n1");

	const RList *neighbours_n2 = r_graph_get_neighbours (g, n2);
	mu_assert_eq (r_list_length (neighbours_n2), 0, "n2 shouldn't have any out-edge");

	const RList *innodes_n2 = r_graph_innodes(g, n2);
	RGraphNode *act_n1 = r_list_first (innodes_n2);
	mu_assert_eq ((int)(intptr_t)act_n1, (int)(intptr_t)n1,
			"n2 should be the neighbour of n1");

	r_graph_free (g);
	mu_end;
}

bool test_empty_graph(void) {
	RGraph *g = r_graph_new ();
	const RList *l = r_graph_get_nodes (g);
	mu_assert_eq (r_list_length (l), 0, "No nodes in empty graph");
	r_graph_free (g);
	mu_end;
}

int all_tests() {
	mu_run_test(test_empty_graph);
	mu_run_test(test_add_del_node);
	mu_run_test(test_add_del_edge);
	return tests_passed != tests_run;
}

int main(int argc, char **argv) {
	return all_tests();
}
