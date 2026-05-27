#include "Table.h"

int Table::columnIndex(const QString& col) const {
    for (int i = 0; i < columns.size(); ++i) {
        if (columns[i].compare(col, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

bool Table::hasColumn(const QString& col) const {
    return columnIndex(col) >= 0;
}
