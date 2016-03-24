#include <iostream>
#include <vector>
#include <unordered_set>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <climits>
#include <pthread.h>
#include <ctime>
#include <unistd.h>
#include <sys/time.h>

using namespace std;

/* global variable definition */
int num_of_thread = 0;
int num_of_cities = 0;
vector<vector<int> > dist_matrix;

/* class definition  */

class Node {
	public: 
		int vertex;
		vector<Node> *child_nodes;
		bool selected;	
		Node (int index) {
			vertex   = index;
			selected = false;
			child_nodes = NULL;
		}
};

/* structure definition */

typedef struct SubSetNode {
	int start_index;
	vector<int> *sets;
	SubSetNode(int vertex, vector<int> *subset) : start_index(vertex), sets(subset) {}
} SubSetNode;

typedef struct ThreadData {
	vector<SubSetNode *> *sets;
        Node *root;
        double result;
	int selected_index;
	int offset;
	ThreadData(vector<SubSetNode *> *p_sets, Node *node) : sets(p_sets), root(node), selected_index(0), offset(0) {}
} ThreadData;

/* function definition */

double GetMinCostRouteHelper (int start_index, vector<int> &sets, Node &root) {
	if (sets.size() == 1) {
                root.child_nodes = new vector<Node>(1, Node(sets[0]));
                root.child_nodes->at(0).child_nodes = new vector<Node>(1, Node(sets[0]));
                root.child_nodes->at(0).selected = true;
                return dist_matrix[start_index][sets[0]] + dist_matrix[sets[0]][0];
	} else if (sets.size() == 0) {
                root.child_nodes = new vector<Node>(1, Node(start_index));
		return dist_matrix[start_index][0];
        }
        
	double min_cost = INT_MAX;
	int selected_index = 0;
        root.child_nodes = new vector<Node>(sets.size(), Node(-1));

        int i = 0;
        for (auto iter = sets.begin();iter != sets.end();iter++) {
                root.child_nodes->at(i) = Node(*iter);
               	vector<int> new_sets(sets);
		int destination = *iter;
                new_sets.erase(new_sets.begin()+i);
                double cost_of_current_node = dist_matrix[start_index][destination];
                double cost_of_other_nodes = GetMinCostRouteHelper(destination, new_sets, root.child_nodes->at(i));
                double current_cost = cost_of_current_node + cost_of_other_nodes;
                if (min_cost > current_cost) {
                        min_cost = current_cost;
                        selected_index = i;
                }
                i++;
        }
        root.child_nodes->at(selected_index).selected = true;
        return min_cost;	
}


void *GetMinCostRouteThreadFunc(void *arg) {
       	ThreadData *tdata = (ThreadData *)arg;
	int min_cost = INT_MAX;
	int current_cost = 0;
	int selected_index = 0;
	int i = 0;
	int offset = tdata->offset;
	Node *root = tdata->root;
	for (auto iter = tdata->sets->begin();iter != tdata->sets->end();iter++) {
		current_cost = GetMinCostRouteHelper((*iter)->start_index, *((*iter)->sets), root->child_nodes->at(i+offset));		
		current_cost += dist_matrix[0][(*iter)->start_index];
		if (min_cost > current_cost) {
			min_cost = current_cost;
			selected_index = i;	
		}
		i++;
	}	
	tdata->result = min_cost;
	tdata->selected_index = selected_index;
        pthread_exit(NULL);
}


