// M(arine)alloc - Marine Resource Allocation System
// Build: g++ -std=c++17 -O2 marinealloc.cpp -o marinealloc
// Run  : ./marinealloc                              (built-in demo, writes aquaflow_output.json)
//        ./marinealloc --input in.txt --output out.json   (used by server.py when you press Go)

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <queue>
#include <map>
#include <set>
#include <array>
#include <iomanip>
#include <stdexcept>
#include <cstdio>

using namespace std;

constexpr double INF_CAP = 1e9;
constexpr double EPS     = 1e-9;

// ============================================================================
// [1] CUSTOM MAX-HEAP - O(log N) push/pop
// ============================================================================
template <typename T>
class CustomMaxHeap {
    vector<T> h;

    void siftUp(int i) {
        while (i > 0) {
            int p = (i - 1) / 2;
            if (h[p] < h[i]) {
                swap(h[p], h[i]);
                i = p;
            } else {
                break;
            }
        }
    }

    void siftDown(int i) {
        int n = h.size();
        while (true) {
            int l = 2 * i + 1;
            int r = 2 * i + 2;
            int big = i;

            if (l < n && h[big] < h[l]) big = l;
            if (r < n && h[big] < h[r]) big = r;
            if (big == i) break;

            swap(h[i], h[big]);
            i = big;
        }
    }

public:
    bool empty() const { return h.empty(); }
    size_t size() const { return h.size(); }

    void push(const T& x) {
        h.push_back(x);
        siftUp(h.size() - 1);
    }

    T pop() {
        if (h.empty()) throw runtime_error("Heap is empty.");
        T top = h[0];
        h[0] = h.back();
        h.pop_back();
        if (!h.empty()) siftDown(0);
        return top;
    }
};

// ============================================================================
// [2] UNION-FIND (path compression + union by size) - Shoal detection
// ============================================================================
class UnionFind {
    vector<int> parent, sz;
    int comps;

public:
    UnionFind(int n) : parent(n), sz(n, 1), comps(n) {
        for (int i = 0; i < n; i++) parent[i] = i;
    }

    int find(int i) {
        return parent[i] == i ? i : parent[i] = find(parent[i]);
    }

    void unite(int a, int b) {
        a = find(a);
        b = find(b);
        if (a == b) return;
        if (sz[a] < sz[b]) swap(a, b);
        parent[b] = a;
        sz[a] += sz[b];
        comps--;
    }

    int components() const { return comps; }
};

// ============================================================================
// [3] SEGMENT TREE - Range sum + point update over flattened grid
// ============================================================================
class SegmentTree {
    vector<double> t;
    int n;

    void build(const vector<double>& a, int node, int s, int e) {
        if (s == e) {
            t[node] = a[s];
            return;
        }
        int m = (s + e) / 2;
        build(a, 2 * node, s, m);
        build(a, 2 * node + 1, m + 1, e);
        t[node] = t[2 * node] + t[2 * node + 1];
    }

    void upd(int node, int s, int e, int i, double v) {
        if (s == e) {
            t[node] += v;
            return;
        }
        int m = (s + e) / 2;
        if (i <= m) {
            upd(2 * node, s, m, i, v);
        } else {
            upd(2 * node + 1, m + 1, e, i, v);
        }
        t[node] = t[2 * node] + t[2 * node + 1];
    }

    double qry(int node, int s, int e, int l, int r) {
        if (r < s || e < l) return 0.0;
        if (l <= s && e <= r) return t[node];
        int m = (s + e) / 2;
        return qry(2 * node, s, m, l, r) + qry(2 * node + 1, m + 1, e, l, r);
    }

public:
    SegmentTree(const vector<double>& a) : n(a.size()) {
        t.assign(4 * max(n, 1), 0.0);
        if (n) build(a, 1, 0, n - 1);
    }

    void add(int i, double v) {
        if (i >= 0 && i < n) upd(1, 0, n - 1, i, v);
    }

    double get_sum(int l, int r) {
        return (l < 0 || r >= n || l > r) ? 0.0 : qry(1, 0, n - 1, l, r);
    }
};

// ============================================================================
// [4] AVL INTERVAL TREE - Ecological ban overlap detection
// ============================================================================
struct Ban {
    int start_day, end_day;
    string reason;
};

