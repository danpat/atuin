#include "common.hpp"
#include "merge.hpp"

void dump(const tile_line_vector &lines, const coordinate_line_map &starts, const coordinate_line_map &ends) {
    std::clog << "----------" << std::endl;
    for (const auto & startlist : starts) {
        std::clog << "  Lines starting at  " << startlist.first.get<0>() << "," << startlist.first.get<1>() << std::endl;
        for (const auto & start : startlist.second) {
            const auto &line = lines[start];

            std::clog << "    Line: " << start << ": ";

            for (const auto &pt : line) {
                std::clog << pt.get<0>() << "," << pt.get<1>() << " ";
            }
            std::clog << std::endl;
        }
    }

    for (const auto & endlist : ends) {
        std::clog << "  Lines ending at  " << endlist.first.get<0>() << "," << endlist.first.get<1>() << std::endl;
        for (const auto & end : endlist.second) {
            const auto &line = lines[end];

            std::clog << "    Line: " << end << ": ";

            for (const auto &pt : line) {
                std::clog << pt.get<0>() << "," << pt.get<1>() << " ";
            }
            std::clog << std::endl;
        }
    }
}

util::tile::tile_linestring_t makesegment(int x1, int y1, int x2, int y2) {
    std::clog << "Making segment " << x1 << "," << y1 << " - " << x2 << "," << y2 << std::endl;
    util::tile::tile_linestring_t segment;
    segment.emplace_back(x1,y1);
    segment.emplace_back(x2,y2);
    return segment;
}

void test1() {
    tile_line_vector lines;
    coordinate_line_map starts;
    coordinate_line_map ends;

    merge(makesegment(1,1,2,2), lines, starts, ends);
    dump(lines, starts, ends);
    merge(makesegment(2,2,3,3), lines, starts, ends);
    dump(lines, starts, ends);
    merge(makesegment(3,3,4,4), lines, starts, ends);
    dump(lines, starts, ends);
    merge(makesegment(7,7,4,4), lines, starts, ends);
    dump(lines, starts, ends);
    merge(makesegment(5,5,6,6), lines, starts, ends);
    dump(lines, starts, ends);
    merge(makesegment(0,0,1,1), lines, starts, ends);
    dump(lines, starts, ends);
    merge(makesegment(4,4,5,5), lines, starts, ends);
    dump(lines, starts, ends);
    merge(makesegment(4,4,5,5), lines, starts, ends);
    dump(lines, starts, ends);
}

void test2() {
    tile_line_vector lines;
    coordinate_line_map starts;
    coordinate_line_map ends;

    merge(makesegment(1,0,2,0), lines, starts, ends);
    dump(lines, starts, ends);
    merge(makesegment(2,0,3,0), lines, starts, ends);
    dump(lines, starts, ends);

    merge(makesegment(3,1,2,1), lines, starts, ends);
    dump(lines, starts, ends);

    merge(makesegment(2,1,1,1), lines, starts, ends);
    dump(lines, starts, ends);

    merge(makesegment(1,1,1,0), lines, starts, ends);
    dump(lines, starts, ends);

    merge(makesegment(3,0,3,1), lines, starts, ends);
    dump(lines, starts, ends);
}

int main(int argc, char* argv[])
{

    //test1();
    test2();
}
