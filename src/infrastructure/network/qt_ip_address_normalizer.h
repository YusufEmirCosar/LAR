#pragma once

/**
 * @file qt_ip_address_normalizer.h
 * @brief Canonicalization helper for exact IPv4 and IPv6 policy matching.
 */

#include <QHostAddress>
#include <QString>

QString canonicalIpAddress(const QHostAddress &address);
QString canonicalIpAddress(const QString &address);
