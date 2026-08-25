#include "application/request_result_gate.h"

#include <QtTest>

namespace {

MappingLoadResult resultFor(quint64 request, bool loaded = true) {
    MappingLoadResult result;
    result.request = {request};
    result.loaded = loaded;
    return result;
}

CommandDispatch accepted(quint64 request) {
    return {{request}, true, {}};
}

} // namespace

class RequestResultGateTests final : public QObject {
    Q_OBJECT

  private slots:
    void selectsExactSynchronousCompletionAmongStaleEvents();
    void acceptsOneMatchingAsynchronousCompletion();
    void rejectedDispatchClearsCandidatesAndPendingRequest();
};

void RequestResultGateTests::selectsExactSynchronousCompletionAmongStaleEvents() {
    RequestResultGate<MappingLoadResult> gate;
    gate.beginDispatch();
    QVERIFY(!gate.receive(resultFor(41)).has_value());
    QVERIFY(!gate.receive(resultFor(42)).has_value());
    QVERIFY(!gate.receive(resultFor(43)).has_value());

    const auto completion = gate.finishDispatch(accepted(42));
    QVERIFY(completion.has_value());
    QCOMPARE(completion->request, RuntimeRequestId{42});
    QVERIFY(completion->loaded);
    QVERIFY(!gate.pendingRequest().isValid());
}

void RequestResultGateTests::acceptsOneMatchingAsynchronousCompletion() {
    RequestResultGate<MappingLoadResult> gate;
    gate.beginDispatch();
    QVERIFY(!gate.finishDispatch(accepted(7)).has_value());
    QCOMPARE(gate.pendingRequest(), RuntimeRequestId{7});

    QVERIFY(!gate.receive(resultFor(6)).has_value());
    const auto completion = gate.receive(resultFor(7));
    QVERIFY(completion.has_value());
    QVERIFY(!gate.receive(resultFor(7)).has_value());
    QVERIFY(!gate.pendingRequest().isValid());
}

void RequestResultGateTests::rejectedDispatchClearsCandidatesAndPendingRequest() {
    RequestResultGate<MappingLoadResult> gate;
    gate.beginDispatch();
    QVERIFY(!gate.receive(resultFor(9)).has_value());
    QVERIFY(!gate.finishDispatch({{}, false, QStringLiteral("rejected")}).has_value());
    QVERIFY(!gate.pendingRequest().isValid());
    QVERIFY(!gate.receive(resultFor(9)).has_value());
}

QTEST_GUILESS_MAIN(RequestResultGateTests)
#include "request_result_gate_tests.moc"
