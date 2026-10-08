#pragma once

#include <QString>

// Points are u128 on chain and arrive as decimal strings. They stay strings here: no
// fixed-width integer or JS Number holds every u128, and QString::toULongLong() reads
// anything past 2^64 as 0.
namespace points {

// `value` without leading zeros. Anything that isn't a non-negative integer counts as "0".
inline QString normalized(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return QStringLiteral("0");
    for (const QChar c : trimmed)
        if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            return QStringLiteral("0");
    qsizetype start = 0;
    while (start < trimmed.size() - 1 && trimmed.at(start) == QLatin1Char('0'))
        ++start;
    return trimmed.mid(start);
}

inline bool isZero(const QString& value)
{
    return normalized(value) == QLatin1String("0");
}

inline QString add(const QString& a, const QString& b)
{
    const QString x = normalized(a), y = normalized(b);
    QString sum;
    int carry = 0;
    for (qsizetype i = x.size() - 1, j = y.size() - 1; i >= 0 || j >= 0 || carry; --i, --j) {
        const int digit = (i >= 0 ? x.at(i).digitValue() : 0) + (j >= 0 ? y.at(j).digitValue() : 0) + carry;
        sum.prepend(QChar(u'0' + digit % 10));
        carry = digit / 10;
    }
    return normalized(sum);
}

} // namespace points
