#include <osmium/osm.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/handler.hpp>
#include <osmium/index/map/all.hpp>

#include <osmium/io/pbf_input.hpp> // IWYU pragma: export
#include <osmium/io/xml_input.hpp> // IWYU pragma: export
#include <osmium/io/o5m_input.hpp> // IWYU pragma: export
#include <osmium/io/file.hpp>
#include <osmium/visitor.hpp>

#include <boost/timer/timer.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>


#include <unordered_map>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cinttypes>

#include "common.hpp"
#include "server_http.hpp"
#include "vector_tile.hpp"
#include "web_mercator.hpp"
#include "tile.hpp"
#include "merge.hpp"



typedef osmium::index::map::Dummy<osmium::unsigned_object_id_type, osmium::Location> index_neg_type;
typedef osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type, osmium::Location> index_pos_type;
//typedef osmium::index::map::DenseMemArray<osmium::unsigned_object_id_type, osmium::Location> index_pos_type;
typedef osmium::handler::NodeLocationsForWays<index_pos_type, index_neg_type> location_handler_type;

typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;

void usage(char* name) {
    std::cerr << "Usage: " << name << " <map.pbf> <freeflow.csv> <current.csv>" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Starts up a tileserver that can generate traffic vector tiles." << std::endl;
    std::cerr << "  map.pbf  - the map you want to serve tiles from" << std::endl;
    std::cerr << "  freeflow.csv  - A CSV file containing nodeA,nodeB,speed with the free flow speeds of roads " << std::endl;
    std::cerr << "  current.csv  - A CSV file containing nodeA,nodeB,speed with the current speeds of roads " << std::endl;
    std::cerr << "  config.yaml  - A simple configuration file that defines join thresholds and road heirarchies" << std::endl;

}

struct Extractor final : osmium::handler::Handler {

    std::vector<rtree_value_t> &segments;
    const boost::geometry::strategy::distance::haversine<double> haversine;

    Extractor (std::vector<rtree_value_t> & segments_) : segments(segments_), haversine(util::web_mercator::detail::EARTH_RADIUS_WGS84) {}

    static const bool usable(const osmium::Way &way)
    {
        const char *highway = way.tags().get_value_by_key("highway");
        // Check to see if it's an interesting type of way.  We're only
        // interested in roads at the moment.
        return highway != nullptr &&
            (std::strcmp(highway, "motorway") == 0 || std::strcmp(highway, "motorway_link") == 0 ||
             std::strcmp(highway, "trunk") == 0 || std::strcmp(highway, "trunk_link") == 0 ||
             std::strcmp(highway, "primary") == 0 || std::strcmp(highway, "primary_link") == 0 ||
             std::strcmp(highway, "secondary") == 0 || std::strcmp(highway, "secondary_link") == 0 ||
             std::strcmp(highway, "tertiary") == 0 || std::strcmp(highway, "tertiary_link") == 0 ||
             std::strcmp(highway, "residential") == 0 || std::strcmp(highway, "living_street") == 0 ||
             std::strcmp(highway, "unclassified") == 0 || std::strcmp(highway, "service") == 0 ||
             std::strcmp(highway, "ferry") == 0 || std::strcmp(highway, "movable") == 0 ||
             std::strcmp(highway, "shuttle_train") == 0 || std::strcmp(highway, "default") == 0);
    }

    static const int get_minzoom(const osmium::Way &way) {
        const char *highway = way.tags().get_value_by_key("highway");
        if (highway == nullptr) return -1;
        if ( std::strcmp(highway, "motorway") == 0 ) return 4;
        if ( std::strcmp(highway, "trunk") == 0 || std::strcmp(highway, "primary") == 0 ) return 9;
        if ( std::strcmp(highway, "primary_link") == 0 || std::strcmp(highway, "motorway_link") == 0 || std::strcmp(highway, "trunk_link") == 0) return 11;
        if ( std::strcmp(highway, "secondary") == 0 || std::strcmp(highway, "secondary_link") == 0) return 13;
        if ( std::strcmp(highway, "tertiary") == 0 || std::strcmp(highway, "tertiary_link") == 0) return 14;
        if ( std::strcmp(highway, "residential") == 0 || std::strcmp(highway, "living_street") == 0) return 15;
        if ( std::strcmp(highway, "service") == 0 || std::strcmp(highway, "unclassified") == 0) return 16;
        return -1;
    }

