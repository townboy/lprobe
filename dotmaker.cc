#include <iostream>
#include <string>
#include <regex>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdlib>
#include <unistd.h>
#include <getopt.h>

namespace lprobe {

bool output_text = false;
bool output_image = false;
bool all_function = false;

//size=\"30,40\";\n
const std::string image_header = 
"digraph callgraph {\n\
    margin=0.1;\n\
    nodesep=0.8;\n\
    ranksep=0.5;\n\
    rankdir=LR;\n\
    edge[arrowsize=1.0, arrowhead=vee];\n\
    node[shape=record, fontsize=20, width=1, height=1, fixedsize=false];\n";

std::string image_tail = "}";

struct Data {
    int tid;
    std::string thread_name;
    std::string func_name;
    std::string type;
    std::vector<std::string> backtraces;
    bool broken_backtrace;
    int time;
};

class Node {
public:
    Node(bool pp, std::string dn): probe_point(pp), display_name(dn) { }
    bool is_probe_point() { return probe_point; }
    std::string get_display_name() { return display_name; }
    void add_next(int nodeid) { nexts.push_back(nodeid); }
    int get_last_next() { 
        return nexts.empty() ? -1 : nexts.back();
    }

    std::vector<int> &get_nexts() {
        return nexts;
    }

private: 
    bool probe_point;
    std::vector<int> nexts;
    std::string display_name;
};

std::string update_human_name(std::string str) {
    std::regex escape("<|>|\\{|\\}");
    auto ret = std::regex_replace(str, escape, "\\$&");
    auto pos = ret.rfind("(");
    if (pos != std::string::npos) {
        return ret.substr(0, pos);
    }
    return ret;
}

class Graph {
public:
    void handle(Data d) {
        /*
        std::cout << d.thread_name << " " << d.func_name << std::endl;
        for (auto bt : d.backtraces) {
            std::cout << bt << std::endl;
        }
        std::cout << std::endl;
        */

        if (d.type == "call") {
            if (!d.broken_backtrace || all_function) {
                int nodeid = new_node(true, std::to_string(d.time) + " " + d.backtraces[0]);
                get_node(find_head(d, nodeid)).add_next(nodeid);
            }
        }
        else if (d.type == "return") {
            auto &p = path[d.tid];
            if (p.size() > 1) {
                p.pop_back();
            }
            for (auto i = p.size() - 1; i > 0; i --) {
                int nodeid = p.back();
                if (get_node(nodeid).is_probe_point()) {
                    break;
                }
                p.pop_back();
            }
        }
        else if (d.type == "inline") {
            if (!d.broken_backtrace || all_function) {
                int nodeid = new_node(true, std::to_string(d.time) + " " + d.func_name);
                get_node(find_head(d, -1)).add_next(nodeid);
            }
        }
        else {
            std::cout << "type error" << std::endl;
        }
    }

    int find_head(Data d, int extend_node) {
        if (path.find(d.tid) == path.end()) {
            int nodeid =  new_node(false, std::to_string(d.tid) + " " + d.thread_name);
            path[d.tid].push_back(nodeid);
        }

        auto &p = path[d.tid];
        int head = p.back();
        
        if (d.broken_backtrace) {
            if (extend_node != -1) {
                p.push_back(extend_node);
            }
            return head;
        }

        auto bt = d.backtraces;
        if (d.type == "call") {
            bt.erase(bt.begin());
        }

        for (int i = bt.size() - p.size(); i >= 0; i --) {
            int nid = get_node(head).get_last_next();
            if (nid == -1 || get_node(nid).get_display_name() != bt[i]) {
                nid = new_node(false, bt[i]);
                get_node(head).add_next(nid);
            }

            head = nid;
            if (extend_node != -1) {
                p.push_back(nid);
            }
        }
        if (extend_node != -1) {
            p.push_back(extend_node);
        }
        return head;
    }

    int new_node(bool pp, std::string dn) {
        nodes.emplace_back(pp, dn);
        return nodes.size() - 1;
    }

    Node& get_node(int nodeid) {
        if (nodeid >= nodes.size() || nodeid < 0) {
            std::cout << "no such node" << std::endl;
        }
        return nodes[nodeid];
    }

    void travel_text(int nodeid) {
        width += 2;
        for (auto i = 0; i < width; i ++) {
            std::cout << " ";
        }
        auto &n = get_node(nodeid);
        std::cout << "-> " << update_human_name(n.get_display_name());
        if (n.is_probe_point()) {
            std::cout << " probe";
        }
        std::cout << std::endl;
        for (auto next : n.get_nexts()) {
            travel_text(next);
        }

        for (auto i = 0; i < width; i ++) {
            std::cout << " ";
        }
        std::cout << "<- " << update_human_name(n.get_display_name()) << std::endl;
        width -= 2;
    }

