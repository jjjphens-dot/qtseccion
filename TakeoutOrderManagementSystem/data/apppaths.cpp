#include "apppaths.h"
#include <QDir>
#include <QStandardPaths>
namespace takeout {
AppPaths AppPaths::resolve(const QString& overrideDirectory) {
    return {QDir(overrideDirectory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : overrideDirectory).absolutePath()};
}
QString AppPaths::dataFile() const { return QDir(directory).filePath("appdata.json"); }
QString AppPaths::lockFile() const { return QDir(directory).filePath("appdata.lock"); }
} // namespace takeout