struct AVLNode {
    Ban b;
    int max_end, height;
    AVLNode *left, *right;

    AVLNode(Ban x)
        : b(x), max_end(x.end_day), height(1), left(nullptr), right(nullptr) {}
};

class AVLIntervalTree {
    AVLNode* root = nullptr;

    int ht(AVLNode* n) { return n ? n->height : 0; }
    int mx(AVLNode* n) { return n ? n->max_end : 0; }

    void upd(AVLNode* n) {
        n->height = 1 + max(ht(n->left), ht(n->right));
        n->max_end = max({n->b.end_day, mx(n->left), mx(n->right)});
    }

    AVLNode* rotR(AVLNode* y) {
        AVLNode* x = y->left;
        y->left = x->right;
        x->right = y;
        upd(y);
        upd(x);
        return x;
    }

    AVLNode* rotL(AVLNode* x) {
        AVLNode* y = x->right;
        x->right = y->left;
        y->left = x;
        upd(x);
        upd(y);
        return y;
    }

    AVLNode* insert(AVLNode* n, Ban b) {
        if (!n) return new AVLNode(b);

        if (b.start_day < n->b.start_day) {
            n->left = insert(n->left, b);
        } else {
            n->right = insert(n->right, b);
        }
        upd(n);

        int bal = ht(n->left) - ht(n->right);
        if (bal > 1) {
            if (ht(n->left->left) < ht(n->left->right)) {
                n->left = rotL(n->left);
            }
            return rotR(n);
        }
        if (bal < -1) {
            if (ht(n->right->right) < ht(n->right->left)) {
                n->right = rotR(n->right);
            }
            return rotL(n);
        }
        return n;
    }

    void find(AVLNode* n, int s, int e, vector<Ban>& res) {
        if (!n) return;
        if (n->b.start_day <= e && s <= n->b.end_day) {
            res.push_back(n->b);
        }
        if (n->left && n->left->max_end >= s) find(n->left, s, e, res);
        if (n->right && n->b.start_day <= e) find(n->right, s, e, res);
    }

    void destroy(AVLNode* n) {
        if (n) {
            destroy(n->left);
            destroy(n->right);
            delete n;
        }
    }

public:
    ~AVLIntervalTree() { destroy(root); }

    void addBan(int s, int e, const string& r) {
        root = insert(root, {s, e, r});
    }

    vector<Ban> getOverlaps(int s, int e) {
        vector<Ban> res;
        find(root, s, e, res);
        return res;
    }
};

// ============================================================================
// [5] EDMONDS-KARP with INCREMENTAL sink edges
// Flow is derived as (original capacity - residual), accounting for back-edges.
// ============================================================================
class AllocationGraph {
    int n;
    vector<vector<double>> cap, orig;
    vector<vector<int>> adj;

    double bfs(int s, int t, vector<int>& par) {
        fill(par.begin(), par.end(), -1);
        par[s] = -2;

        queue<pair<int, double>> q;
        q.push({s, 1e18});

        while (!q.empty()) {
            auto [cur, f] = q.front();
            q.pop();

            for (int nx : adj[cur]) {
                if (par[nx] == -1 && cap[cur][nx] > EPS) {
                    par[nx] = cur;
                    double nf = min(f, cap[cur][nx]);
                    if (nx == t) return nf;
                    q.push({nx, nf});
                }
            }
        }
        return 0;
    }

public:
    AllocationGraph(int nodes)
        : n(nodes), cap(nodes, vector<double>(nodes, 0)), orig(cap), adj(nodes) {}

    void addEdge(int u, int v, double c) {
        if (c <= 0) return;
        if (orig[u][v] == 0 && orig[v][u] == 0) {
            adj[u].push_back(v);
            adj[v].push_back(u);
        }
        cap[u][v] += c;
        orig[u][v] += c;
    }

    // Continues from the current residual graph, so earlier flow is never taken away.
    double augment(int s, int t) {
        double total = 0, f;
        vector<int> par(n);
        while ((f = bfs(s, t, par)) > 0) {
            total += f;
            for (int c = t; c != s; c = par[c]) {
                cap[par[c]][c] -= f;
                cap[c][par[c]] += f;
            }
        }
        return total;
    }

