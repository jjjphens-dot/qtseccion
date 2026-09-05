#include "jsonrepository.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cmath>

namespace takeout {
namespace {
const QStringList collections = {"accounts", "shops", "dishes", "carts", "orders"};
Error corrupt(const QString& field) {
    return {ErrorCode::CorruptData, QStringLiteral("数据结构无效：%1").arg(field), field};
}
}
Result<StoreSnapshot> JsonRepository::load() const {
    if (!QFileInfo::exists(m_path)) {
        if (QFileInfo::exists(m_path + ".bak"))
            return Result<StoreSnapshot>::failure(notImplemented(QStringLiteral("备份恢复（W03/W08）")));
        return Result<StoreSnapshot>::success(StoreSnapshot{});
    }
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return Result<StoreSnapshot>::failure({ErrorCode::Persistence, file.errorString(), m_path});
    if (file.size() > Limits::MaxFileBytes)
        return Result<StoreSnapshot>::failure(corrupt(QStringLiteral("fileSize")));
    const auto bytes = file.read(Limits::MaxFileBytes + 1);
    if (file.error() != QFileDevice::NoError)
        return Result<StoreSnapshot>::failure({ErrorCode::Persistence, file.errorString(), m_path});
    if (bytes.size() > Limits::MaxFileBytes)
        return Result<StoreSnapshot>::failure(corrupt(QStringLiteral("fileSize")));
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return Result<StoreSnapshot>::failure(corrupt(QStringLiteral("JSON")));
    const auto root = doc.object();
    const auto version = root.value("schemaVersion");
    if (!version.isDouble() || std::floor(version.toDouble()) != version.toDouble())
        return Result<StoreSnapshot>::failure(corrupt(QStringLiteral("schemaVersion")));
    if (version.toDouble() != Limits::SchemaVersion)
        return Result<StoreSnapshot>::failure({ErrorCode::UnsupportedVersion,
            QStringLiteral("不支持的数据版本"), "schemaVersion"});
    const auto revision = root.value("revision");
    if (!revision.isDouble() || revision.toDouble() < 0
        || revision.toDouble() > 9007199254740991.0
        || std::floor(revision.toDouble()) != revision.toDouble())
        return Result<StoreSnapshot>::failure(corrupt(QStringLiteral("revision")));
    const auto date = QDateTime::fromString(root.value("savedAt").toString(), Qt::ISODateWithMs);
    if (!date.isValid())
        return Result<StoreSnapshot>::failure(corrupt(QStringLiteral("savedAt")));
    for (const auto& key : collections) {
        if (!root.value(key).isArray())
            return Result<StoreSnapshot>::failure(corrupt(key));
    }
    // Fail closed: full entity codec/invariants belong to W03, not this skeleton.
    for (const auto& key : collections) {
        if (!root.value(key).toArray().isEmpty())
            return Result<StoreSnapshot>::failure(notImplemented(QStringLiteral("非空业务数据加载（W03）")));
    }
    if (root.size() != 8)
        return Result<StoreSnapshot>::failure(corrupt(QStringLiteral("unknownFields")));
    StoreSnapshot snapshot;
    snapshot.revision = revision.toInteger();
    snapshot.savedAt = date;
    return Result<StoreSnapshot>::success(snapshot);
}

Result<void> JsonRepository::save(const StoreSnapshot& snapshot) {
    if (!snapshot.isEmpty())
        return Result<void>::failure(notImplemented(QStringLiteral("非空业务数据保存（W03）")));
    if (snapshot.schemaVersion != Limits::SchemaVersion || snapshot.revision < 0
        || snapshot.revision > 9007199254740991LL || !snapshot.savedAt.isValid())
        return Result<void>::failure(corrupt(QStringLiteral("snapshot")));
    // Never overwrite an unsupported/corrupt business file with the empty shell.
    const auto previous = load();
    if (!previous.ok()) return Result<void>::failure(previous.error());
    QJsonObject root{{"schemaVersion", snapshot.schemaVersion},
                     {"revision", snapshot.revision},
                     {"savedAt", snapshot.savedAt.toUTC().toString(Qt::ISODateWithMs)}};
    for (const auto& key : collections) root.insert(key, QJsonArray{});
    const auto bytes = QJsonDocument(root).toJson();
    QSaveFile file(m_path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly))
        return Result<void>::failure({ErrorCode::Persistence, file.errorString(), m_path});
    if (file.write(bytes) != bytes.size()) {
        const auto error = file.errorString();
        file.cancelWriting();
        return Result<void>::failure({ErrorCode::Persistence, error, m_path});
    }
    if (!file.commit())
        return Result<void>::failure({ErrorCode::Persistence, file.errorString(), m_path});
    return Result<void>::success();
}
} // namespace takeout
