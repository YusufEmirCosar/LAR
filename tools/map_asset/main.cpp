
#include "geojson_source_reader.h"
#include "map_asset_writer.h"
#include "map_mesh_compiler.h"

#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    QTextStream errors(stderr);
    if (argc != 4) {
        errors << "Usage: lar-map-asset-compiler "
                  "<source.geojson> <output.larmap> "
                  "<manifest.json>\n";
        return 2;
    }

    const auto source = lar::map::tool::GeoJsonSourceReader::read(QString::fromLocal8Bit(argv[1]));
    if (!source.succeeded()) {
        errors << source.message << '\n';
        return 3;
    }
    auto compiled = lar::map::tool::MapMeshCompiler::compile(source.map);
    if (!compiled.succeeded()) {
        errors << compiled.message << '\n';
        return 4;
    }

    QString writeError;
    if (!lar::map::tool::MapAssetWriter::write(QString::fromLocal8Bit(argv[2]),
                                               QString::fromLocal8Bit(argv[3]), compiled.mesh,
                                               &writeError)) {
        errors << writeError << '\n';
        return 5;
    }
    return 0;
}