    double flow(int u, int v) {
        return max(0.0, orig[u][v] - cap[u][v]);
    }
};

// ============================================================================
// APPLICATION MODELS
// ============================================================================
struct FleetRequest {
    string id, fleet_name;
    int start_x, start_y, end_x, end_y;
    int start_day, end_day;
    double requested_tons;
    int urgency_score;

    bool operator<(const FleetRequest& o) const {
        if (urgency_score != o.urgency_score) return urgency_score < o.urgency_score;
        return end_day > o.end_day; // earlier deadline wins ties
    }
};

struct Record {
    FleetRequest req;
    string status, reason;
    double available = 0;
    double allocated = 0;
    vector<array<double, 3>> zones; // {row, col, tons}
};

struct Shoal {
    int cells = 0;
    double initial_tons = 0;
    double remaining_tons = 0;
    double max_harvest = 0;
};

static string esc(const string& s) {
    string o;
    for (char c : s) {
        if (c == '"' || c == '\\') {
            o += '\\';
            o += c;
        } else if (c == '\n') {
            o += "\\n";
        } else if (c == '\t') {
            o += "\\t";
        } else {
            o += c;
        }
    }
    return o;
}

struct Layer {
    int ri;                          // index into records
    vector<pair<int, double>> cells; // {flat cell index, tons}
};

struct Frame {
    string label;
    vector<Layer> layers;
};

static string num2(double v) {
    char b[64];
    snprintf(b, sizeof b, "%.2f", v);
    return b;
}

// ============================================================================
// ENGINE
// ============================================================================
class AquaFlowEngine {
    int R, C;
    SegmentTree* stock = nullptr;
    AVLIntervalTree bans;
    CustomMaxHeap<FleetRequest> pending;

    vector<vector<double>> initial_map;
    vector<vector<int>> shoal_id;
    vector<Shoal> shoals;
    double max_depletion_ratio = 0.90; // Max 90% catch per shoal (conserves 10% biomass)

    vector<Ban> ban_list;
    vector<Record> records;
    vector<Frame> frames;
    vector<string> event_log;
    double total_flow = 0;

    int idx(int r, int c) { return r * C + c; }
    void say(const string& s) { cout << s << "\n"; event_log.push_back(s); }

public:
    AquaFlowEngine(int r, int c, double cap = 0.90)
        : R(r), C(c), max_depletion_ratio(cap) {}

    ~AquaFlowEngine() { delete stock; }

    AquaFlowEngine(const AquaFlowEngine&) = delete;
    AquaFlowEngine& operator=(const AquaFlowEngine&) = delete;

    void setDepletionCap(double ratio) { max_depletion_ratio = ratio; }

    void registerBan(int s, int e, const string& reason) {
        if (s > e) throw invalid_argument("Ban start must not exceed end.");
        bans.addBan(s, e, reason);
        ban_list.push_back({s, e, reason});
        say("[RULE ADDED] Temporal Ban: Days " + to_string(s) + "-" + to_string(e) + " (" + reason + ")");
    }

