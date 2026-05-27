#pragma once

#include <QString>
#include <stdexcept>

class SqlError : public std::runtime_error {
public:
    SqlError(const QString& message, int position = -1)
        : std::runtime_error(message.toStdString()),
          message_(message),
          position_(position) {}

    const QString& message() const noexcept { return message_; }
    int position() const noexcept { return position_; } // 1-based column,-1 表示未知

private:
    QString message_;
    int     position_;
};
