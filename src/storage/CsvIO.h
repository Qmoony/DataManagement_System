#pragma once

#include "Table.h"

namespace CsvIO {
    // 从指定路径读取 CSV,首行视为表头。失败抛 std::runtime_error。
    Table load(const QString& filepath, const QString& tableName);

    // 写回 CSV(全量覆盖)。失败抛 std::runtime_error。
    void save(const QString& filepath, const Table& table);
}