    void initializeOceanState(const vector<vector<double>>& map) {
        if ((int)map.size() != R) throw invalid_argument("Ocean map row count mismatch.");
        for (auto& row : map) {
            if ((int)row.size() != C) throw invalid_argument("Ocean map column count mismatch.");
        }

        say("\n===== STAGE 1: INITIALIZING STATIC OCEAN MAP =====");
        initial_map = map;

        vector<double> flat(R * C, 0.0);
        UnionFind dsu(R * C);
        int dr[] = {-1, 1, 0, 0};
        int dc[] = {0, 0, -1, 1};
        int empty = 0;

        for (int r = 0; r < R; r++) {
            for (int c = 0; c < C; c++) {
                if (map[r][c] <= 0) {
                    empty++;
                    continue;
                }
                flat[idx(r, c)] = map[r][c];

                for (int i = 0; i < 4; i++) {
                    int nr = r + dr[i];
                    int nc = c + dc[i];
                    if (nr >= 0 && nr < R && nc >= 0 && nc < C && map[nr][nc] > 0) {
                        dsu.unite(idx(r, c), idx(nr, nc));
                    }
                }
            }
        }

        // Label shoals 1..k and collect their size/biomass
        shoals.clear();
        shoal_id.assign(R, vector<int>(C, 0));
        std:: map<int, int> label;

        for (int r = 0; r < R; r++) {
            for (int c = 0; c < C; c++) {
                if (map[r][c] > 0) {
                    int root = dsu.find(idx(r, c));
                    if (!label.count(root)) {
                        label[root] = (int)label.size() + 1;
                        shoals.push_back({0, 0.0, 0.0, 0.0});
                    }
                    int id = label[root];
                    shoal_id[r][c] = id;
                    shoals[id - 1].cells++;
                    shoals[id - 1].initial_tons += map[r][c];
                    shoals[id - 1].remaining_tons += map[r][c];
                }
            }
        }

        for (auto& sh : shoals) {
            sh.max_harvest = sh.initial_tons * max_depletion_ratio;
        }

        delete stock;
        stock = new SegmentTree(flat);

        say("[DSU] Found " + to_string(dsu.components() - empty) + " distinct contiguous shoals.");
        say("[POLICY] Shoal Depletion Cap: " + to_string((int)(max_depletion_ratio * 100)) +
            "% max harvest (min " + to_string(100 - (int)(max_depletion_ratio * 100)) + "% conserved).");

        for (size_t i = 0; i < shoals.size(); i++) {
            say("      Shoal " + to_string(i + 1) + ": " + to_string(shoals[i].cells) +
                " cells, " + to_string((int)shoals[i].initial_tons) +
                " tons (safe catch limit: " + to_string((int)shoals[i].max_harvest) + " t)");
        }
        say("[SEGMENT TREE] 1D array built over " + to_string(R * C) + " cells.");
    }

    void submitRequest(const FleetRequest& q) {
        Record rec;
        rec.req = q;

        if (q.start_x < 1 || q.end_x > R || q.start_x > q.end_x ||
            q.start_y < 1 || q.end_y > C || q.start_y > q.end_y) {
            rec.status = "REJECTED";
            rec.reason = "Invalid spatial boundaries";
            records.push_back(rec);
            say("[REJECTED] " + q.id + ": invalid spatial boundaries.");
            return;
        }

        if (q.start_day > q.end_day || q.requested_tons <= 0) {
            rec.status = "REJECTED";
            rec.reason = "Invalid timeline or quantity";
            records.push_back(rec);
            say("[REJECTED] " + q.id + ": invalid timeline or quantity.");
            return;
        }

        pending.push(q);
        say("[RECEIVED] " + q.id + " from " + q.fleet_name + " (urgency " + to_string(q.urgency_score) + ") queued.");
    }