    void way(const osmium::Way& way) {

        // Figure out which directions we need to process
        const char *oneway = way.tags().get_value_by_key("oneway");
        bool forward = (!oneway || std::strcmp(oneway, "yes") == 0 || std::strcmp(oneway, "no") == 0);
        bool reverse = (!oneway || std::strcmp(oneway, "-1") == 0 || std::strcmp(oneway, "no") == 0);

        // Check for implied oneway on motorways when it's not specified
        if (!oneway) {
            const char *highway = way.tags().get_value_by_key("highway");
            if (highway && std::strcmp(highway,"motorway") == 0)
            {
                forward = true;
                reverse = false;
            }
        }

        const auto minzoom = get_minzoom(way);

        if (minzoom > -1 && way.nodes().size() > 1 && (forward || reverse))
        {
            const auto s = way.nodes().size();
            for (std::remove_const_t<decltype(s)> i{0}; i<s-1; ++i)
            {
                const auto a = way.nodes()[i];
                const auto b = way.nodes()[i+1];

                // Throw out self-loops and invalid noderefs
                if (a.ref() == b.ref()) continue;
                if (!a.location().valid()) continue;
                if (!b.location().valid()) continue;

                segments.push_back({wgs84_segment_t{{ a.location().lon(), a.location().lat(), minzoom }, { b.location().lon(), b.location().lat(), minzoom }}, {a.ref(), b.ref()}});

            }
        }

    }
};

