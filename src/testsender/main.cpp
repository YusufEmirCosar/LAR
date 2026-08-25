
#include "domain/statevalidator.h"
#include "infrastructure/mapping/json_mapping_repository.h"
#include "testsender/scenarios.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QHostAddress>
#include <QTextStream>
#include <QTimer>
#include <QUdpSocket>

#include <functional>

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("lar-test-sender"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Send deterministic Plane + Target or DLZ test packets over UDP."));
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
    const QCommandLineOption scenarioOption({QStringLiteral("s"), QStringLiteral("scenario")},
                                            QStringLiteral("Built-in scenario to send."),
                                            QStringLiteral("name"), QStringLiteral("default"));
    const QCommandLineOption listScenariosOption(
        QStringLiteral("list-scenarios"), QStringLiteral("List built-in scenarios and exit."));
    parser.addOptions({hostOption, portOption, countOption, intervalOption, mapOption,
                       scenarioOption, listScenariosOption});
    parser.process(application);

    if (parser.isSet(listScenariosOption)) {
        QTextStream output(stdout);
        for (const QString &name : TestSenderScenarios::names())
            output << name << "\t" << TestSenderScenarios::description(name) << '\n';
        return 0;
    }

    const QString scenario = parser.value(scenarioOption);
    if (!TestSenderScenarios::contains(scenario)) {
        QTextStream(stderr) << "Unknown scenario: " << scenario
                            << ". Use --list-scenarios to see valid names.\n";
        return 2;
    }

    bool portOk = false;
    bool countOk = false;
    bool intervalOk = false;
    const int port = parser.value(portOption).toInt(&portOk);
    const int packetCount = parser.value(countOption).toInt(&countOk);
    const QString intervalText =
        parser.isSet(intervalOption)
            ? parser.value(intervalOption)
            : QString::number(TestSenderScenarios::defaultIntervalMs(scenario));
    const int intervalMs = intervalText.toInt(&intervalOk);
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
    TestSenderScenarios::initialize(scenario, &state.plane, &state.target, &state.dlzInputs);

    QUdpSocket socket;
    QTimer timer;
    timer.setTimerType(Qt::PreciseTimer);
    int sent = 0;
    const std::function<void()> sendPacket = [&] {
        const double elapsedSeconds = double(sent) * double(intervalMs) / 1000.0;
        TestSenderScenarios::update(scenario, sent, elapsedSeconds, &state.plane, &state.target,
                                    &state.dlzInputs);
        // The mapping's availability mask is the validation scope. This lets
        // the same sender exercise legacy LAR maps and the atomic DLZ map.
        state.availableFields = mapping.availableFields();
        QString validationError;
        if (!StateValidator::validate(state, &validationError)) {
            QTextStream(stderr) << "Scenario generated invalid state: " << validationError << '\n';
            application.exit(1);
            return;
        }
        const QByteArray datagram = mapping.encode(state);
        if (socket.writeDatagram(datagram, destination, quint16(port)) != datagram.size()) {
            QTextStream(stderr) << "UDP send failed: " << socket.errorString() << '\n';
            application.exit(1);
            return;
        }
        ++sent;
        if (sent >= packetCount) {
            QTextStream(stdout) << "Sent " << sent << " packets using scenario '" << scenario
                                << "' to " << destination.toString() << ':' << port << ".\n";
            application.quit();
        }
    };
    QObject::connect(&timer, &QTimer::timeout, &application, sendPacket);
    timer.start(intervalMs);
    QTimer::singleShot(0, &application, sendPacket);
    return application.exec();
}