    void processAndAllocate() {
        say("\n===== STAGE 2: PRIORITY TRIAGE =====");
        vector<Record> batch;
        vector<int> batch_idx; // position of each approved request inside records
        frames.clear();
        frames.push_back({"Start: nothing allocated yet", {}});
        vector<double> promised(shoals.size(), 0.0); // tracks quota reserved per shoal

        while (!pending.empty()) {
            Record rec;
            rec.req = pending.pop();
            auto& q = rec.req;
            say("Evaluating " + q.id + " (" + q.fleet_name + ")");

            auto ov = bans.getOverlaps(q.start_day, q.end_day);
            if (!ov.empty()) {
                rec.status = "REJECTED";
                rec.reason = "Ecological ban: " + ov[0].reason;
                say("   -> REJECTED: " + rec.reason);
                records.push_back(rec);
                continue;
            }

            for (int r = q.start_x - 1; r < q.end_x; r++) {
                rec.available += stock->get_sum(idx(r, q.start_y - 1), idx(r, q.end_y - 1));
            }

            // Cap available by remaining shoal harvest quota in the requested region
            set<int> rshoals;
            double shoal_budget = 0.0;
            for (int r = q.start_x - 1; r < q.end_x; r++)
                for (int c = q.start_y - 1; c < q.end_y; c++) {
                    int sid = shoal_id[r][c];
                    if (sid > 0 && rshoals.insert(sid).second)
                        shoal_budget += max(0.0, shoals[sid - 1].max_harvest - promised[sid - 1]);
                }
            rec.available = min(rec.available, shoal_budget);

            if (rec.available <= 0) {
                rec.status = "REJECTED";
                rec.reason = "Shoal depletion cap reached in region";
                say("   -> REJECTED: " + rec.reason);
                records.push_back(rec);
                continue;
            }

            // Reserve quota from each shoal proportional to its share of the budget
            double to_promise = min(rec.available, q.requested_tons);
            for (int sid : rshoals) {
                double share = max(0.0, shoals[sid - 1].max_harvest - promised[sid - 1]);
                if (shoal_budget > EPS)
                    promised[sid - 1] += share / shoal_budget * to_promise;
            }

            say("   -> APPROVED (requested " + to_string((int)q.requested_tons) +
                ", effective stock " + to_string((int)rec.available) + ")");
            rec.status = "APPROVED";
            batch_idx.push_back((int)records.size());
            records.push_back(rec);
            batch.push_back(rec);
        }

        if (batch.empty()) return;

        say("\n===== STAGE 3: MAX-FLOW ALLOCATION (WITH SHOAL DEPLETION CAP) =====");
        int K = (int)shoals.size();
        int Z = R * C;
        int F = (int)batch.size();
        int SRC = 0;
        int SINK = K + Z + F + 1;

        AllocationGraph g(SINK + 1);

        // SRC -> Shoals (capacity = safe harvest limit per shoal)
        for (int s = 1; s <= K; s++) {
            g.addEdge(SRC, s, shoals[s - 1].max_harvest);
        }

        // Shoals -> Cells (capacity = current cell stock)
        for (int r = 0; r < R; r++) {
            for (int c = 0; c < C; c++) {
                int sid = shoal_id[r][c];
                if (sid > 0) {
                    int cell_node = K + 1 + idx(r, c);
                    g.addEdge(sid, cell_node, stock->get_sum(idx(r, c), idx(r, c)));
                }
            }
        }

        // Cells -> Fleets (capacity = infinity)
        for (int f = 0; f < F; f++) {
            auto& q = batch[f].req;
            int fn = K + Z + 1 + f;
            for (int r = q.start_x - 1; r < q.end_x; r++) {
                for (int c = q.start_y - 1; c < q.end_y; c++) {
                    int cell_node = K + 1 + idx(r, c);
                    g.addEdge(cell_node, fn, INF_CAP);
                }
            }
        }

        // Batch is already in priority order. Open each fleet's sink edge one at a time and augment:
        // flow into SINK is never cancelled, so a higher-priority fleet keeps what it got.
        for (int f = 0; f < F; f++) {
            int fn = K + Z + 1 + f;
            g.addEdge(fn, SINK, batch[f].req.requested_tons);
            double got = g.augment(SRC, SINK);
            total_flow += got;
            say("[MAX-FLOW] " + batch[f].req.id + " -> +" + to_string((int)got) + " tons pushed");

            Frame fr;
            fr.label = batch[f].req.id + " " + batch[f].req.fleet_name + ": +" + num2(got) + " t pushed";
            for (int j = 0; j <= f; j++) {
                auto& qj = batch[j].req;
                int fnj = K + Z + 1 + j;
                Layer L;
                L.ri = batch_idx[j];
                for (int r = qj.start_x - 1; r < qj.end_x; r++)
                    for (int c = qj.start_y - 1; c < qj.end_y; c++) {
                        double t = g.flow(K + 1 + idx(r, c), fnj);
                        if (t > 1e-6) L.cells.push_back({idx(r, c), t});
                    }
                fr.layers.push_back(L);
            }
            frames.push_back(fr);
        }
        say("[MAX-FLOW] Total distributed: " + to_string((int)total_flow) + " tons.");

        say("\n--- FINAL ALLOCATION MANIFEST ---");
        for (int f = 0; f < F; f++) {
            Record& rec = batch[f];
            auto& q = rec.req;
            int fn = K + Z + 1 + f;

            rec.allocated = g.flow(fn, SINK);
            for (int r = q.start_x - 1; r < q.end_x; r++) {
                for (int c = q.start_y - 1; c < q.end_y; c++) {
                    int cell_node = K + 1 + idx(r, c);
                    double t = g.flow(cell_node, fn);
                    if (t > EPS) {
                        stock->add(idx(r, c), -t);
                        int sid = shoal_id[r][c];
                        if (sid > 0) shoals[sid - 1].remaining_tons -= t;
                        rec.zones.push_back({(double)r, (double)c, t});
                    }
                }
            }

            if (rec.allocated + EPS < q.requested_tons) {
                if (rec.allocated < EPS) {
                    rec.status = "REJECTED";
                    rec.reason = "Shoal depletion cap reached";
                } else {
                    rec.status = "PARTIAL";
                    rec.reason = "Capped by shoal conservation limit";
                }
            } else {
                rec.status = "FULFILLED";
            }

            cout << left << setw(16) << q.fleet_name
                 << "| Requested " << setw(6) << q.requested_tons
                 << "| Allocated " << rec.allocated << " t (" << rec.status << ")\n";
            records[batch_idx[f]] = rec;
        }

        say("\nPost-allocation Shoal Status:");
        for (size_t i = 0; i < shoals.size(); i++) {
            double harvested = shoals[i].initial_tons - shoals[i].remaining_tons;
            double pct = (shoals[i].initial_tons > 0) ? (harvested / shoals[i].initial_tons * 100.0) : 0.0;
            say("      Shoal " + to_string(i + 1) + ": " +
                to_string((int)shoals[i].remaining_tons) + "/" + to_string((int)shoals[i].initial_tons) +
                " t remaining (" + to_string((int)pct) + "% harvested, cap " +
                to_string((int)(max_depletion_ratio * 100)) + "%)");
        }

        say("\nRemaining stock grid:");
        for (int r = 0; r < R; r++) {
            string line;
            for (int c = 0; c < C; c++) {
                line += to_string((int)stock->get_sum(idx(r, c), idx(r, c))) + "\t";
            }
            say(line);
        }
    }

