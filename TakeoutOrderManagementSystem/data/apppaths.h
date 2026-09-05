#pragma once
#include <QString>
namespace takeout {
struct AppPaths {
    QString directory;
    static AppPaths resolve(const QString& overrideDirectory = {});
    QString dataFile() const;
    QString lockFile() const;
};
} // namespace takeout
