#include <iostream>
#include <vector>
#include <thread>
#include <random>
#include <mutex>
#include <map>
#include <chrono>

using namespace std;

// Constants
#define NODES_COUNT 5                  // Number of nodes
#define FAILURE_THRESHOLD 2            // Number of faulty nodes the system can tolerate
#define MAJORITY ((NODES_COUNT * 2) / 3) // 2/3 majority for PBFT commit

// Node Class
class Node {
public:
    int node_id;
    bool is_leader;
    bool is_faulty;
    string block;

    Node(int id) : node_id(id), is_leader(false), is_faulty(false), block("") {}

    void print_status() {
        cout << "Node " << node_id << " - " << (is_leader ? "Leader" : "Follower") 
             << " - " << (is_faulty ? "Faulty" : "Healthy") << endl;
    }
};
// Raft Consensus Class
class RaftConsensus {
private:
    mutex mtx;

public:
    vector<Node*>& nodes;
    Node* current_leader;

    RaftConsensus(vector<Node*>& nodes) : nodes(nodes), current_leader(nullptr) {}

    void elect_leader() {
        lock_guard<mutex> lock(mtx);
        if (current_leader == nullptr || current_leader->is_faulty) {
            cout << "Electing a new leader..." << endl;

            for (auto& node : nodes) {
                if (!node->is_faulty) {
                    current_leader = node;
                    current_leader->is_leader = true;
                    cout << "Leader elected: Node " << current_leader->node_id << endl;
                    return;
                }
            }
        }
    }
void handle_fault() {
        lock_guard<mutex> lock(mtx);
        if (current_leader && current_leader->is_faulty) {
            cout << "Leader Node " << current_leader->node_id << " is faulty. Re-electing a new leader..." << endl;
            current_leader->is_leader = false;
            elect_leader();
        }
    }
};

// PBFT Consensus Class
class PBFTConsensus {
private:
    mutex mtx;

public:
    vector<Node*>& nodes;
    string block;

    PBFTConsensus(vector<Node*>& nodes) : nodes(nodes) {}

    void propose_block(Node* leader) {
        lock_guard<mutex> lock(mtx);
        block = "Block by Node " + to_string(leader->node_id);
        cout << "Leader Node " << leader->node_id << " proposed a new block: " << block << endl;
    }
 void prepare_block_parallel() {
        mutex mtx;
        vector<thread> threads;
        int prepared_votes = 0;

        for (auto& node : nodes) {
            threads.emplace_back([&]() {
                if (!node->is_faulty) {
                    lock_guard<mutex> lock(mtx);
                    cout << "Node " << node->node_id << " prepared the block: " << block << endl;
                    prepared_votes++;
                }
            });
        }

        for (auto& t : threads) t.join();

        if (prepared_votes >= MAJORITY) {
            commit_block();
        } else {
            cout << "Failed to reach consensus for block: " << block << endl;
        }
    }
    void commit_block() {
        mutex mtx;
        vector<thread> threads;
        int committed_votes = 0;

        for (auto& node : nodes) {
            threads.emplace_back([&]() {
                if (!node->is_faulty) {
                    lock_guard<mutex> lock(mtx);
                    cout << "Node " << node->node_id << " committed the block: " << block << endl;
                    committed_votes++;
                }
            });
        }

        for (auto& t : threads) t.join();

        if (committed_votes >= MAJORITY) {
            cout << "Block " << block << " has been successfully committed." << endl;
        }
    }
};

// Hybrid Consensus Class
class HybridConsensus {
private:
    map<int, int> fault_tracker;
public:
    vector<Node*>& nodes;
    RaftConsensus& raft_consensus;
    PBFTConsensus& pbft_consensus;

    HybridConsensus(vector<Node*>& nodes, RaftConsensus& raft, PBFTConsensus& pbft)
        : nodes(nodes), raft_consensus(raft), pbft_consensus(pbft) {}

    void start() {
        // Step 1: Leader Election via Raft
        raft_consensus.elect_leader();

        // Step 2: Block Proposal and Validation via PBFT
        Node* leader = raft_consensus.current_leader;
        if (leader) {
            pbft_consensus.propose_block(leader);
            pbft_consensus.prepare_block_parallel();
        }
    }

    void introduce_fault() {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(0, nodes.size() - 1);
        Node* faulty_node = nodes[dis(gen)];
        faulty_node->is_faulty = true;
        fault_tracker[faulty_node->node_id]++;
        cout << "Node " << faulty_node->node_id << " is now faulty." << endl;

        // Isolate node if it repeatedly fails
        if (fault_tracker[faulty_node->node_id] > 2) {
            cout << "Node " << faulty_node->node_id << " has been isolated due to repeated faults." << endl;
 }
    }
};

// Main Function
int main() {
    // Create Nodes
    vector<Node*> nodes;
    for (int i = 0; i < NODES_COUNT; i++) {
        nodes.push_back(new Node(i));
    }

    // Initialize Consensus Mechanisms
    RaftConsensus raft_consensus(nodes);
    PBFTConsensus pbft_consensus(nodes);
    HybridConsensus hybrid_consensus(nodes, raft_consensus, pbft_consensus);

    // Start Consensus Process
    hybrid_consensus.start();

    // Introduce faults and handle them
    this_thread::sleep_for(chrono::seconds(2));
    hybrid_consensus.introduce_fault();
    this_thread::sleep_for(chrono::seconds(2));
    raft_consensus.handle_fault();

    // Cleanup
    for (Node* node : nodes) {
        delete node;
    }

    return 0;
}