    void exportJSON(const string& path) {
        ofstream o(path);
        if (!o.is_open()) throw runtime_error("Cannot open output file: " + path);
        o << fixed << setprecision(2);

        o << "{\"rows\":" << R << ",\"cols\":" << C << ",\"totalFlow\":" << total_flow << ",\"initial\":[";
        for (int r = 0; r < R; r++) {
            o << (r ? "," : "") << "[";
            for (int c = 0; c < C; c++) o << (c ? "," : "") << initial_map[r][c];
            o << "]";
        }

        o << "],\"final\":[";
        for (int r = 0; r < R; r++) {
            o << (r ? "," : "") << "[";
            for (int c = 0; c < C; c++) o << (c ? "," : "") << stock->get_sum(idx(r, c), idx(r, c));
            o << "]";
        }

        o << "],\"shoalId\":[";
        for (int r = 0; r < R; r++) {
            o << (r ? "," : "") << "[";
            for (int c = 0; c < C; c++) o << (c ? "," : "") << shoal_id[r][c];
            o << "]";
        }

        o << "],\"shoals\":[";
        for (size_t i = 0; i < shoals.size(); i++) {
            o << (i ? "," : "") << "{\"cells\":" << shoals[i].cells
              << ",\"tons\":" << shoals[i].remaining_tons
              << ",\"initial\":" << shoals[i].initial_tons
              << ",\"cap\":" << shoals[i].max_harvest << "}";
        }

        o << "],\"bans\":[";
        for (size_t i = 0; i < ban_list.size(); i++) {
            o << (i ? "," : "") << "{\"s\":" << ban_list[i].start_day
              << ",\"e\":" << ban_list[i].end_day
              << ",\"reason\":\"" << esc(ban_list[i].reason) << "\"}";
        }

        o << "],\"requests\":[";
        for (size_t i = 0; i < records.size(); i++) {
            auto& x = records[i];
            auto& q = x.req;
            o << (i ? "," : "") << "{\"id\":\"" << esc(q.id)
              << "\",\"fleet\":\"" << esc(q.fleet_name)
              << "\",\"box\":[" << q.start_x << "," << q.start_y << "," << q.end_x << "," << q.end_y
              << "],\"days\":[" << q.start_day << "," << q.end_day
              << "],\"tons\":" << q.requested_tons
              << ",\"urgency\":" << q.urgency_score
              << ",\"status\":\"" << x.status
              << "\",\"reason\":\"" << esc(x.reason)
              << "\",\"available\":" << x.available
              << ",\"allocated\":" << x.allocated
              << ",\"zones\":[";
            for (size_t k = 0; k < x.zones.size(); k++) {
                o << (k ? "," : "") << "[" << x.zones[k][0] << "," << x.zones[k][1] << "," << x.zones[k][2] << "]";
            }
            o << "]}";
        }

        o << "],\"log\":[";
        for (size_t i = 0; i < event_log.size(); i++) {
            o << (i ? "," : "") << "\"" << esc(event_log[i]) << "\"";
        }
        o << "],\"frames\":[";
        for (size_t i = 0; i < frames.size(); i++) {
            o << (i ? "," : "") << "{\"label\":\"" << esc(frames[i].label) << "\",\"layers\":[";
            for (size_t k = 0; k < frames[i].layers.size(); k++) {
                auto& L = frames[i].layers[k];
                o << (k ? "," : "") << "{\"ri\":" << L.ri << ",\"cells\":{";
                for (size_t m = 0; m < L.cells.size(); m++)
                    o << (m ? "," : "") << "\"" << L.cells[m].first << "\":" << L.cells[m].second;
                o << "}}";
            }
            o << "]}";
        }
        o << "]}";
    }
};