int main(int argc, char* argv[])
{
    std::shared_ptr<line_rtree_t> rtree_ptr;

    std::cerr << "Parsing " << argv[1] << std::endl;
    try
    {
        std::vector<rtree_value_t> segments;

        osmium::io::File pbfFile{argv[1]};

        osmium::io::Reader fileReader(pbfFile, osmium::osm_entity_bits::way | osmium::osm_entity_bits::node);
        Extractor extractor(segments);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        const auto temp_name = std::tmpnam(nullptr);
#pragma clang diagnostic pop
        int fd = open(temp_name, O_RDWR | O_CREAT, 0666);
        if (fd == -1)
        {
            throw std::runtime_error(strerror(errno));
        }

        // unlinking before we close the file descriptor means the file
        // will get automatically deleted when our program exits and
        // releases the file descriptor
        unlink(temp_name);
        index_pos_type index_pos{fd};
        index_neg_type index_neg;
        location_handler_type location_handler(index_pos, index_neg);
        location_handler.ignore_errors();
        osmium::apply(fileReader, location_handler, extractor);

        std::cerr << "Starting RTree construction" << std::endl;
        rtree_ptr = std::make_shared<line_rtree_t>(segments);
        std::cerr << "Loaded " << segments.size() << " into the rtree" << std::endl;
    }
    catch (const osmium::xml_error &e)
    {
        std::cerr << "Error: xml parse error in " << argv[1] << ": " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (const osmium::io_error &e)
    {
        std::cerr << "Error: error reading file " << argv[1] << ": " << e.what() << std::endl;
        return EXIT_FAILURE;
    }


    HttpServer server(8080,1);

    server.resource["^/tile/([0-9]+)/([0-9]+)/([0-9]+).mvt"]["GET"] = [&rtree_ptr](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {

        std::thread work_thread([&rtree_ptr, request, response] {

        int x = std::stoi(request->path_match[1]);
        int y = std::stoi(request->path_match[2]);
        int z = std::stoi(request->path_match[3]);

        // TODO: validate the x/y/z


    	double min_lon, min_lat, max_lon, max_lat;

        util::web_mercator::xyzToWGS84( x, y, z, min_lon, min_lat, max_lon, max_lat);

        wgs84_box_t search_box({min_lon, min_lat, 0}, {max_lon, max_lat, z});
        std::vector<rtree_value_t> results;
        rtree_ptr->query(boost::geometry::index::intersects(search_box), std::back_inserter(results));

        double min_merc_x, min_merc_y, max_merc_x, max_merc_y;
        util::web_mercator::xyzToMercator(x, y, z, min_merc_x, min_merc_y, max_merc_x, max_merc_y);
        util::tile::mercator_box_t tile_bbox({min_merc_x, min_merc_y}, {max_merc_x, max_merc_y});

        /* GEOJSON
        std::stringstream s;
        s << "{\"type\":\"FeatureCollection\",\"features\":[\n";

        bool first = true;
        char lon1[16];
        char lat1[16];
        char lon2[16];
        char lat2[16];
        for (const auto &result : results) {
            if (first) { first = false; } else { s << ",\n"; }


            std::snprintf(lon1, sizeof(lon1), "%.7f", result.first.first.get<0>());
            std::snprintf(lat1, sizeof(lat1), "%.7f", result.first.first.get<1>());
            std::snprintf(lon2, sizeof(lon2), "%.7f", result.first.second.get<0>());
            std::snprintf(lat2, sizeof(lat2), "%.7f", result.first.second.get<1>());

            s << "{\"type\":\"Feature\",\"properties\":[],\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[" << lon1 << "," << lat1 << "],[" << lon2 << "," << lat2 << "]]}}";
        }
        s << "]}";

        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << s.str().length() << "\r\n\r\n" << s.str();
        */


        /**
         * Now, iterate over all the segments, and join them into longer
         * lines, if possible.  This means fewer features on the tile
         * and a smaller tile size to encode.
         * We also take this opportunity to eliminate segments of 0
         * length (where they form part of a longer line).
         **/

        tile_line_vector lines;
        coordinate_line_map starts;
        coordinate_line_map ends;

        for (const auto &segment : results) {
            std::int32_t start_x = 0;
            std::int32_t start_y = 0;
            const auto tile_line = util::tile::segmentToTileLine(segment.first, tile_bbox);

            if (tile_line.size() != 2) continue;

            merge(tile_line, lines, starts, ends);

        }

        std::string pbf_buffer;
        {

            protozero::pbf_writer tile_writer{pbf_buffer};
            {
                // Add a layer object to the PBF stream.  3=='layer' from the vector tile spec (2.1)
                protozero::pbf_writer line_layer_writer(tile_writer, util::vector_tile::LAYER_TAG);
                line_layer_writer.add_uint32(util::vector_tile::VERSION_TAG, 2); // version
                // Field 1 is the "layer name" field, it's a string
                line_layer_writer.add_string(util::vector_tile::NAME_TAG, "geom"); // name
                // Field 5 is the tile extent.  It's a uint32 and should be set to 4096
                // for normal vector tiles.
                line_layer_writer.add_uint32(util::vector_tile::EXTENT_TAG,
                                             util::vector_tile::EXTENT); // extent
                std::int32_t id = 1;
                for (const auto & startlist : starts) {
                    for (const auto &start : startlist.second) {
                        const auto &line = lines[start];
                        std::int32_t start_x = 0;
                        std::int32_t start_y = 0;
                        protozero::pbf_writer feature_writer(line_layer_writer, util::vector_tile::FEATURE_TAG);
                        feature_writer.add_enum(util::vector_tile::GEOMETRY_TAG, util::vector_tile::GEOMETRY_TYPE_LINE);
                        feature_writer.add_uint64(util::vector_tile::ID_TAG, id++);
                        {
                            protozero::packed_field_uint32 geometry(feature_writer, util::vector_tile::FEATURE_GEOMETRIES_TAG);
                            util::tile::encodeLinestring(line, geometry, start_x, start_y);
                        }
                    }
                }
                /*
                std::int32_t id = 1;
                for (const auto &segment : results) {
                    std::int32_t start_x = 0;
                    std::int32_t start_y = 0;
                    const auto tile_line = util::tile::segmentToTileLine(segment.first, tile_bbox);
                    // Only encode if there's actually geometry, the VT 2+ spec requires this.
                    if (tile_line.size() > 1) {
                        protozero::pbf_writer feature_writer(line_layer_writer, util::vector_tile::FEATURE_TAG);
                        feature_writer.add_enum(util::vector_tile::GEOMETRY_TAG, util::vector_tile::GEOMETRY_TYPE_LINE);
                        // TODO: should ID be globally unique?
                        feature_writer.add_uint64(util::vector_tile::ID_TAG, x*y+id++);
                        {
                            protozero::packed_field_uint32 geometry(feature_writer, util::vector_tile::FEATURE_GEOMETRIES_TAG);
                            util::tile::encodeLinestring(tile_line, geometry, start_x, start_y);
                        }
                    }
                }
                */
            }
        }

        //std::cout << "GET /" << x << "/" << y << "/" << z << ".mvt - " << pbf_buffer.size() << " bytes\n";

        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << pbf_buffer.size() << "\r\n";
        *response << "Content-Type: application/vnd.mapbox-vector-tile\r\n";
        *response << "Access-Control-Allow-Origin: *\r\n\r\n";
        *response << pbf_buffer;
        });
        work_thread.detach();
    };

    server.default_resource["GET"]=[](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {
        std::string content="Not found";
        *response << "HTTP/1.1 404 Not Found\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
    };

    std::thread server_thread([&server](){
        //Start server
        server.start();
    });

    server_thread.join();

    return 0;
}