    void travel_image(int nodeid, std::string label) {
        auto &nexts = get_node(nodeid).get_nexts();
        if (nexts.empty()) {
            return;
        }
        auto labels = format_nodes(nexts);
        std::cout << "    " << label << " -> " << labels.front() << " [dir=both, arrowtail=dot];\n";
        for (auto i = 0; i < labels.size() ; i++) {
            travel_image(nexts[i], labels[i]);
        }
    }

    std::vector<std::string> format_nodes(std::vector<int> nodeids) {
        std::string label = "n" + std::to_string(nodeids.front());
        std::cout << "    " << label << " [label=\"" ;
        std::vector<std::string> ret;
        for (auto i = 0; i < nodeids.size(); i ++) {
            std::cout << "<f" << std::to_string(i) << ">" << 
                update_human_name(get_node(nodeids[i]).get_display_name());
            if (i != nodeids.size() - 1) {
                std::cout << "|";
            }
            ret.push_back(label + ":f" + std::to_string(i));
        }
        std::cout << "\"];\n";
        return ret;
    }

    void output() {
        if (output_image) {
            std::cout << image_header;
        }
        // find every thread
        for (auto p : path) {
            if (output_text) {
                std::cout << p.first << " " << p.second.front() << std::endl;
                width = 0;
                travel_text(p.second.front());
            }
            else if (output_image) {
                //std::cout << p.first << " " << p.second.front() << std::endl;
                auto labels = format_nodes(std::vector<int>{p.second.front()});
                travel_image(p.second.front(), labels.front());
            }
        }
        if (output_image) {
            std::cout << image_tail;
        }
    }

private:
    std::vector<Node> nodes;
    std::unordered_map<int, std::vector<int>> path;
    std::unordered_map<int, int> starts;

    int width;
};

Graph g;

void input() {
    int count = 0;
    int broken_bt_count = 0;
    std::string line;
    std::regex idre("\\s+\\d+ (\\S+)\\((\\d+)\\): process\\(\\S+\\)\\.function\\(\"(\\S+)\\)\\.(\\w+)");
    //                    thread_name     tid                                      functionname      type
    
    std::regex btre(" \\S+ : (.+)\\+.+");
    std::regex ppre("(\\S+)@\\S+");
    std::unique_ptr<Data> d;
    while (std::getline(std::cin, line)) {
     //   std::cout << line << std::endl;
        std::smatch sm;
        if (std::regex_match(line, sm, idre)) {
            if (d) { //avaiable
                g.handle(*d);
            }
            d = std::make_unique<Data>();
            d->time = count;
            d->thread_name = sm[1];
            d->tid = std::atoi(sm[2].str().c_str());
            std::smatch funcname_sm;
            // function("foo@bar.cc")
            std::string fullfuncname = sm[3];
            if (std::regex_match(fullfuncname, funcname_sm, ppre)) {
                d->func_name = funcname_sm[1];
            }
            // function("foo")
            else {
                d->func_name = sm[3];
            }
            d->type = sm[4];
            d->broken_backtrace = false;
            count ++;
        }
        else if (std::regex_match(line, sm, btre)) {
                d->backtraces.push_back(sm[1]);
        }
        else {
            broken_bt_count ++;
            d->broken_backtrace = true;
        }
    }
    if (d) {
        g.handle(*d);
    }
    std::cerr << "broken_bt_count  " << broken_bt_count << std::endl;
}

void output() {
    g.output();
}

void usage() {
    std::cout << "usage: lprobe-dotmaker [options]\n" 
              << "  --help -h                  - print help\n"
              << "  --dot  -d                  - output dot format file\n"
              << "  --all_function  -a         - detect all functions\n"
              << "  --text -t                  - output human readable text\n";
    exit(0);
}

}

int main(int argc, char *argv[]) {
    struct option long_options[] = {
        {"dot", no_argument, 0, 'd'},
        {"text", no_argument, 0, 't'},
        {"all_function", no_argument, 0, 'a'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0},
    };
    int c;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "dtah", long_options, nullptr)) != -1) {
        switch (c) {
            case 't':
                lprobe::output_text = true;
                break;
            case 'd':
                lprobe::output_image = true;
                break;
            case 'a':
                lprobe::all_function = true;
                break;
            case 'h':
                lprobe::usage();
        }
    }
    if (!lprobe::output_text && !lprobe::output_image) {
        std::cerr << "choose image or text output" << std::endl;
        return 0;
    }
    lprobe::input();
    lprobe::output();
    return 0;
}
