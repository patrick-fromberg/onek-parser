module;

#include <map>
#include <fstream>
#include <string_view>
#include <vector>

export module ast_graph;

export namespace onek {

    ushort uuid = 0;//todo: put somewhere else

    class ast_graph {
        public:
        std::map<ushort, std::string> vertices_s;// todo
        std::map<ushort, ushort> vertices_i;     // todo
        std::vector<std::pair<ushort, ushort>> edges;
        std::ofstream file;

        explicit ast_graph(char const *filename) {
            file.open(filename);
            file << "digraph {\n";
        }

        ~ast_graph() {
            for (auto const &e : edges) {
                auto xxx = vertices_s[e.second];
                file << "    \"" << vertices_s[e.first] << "(" << vertices_i[e.first] << ")\" [label=\"" << vertices_s[e.first] << "\"] ";
                file << "    \"" << vertices_s[e.second] << "(" << vertices_i[e.second] << ")\" [label=\"" << vertices_s[e.second] << "\"] ";
                file << "    \"" << vertices_s[e.first] << "(" << vertices_i[e.first] << ")\"";
                file << " -> ";
                file << "\"" << vertices_s[e.second] << "(" << vertices_i[e.second] << ")\"\n";
            }
            file << "}\n";
            file.close();
        }

        void add_edge(std::string_view parent_name, ushort parent_id, std::string_view child_name, std::string_view info, ushort child_id) {
            if (vertices_s[parent_id].empty())
                vertices_s[parent_id] = parent_name;
            if (vertices_s[child_id].empty())
                vertices_s[child_id] = std::string(child_name) + "\\n" + std::string(info);
            vertices_i[parent_id] = parent_id;//   ++counter;
            vertices_i[child_id] = child_id;  // ++counter;
            edges.emplace_back(parent_id, child_id);
        }
    };
}