// ============================================================================
// MAIN DRIVER
// ============================================================================
static void runFromFile(const string& inPath, const string& outPath) {
    ifstream in(inPath);
    if (!in) throw runtime_error("Cannot open input file: " + inPath);

    int R, C;
    double cap;
    if (!(in >> R >> C >> cap) || R < 1 || C < 1) throw runtime_error("Bad input header.");

    vector<vector<double>> grid(R, vector<double>(C));
    for (auto& row : grid)
        for (auto& v : row) in >> v;

    AquaFlowEngine engine(R, C, cap);

    int B;
    in >> B;
    for (int i = 0; i < B; i++) {
        int s, e;
        string reason;
        in >> s >> e;
        getline(in >> ws, reason);
        engine.registerBan(s, e, reason);
    }

    engine.initializeOceanState(grid);

    int F;
    in >> F;
    for (int i = 0; i < F; i++) {
        FleetRequest q;
        getline(in >> ws, q.id);
        getline(in >> ws, q.fleet_name);
        in >> q.start_x >> q.start_y >> q.end_x >> q.end_y >> q.start_day >> q.end_day
           >> q.requested_tons >> q.urgency_score;
        if (!in) throw runtime_error("Bad fleet data in input file.");
        engine.submitRequest(q);
    }

    engine.processAndAllocate();
    engine.exportJSON(outPath);
}

int main(int argc, char** argv) {
    try {
        if (argc == 5 && string(argv[1]) == "--input" && string(argv[3]) == "--output") {
            runFromFile(argv[2], argv[4]);
            return 0;
        }
        AquaFlowEngine engine(5, 5);
        cout << "==== AQUAFLOW : MARINE RESOURCE ALLOCATION SYSTEM ====\n";

        engine.registerBan(150, 180, "Monsoon Breeding Season");
        engine.registerBan(300, 320, "Coral Spawning Window");

        engine.initializeOceanState({
            {0,   0,   50,  50,  0},
            {0,   0,   100, 50,  0},
            {0,   0,   0,   0,   0},
            {200, 300, 0,   0,   0},
            {150, 200, 0,   0,   0}
        });

        // ID, Company, StartRow, StartCol, EndRow, EndCol, StartDay, EndDay, Tons, Urgency (1-based coords)
        engine.submitRequest({"REQ-01", "Tata Seafood",     4, 1, 5, 2, 100, 120, 400.0, 5});
        engine.submitRequest({"REQ-02", "Emergency Fleet",  4, 1, 5, 2, 100, 120, 600.0, 10});
        engine.submitRequest({"REQ-03", "Illegal Fishers",  1, 1, 3, 3, 160, 190,  50.0, 2});
        engine.submitRequest({"REQ-04", "Coast Catch Co.",  1, 3, 2, 4, 100, 110, 150.0, 7});
        engine.submitRequest({"REQ-05", "Bad Bounds Co.",   6, 1, 5, 2, 100, 110, 150.0, 1});

        engine.processAndAllocate();
        engine.exportJSON("aquaflow_output.json");

        cout << "\nWrote aquaflow_output.json (open dashboard.html and load it)\n";
    } catch (const exception& e) {
        cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
