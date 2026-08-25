#include "application/ip_access_policy.h"
#include "infrastructure/network/qt_ip_access_policy_repository.h"
#include "infrastructure/network/qt_ip_address_normalizer.h"

#include <QtTest>

class IpAccessPolicyTests final : public QObject {
    Q_OBJECT

  private slots:
    void parsesExactAddressesAndComments();
    void rejectsMalformedOrEmptyFiles();
};

void IpAccessPolicyTests::parsesExactAddressesAndComments() {
    IpAccessPolicy policy;
    QString error;
    QVERIFY(QtIpAccessPolicyRepository::parseText(
        QByteArrayLiteral("# loopback\n 127.0.0.1\n::ffff:192.0.2.7\n2001:DB8::1\n"), &policy,
        &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(policy.mode(), IpAccessPolicy::Mode::WhitelistOnly);
    QCOMPARE(policy.addresses().size(), 3);
    QVERIFY(policy.accepts(canonicalIpAddress(QStringLiteral("127.0.0.1"))));
    QVERIFY(policy.accepts(canonicalIpAddress(QStringLiteral("::ffff:192.0.2.7"))));
    QVERIFY(policy.accepts(canonicalIpAddress(QStringLiteral("2001:db8::1"))));
    QVERIFY(!policy.accepts(canonicalIpAddress(QStringLiteral("127.0.0.2"))));
    QCOMPARE(canonicalIpAddress(QStringLiteral("127.000.000.001")), QStringLiteral("127.0.0.1"));
}

void IpAccessPolicyTests::rejectsMalformedOrEmptyFiles() {
    IpAccessPolicy policy;
    QString error;
    QVERIFY(!QtIpAccessPolicyRepository::parseText(QByteArrayLiteral("\n# only comments\n"),
                                                   &policy, &error));
    QVERIFY(error.contains(QStringLiteral("does not contain")));
    QVERIFY(!QtIpAccessPolicyRepository::parseText(QByteArrayLiteral("10.0.0.1/24\n"), &policy,
                                                   &error));
    QVERIFY(error.contains(QStringLiteral("Invalid IP")));
    QVERIFY(!QtIpAccessPolicyRepository::parseText(QByteArrayLiteral("10.0.0.1 # inline\n"),
                                                   &policy, &error));
}

QTEST_MAIN(IpAccessPolicyTests)
#include "ip_access_policy_tests.moc"
