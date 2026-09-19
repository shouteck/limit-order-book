#include "lob/replay.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace lob {

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, ',')) out.push_back(cur);
    return out;
}

Side side_of(const std::string& s) {
    if (s == "B") return Side::Buy;
    if (s == "S") return Side::Sell;
    throw std::runtime_error("bad side: " + s);
}

OrderType type_of(const std::string& s) {
    if (s == "L") return OrderType::Limit;
    if (s == "M") return OrderType::Market;
    throw std::runtime_error("bad order type: " + s);
}

TimeInForce tif_of(const std::string& s) {
    if (s == "G") return TimeInForce::GTC;
    if (s == "I") return TimeInForce::IOC;
    if (s == "F") return TimeInForce::FOK;
    throw std::runtime_error("bad tif: " + s);
}

} // namespace

std::vector<Event> read_events(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);

    std::vector<Event> events;
    std::string line;
    std::size_t lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        if (line.empty() || line[0] == '#') continue;
        auto f = split(line);
        try {
            Event e;
            if (f[0] == "A") {
                if (f.size() != 7) throw std::runtime_error("add needs 7 fields");
                e.kind      = EventKind::Add;
                e.order.id    = std::stoull(f[1]);
                e.order.side  = side_of(f[2]);
                e.order.type  = type_of(f[3]);
                e.order.tif   = tif_of(f[4]);
                e.order.price = std::stoll(f[5]);
                e.order.qty   = static_cast<Quantity>(std::stoul(f[6]));
            } else if (f[0] == "C") {
                if (f.size() != 2) throw std::runtime_error("cancel needs 2 fields");
                e.kind     = EventKind::Cancel;
                e.order.id = std::stoull(f[1]);
            } else if (f[0] == "M") {
                if (f.size() != 4) throw std::runtime_error("modify needs 4 fields");
                e.kind        = EventKind::Modify;
                e.order.id    = std::stoull(f[1]);
                e.order.price = std::stoll(f[2]);
                e.order.qty   = static_cast<Quantity>(std::stoul(f[3]));
            } else {
                throw std::runtime_error("unknown event kind: " + f[0]);
            }
            events.push_back(e);
        } catch (const std::exception& ex) {
            throw std::runtime_error(path + ":" + std::to_string(lineno) +
                                     ": " + ex.what());
        }
    }
    return events;
}

void write_events(const std::string& path, const std::vector<Event>& events) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write " + path);
    out << "# A,id,side(B/S),type(L/M),tif(G/I/F),price,qty | C,id | M,id,price,qty\n";
    for (const Event& e : events) {
        switch (e.kind) {
        case EventKind::Add:
            out << 'A' << ',' << e.order.id << ','
                << (e.order.side == Side::Buy ? 'B' : 'S') << ','
                << (e.order.type == OrderType::Limit ? 'L' : 'M') << ','
                << (e.order.tif == TimeInForce::GTC ? 'G'
                    : e.order.tif == TimeInForce::IOC ? 'I' : 'F') << ','
                << e.order.price << ',' << e.order.qty << '\n';
            break;
        case EventKind::Cancel:
            out << 'C' << ',' << e.order.id << '\n';
            break;
        case EventKind::Modify:
            out << 'M' << ',' << e.order.id << ',' << e.order.price << ','
                << e.order.qty << '\n';
            break;
        }
    }
}

} // namespace lob
