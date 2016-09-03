#pragma once

#include "tile.hpp"
#include <unordered_map>
#include <vector>
#include <cstdint>

typedef std::vector<util::tile::tile_linestring_t> tile_line_vector;
typedef std::unordered_map<util::tile::tile_point_t, std::vector<std::size_t>, util::tile::tile_point_hash, util::tile::tile_point_equal> coordinate_line_map;

void merge(const util::tile::tile_linestring_t &tile_line, tile_line_vector &lines, coordinate_line_map &starts, coordinate_line_map &ends) {

    const auto endmatch = ends.find(tile_line.front());
    const auto startmatch = starts.find(tile_line.back());

    // Joining two existing lines
    if (endmatch != ends.end() && startmatch != starts.end())
    {
        // Do the append
        // We just pick the first value we find (TODO: a heuristic to optimize line lengths?)
        const auto firstline = endmatch->second.back();
        const auto secondline = startmatch->second.back();

        // If this won't create a self-join (we're only handling lines here, not polygons
        if (firstline != secondline)
        {
            //std::cout << "Copying from " << secondline << " to  " << firstline << " and size is  " << lines.size() << std::endl;
            lines[firstline].insert(lines[firstline].end(), lines[secondline].begin(), lines[secondline].end());

            // Remove the item we found
            endmatch->second.pop_back();
            startmatch->second.pop_back();
            if (endmatch->second.empty()) { ends.erase(endmatch->first); }
            if (startmatch->second.empty()) { starts.erase(startmatch->first); }

            // Make sure the end of the second line is no longer linked
            const auto tmpsecond = ends.find(lines[firstline].back());
            tmpsecond->second.erase(std::remove(tmpsecond->second.begin(), tmpsecond->second.end(), secondline), tmpsecond->second.end());

            // The endpoint of the second line now belongs to the first line,
            // so make sure the last coordinate is in the ends index.
            auto result = ends.emplace(lines[firstline].back(), std::vector<std::size_t>{firstline});
            if (!result.second) { result.first->second.push_back(firstline); }

            // No need to delete the line, nobody refers to it any more
            //lines.erase(lines.begin()+secondline);

            return;
        }
    }

    // Appending to the end of another line
    // Note: we mail end up here if the previous step detected a join, but deicded it would
    // create a loop.
    if (endmatch != ends.end())
    {
        // If there is already a line, and this line has 0 length, discard this line
        if (util::tile::tile_point_equal()(tile_line.front(), tile_line.back())) return;
        const auto endindex = endmatch->second.back();
        const auto lastpt = tile_line.back();
        // Remove the old endpoint
        endmatch->second.pop_back();
        if (endmatch->second.empty()) { ends.erase(endmatch->first); }

        // Add the coordinate to the vector
        lines[endindex].emplace_back(lastpt.get<0>(), lastpt.get<1>());

        // Ensure the new end coordinate links to the right vector
        auto result = ends.emplace(lines[endindex].back(), std::vector<std::size_t>{endindex});
        if (!result.second) { result.first->second.push_back(endindex); }

        return;
    }

    // Prepending to an existing line
    if (startmatch != starts.end()) {
        // If there is already a line, and this line has 0 length, discard this line
        if (util::tile::tile_point_equal()(tile_line.front(), tile_line.back())) return;
        const auto startindex = startmatch->second.back();
        const auto firstpt = tile_line.front();

        // Remove the old startpoint
        startmatch->second.pop_back();
        if (startmatch->second.empty()) { starts.erase(startmatch->first); }

        // Prepend the new coordinate
        lines[startindex].emplace(lines[startindex].begin(), firstpt.get<0>(), firstpt.get<1>());

        // Ensure the new start coordinate
        auto result = starts.emplace(lines[startindex].front(), std::vector<std::size_t>{startindex});
        if (!result.second) { result.first->second.push_back(startindex); }

        return;
    }

    // Nothing to join to, insert a new line
    lines.push_back(tile_line);
    const auto lineid = lines.size() - 1;
    auto startresult = starts.emplace(tile_line.front(), std::vector<std::size_t>{ lineid });
    if (!startresult.second) { startresult.first->second.push_back(lineid); }
    auto endresult = ends.emplace(tile_line.back(), std::vector<std::size_t>{ lines.size() -1 });
    if (!endresult.second) { endresult.first->second.push_back(lineid); }
}
