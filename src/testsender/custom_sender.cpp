
#include "domain/statevalidator.h"
#include "infrastructure/mapping/json_mapping_repository.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QHostAddress>
#include <QTextStream>
#include <QTimer>
#include <QUdpSocket>

#include <functional>

namespace CustomScenario {

// EDIT HERE: Set every initial Plane and Target value used by your mapping.
void initialize(Plane *plane, Target *target, dlz::TelemetryInputs *dlzInputs = nullptr) {
    *plane = {};
    plane->location[0] = 0.71558;
    plane->location[1] = 0.50490;
    plane->location[2] = 3200.0;
    plane->euler[0] = 0.75;
    plane->velocity[0] = 720.0;

    *target = {};
    target->iz_pos[0] = 0.71570;
    target->iz_pos[1] = 0.50520;
    target->iz_pos[2] = 1.0;
    target->ir_pos[0] = 0.71575;
    target->ir_pos[1] = 0.50530;
    target->ir_pos[2] = 1.0;
    target->iz_theta1 = -0.55;
    target->iz_theta2 = 0.70;
    target->iz_r1 = 1800.0;
    target->iz_r2 = 18000.0;
    target->ir_r = 5'000'000.0;
    if (dlzInputs)
        *dlzInputs = {20.0, 0.0, 30000.0};
}

// EDIT HERE: Update state before each packet. Prefer formulas based on elapsedSeconds
// so the same command always generates the same sequence.
void update(int packetIndex, double elapsedSeconds, Plane *plane, Target *target,
            dlz::TelemetryInputs *dlzInputs = nullptr) {
    Q_UNUSED(packetIndex)
    target->time = elapsedSeconds;
    plane->location[1] = 0.50490 + 0.00001 * elapsedSeconds;
    Q_UNUSED(dlzInputs)
}

// EDIT HERE: Change encoded bytes after validation to exercise malformed-packet handling.
void mutateDatagram(int packetIndex, QByteArray *datagram) {
    Q_UNUSED(packetIndex)
    Q_UNUSED(datagram)
}

} // namespace CustomScenario

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("lar-custom-test-sender"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Send an editable custom Plane + Target scenario over UDP."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption hostOption({QStringLiteral("H"), QStringLiteral("host")},
                                        QStringLiteral("Destination IP address."),
                                        QStringLiteral("address"), QStringLiteral("127.0.0.1"));
    const QCommandLineOption portOption({QStringLiteral("p"), QStringLiteral("port")},
                                        QStringLiteral("Destination UDP port."),
                                        QStringLiteral("port"), QStringLiteral("45454"));
    const QCommandLineOption countOption({QStringLiteral("c"), QStringLiteral("count")},
                                         QStringLiteral("Number of packets to send."),
                                         QStringLiteral("count"), QStringLiteral("100"));
    const QCommandLineOption intervalOption({QStringLiteral("i"), QStringLiteral("interval")},
                                            QStringLiteral("Milliseconds between packets."),
                                            QStringLiteral("milliseconds"), QStringLiteral("100"));
    const QCommandLineOption mapOption({QStringLiteral("m"), QStringLiteral("map")},
                                       QStringLiteral("JSON field mapping used to encode packets."),
                                       QStringLiteral("path"));
    parser.addOptions({hostOption, portOption, countOption, intervalOption, mapOption});
    parser.process(application);

    bool portOk = false;
    bool countOk = false;
    bool intervalOk = false;
    const int port = parser.value(portOption).toInt(&portOk);
    const int packetCount = parser.value(countOption).toInt(&countOk);
    const int intervalMs = parser.value(intervalOption).toInt(&intervalOk);
    QHostAddress destination;
    PacketMapping mapping;
    JsonMappingRepository mappingRepository;
    QString mappingError;
    if (parser.value(mapOption).isEmpty() ||
        !mappingRepository.loadFile(parser.value(mapOption), &mapping, &mappingError)) {
        QTextStream(stderr) << "Invalid mapping: " << mappingError << '\n';
        return 2;
    }
    if (!destination.setAddress(parser.value(hostOption)) || !portOk || port < 1 || port > 65535 ||
        !countOk || packetCount < 1 || !intervalOk || intervalMs < 1) {
        QTextStream(stderr) << "Invalid host, port, count, or interval.\n";
        return 2;
    }

    DecodedState state{};
    CustomScenario::initialize(&state.plane, &state.target, &state.dlzInputs);

    QUdpSocket socket;
    const QHostAddress localAddress =
        destination.protocol() == QHostAddress::IPv6Protocol ? QHostAddress::AnyIPv6
                                                              : QHostAddress::AnyIPv4;
    if (!socket.bind(localAddress, 0)) {
        QTextStream(stderr) << "UDP bind failed: " << socket.errorString() << '\n';
        return 1;
    }
    QTimer timer;
    int sent = 0;
    const std::function<void()> sendPacket = [&] {
        const double elapsedSeconds = double(sent) * double(intervalMs) / 1000.0;
        CustomScenario::update(sent, elapsedSeconds, &state.plane, &state.target, &state.dlzInputs);

        state.availableFields = mapping.availableFields();
        QString validationError;
        if (!StateValidator::validate(state, &validationError)) {
            QTextStream(stderr) << "Custom scenario generated invalid state: " << validationError
                                << '\n';
            application.exit(1);
            return;
        }

        QByteArray datagram = mapping.encode(state);
        CustomScenario::mutateDatagram(sent, &datagram);
        if (socket.writeDatagram(datagram, destination, quint16(port)) != datagram.size()) {
            QTextStream(stderr) << "UDP send failed: " << socket.errorString() << '\n';
            application.exit(1);
            return;
        }
        ++sent;
        if (sent >= packetCount) {
            QTextStream(stdout) << "Sent " << sent << " custom packets to "
                                << destination.toString() << ':' << port << ".\n";
            application.quit();
        }
    };
    QObject::connect(&timer, &QTimer::timeout, &application, sendPacket);
    timer.start(intervalMs);
    QTimer::singleShot(0, &application, sendPacket);
    return application.exec();
}
