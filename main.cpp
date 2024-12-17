#include <iostream>
#include <vector>
#include <thread>
#include <random>
#include <mutex>
#include <map>
#include <chrono>
#include <algorithm>

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
    bool is_election_in_progress = false;  // Prevent overlapping elections

public:
    vector<Node*>& nodes;
    Node* current_leader;

    RaftConsensus(vector<Node*>& nodes) : nodes(nodes), current_leader(nullptr) {}

    void elect_leader() {
        // If the lock is unavailable, skip this election
        if (!mtx.try_lock()) {
            cout << "Election skipped due to lock contention." << endl;
            return;
        }

        // Prevent multiple elections from running concurrently
        if (is_election_in_progress) {
            cout << "Election already in progress. Skipping..." << endl;
            mtx.unlock();
            return;
        }

        is_election_in_progress = true;  // Mark the election as in progress
        cout << "\nElecting a new leader..." << endl;

        // Shuffle the nodes vector to randomize leader selection
        random_device rd;
        mt19937 gen(rd());
        shuffle(nodes.begin(), nodes.end(), gen);

        for (auto& node : nodes) {
            if (!node->is_faulty) {
                if (current_leader) current_leader->is_leader = false;  // Reset previous leader
                current_leader = node;
                current_leader->is_leader = true;
                cout << "Leader elected: Node " << current_leader->node_id << endl;
                is_election_in_progress = false;  // Election complete
                mtx.unlock();
                return;
            }
        }

        // If no healthy nodes are found
        current_leader = nullptr;
        cout << "No healthy nodes available for leader election!" << endl;
        is_election_in_progress = false;  // Election complete
        mtx.unlock();
    }

    void handle_fault() {
        lock_guard<mutex> lock(mtx);
        if (current_leader && current_leader->is_faulty) {
            cout << "Leader Node " << current_leader->node_id << " is faulty. Re-electing a new leader..." << endl;
            current_leader->is_leader = false;
            current_leader = nullptr;
            elect_leader();
        }
    }

    void periodic_election() {
        while (true) {
            this_thread::sleep_for(chrono::seconds(5));  // Trigger re-election every 5 seconds
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
        block = "Block proposed by Node " + to_string(leader->node_id);
        cout << "\nLeader Node " << leader->node_id << " proposed a new block: " << block << endl;
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
            cout << "Block '" << block << "' has been successfully committed.\n" << endl;
        } else {
            cout << "Block commitment failed due to insufficient votes.\n" << endl;
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
        raft_consensus.elect_leader();

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

        int faulty_count = 0;
        for (auto& node : nodes) {
            if (node->is_faulty) faulty_count++;
        }

        if (faulty_count < NODES_COUNT - 1) { // Prevent all nodes from being faulty
            Node* faulty_node = nodes[dis(gen)];
            while (faulty_node->is_faulty) {
                faulty_node = nodes[dis(gen)];
            }
            faulty_node->is_faulty = true;
            cout << "Node " << faulty_node->node_id << " is now faulty." << endl;
        } else {
            cout << "Cannot introduce more faults. At least one healthy node is required." << endl;
        }
    }
};

// Main Function
int main() {
    vector<Node*> nodes;
    for (int i = 0; i < NODES_COUNT; i++) {
        nodes.push_back(new Node(i));
    }

    RaftConsensus raft_consensus(nodes);
    PBFTConsensus pbft_consensus(nodes);
    HybridConsensus hybrid_consensus(nodes, raft_consensus, pbft_consensus);

    thread periodic_thread(&RaftConsensus::periodic_election, &raft_consensus);
    periodic_thread.detach();

    for (int i = 0; i < 3; ++i) {
        hybrid_consensus.start();
        this_thread::sleep_for(chrono::seconds(3));
        hybrid_consensus.introduce_fault();
        raft_consensus.handle_fault();
    }

    for (Node* node : nodes) {
        delete node;
    }
    return 0;
}