double GetMinCostRoute (int start_index, Node &root) {
        int sets_size = num_of_cities-1;
        int basic_thread_size = sets_size/num_of_thread;
        int extra_thread_size = sets_size%num_of_thread;
        int start = 0;
        int end = 0;

	vector<int> sets;
        for (int i = 1;i <= sets_size;i++) {
                sets.push_back(i);
        }

	root.child_nodes = new vector<Node>(sets_size, Node(-1));

	// generate subsets
	vector<SubSetNode *> sub_set_nodes;
	for (int i = 1;i <= sets_size;i++) {
		vector<int> *sub_set = new vector<int>(sets.begin(), sets.end());
		sub_set->erase(sub_set->begin()+(i-1));
		root.child_nodes->at(i-1) = Node(i);
		SubSetNode *sub_set_node = new SubSetNode(i, sub_set);
		sub_set_nodes.push_back(sub_set_node);
	}

	double min_cost = INT_MAX;
	int current_cost = 0;
	int selected_index = 0;
	vector<pthread_t*> threads;
	vector<ThreadData*> tdatas;
	int min_thread_num = min(num_of_thread, num_of_cities);
        for (int i = 0;i < min_thread_num;i++) {
		pthread_t *thread = new pthread_t;
                threads.push_back(thread);
		if (i < extra_thread_size) {
                        end = start+basic_thread_size+1;
                } else {
                        end = start+basic_thread_size;
		}
		vector<SubSetNode *> *p_sets = new vector<SubSetNode *>(sub_set_nodes.begin()+start, sub_set_nodes.begin()+end);
                ThreadData *tdata = new ThreadData(p_sets, &root);		
		tdata->offset = start;
		tdatas.push_back(tdata);
		pthread_create(thread, NULL, &GetMinCostRouteThreadFunc, tdata);
		start = end;
        }

	for (int i = 0;i < num_of_thread;i++) {
		pthread_join(*threads[i], NULL);
	}

	int accu_sum = 0;
	for (int i = 0;i < num_of_thread;i++) {
		if (min_cost > tdatas[i]->result) {
                	min_cost = tdatas[i]->result;
                       	selected_index = tdatas[i]->selected_index + accu_sum;
                }
		accu_sum += tdatas[i]->sets->size();
	}

	root.child_nodes->at(selected_index).selected = true;

	// free resource	
	for (int i = 0;i < num_of_thread;i++) {
		free(threads[i]);
		free(tdatas[i]);
	}
	
	return min_cost;
}


int main (int argc, char *argv[]) {
	num_of_cities = atoi(argv[1]);
	num_of_thread = atoi(argv[2]);

	if (num_of_thread == 0) {
		cout << "thread number can't be 0" << endl;
		return 0;
	}

	ifstream dist_matrix_file;
	dist_matrix_file.open(argv[3]);
	if (!dist_matrix_file.is_open()) {
		cout << "no such file" << endl;
		return 0;
	}

	// generate distance matrix	
	string token;
	int row = 0;
	while (getline(dist_matrix_file, token)) {
		istringstream line(token);
		int col = 0;
		vector<int> row_vec;
		while (line >> token) {
			row_vec.push_back(atoi(token.c_str()));
		}
		dist_matrix.push_back(row_vec);
		row++;
	}
	dist_matrix_file.close();

	if (row == 0 || num_of_cities == 0) {
		cout << "Best path:" << endl;
		cout << "Distance: 999999" << endl;
		return 0;
	} else if (row == 1 || num_of_cities == 1) {
		cout << "Best path: 0 " << endl;
                cout << "Distance: 999999" << endl;
                return 0;
	}

	// Held Karp algorithm
	Node root(0);
	int min_cost = GetMinCostRoute(0, root);
	
	// retrieve path
	int last_dist = 0;
	Node cur = root;
	cout << "Best path: 0 ";
	for (int i = 1;i < num_of_cities;i++) {
		auto child_nodes = cur.child_nodes;
		for (auto iter = child_nodes->begin();iter != child_nodes->end();iter++) {
			Node child_node = *iter;
			if (child_node.selected) {
				cout << child_node.vertex << " ";
				cur = child_node;
				last_dist = dist_matrix[child_node.vertex][0];
				break;
			}
		}
	}
	cout << endl;
	cout << "Distance: " << (min_cost-last_dist) << endl;
	
	return 0;
}
